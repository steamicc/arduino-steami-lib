// SPDX-License-Identifier: GPL-3.0-or-later

#include "SteamiButtons.h"

#include <Arduino.h>

namespace {

constexpr SteamiFunctionInfo kInfo = {SteamiFunction::BUTTONS, "SteamiButtons", "D-PAD expander",
                                      "The A, B and MENU buttons still work.",
                                      "the D-PAD will read as released."};

constexpr uint32_t kDebounceMs = 20;
// After a read that found a whole D-PAD tap: a button pressed again before this time is taken for
// a bounce of the press of that tap, after it for the start of a second tap.
constexpr uint32_t kBounceMs = 5;

// Expander bit for UP, DOWN, LEFT, RIGHT, in SteamiButton order.
constexpr uint8_t kDpadBits[4] = {MCP23009_BTN_UP, MCP23009_BTN_DOWN, MCP23009_BTN_LEFT,
                                  MCP23009_BTN_RIGHT};

// The object the interrupts of A, B and MENU and the D-PAD callbacks update: the one started or
// called last, nullptr once it is gone. The handlers hold no pointer of their own, so none can
// dangle.
SteamiButtons* gActive = nullptr;

}  // namespace

SteamiButtons::SteamiButtons(SteamiBoard& board) : _handle(board, kInfo) {}

SteamiButtons::~SteamiButtons() {
    noInterrupts();
    if (gActive == this) {
        gActive = nullptr;
    }
    interrupts();
}

bool SteamiButtons::begin() {
    const bool wasStarted = _handle.status() != SteamiStatus::NOT_STARTED;
    const bool ok = _handle.begin();
    if (!_pinsReady) {
        _pinsReady = true;
        const SteamiBoardPins& pins = _handle.board().pins();
        _mcuPins[0] = pins.buttonA;
        _mcuPins[1] = pins.buttonB;
        _mcuPins[2] = pins.buttonMenu;
        void (*const handlers[MCU_COUNT])() = {edgeA, edgeB, edgeMenu};
        noInterrupts();
        gActive = this;
        interrupts();
        for (uint8_t i = 0; i < MCU_COUNT; i++) {
            pinMode(_mcuPins[i], INPUT_PULLUP);
            // CHANGE: the debounce runs on both edges. attachInterrupt() keeps the pull-up.
            attachInterrupt(digitalPinToInterrupt(_mcuPins[i]), handlers[i], CHANGE);
        }
    }
    if (ok && !wasStarted) {
        SteamiMCP23009E* expander = _handle.board().mcp23009e();
        if (expander != nullptr) {
            for (uint8_t d = 0; d < 4; d++) {
                expander->setup(kDpadBits[d], MCP23009_DIR_INPUT, MCP23009_PULLUP);
            }
            _dpadLevel = 0;
            _dpadUnregistered = registerDpad(*expander, 0x0F);
        }
    }
    return ok;
}

// Registers the D-PAD callbacks of the directions in `mask` (bit d) and reads their level, which
// the registration takes as the last known one: it gives no event. Returns the directions left
// unregistered by a bus error (all of `mask` when the level read fails), for the next sample().
uint8_t SteamiButtons::registerDpad(SteamiMCP23009E& expander, uint8_t mask) {
    uint8_t done = 0;
    for (uint8_t d = 0; d < 4; d++) {
        if ((mask & (1U << d)) == 0) {
            continue;
        }
        const bool registered = expander.interruptOnChange(kDpadBits[d], [d](uint8_t level) {
            if (gActive != nullptr) {
                gActive->dpadEvent(d, level);
            }
        });
        if (registered) {
            done |= 1U << d;
        }
    }
    uint8_t port = 0xFF;
    if (done != 0 && !expander.gpio(port)) {
        return mask;
    }
    for (uint8_t d = 0; d < 4; d++) {
        const uint8_t bit = 1U << d;
        if ((done & bit) == 0) {
            continue;
        }
        if ((port & (1U << kDpadBits[d])) == 0) {
            _dpadLevel |= bit;
        } else {
            _dpadLevel = static_cast<uint8_t>(_dpadLevel & ~bit);
        }
    }
    return static_cast<uint8_t>(mask & ~done);
}

int SteamiButtons::index(SteamiButton button) {
    const int i = static_cast<int>(button);
    return (i >= 1 && i <= COUNT) ? i - 1 : -1;
}

bool SteamiButtons::debounce(uint8_t i, bool down, uint32_t t) {
    const uint8_t bit = 1U << i;
    if (down == ((_state & bit) != 0) || t - _changedAt[i] < kDebounceMs) {
        return false;
    }
    _state = static_cast<uint8_t>(_state ^ bit);
    _changedAt[i] = t;
    if (down) {
        _pressed |= bit;
        if (_presses[i] != UINT8_MAX) {
            _presses[i]++;
        }
    } else {
        _released |= bit;
    }
    return true;
}

