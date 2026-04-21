// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <map>

#define HIGH 1
#define LOW 0
#define OUTPUT 1
#define INPUT 0

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

inline void delay(uint32_t ms) {
    millisClock() += ms;
}
