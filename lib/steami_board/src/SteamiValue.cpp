// SPDX-License-Identifier: GPL-3.0-or-later

#include "SteamiValue.h"

#include <math.h>

namespace {

// 10^d for d = 0..STEAMI_VALUE_MAX_DECIMALS.
const uint32_t kPow10[STEAMI_VALUE_MAX_DECIMALS + 1] = {1, 10, 100, 1000, 10000};

// Floats from 1e9 on print "--": below it, the integer part fits in uint32_t.
constexpr float kFloatLimit = 1e9f;

// Writes "--" cut to size - 1 chars; size > 0.
size_t writeDashes(char* out, size_t size) {
    size_t n = 0;
    while (n < 2 && n + 1 < size) {
        out[n++] = '-';
    }
    out[n] = '\0';
    return n;
}

// Number of decimal digits of v, 1 for 0.
template <typename U>
size_t digitCount(U v) {
    size_t n = 1;
    while (v >= 10) {
        v /= 10;
        n++;
    }
    return n;
}

// Writes the count lowest decimal digits of v so that the last one lands at end[-1],
// with leading zeros when v has fewer digits.
template <typename U>
void writeDigits(char* end, U v, size_t count) {
    for (size_t i = 0; i < count; i++) {
        *--end = static_cast<char>('0' + static_cast<int>(v % 10));
        v /= 10;
    }
}

size_t formatInteger(char* out, size_t size, bool negative, unsigned long magnitude) {
    const size_t digits = digitCount(magnitude);
    const size_t length = digits + (negative ? 1 : 0);
    if (length > size - 1) {
        return writeDashes(out, size);
    }
    if (negative) {
        out[0] = '-';
    }
    writeDigits(out + length, magnitude, digits);
    out[length] = '\0';
    return length;
}

// Float and uint32_t arithmetic only (single-precision FPU, no double, no printf).
size_t formatFloat(char* out, size_t size, float v, uint8_t decimals) {
    if (isnan(v) || !(fabsf(v) < kFloatLimit)) {
        return writeDashes(out, size);
    }
    const uint8_t d = decimals < STEAMI_VALUE_MAX_DECIMALS ? decimals : STEAMI_VALUE_MAX_DECIMALS;
    const float magnitude = fabsf(v);
    uint32_t whole = static_cast<uint32_t>(magnitude);
    // Exact: whole is magnitude truncated toward zero, so the difference loses no bit.
    const float fraction = magnitude - static_cast<float>(whole);
    uint32_t decimalsValue =
        static_cast<uint32_t>(lroundf(fraction * static_cast<float>(kPow10[d])));
    if (decimalsValue >= kPow10[d]) {  // half away from zero carried into the integer part
        decimalsValue -= kPow10[d];
        whole++;
    }
    const bool negative = v < 0.0f && (whole != 0 || decimalsValue != 0);
    const size_t digits = digitCount(whole);
    const size_t length = (negative ? 1 : 0) + digits + (d > 0 ? 1 + d : 0);
    if (length > size - 1) {
        return writeDashes(out, size);
    }
    char* p = out;
    if (negative) {
        *p++ = '-';
    }
    p += digits;
    writeDigits(p, whole, digits);
    if (d > 0) {
        *p++ = '.';
        p += d;
        writeDigits(p, decimalsValue, d);
    }
    *p = '\0';
    return length;
}

// Copies at most size - 1 bytes of text, backing off to the start of a UTF-8 sequence that
// the cut would split.
size_t formatText(char* out, size_t size, const char* text) {
    size_t n = 0;
    while (n + 1 < size && text[n] != '\0') {
        n++;
    }
    while (n > 0 && (static_cast<uint8_t>(text[n]) & 0xC0) == 0x80) {
        n--;
    }
    for (size_t i = 0; i < n; i++) {
        out[i] = text[i];
    }
    out[n] = '\0';
    return n;
}

}  // namespace

size_t SteamiValue::format(char* out, size_t size, uint8_t decimals) const {
    if (out == nullptr || size == 0) {
        return 0;
    }
    switch (_kind) {
        case Kind::INT:
            if (_v.i < 0) {
                // 0 - (unsigned long)i is |i| without overflow, LONG_MIN included.
                return formatInteger(out, size, true, 0UL - static_cast<unsigned long>(_v.i));
            }
            return formatInteger(out, size, false, static_cast<unsigned long>(_v.i));
        case Kind::UINT:
            return formatInteger(out, size, false, _v.u);
        case Kind::FLOAT:
            return formatFloat(out, size, _v.f, decimals);
        case Kind::TEXT:
            if (_v.s == nullptr) {
                return writeDashes(out, size);
            }
            return formatText(out, size, _v.s);
    }
    return writeDashes(out, size);
}