// Interrupt of A, B or MENU: the debounce at the time of the edge, with the level read now.
void SteamiButtons::edge(uint8_t i) {
    if (_primed) {
        debounce(i, digitalRead(_mcuPins[i]) == LOW, millis());
    }
}

void SteamiButtons::edgeA() {
    if (gActive != nullptr) {
        gActive->edge(0);
    }
}

void SteamiButtons::edgeB() {
    if (gActive != nullptr) {
        gActive->edge(1);
    }
}

void SteamiButtons::edgeMenu() {
    if (gActive != nullptr) {
        gActive->edge(2);
    }
}

// D-PAD callback, run by SteamiMCP23009E::interruptEvent() (from sample() or from a poll() of the
// sketch), once per event in order: the level captured at the interrupt, then the current one.
void SteamiButtons::dpadEvent(uint8_t d, uint8_t level) {
    const uint8_t bit = 1U << d;
    if (level == MCP23009_LOGIC_LOW) {
        _dpadDown |= bit;
        _dpadLevel |= bit;
    } else {
        _dpadUp |= bit;
        _dpadLevel = static_cast<uint8_t>(_dpadLevel & ~bit);
    }
}

// Reads the latch of the expander, interrupts enabled (the read uses the I2C bus). `level`: the
// D-PAD buttons pressed now; `down` and `up`: a pressed or a released level seen since the last
// read. False on a bus error or without the expander: the D-PAD then reads released.
bool SteamiButtons::readDpad(uint8_t& level, uint8_t& down, uint8_t& up) {
    bool ok = false;
    if (_handle.isAvailable()) {
        SteamiMCP23009E* expander = _handle.board().mcp23009e();
        ok = expander != nullptr && expander->interruptEvent();
    }
    level = ok ? _dpadLevel : 0;
    down = _dpadDown;
    up = _dpadUp;
    _dpadDown = 0;
    _dpadUp = 0;
    return ok;
}

void SteamiButtons::waitSince(uint32_t since, uint32_t ms) {
    const uint32_t waited = millis() - since;
    if (waited < ms) {
        delay(ms - waited);
    }
}

// The latch keeps a change, but not its time: a bounce made just after a read would look, at the
// next read, like a new tap. So once a read changed the D-PAD, the latch is read again 20 ms
// later: what changed in between is a bounce, and only the level then counts. A change that level
// gives is settled the same way, once.
//
// `tapped`: the D-PAD buttons the read found as a whole tap, pressed then released. Pressed again
// at the read 20 ms later, such a button may have bounced (the read fell while the contact of its
// press opened for a moment) or been tapped again. A read 5 ms after the first tells them apart:
// still released then, the button was really released, and the press is a second tap.
void SteamiButtons::settleDpad(uint32_t since, uint8_t tapped) {
    uint8_t lifted = 0;  // bit d: a whole tap whose release is not a bounce of its press
    if (tapped != 0) {
        waitSince(since, kBounceMs);
        uint8_t level = 0;
        uint8_t down = 0;
        uint8_t up = 0;
        readDpad(level, down, up);
        lifted = static_cast<uint8_t>(tapped & ~level);
    }
    for (uint8_t round = 0; round < 2; round++) {
        waitSince(since, kDebounceMs);
        uint8_t level = 0;
        uint8_t down = 0;
        uint8_t up = 0;
        readDpad(level, down, up);
        noInterrupts();
        const uint32_t t = millis();
        bool changed = false;
        for (uint8_t d = 0; d < 4; d++) {
            const uint8_t i = MCU_COUNT + d;
            const uint8_t bit = 1U << i;
            const bool held = (level & (1U << d)) != 0;
            if (held && (lifted & (1U << d)) != 0 && (_state & bit) != 0) {
                // The release of the first tap, then the press of a second one: the button stays
                // pressed. Both changes are 20 ms or more after the release found at the read.
                _released |= bit;
                _pressed |= bit;
                if (_presses[i] != UINT8_MAX) {
                    _presses[i]++;
                }
                _changedAt[i] = t;
                changed = true;
            } else if (held != ((_state & bit) != 0) && debounce(i, held, t)) {
                changed = true;
            }
        }
        interrupts();
        lifted = 0;
        if (!changed) {
            return;
        }
        since = t;
    }
}

