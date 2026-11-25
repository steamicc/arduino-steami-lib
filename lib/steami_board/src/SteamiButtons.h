// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "SteamiBoard.h"
#include "SteamiHandle.h"
#include "SteamiTypes.h"

/// The seven buttons: A, B and MENU on MCU pins, the D-PAD on the MCP23009E expander. All are
/// active low. Every call samples the buttons once: a press is seen as soon as one call happens
/// while the button is down, and the bounces of the next 20 ms are ignored. Call the buttons at
/// least every 50 ms (a quick tap lasts about 100 ms); a tap shorter than the time between two
/// calls can be missed (see issue #251).
class SteamiButtons {
   public:
    /// Stores the address of the board only; touches no hardware.
    explicit SteamiButtons(SteamiBoard& board = Steami);

    /// Sets A, B and MENU as inputs with pull-ups, and the D-PAD pins of the expander. False, with
    /// one message, when the expander does not answer: A, B and MENU still work, the D-PAD reads
    /// as released, as it does after a runtime bus error. Idempotent.
    bool begin();
    bool isAvailable() const { return _handle.isAvailable(); }
    SteamiStatus status() const { return _handle.status(); }
    const char* error() const { return _handle.error(); }
    const char* chip() const { return _handle.chip(); }

    /// Debounced state; false for NONE.
    bool isPressed(SteamiButton button);

    /// True once per debounced press (or release) since the last call: the edge is consumed.
    bool wasPressed(SteamiButton button);
    bool wasReleased(SteamiButton button);

    /// Waits for a new debounced press and returns that button; NONE after `timeoutMs`, and 0
    /// waits forever. Presses latched before the call are dropped.
    SteamiButton waitForPress(uint32_t timeoutMs = 0);

   private:
    static constexpr uint8_t COUNT = 7;

    void sample();
    static int index(SteamiButton button);

    SteamiHandle _handle;
    uint32_t _changedAt[COUNT] = {};  // time of the last accepted change
    uint8_t _state = 0;               // debounced, bit i set: button i pressed
    uint8_t _pressed = 0;             // latched edges
    uint8_t _released = 0;
    bool _pinsReady = false;
    bool _primed = false;
};
