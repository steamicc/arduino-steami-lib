// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <functional>
#include <iomanip>
#include <map>
#include <sstream>
#include <string>

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

inline void delay(uint32_t ms) {
    millisClock() += ms;
}

inline void attachInterrupt(uint8_t /* pin */, const std::function<void()>& /* isr */,
                            int /* mode */) {
    // null operation for native tests
}

inline int digitalPinToInterrupt(int pin) {
    return pin;
}

class String {
   public:
    String() = default;

    String(const char* value) : _value(value != nullptr ? value : "") {}

    String(const std::string& value) : _value(value) {}

    String(char value) : _value(1, value) {}

    String(int value) : _value(std::to_string(value)) {}

    String(unsigned int value) : _value(std::to_string(value)) {}

    String(long value) : _value(std::to_string(value)) {}

    String(unsigned long value) : _value(std::to_string(value)) {}

    String(float value, unsigned int decimals) { setFloat(value, decimals); }

    String(double value, unsigned int decimals) { setFloat(value, decimals); }

    size_t length() const { return _value.length(); }

    const char* c_str() const { return _value.c_str(); }

    char operator[](size_t index) const { return _value[index]; }

    String substring(unsigned int from, unsigned int to) const {
        if (from >= _value.length()) {
            return String("");
        }

        if (to > _value.length()) {
            to = _value.length();
        }

        if (to <= from) {
            return String("");
        }

        return String(_value.substr(from, to - from));
    }

    bool endsWith(const char* suffix) const {
        if (suffix == nullptr) {
            return false;
        }

        const std::string suffixString(suffix);

        if (suffixString.length() > _value.length()) {
            return false;
        }

        return _value.compare(_value.length() - suffixString.length(), suffixString.length(),
                              suffixString) == 0;
    }

    void remove(unsigned int index) {
        if (index < _value.length()) {
            _value.erase(index);
        }
    }

    void remove(unsigned int index, unsigned int count) {
        if (index < _value.length()) {
            _value.erase(index, count);
        }
    }

    String& operator=(const char* value) {
        _value = value != nullptr ? value : "";
        return *this;
    }

    String& operator+=(const String& other) {
        _value += other._value;
        return *this;
    }

    String& operator+=(const char* value) {
        if (value != nullptr) {
            _value += value;
        }
        return *this;
    }

    String& operator+=(char value) {
        _value += value;
        return *this;
    }

   private:
    std::string _value;

    void setFloat(double value, unsigned int decimals) {
        std::ostringstream stream;
        stream << std::fixed << std::setprecision(decimals) << value;
        _value = stream.str();
    }
};
