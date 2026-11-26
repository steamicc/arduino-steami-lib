// SPDX-License-Identifier: GPL-3.0-or-later

#include "SteamiScreen.h"

#include "SteamiFont.h"
#include "SteamiTrig.h"

namespace {

int32_t smaller(int32_t a, int32_t b) {
    return a < b ? a : b;
}

int32_t larger(int32_t a, int32_t b) {
    return a > b ? a : b;
}

/// isqrt of a value the caller has proven non-negative.
int32_t rootOf(int32_t v) {
    return static_cast<int32_t>(steamiIsqrt(static_cast<uint32_t>(v)));
}

bool isNarrow(uint8_t code) {
    return code == '.' || code == ':';
}

}  // namespace

int32_t SteamiScreen::Region::width() const {
    return right < left ? 0 : right - left + 1;
}

uint8_t SteamiScreen::maxChars() {
    return static_cast<uint8_t>(width() / (SteamiDisplay::CHAR_W * k()));
}

void SteamiScreen::setDecimals(uint8_t d) {
    _decimals = d > STEAMI_VALUE_MAX_DECIMALS ? STEAMI_VALUE_MAX_DECIMALS : d;
}

int32_t SteamiScreen::u(int32_t v128) {
    return v128 * width() / 128;  // int32_t: any int16_t v128 times a 32767 px width fits
}

uint8_t SteamiScreen::k() {
    const int32_t v = (static_cast<int32_t>(width()) + 64) / 128;
    return static_cast<uint8_t>(v < 1 ? 1 : smaller(v, 255));  // 255 only past a 32k px panel
}

SteamiScreen::Region SteamiScreen::chord(int32_t top, int32_t bottom) {
    const int32_t r = radius();
    const int32_t cx = centerX();
    const int32_t cy = centerY();
    // Tested before any subtraction, so a row far off the panel never overflows.
    if (top < cy - r || top > cy + r || bottom < cy - r || bottom > cy + r) {
        return {0, -1};
    }
    const int32_t dt = top - cy;
    const int32_t db = bottom - cy;
    const int32_t half = smaller(rootOf(r * r - dt * dt), rootOf(r * r - db * db));
    return {cx - half, cx + half};
}

size_t SteamiScreen::glyphCount(const char* s) {
    return steamiGlyphCount(s);
}

uint8_t SteamiScreen::inkRows(const char* s, size_t glyphs, Ink ink) {
    uint8_t mask = 0;
    size_t n = 0;
    for (uint8_t code = steamiNextGlyph(s); code != 0 && n < glyphs;
         code = steamiNextGlyph(s), n++) {
        if (ink == Ink::LABEL) {
            const uint8_t* g = steamiGlyph5x7(code);
            for (uint8_t c = 0; c < 5; c++) {
                mask |= g[c];
            }
            continue;
        }
        const uint8_t* g = steamiGlyph8x8(code);
        const bool window = ink == Ink::VALUE && isNarrow(code);
        const uint8_t end = window ? 6 : 8;
        for (uint8_t c = window ? 2 : 0; c < end; c++) {
            mask |= g[c];
        }
    }
    return ink == Ink::LABEL ? static_cast<uint8_t>(mask & 0x7F) : mask;
}

SteamiScreen::Region SteamiScreen::inkChord(const char* s, size_t glyphs, Ink ink, int32_t y,
                                            uint8_t f) {
    const uint8_t m = inkRows(s, glyphs, ink);
    if (m == 0) {
        return {0, static_cast<int32_t>(width()) - 1};
    }
    int32_t first = 0;
    while (((m >> first) & 1) == 0) {
        first++;
    }
    int32_t last = 7;
    while (((m >> last) & 1) == 0) {
        last--;
    }
    return chord(y + first * f, y + (last + 1) * f - 1);
}

int32_t SteamiScreen::cellWidth(const char* s, size_t glyphs, Ink ink, uint8_t f) {
    int32_t w = 0;
    size_t n = 0;
    for (uint8_t code = steamiNextGlyph(s); code != 0 && n < glyphs;
         code = steamiNextGlyph(s), n++) {
        if (ink == Ink::LABEL) {
            w += 6;
        } else {
            w += ink == Ink::VALUE && isNarrow(code) ? 4 : 8;
        }
    }
    return w * f;
}

