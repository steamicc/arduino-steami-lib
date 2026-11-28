// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "SteamiBoard.h"
#include "SteamiHandle.h"
#include "SteamiTypes.h"

/// The seven buttons: A, B and MENU on MCU pins, the D-PAD on the MCP23009E expander. All are
/// active low, with a 20 ms lock-out debounce: a change is taken at once, then the bounces of the
/// next 20 ms are ignored. Every call (isPressed, wasPressed, wasReleased) samples the buttons.
///
/// A tap made between two calls is not missed, whatever the time between them (issue #251):
/// - A, B and MENU: begin() attaches an interrupt (CHANGE) to each pin, which runs the debounce on
///   every edge, at the time of the edge. A tap made between two calls gives, at the next call,
///   wasPressed() and wasReleased() true and isPressed() false. A release then a new tap between
///   two calls give the release and the press.
/// - D-PAD: no MCU interrupt (INT_EXPANDER, PB0, shares EXTI line 0 with MENU, PA0). Each call
///   reads the latch of the expander (SteamiMCP23009E::interruptEvent()), which keeps the first
///   change made since the previous read. A press found there is taken if 20 ms passed since the
///   last accepted change of that button. A release is taken only once the reads found no change
///   on that button for 20 ms (and 20 ms after the last accepted change): a read made during a
///   release bounce then gives no false press. A tap made between two calls gives wasPressed()
///   and isPressed() true at the next call; the release comes at the first call made 20 ms or
///   more after it, if that button did not change again.
/// - At most one press per button between two calls: wasPressed() is a flag, not a counter.
///
/// Limits: a D-PAD tap made before the release of the previous one is reported merges with it;
/// a D-PAD press and its release made while another expander interrupt is pending leave no trace
/// in the chip. The D-PAD is followed with SteamiMCP23009E::watchChanges(): the sketch can register
/// its own callbacks on the D-PAD pins of Steami.mcp23009e(), or disable them, and both keep
/// working; after a reset() of the expander, or after powerOn() following a powerOff(), the next
/// call sets the D-PAD up again (while the expander is off, the D-PAD reads released).
/// A sketch that attaches its own interrupt to A_BUTTON, B_BUTTON or MENU_BUTTON
/// (USER_BTN), or to another pin of the same EXTI line (0 for MENU, 7 for A, 8 for B), replaces
/// the one of SteamiButtons: that button is then only sampled at each call. Only one
/// SteamiButtons object gets the interrupts: the one started or called last.
class SteamiButtons {
   public:
    /// Stores the address of the board only; touches no hardware.
    explicit SteamiButtons(SteamiBoard& board = Steami);

    /// The interrupts of A, B and MENU stay attached and the D-PAD callbacks stay registered on
    /// the expander; once the object is gone, they do nothing.
    ~SteamiButtons();

    /// Sets A, B and MENU as inputs with pull-ups and attaches their interrupts, and sets the D-PAD
    /// pins of the expander with their interrupt callbacks. False, with one message, when the
    /// expander does not answer: A, B and MENU still work, the D-PAD reads as released, as it does
    /// after a runtime bus error. Idempotent.
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
    /// waits forever. Presses made before the call are dropped, the taps latched by an interrupt
    /// or by the expander included, and a button already held must be released first.
    SteamiButton waitForPress(uint32_t timeoutMs = 0);

   private:
    static constexpr uint8_t COUNT = 7;
    static constexpr uint8_t MCU_COUNT = 3;  // A, B, MENU: bits 0 to 2; the D-PAD: bits 3 to 6

    void sample();
    static int index(SteamiButton button);
    bool take(uint8_t& latch, SteamiButton button);
    // Lock-out debounce of button i with its level `down` at time t. Runs from the interrupt of
    // the pin and from sample(), with the interrupts masked.
    void debounce(uint8_t i, bool down, uint32_t t);
    void edge(uint8_t i);
    void dpadEvent(uint8_t d, uint8_t level);
    static void dpadChange(uint8_t gpx, uint8_t level);
    void watchDpad(SteamiMCP23009E& expander, uint8_t mask);
    static void edgeA();
    static void edgeB();
    static void edgeMenu();

    SteamiHandle _handle;
    uint32_t _mcuPins[MCU_COUNT] = {};
    // Shared with the interrupts of A, B and MENU: read and written with the interrupts masked.
    uint32_t _changedAt[COUNT] = {};  // time of the last accepted change
    uint8_t _state = 0;               // debounced, bit i set: button i pressed
    uint8_t _pressed = 0;             // latched edges
    uint8_t _released = 0;
    bool _primed = false;
    // D-PAD, bit d for kDpadBits[d], set by the expander callbacks: the level of the last event
    // (set: pressed), a pressed level seen since the last sample, and the time of the last event.
    uint8_t _dpadLevel = 0;
    uint8_t _dpadDown = 0;
    uint32_t _dpadEventAt[4] = {};
    bool _pinsReady = false;
};
