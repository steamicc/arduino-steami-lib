// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <vector>

#include "Print.h"
#include "WString.h"

// Mirror the include-guard symbol the real Arduino.h advertises.
// Driver code uses `#ifdef Arduino_h` to gate pin operations; without
// this define those bodies would compile out in native tests and make
// the wake / interrupt paths untestable.
#define Arduino_h 1

#define HIGH 1
#define LOW 0
#define OUTPUT 1
#define INPUT 0
#define INPUT_PULLUP 2

// STeaMi internal pins (arbitrary distinct values on the host).
#define I2C_INT_SDA 200
#define I2C_INT_SCL 201
#define A_BUTTON 202
#define B_BUTTON 203
#define MENU_BUTTON 204
#define RST_EXPANDER 205
#define INT_EXPANDER 206
#define CS_DISPLAY 207
#define RST_DISPLAY 208
#define SPI_INT_MOSI 209
#define SPI_INT_MISO 210
#define SPI_INT_SCK 211

#define FALLING 2
#define RISING 3
#define CHANGE 1

inline std::map<int, int>& gpioPinState() {
    static std::map<int, int> state;
    return state;
}

inline std::map<int, int>& gpioPinMode() {
    static std::map<int, int> mode;
    return mode;
}

inline void pinMode(int pin, int mode) {
    gpioPinMode()[pin] = mode;
}

inline void digitalWrite(int pin, int value) {
    gpioPinState()[pin] = value;
}

inline int digitalRead(int pin) {
    return gpioPinState()[pin];
}

// Simulated monotonic clock in milliseconds. delay() advances it so code that
// polls until millis() - start >= timeout terminates on the host the same way
// it would on hardware. Tests can assign `millisClock() = <value>` directly
// to jump the clock (useful for exercising overflow paths).
//
// Width is pinned to uint32_t to match the Arduino core's millis() return
// type — on Linux `unsigned long` is 64-bit and would silently mask any
// 32-bit overflow bug the code under test might have on the target.
inline uint32_t& millisClock() {
    static uint32_t clock = 0;
    return clock;
}

inline uint32_t millis() {
    return millisClock();
}

// Run at each millisecond the clock advances, when set: what happens on the board while the code
// waits (a finger on a button, for example). Tests that set it clear it at the end.
inline std::function<void()>& clockHook() {
    static std::function<void()> hook;
    return hook;
}

inline void delay(uint32_t ms) {
    if (!clockHook()) {
        millisClock() += ms;
        return;
    }
    for (uint32_t k = 0; k < ms; k++) {
        millisClock() += 1;
        std::function<void()> hook = clockHook();
        if (hook) {
            hook();
        }
    }
}

// Interrupt attached to a pin: the handler and the mode (FALLING, RISING or CHANGE).
struct AttachedInterrupt {
    std::function<void()> isr;
    int mode;
};

// Interrupts attached with attachInterrupt(), by pin. As on the board, attaching again replaces
// the handler of the pin.
inline std::map<int, AttachedInterrupt>& attachedInterrupts() {
    static std::map<int, AttachedInterrupt> attached;
    return attached;
}

inline void attachInterrupt(uint8_t pin, const std::function<void()>& isr, int mode) {
    attachedInterrupts()[pin] = {isr, mode};
}

// Pins passed to detachInterrupt(), in call order.
inline std::vector<int>& detachedInterrupts() {
    static std::vector<int> pins;
    return pins;
}

inline void detachInterrupt(int pin) {
    detachedInterrupts().push_back(pin);
    attachedInterrupts().erase(pin);
}

// Runs the handler attached to `pin`, as an edge on that pin would on the board. Does nothing
// when no handler is attached.
inline void fireInterrupt(int pin) {
    auto it = attachedInterrupts().find(pin);
    if (it != attachedInterrupts().end() && it->second.isr) {
        std::function<void()> isr = it->second.isr;
        isr();
    }
}

// The host runs no interrupt on its own: masking them is a null operation.
inline void noInterrupts() {}
inline void interrupts() {}

inline int digitalPinToInterrupt(int pin) {
    return pin;
}

inline void delayMicroseconds(uint32_t) {}