void SteamiButtons::sample() {
    if (_handle.status() == SteamiStatus::NOT_STARTED) {
        begin();
    }
    noInterrupts();
    gActive = this;  // the object called last gets the interrupts and the D-PAD events
    interrupts();
    // The D-PAD first, interrupts enabled: the read uses the I2C bus.
    if (_dpadUnregistered != 0 && _handle.isAvailable()) {
        SteamiMCP23009E* expander = _handle.board().mcp23009e();
        if (expander != nullptr) {
            // A bus error at the registration: a held direction is then a press.
            _dpadUnregistered = registerDpad(*expander, _dpadUnregistered);
        }
    }
    uint8_t level = 0;  // bit d: pressed
    uint8_t down = 0;
    uint8_t up = 0;
    const bool read = readDpad(level, down, up);

    noInterrupts();
    const uint32_t t = millis();
    uint8_t mcu = 0;  // bit i: pressed
    for (uint8_t i = 0; i < MCU_COUNT; i++) {
        if (digitalRead(_mcuPins[i]) == LOW) {
            mcu |= 1U << i;
        }
    }
    if (!_primed) {
        _primed = true;
        _state = static_cast<uint8_t>(mcu | (level << MCU_COUNT));  // held at start: not a press
        for (uint8_t i = 0; i < COUNT; i++) {
            _changedAt[i] = t - kDebounceMs;  // the next change is taken at once
        }
        interrupts();
        return;
    }
    // A, B and MENU: the interrupt took the edges; the level read here catches what it could not
    // (a change at the end of a lock-out, a handler replaced by the sketch).
    for (uint8_t i = 0; i < MCU_COUNT; i++) {
        debounce(i, (mcu & (1U << i)) != 0, t);
    }
    // D-PAD: the first change the latch kept since the last read, at once; the level after it is
    // for settleDpad(). Releases first: a press found as the lever leaves another direction is a
    // rebound unless it is still there 20 ms later, so it waits for settleDpad() too.
    bool settle = false;
    bool released = false;
    uint8_t tapped = 0;  // bit d: pressed then released, both found by this read
    for (uint8_t d = 0; d < 4; d++) {
        const uint8_t i = MCU_COUNT + d;
        const uint8_t bit = 1U << d;
        if ((_state & (1U << i)) != 0 && ((up & bit) != 0 || (level & bit) == 0) &&
            debounce(i, false, t)) {
            settle = true;
            released = true;
        }
    }
    for (uint8_t d = 0; d < 4; d++) {
        const uint8_t i = MCU_COUNT + d;
        const uint8_t bit = 1U << d;
        if ((_state & (1U << i)) != 0 || ((down | level) & bit) == 0) {
            continue;
        }
        // Pressed now, as another direction was released: settleDpad() confirms it or not.
        const bool rebound = released && (level & bit) != 0;
        if (rebound) {
            settle = true;
        } else if (debounce(i, true, t)) {
            settle = true;
            if ((level & bit) == 0) {
                tapped |= bit;
            }
        }
    }
    interrupts();
    if (read && settle) {
        settleDpad(t, tapped);
    }
}

bool SteamiButtons::isPressed(SteamiButton button) {
    sample();
    const int i = index(button);
    noInterrupts();
    const uint8_t state = _state;
    interrupts();
    return i >= 0 && (state & (1U << i)) != 0;
}

// Returns and clears the bit of `button` in `latch`, a latch the interrupts also set.
bool SteamiButtons::take(uint8_t& latch, SteamiButton button) {
    sample();
    const int i = index(button);
    if (i < 0) {
        return false;
    }
    const uint8_t bit = 1U << i;
    noInterrupts();
    const bool set = (latch & bit) != 0;
    latch = static_cast<uint8_t>(latch & ~bit);
    interrupts();
    return set;
}

bool SteamiButtons::wasPressed(SteamiButton button) {
    return take(_pressed, button);
}

bool SteamiButtons::wasReleased(SteamiButton button) {
    return take(_released, button);
}

uint8_t SteamiButtons::getPresses(SteamiButton button) {
    sample();
    const int i = index(button);
    if (i < 0) {
        return 0;
    }
    noInterrupts();
    const uint8_t presses = _presses[i];
    _presses[i] = 0;
    interrupts();
    return presses;
}

SteamiButton SteamiButtons::waitForPress(uint32_t timeoutMs) {
    sample();  // takes what happened before the call, still in the expander latch included
    noInterrupts();
    _pressed = 0;
    interrupts();
    const uint32_t start = millis();
    for (;;) {
        sample();
        int found = -1;
        noInterrupts();
        for (uint8_t i = 0; i < COUNT; i++) {
            if ((_pressed & (1U << i)) != 0) {
                _pressed = static_cast<uint8_t>(_pressed & ~(1U << i));
                found = i;
                break;
            }
        }
        interrupts();
        if (found >= 0) {
            return static_cast<SteamiButton>(found + 1);
        }
        if (timeoutMs != 0 && millis() - start >= timeoutMs) {
            return SteamiButton::NONE;
        }
        delay(5);
    }
}
