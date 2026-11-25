// SPDX-License-Identifier: GPL-3.0-or-later

#include "SteamiTrig.h"

namespace {

// round(16384 * sin(i degrees)) for i = 0..90.
const int16_t kSinQ14[91] = {
    0,     286,   572,   857,   1143,  1428,  1713,  1997,  2280,  2563,  2845,  3126,  3406,
    3686,  3964,  4240,  4516,  4790,  5063,  5334,  5604,  5872,  6138,  6402,  6664,  6924,
    7182,  7438,  7692,  7943,  8192,  8438,  8682,  8923,  9162,  9397,  9630,  9860,  10087,
    10311, 10531, 10749, 10963, 11174, 11381, 11585, 11786, 11982, 12176, 12365, 12551, 12733,
    12911, 13085, 13255, 13421, 13583, 13741, 13894, 14044, 14189, 14330, 14466, 14598, 14726,
    14849, 14968, 15082, 15191, 15296, 15396, 15491, 15582, 15668, 15749, 15826, 15897, 15964,
    16026, 16083, 16135, 16182, 16225, 16262, 16294, 16322, 16344, 16362, 16374, 16382, 16384};

}  // namespace

int16_t steamiSinQ14(int32_t deg) {
    int32_t d = deg % 360;
    if (d < 0) {
        d += 360;
    }
    if (d <= 90) {
        return kSinQ14[d];
    }
    if (d <= 180) {
        return kSinQ14[180 - d];
    }
    if (d <= 270) {
        return static_cast<int16_t>(-kSinQ14[d - 180]);
    }
    return static_cast<int16_t>(-kSinQ14[360 - d]);
}

int16_t steamiCosQ14(int32_t deg) {
    int32_t d = deg % 360;
    if (d < 0) {
        d += 360;
    }
    return steamiSinQ14(d + 90);
}

int32_t steamiRoundQ14(int32_t v) {
    const int64_t w = v;  // wide: v + 8192 and -v must not overflow
    if (w >= 0) {
        return static_cast<int32_t>((w + 8192) / 16384);
    }
    return static_cast<int32_t>(-((-w + 8192) / 16384));
}

uint32_t steamiIsqrt(uint32_t v) {
    uint32_t result = 0;
    uint32_t bit = 1UL << 30;
    while (bit > v) {
        bit >>= 2;
    }
    while (bit != 0) {
        if (v >= result + bit) {
            v -= result + bit;
            result = (result >> 1) + bit;
        } else {
            result >>= 1;
        }
        bit >>= 2;
    }
    return result;
}
