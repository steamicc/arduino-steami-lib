// SPDX-License-Identifier: GPL-3.0-or-later
//
// Integer sine, cosine and square root for the drawing layer. No floating
// point: the values are Q14 fixed point (16384 stands for 1.0).

#pragma once

#include <stdint.h>

// round(16384 * sin(deg)) for any integer degree (normalised modulo 360).
int16_t steamiSinQ14(int32_t deg);

// round(16384 * cos(deg)) for any integer degree, same as steamiSinQ14(deg + 90).
int16_t steamiCosQ14(int32_t deg);

// v / 16384 rounded to nearest, halves away from zero.
int32_t steamiRoundQ14(int32_t v);

// floor(sqrt(v)).
uint32_t steamiIsqrt(uint32_t v);
