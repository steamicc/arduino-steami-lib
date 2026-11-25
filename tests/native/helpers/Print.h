// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

// Minimal host-side model of Arduino's Print, enough for drivers that
// stream text (dump, diagnostics). println() ends the line with "\n" only.
class Print {
   public:
    virtual ~Print() = default;
    virtual size_t write(uint8_t c) = 0;

    size_t write(const char* s) {
        size_t n = 0;
        while (s && *s) {
            n += write(static_cast<uint8_t>(*s++));
        }
        return n;
    }

    size_t print(const char* s) { return write(s); }
    size_t print(int32_t v) { return write(std::to_string(v).c_str()); }

    size_t print(uint32_t v, int base = 10) {
        if (v == 0) {
            return write("0");
        }
        std::string digits;
        while (v > 0) {
            digits.insert(digits.begin(), "0123456789abcdef"[v % base]);
            v /= base;
        }
        return write(digits.c_str());
    }

    size_t println(const char* s = "") { return print(s) + write("\n"); }
    size_t println(int32_t v) { return print(v) + write("\n"); }
};

// Collects everything written to it so tests can assert on the text.
class StringPrint : public Print {
   public:
    std::string text;

    size_t write(uint8_t c) override {
        text.push_back(static_cast<char>(c));
        return 1;
    }
};
