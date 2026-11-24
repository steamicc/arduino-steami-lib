// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <cstdio>
#include <string>
#include <utility>

// Host-side stand-in for the Arduino `String` class, backed by
// std::string. Only the subset used by the drivers is provided; extend it
// when a driver starts relying on another method.
class String {
   public:
    String() = default;
    String(const char* text)
        : _s(text != nullptr ? text : "") {}       // NOLINT(google-explicit-constructor)
    String(const std::string& text) : _s(text) {}  // NOLINT(google-explicit-constructor)
    explicit String(char c) : _s(1, c) {}
    explicit String(int value) : _s(std::to_string(value)) {}
    explicit String(unsigned int value) : _s(std::to_string(value)) {}
    explicit String(long value) : _s(std::to_string(value)) {}
    explicit String(unsigned long value) : _s(std::to_string(value)) {}

    // Arduino formats floats with a fixed number of decimals (dtostrf).
    String(float value, unsigned int decimals) { formatDouble(value, decimals); }
    String(double value, unsigned int decimals) { formatDouble(value, decimals); }

    const char* c_str() const { return _s.c_str(); }
    unsigned int length() const { return static_cast<unsigned int>(_s.size()); }
    char operator[](unsigned int index) const { return index < _s.size() ? _s[index] : '\0'; }

    bool endsWith(const String& suffix) const {
        return _s.size() >= suffix._s.size() &&
               _s.compare(_s.size() - suffix._s.size(), suffix._s.size(), suffix._s) == 0;
    }

    void remove(unsigned int index) {
        if (index < _s.size()) {
            _s.erase(index);
        }
    }

    void remove(unsigned int index, unsigned int count) {
        if (index < _s.size()) {
            _s.erase(index, count);
        }
    }

    String substring(unsigned int from) const { return substring(from, length()); }

    String substring(unsigned int from, unsigned int to) const {
        if (from > to) {
            std::swap(from, to);
        }
        if (from >= _s.size()) {
            return String();
        }
        return String(_s.substr(from, to - from));
    }

    String& operator+=(const String& other) {
        _s += other._s;
        return *this;
    }
    String& operator+=(const char* text) {
        _s += (text != nullptr ? text : "");
        return *this;
    }
    String& operator+=(char c) {
        _s += c;
        return *this;
    }

    bool operator==(const String& other) const { return _s == other._s; }
    bool operator!=(const String& other) const { return _s != other._s; }
    bool operator==(const char* text) const { return _s == (text != nullptr ? text : ""); }
    bool operator!=(const char* text) const { return !(*this == text); }

   private:
    void formatDouble(double value, unsigned int decimals) {
        char buf[64];
        snprintf(buf, sizeof(buf), "%.*f", static_cast<int>(decimals), value);
        _s = buf;
    }

    std::string _s;
};
