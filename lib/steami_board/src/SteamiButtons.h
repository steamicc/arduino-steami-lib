// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "SteamiBoard.h"
#include "SteamiHandle.h"
#include "SteamiTypes.h"

/// The seven buttons: A, B and MENU on MCU pins, the D-PAD on the MCP23009E expander. All are
/// active low, with a 20 ms lock-out debounce: a change is taken at once, then the bounces of the
/// next 20 ms are ignored. Every call (isPressed, wasPressed, wasReleased) samples the buttons.
///
/// A tap made between two calls is not missed, whatever the time between them (issues #251, #341):
/// - A, B and MENU: begin() attaches an interrupt (CHANGE) to each pin, which runs the debounce on
///   every edge, at the time of the edge. A tap made between two calls gives, at the next call,
///   wasPressed() and wasReleased() true and isPressed() false.
/// - D-PAD: no MCU interrupt (INT_EXPANDER, PB0, shares EXTI line 0 with MENU, PA0). Each call
///   reads the latch of the expander (SteamiMCP23009E::interruptEvent()), which keeps the first
///   change made since the previous read: that change is taken at once. The latch does not keep
///   the time of the change, so once a read changed the D-PAD, the call reads it again 20 ms
///   later and only the level then counts: a contact bounce is not a new tap. That call then
///   takes 20 ms more (40 ms at most, for a tap that ends during the wait). A tap made between two
///   calls gives the same as on A at the next call, and a release is reported by the call that
///   reads it. When that read found a whole tap (pressed, then released), a press found 20 ms later
///   is a second tap if the button was still released 5 ms after the read; pressed again within
///   those 5 ms, it is taken for a bounce of the first press.
/// - The D-PAD is one lever: released from one direction, it can touch the opposite one for a
///   moment. A press found in the read that releases another direction counts only if it is still
///   there 20 ms later.
/// - wasPressed() and wasReleased() are flags: several presses (or releases) made between two calls
///   give one, and a press nobody reads is not reported later more than once. getPresses() counts
///   every press (micro:bit get_presses()), for a sketch that counts taps with a slow loop.
///
/// Limits: two D-PAD taps made between two reads of the expander give one (as does a second tap
/// pressed within 5 ms after a read that found the first one whole), and a D-PAD tap made while
/// another expander interrupt is pending leaves no trace in the chip. A sketch that attaches
/// its own interrupt to A_BUTTON, B_BUTTON or MENU_BUTTON (USER_BTN), or to another pin of the same
/// EXTI line (0 for MENU, 7 for A, 8 for B), replaces the one of SteamiButtons: that button is then
/// only sampled at each call. On STM32duino 3.1, detachInterrupt() on any pin of EXTI lines 5 to 9
/// (P1, P2, P7, P10, P12, the Jacdac pins) also turns off the interrupts of A (line 7) and B (line
/// 8) until the next attachInterrupt(). Only one SteamiButtons object gets the interrupts: the one
/// started or called last.
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
    /// after a runtime bus error. A D-PAD direction whose registration hits a bus error is
    /// registered again by the next calls, its level read then (held: a press). Idempotent.
    bool begin();
    bool isAvailable() const { return _handle.isAvailable(); }
    SteamiStatus status() const { return _handle.status(); }
    const char* error() const { return _handle.error(); }
    const char* chip() const { return _handle.chip(); }

    /// Debounced state; false for NONE.
    bool isPressed(SteamiButton button);

    /// True when the button was pressed (or released) since the last call of that method for that
    /// button, and clears that flag: several presses made between two calls give one.
    bool wasPressed(SteamiButton button);
    bool wasReleased(SteamiButton button);

    /// The debounced presses of `button` since the last getPresses() for that button (or since
    /// begin()), and sets that count to zero, like get_presses() on the micro:bit. It stops at 255
    /// and does not depend on wasPressed() or waitForPress(); 0 for NONE.
    uint8_t getPresses(SteamiButton button);

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
    // Lock-out debounce of button i with its level `down` at time t; true when the change is
    // taken. Runs from the interrupt of the pin and from sample(), with the interrupts masked.
    bool debounce(uint8_t i, bool down, uint32_t t);
    void edge(uint8_t i);
    void dpadEvent(uint8_t d, uint8_t level);
    uint8_t registerDpad(SteamiMCP23009E& expander, uint8_t mask);
    bool readDpad(uint8_t& level, uint8_t& down, uint8_t& up);
    void settleDpad(uint32_t since, uint8_t tapped);
    static void waitSince(uint32_t since, uint32_t ms);
    static void edgeA();
    static void edgeB();
    static void edgeMenu();

    SteamiHandle _handle;
    uint32_t _mcuPins[MCU_COUNT] = {};
    // Shared with the interrupts of A, B and MENU: read and written with the interrupts masked.
    uint32_t _changedAt[COUNT] = {};  // time of the last accepted change
    uint8_t _state = 0;               // debounced, bit i set: button i pressed
    uint8_t _pressed = 0;             // bit i set: button i pressed since wasPressed()
    uint8_t _released = 0;
    uint8_t _presses[COUNT] = {};  // presses since getPresses(), up to 255
    bool _primed = false;
    // D-PAD, bit d for kDpadBits[d], set by the expander callbacks: the level of the last event
    // (set: pressed), and a pressed or a released level seen since the last read.
    uint8_t _dpadLevel = 0;
    uint8_t _dpadDown = 0;
    uint8_t _dpadUp = 0;
    uint8_t _dpadUnregistered = 0;  // directions a bus error left unregistered, retried by sample()
    bool _pinsReady = false;
};
