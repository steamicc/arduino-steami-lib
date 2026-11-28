// SPDX-License-Identifier: GPL-3.0-or-later

#include "SteamiButtons.h"

#include <Arduino.h>

namespace {

constexpr SteamiFunctionInfo kInfo = {SteamiFunction::BUTTONS, "SteamiButtons", "D-PAD expander",
                                      "The A, B and MENU buttons still work.",
                                      "the D-PAD will read as released."};

constexpr uint32_t kDebounceMs = 20;

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
                // No event for the level at registration: it is read below.
                expander->interruptOnChange(kDpadBits[d], [d](uint8_t level) {
                    if (gActive != nullptr) {
                        gActive->dpadEvent(d, level);
                    }
                });
            }
            uint8_t port = 0xFF;
            expander->gpio(port);  // on an error the D-PAD reads released
            _dpadLevel = 0;
            for (uint8_t d = 0; d < 4; d++) {
                if ((port & (1U << kDpadBits[d])) == 0) {
                    _dpadLevel |= 1U << d;
                }
                _dpadEventAt[d] = millis() - kDebounceMs;
            }
        }
    }
    return ok;
}

int SteamiButtons::index(SteamiButton button) {
    const int i = static_cast<int>(button);
    return (i >= 1 && i <= COUNT) ? i - 1 : -1;
}

void SteamiButtons::debounce(uint8_t i, bool down, uint32_t t) {
    const uint8_t bit = 1U << i;
    if (down == ((_state & bit) != 0) || t - _changedAt[i] < kDebounceMs) {
        return;
    }
    _state = static_cast<uint8_t>(_state ^ bit);
    _changedAt[i] = t;
    if (down) {
        _pressed |= bit;
    } else {
        _released |= bit;
    }
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
    _dpadEventAt[d] = millis();
    if (level == MCP23009_LOGIC_LOW) {
        _dpadDown |= bit;
        _dpadLevel |= bit;
    } else {
        _dpadLevel = static_cast<uint8_t>(_dpadLevel & ~bit);
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
    uint8_t dpad = 0;  // bit d: pressed
    if (_handle.isAvailable()) {
        SteamiMCP23009E* expander = _handle.board().mcp23009e();
        if (expander != nullptr && expander->interruptEvent()) {
            dpad = _dpadLevel;
        }  // on an error the D-PAD reads released, with no event
    }
    const uint8_t down = _dpadDown;
    _dpadDown = 0;

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
        _state = static_cast<uint8_t>(mcu | (dpad << MCU_COUNT));  // held at start: not a press
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
    // D-PAD: a press found in the latch or in the level; a release only after 20 ms without an
    // event on that button.
    for (uint8_t d = 0; d < 4; d++) {
        const uint8_t i = MCU_COUNT + d;
        const uint8_t bit = 1U << d;
        const bool pressed = (_state & (1U << i)) != 0;
        if (!pressed && ((down | dpad) & bit) != 0) {
            debounce(i, true, t);
        } else if (pressed && (dpad & bit) == 0 && t - _dpadEventAt[d] >= kDebounceMs) {
            debounce(i, false, t);
        }
    }
    interrupts();
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
