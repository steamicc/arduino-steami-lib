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

/// The physical glyph factor of a widget at `scale`: max(scale, 1) times k, the scale capped at
/// 255 / k so that the factor always fits a uint8_t.
uint8_t glyphFactor(uint8_t scale, uint8_t k) {
    const int32_t s = smaller(larger(scale, 1), 255 / k);
    return static_cast<uint8_t>(s * k);
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

SteamiScreen::Region SteamiScreen::rowsChord(uint8_t rows, int32_t y, uint8_t f) {
    if (rows == 0) {
        return {0, static_cast<int32_t>(width()) - 1};
    }
    int32_t first = 0;
    while (((rows >> first) & 1) == 0) {
        first++;
    }
    int32_t last = 7;
    while (((rows >> last) & 1) == 0) {
        last--;
    }
    return chord(y + first * f, y + (last + 1) * f - 1);
}

SteamiScreen::Region SteamiScreen::inkChord(const Run& t) {
    return rowsChord(inkRows(t.s, t.glyphs, t.ink), t.y, t.f);
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

size_t SteamiScreen::fitGlyphs(const Run& t, Region* room, int32_t pad) {
    const int32_t shrink = t.shrink;
    const Region limit = room != nullptr ? *room : Region{0, static_cast<int32_t>(width()) - 1};
    size_t n = t.glyphs;
    Region c{};
    for (; n > 0; n--) {
        const uint8_t rows = inkRows(t.s, n, t.ink);
        c = rowsChord(rows, t.y, t.f);
        if (rows == 0) {
            break;  // no ink always fits
        }
        if (c.width() == 0) {
            continue;
        }
        const int32_t left = larger(c.left + shrink, limit.left);
        const int32_t right = smaller(c.right - shrink, limit.right);
        if (cellWidth(t.s, n, t.ink, t.f) + pad <= right - left + 1) {
            break;
        }
    }
    if (n == 0) {
        c = rowsChord(0, t.y, t.f);  // nothing kept: no ink, the whole panel
    }
    if (room != nullptr) {
        *room = {larger(c.left + shrink, limit.left), smaller(c.right - shrink, limit.right)};
    }
    return n;
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

int32_t SteamiScreen::resolveX(SteamiAt at, int32_t tw, uint8_t f) {
    switch (at) {
        case SteamiAt::W:
        case SteamiAt::NW:
        case SteamiAt::SW:
            return marginEw(f);
        case SteamiAt::E:
        case SteamiAt::NE:
        case SteamiAt::SE:
            return width() - marginEw(f) - tw;
        default:
            return centerX() - tw / 2;
    }
}

int32_t SteamiScreen::resolveY(SteamiAt at, int32_t tw, uint8_t f) {
    switch (at) {
        case SteamiAt::N:
        case SteamiAt::NE:
        case SteamiAt::NW:
            return marginNs(tw, f);
        case SteamiAt::S:
        case SteamiAt::SE:
        case SteamiAt::SW:
            return height() - marginNs(tw, f) - SteamiDisplay::CHAR_H * f;
        default:
            return u(64 - 4 * (f / k()));  // rule 3 row of CENTER, W and E (text only)
    }
}

void SteamiScreen::drawGlyphs(const Run& t, int32_t x, SteamiRgb c) {
    const char* s = t.s;
    const Ink ink = t.ink;
    const uint8_t f = t.f;
    const int32_t y = t.y;
    size_t n = 0;
    for (uint8_t code = steamiNextGlyph(s); code != 0 && n < t.glyphs;
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

bool SteamiScreen::widgetCanDraw() {
    if (status() == SteamiStatus::NOT_STARTED) {
        begin();
    }
    return isAvailable() && width() > 0;
}

void SteamiScreen::text(const char* s, SteamiAt at, SteamiRgb c, uint8_t scale) {
    if (!widgetCanDraw() || s == nullptr) {
        return;
    }
    Run t{s, glyphCount(s), 0, Ink::TEXT, glyphFactor(scale, k())};
    const int32_t tw = cellWidth(s, t.glyphs, Ink::TEXT, t.f);
    t.y = resolveY(at, tw, t.f);
    const int32_t ax = resolveX(at, tw, t.f);
    // W runs from the anchor to the chord's right edge - k, E is its mirror image, and a corner
    // takes the chord shrunk by k at each end; N, S and CENTER take the whole chord.
    Region room{at == SteamiAt::W ? ax : 0,
                at == SteamiAt::E ? ax + tw - 1 : static_cast<int32_t>(width()) - 1};
    const bool centred = at == SteamiAt::N || at == SteamiAt::S || at == SteamiAt::CENTER;
    t.shrink = centred ? 0 : k();
    t.glyphs = fitGlyphs(t, &room);
    const int32_t w = cellWidth(s, t.glyphs, Ink::TEXT, t.f);
    // resolve()'s x for the kept width: centred at N, S and CENTER, where the kept prefix fits
    // the chord, so centring keeps it inside; margin_ew on the west side; on the east side the
    // kept prefix stays right-aligned at the anchor. Elsewhere it is clamped into the room of the
    // kept prefix, which fitGlyphs() found w px wide or more; a prefix without ink draws nothing
    // wherever it is.
    int32_t x = resolveX(at, w, t.f);
    if (t.shrink != 0) {  // not centred: k() is at least 1
        x = larger(room.left, smaller(x, room.right - w + 1));
    }
    drawGlyphs(t, x, c);
}

void SteamiScreen::title(const char* s, SteamiRgb c) {
    text(s, SteamiAt::N, c, 1);
}

bool SteamiScreen::linesFit(const char* const* lines, int32_t count, int32_t y) {
    Run t{nullptr, 0, y, Ink::TEXT, k()};
    for (int32_t i = 0; i < count; i++, t.y += u(STEAMI_SUBTITLE_PITCH)) {
        t.s = lines[i];
        t.glyphs = glyphCount(t.s);
        if (inkRows(t.s, t.glyphs, Ink::TEXT) != 0 && fitGlyphs(t) != t.glyphs) {
            return false;
        }
    }
    return true;
}

void SteamiScreen::subtitle(const char* l1, const char* l2, const char* l3, SteamiRgb c) {
    if (!widgetCanDraw() || l1 == nullptr) {
        return;
    }
    const char* const lines[3] = {l1, l2, l3};
    int32_t count = 1;  // the list ends at the first nullptr
    while (count < 3 && lines[count] != nullptr) {
        count++;
    }
    int32_t widest = 0;
    for (int32_t i = 0; i < count; i++) {
        widest = larger(widest, cellWidth(lines[i], glyphCount(lines[i]), Ink::TEXT, k()));
    }
    const int32_t rowS = resolveY(SteamiAt::S, widest, k());
    const int32_t pitch = u(STEAMI_SUBTITLE_PITCH);
    // device.py's place, then the block rises a row at a time (B7) until every line fits, never
    // higher than its last line's top on the S row.
    const int32_t highest = rowS - (count - 1) * pitch;
    int32_t y = count == 1 ? rowS + u(STEAMI_SUBTITLE_DROP) : rowS - pitch * (count - 1) / 2;
    while (y > highest && !linesFit(lines, count, y)) {
        y--;
    }
    // Each line is cut to the longest prefix that fits and centred on centerX(), which is
    // resolve()'s x at S for the kept width.
    Run t{nullptr, 0, y, Ink::TEXT, k()};
    for (int32_t i = 0; i < count; i++, t.y += pitch) {
        t.s = lines[i];
        t.glyphs = glyphCount(t.s);
        t.glyphs = fitGlyphs(t);
        drawGlyphs(t, resolveX(SteamiAt::S, cellWidth(t.s, t.glyphs, Ink::TEXT, t.f), t.f), c);
    }
}

void SteamiScreen::subtitle(const char* l1, SteamiRgb c) {
    subtitle(l1, nullptr, nullptr, c);
}

void SteamiScreen::subtitle(const char* l1, const char* l2, SteamiRgb c) {
    subtitle(l1, l2, nullptr, c);
}
