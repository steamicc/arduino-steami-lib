// SPDX-License-Identifier: GPL-3.0-or-later
//
// What SteamiScreen::value() shows: a number or a text, written by the library's own
// formatter (no printf("%f"), no dtostrf), so the native build and both cores print the same.

#pragma once

#include <stddef.h>
#include <stdint.h>

// Most digits after the '.' that a float value shows; format() and setDecimals() clamp to it.
constexpr uint8_t STEAMI_VALUE_MAX_DECIMALS = 4;

class SteamiValue {  // what value() shows; implicit by design, one constructor per student type
   public:
    SteamiValue(int v) : _kind(Kind::INT) { _v.i = v; }
    SteamiValue(unsigned v) : _kind(Kind::UINT) { _v.u = v; }
    SteamiValue(long v) : _kind(Kind::INT) { _v.i = v; }
    SteamiValue(unsigned long v) : _kind(Kind::UINT) { _v.u = v; }
    SteamiValue(float v) : _kind(Kind::FLOAT) { _v.f = v; }
    // double stored as float
    SteamiValue(double v) : _kind(Kind::FLOAT) { _v.f = static_cast<float>(v); }
    // nullptr shows "--"
    SteamiValue(const char* text) : _kind(Kind::TEXT) { _v.s = text; }

    // Writes the text into out (at most size - 1 chars): integers as is, floats with `decimals`
    // digits, "--" for NAN, ±INF, nullptr and floats beyond ±999999999; "-0.0" prints "0.0".
    // decimals is clamped to STEAMI_VALUE_MAX_DECIMALS and floats round half away from zero.
    // A number that does not fit in size - 1 chars prints "--" (cut to size - 1), never a cut
    // number; a text is cut without splitting a UTF-8 sequence. Returns strlen(out); size 0
    // writes nothing and returns 0.
    size_t format(char* out, size_t size, uint8_t decimals) const;

   private:
    enum class Kind : uint8_t { INT, UINT, FLOAT, TEXT } _kind;
    union {
        long i;
        unsigned long u;
        float f;
        const char* s;
    } _v;
};
