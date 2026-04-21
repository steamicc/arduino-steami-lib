// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

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
// it would on hardware. Tests can set() directly to control absolute time.
inline unsigned long& millisClock() {
    static unsigned long clock = 0;
    return clock;
}

inline unsigned long millis() {
    return millisClock();
}

inline void delay(unsigned long ms) {
    millisClock() += ms;
}
