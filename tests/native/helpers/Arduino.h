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

inline void delay(unsigned long) {}
