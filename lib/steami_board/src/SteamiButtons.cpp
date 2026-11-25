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

}  // namespace

SteamiButtons::SteamiButtons(SteamiBoard& board) : _handle(board, kInfo) {}

bool SteamiButtons::begin() {
    const bool wasStarted = _handle.status() != SteamiStatus::NOT_STARTED;
    const bool ok = _handle.begin();
    if (!_pinsReady) {
        _pinsReady = true;
        const SteamiBoardPins& pins = _handle.board().pins();
        pinMode(pins.buttonA, INPUT_PULLUP);
        pinMode(pins.buttonB, INPUT_PULLUP);
        pinMode(pins.buttonMenu, INPUT_PULLUP);
    }
    if (ok && !wasStarted) {
        SteamiMCP23009E* expander = _handle.board().mcp23009e();
        if (expander != nullptr) {
            for (uint8_t bit : kDpadBits) {
                expander->setup(bit, MCP23009_DIR_INPUT, MCP23009_PULLUP);
            }
        }
    }
    return ok;
}

int SteamiButtons::index(SteamiButton button) {
    const int i = static_cast<int>(button);
    return (i >= 1 && i <= COUNT) ? i - 1 : -1;
}

void SteamiButtons::sample() {
    if (_handle.status() == SteamiStatus::NOT_STARTED) {
        begin();
    }
    const SteamiBoardPins& pins = _handle.board().pins();
    uint8_t now = 0;
    if (digitalRead(pins.buttonA) == LOW) {
        now |= 1U << 0;
    }
    if (digitalRead(pins.buttonB) == LOW) {
        now |= 1U << 1;
    }
    if (digitalRead(pins.buttonMenu) == LOW) {
        now |= 1U << 2;
    }
    if (_handle.isAvailable()) {
        SteamiMCP23009E* expander = _handle.board().mcp23009e();
        uint8_t port = 0;
        if (expander != nullptr && expander->gpio(port)) {  // on an error the D-PAD reads released
            for (uint8_t i = 0; i < 4; i++) {
                if ((port & (1U << kDpadBits[i])) == 0) {
                    now |= 1U << (3 + i);
                }
            }
        }
    }
    const uint32_t t = millis();
    if (!_primed) {
        _primed = true;
        _state = now;  // held at start: not a press
        for (uint8_t i = 0; i < COUNT; i++) {
            _changedAt[i] = t - kDebounceMs;  // the next change is taken at once
        }
    }
    // Lock-out debounce: a change is taken at once, then the bounces of the next 20 ms are ignored.
    for (uint8_t i = 0; i < COUNT; i++) {
        const uint8_t bit = 1U << i;
        if ((now & bit) == (_state & bit) || t - _changedAt[i] < kDebounceMs) {
            continue;
        }
        _state = (_state & ~bit) | (now & bit);
        _changedAt[i] = t;
        if ((_state & bit) != 0) {
            _pressed |= bit;
        } else {
            _released |= bit;
        }
    }
}

bool SteamiButtons::isPressed(SteamiButton button) {
    sample();
    const int i = index(button);
    return i >= 0 && (_state & (1U << i)) != 0;
}

bool SteamiButtons::wasPressed(SteamiButton button) {
    sample();
    const int i = index(button);
    if (i < 0 || (_pressed & (1U << i)) == 0) {
        return false;
    }
    _pressed &= static_cast<uint8_t>(~(1U << i));
    return true;
}

bool SteamiButtons::wasReleased(SteamiButton button) {
    sample();
    const int i = index(button);
    if (i < 0 || (_released & (1U << i)) == 0) {
        return false;
    }
    _released &= static_cast<uint8_t>(~(1U << i));
    return true;
}

SteamiButton SteamiButtons::waitForPress(uint32_t timeoutMs) {
    _pressed = 0;
    const uint32_t start = millis();
    for (;;) {
        sample();
        for (uint8_t i = 0; i < COUNT; i++) {
            if ((_pressed & (1U << i)) != 0) {
                _pressed &= static_cast<uint8_t>(~(1U << i));
                return static_cast<SteamiButton>(i + 1);
            }
        }
        if (timeoutMs != 0 && millis() - start >= timeoutMs) {
            return SteamiButton::NONE;
        }
        delay(5);
    }
}
