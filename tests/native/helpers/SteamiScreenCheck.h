// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <unity.h>

#include "SteamiMemoryBackend.h"
#include "SteamiRgb.h"

/// The bounds of some pixels of a memory panel, inclusive; count 0 means none (bounds -1).
struct InkBox {
    int16_t x0, y0, x1, y1;
    size_t count;
};

namespace steami_screen_check {

template <typename Match>
InkBox boxOf(const SteamiMemoryBackend& mem, int16_t rowFrom, int16_t rowTo, Match match) {
    InkBox b{32767, 32767, -1, -1, 0};
    const int16_t from = rowFrom < 0 ? 0 : rowFrom;
    const int16_t to = rowTo >= mem.height() ? static_cast<int16_t>(mem.height() - 1) : rowTo;
    for (int16_t y = from; y <= to; y++) {
        for (int16_t x = 0; x < mem.width(); x++) {
            if (!match(mem.at(x, y))) {
                continue;
            }
            b.x0 = x < b.x0 ? x : b.x0;
            b.y0 = y < b.y0 ? y : b.y0;
            b.x1 = x > b.x1 ? x : b.x1;
            b.y1 = y > b.y1 ? y : b.y1;
            b.count++;
        }
    }
    if (b.count == 0) {
        b.x0 = -1;
        b.y0 = -1;
    }
    return b;
}

}  // namespace steami_screen_check

/// The bounds of the pixels holding mem.stored(c) on the rows rowFrom..rowTo. The row band
/// separates the lines of a subtitle, which share one colour.
inline InkBox inkBox(const SteamiMemoryBackend& mem, SteamiRgb c, int16_t rowFrom = 0,
                     int16_t rowTo = 32767) {
    const uint16_t v = mem.stored(c);
    return steami_screen_check::boxOf(mem, rowFrom, rowTo, [v](uint16_t p) { return p == v; });
}

/// The bounds of every non-zero pixel.
inline InkBox inkBoxAll(const SteamiMemoryBackend& mem) {
    return steami_screen_check::boxOf(mem, 0, 32767, [](uint16_t p) { return p != 0; });
}

/// The box is (x0, y0)..(x1, y1) and holds at least one pixel.
inline void expectBox(const InkBox& b, int x0, int y0, int x1, int y1) {
    TEST_ASSERT_EQUAL_INT_MESSAGE(x0, b.x0, "x0");
    TEST_ASSERT_EQUAL_INT_MESSAGE(y0, b.y0, "y0");
    TEST_ASSERT_EQUAL_INT_MESSAGE(x1, b.x1, "x1");
    TEST_ASSERT_EQUAL_INT_MESSAGE(y1, b.y1, "y1");
    TEST_ASSERT_TRUE_MESSAGE(b.count > 0, "no ink");
}