int32_t SteamiScreen::valueWidth(const char* s, uint8_t f) {
    return cellWidth(s, glyphCount(s), Ink::VALUE, f);
}

size_t SteamiScreen::fitGlyphs(const char* s, Ink ink, uint8_t f, int32_t y, Region limit,
                               uint8_t shrink, int32_t pad) {
    for (size_t n = glyphCount(s); n > 0; n--) {
        if (inkRows(s, n, ink) == 0) {
            return n;  // no ink always fits
        }
        const Region c = inkChord(s, n, ink, y, f);
        if (c.width() == 0) {
            continue;
        }
        const int32_t left = larger(c.left + shrink, limit.left);
        const int32_t right = smaller(c.right - shrink, limit.right);
        if (cellWidth(s, n, ink, f) + pad <= right - left + 1) {
            return n;
        }
    }
    return 0;
}

int32_t SteamiScreen::marginNs(int32_t tw, uint8_t f) {
    const int32_t r = radius();
    const int32_t kk = k();
    const int32_t w = width();
    // The safe margin of a 13-cell text; r + 2 when the panel is narrower than 13 cells.
    const int32_t half13 = STEAMI_TITLE_CAP_CELLS * SteamiDisplay::CHAR_W * kk / 2;
    const int32_t cap = r - rootOf(larger(0, r * r - half13 * half13)) + 2;
    const int32_t t = larger(tw, 0);
    // t < 2r below, so (t / 2)^2 < r^2: the argument of the root is positive.
    const int32_t m = t >= 2 * r ? cap : r - rootOf(r * r - (t / 2) * (t / 2)) + 2;
    const int32_t lowest =
        (2 * SteamiDisplay::CHAR_H * f + STEAMI_SCREEN_PAD * kk) * w / (128 * kk);
    return larger(lowest, smaller(m, cap));
}

int32_t SteamiScreen::marginEw(uint8_t f) {
    const int32_t kk = k();
    return (SteamiDisplay::CHAR_W * f + STEAMI_SCREEN_PAD * kk) * width() / (128 * kk);
}

SteamiScreen::Point SteamiScreen::resolve(SteamiAt at, int32_t tw, uint8_t f) {
    const int32_t w = width();
    const int32_t h = height();
    const int32_t ch = SteamiDisplay::CHAR_H * f;
    const int32_t mid = u(64 - 4 * (f / k()));  // rule 3 row of CENTER, W and E (text only)
    const int32_t ns = marginNs(tw, f);
    const int32_t ew = marginEw(f);
    Point p{centerX() - tw / 2, mid};
    switch (at) {
        case SteamiAt::W:
        case SteamiAt::NW:
        case SteamiAt::SW:
            p.x = ew;
            break;
        case SteamiAt::E:
        case SteamiAt::NE:
        case SteamiAt::SE:
            p.x = w - ew - tw;
            break;
        default:
            break;
    }
    switch (at) {
        case SteamiAt::N:
        case SteamiAt::NE:
        case SteamiAt::NW:
            p.y = ns;
            break;
        case SteamiAt::S:
        case SteamiAt::SE:
        case SteamiAt::SW:
            p.y = h - ns - ch;
            break;
        default:
            break;
    }
    return p;
}

void SteamiScreen::drawGlyphs(const char* s, size_t glyphs, Ink ink, int32_t x, int32_t y,
                              SteamiRgb c, uint8_t f) {
    size_t n = 0;
    for (uint8_t code = steamiNextGlyph(s); code != 0 && n < glyphs;
         code = steamiNextGlyph(s), n++) {
        if (ink == Ink::LABEL) {
            glyph(steamiGlyph5x7(code), 0, 5, 7, x, y, c, f);
            x += 6 * f;
        } else if (ink == Ink::VALUE && isNarrow(code)) {
            glyph(steamiGlyph8x8(code), 2, 4, 8, x, y, c, f);
            x += 4 * f;
        } else {
            glyph(steamiGlyph8x8(code), 0, 8, 8, x, y, c, f);
            x += 8 * f;
        }
    }
}

SteamiRgb SteamiScreen::lifted(SteamiRgb fg, SteamiRgb bg) {
    if (isColor()) {
        return fg;
    }
    const int32_t level = smaller(bg.gray4() + STEAMI_MIN_CONTRAST, 15);
    return fg.gray4() < level ? SteamiRgb::gray(static_cast<uint8_t>(level)) : fg;
}
