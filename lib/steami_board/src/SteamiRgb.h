// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stdint.h>

/// A colour, stored as one RGB565 word. An integer is not a colour: write SteamiRgb::gray(15),
/// SteamiRgb(255, 128, 0) or {255, 128, 0}, never 15.
class SteamiRgb {
   public:
    /// Packs 8-bit channels into RGB565 the way GC9A01::color565 does.
    constexpr SteamiRgb(uint8_t r, uint8_t g, uint8_t b)
        : _v(static_cast<uint16_t>(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3))) {}

    /// A grey of 16 levels, 0 black to 15 white; a level above 15 is 15.
    static constexpr SteamiRgb gray(uint8_t level0to15) {
        return SteamiRgb(grayChannel(level0to15), grayChannel(level0to15), grayChannel(level0to15));
    }

    /// A colour from an RGB565 word, for example one read back from a display.
    static constexpr SteamiRgb fromRgb565(uint16_t v) { return SteamiRgb(v, RawTag()); }

    constexpr uint16_t rgb565() const { return _v; }

    /// Grey level 0..15 of this colour (luma 77/150/29 of the expanded 8-bit channels).
    constexpr uint8_t gray4() const {
        return static_cast<uint8_t>(
            ((expand5(_v >> 11) * 77 + expand6((_v >> 5) & 63) * 150 + expand5(_v & 31) * 29) >>
             8) >>
            4);
    }

    constexpr bool operator==(SteamiRgb other) const { return _v == other._v; }
    constexpr bool operator!=(SteamiRgb other) const { return _v != other._v; }

   private:
    struct RawTag {};
    constexpr SteamiRgb(uint16_t v, RawTag) : _v(v) {}

    static constexpr uint8_t grayChannel(uint8_t level) {
        return static_cast<uint8_t>(level > 15 ? 255 : level * 17);
    }
    static constexpr uint32_t expand5(uint32_t c) { return (c << 3) | (c >> 2); }
    static constexpr uint32_t expand6(uint32_t c) { return (c << 2) | (c >> 4); }

    uint16_t _v;
};

constexpr SteamiRgb STEAMI_BLACK(0, 0, 0);
constexpr SteamiRgb STEAMI_DARK(102, 102, 102);
constexpr SteamiRgb STEAMI_GRAY(153, 153, 153);
constexpr SteamiRgb STEAMI_LIGHT(187, 187, 187);
constexpr SteamiRgb STEAMI_WHITE(255, 255, 255);
constexpr SteamiRgb STEAMI_GREEN(119, 255, 119);
constexpr SteamiRgb STEAMI_RED(255, 85, 85);
constexpr SteamiRgb STEAMI_BLUE(85, 85, 255);
constexpr SteamiRgb STEAMI_YELLOW(255, 255, 85);
constexpr SteamiRgb STEAMI_GRID_DARK(51, 51, 51);
