// SPDX-License-Identifier: GPL-3.0-or-later

#include "SteamiScreen.h"

#include <math.h>

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

bool isSide(SteamiAt at) {
    return at == SteamiAt::W || at == SteamiAt::E;
}

bool isCorner(SteamiAt at) {
    return at == SteamiAt::NE || at == SteamiAt::NW || at == SteamiAt::SE || at == SteamiAt::SW;
}

/// The physical glyph factor of a widget at `scale`: max(scale, 1) times k, the scale capped at
/// 255 / k so that the factor always fits a uint8_t.
uint8_t glyphFactor(uint8_t scale, uint8_t k) {
    const int32_t s = smaller(larger(scale, 1), 255 / k);
    return static_cast<uint8_t>(s * k);
}

/// The filled columns of a bar w px wide: device.py's int(w * clamp(v, 0, maxV) / maxV) in float,
/// truncated. NAN v, or maxV not above 0 (NAN included), gives 0; v >= maxV (+INF included) gives
/// w and v <= 0 (-INF included) gives 0, so v is finite below and the result never NAN.
int32_t filledColumns(int32_t w, float v, float maxV) {
    if (isnan(v) || !(maxV > 0.0f) || v <= 0.0f) {
        return 0;
    }
    if (v >= maxV) {
        return w;
    }
    // 0 < v < maxV: the fill is below w. Only a v past FLT_MAX / w makes the product overflow,
    // and then the ratio is taken first.
    const float wide = static_cast<float>(w) * v;
    const float fill = isinf(wide) ? static_cast<float>(w) * (v / maxV) : wide / maxV;
    return smaller(static_cast<int32_t>(fill), w);
}

/// A compass heading in whole degrees, in [-360, 360]: fmodf first, so that lroundf never sees a
/// huge value, then rounded to nearest with halves away from zero. 0 for NAN or +-INF, which
/// point nowhere: the caller draws no needle then.
int32_t roundedHeading(float h) {
    if (isnan(h) || isinf(h)) {
        return 0;
    }
    return static_cast<int32_t>(lroundf(fmodf(h, 360.0f)));
}

/// The fill of gauge(), in degrees 0..270: lroundf(270 * ratio), ratio = (v - minV) / (maxV -
/// minV) clamped to [0, 1]. 0 when maxV == minV (tested before the division) or when the ratio is
/// NAN (NAN in any argument, INF - INF, INF / INF), so lroundf only sees a value in [0, 270].
int32_t gaugeSweep(float v, float minV, float maxV) {
    if (maxV == minV) {
        return 0;
    }
    const float ratio = (v - minV) / (maxV - minV);
    if (isnan(ratio) || ratio <= 0.0f) {
        return 0;
    }
    if (ratio >= 1.0f) {
        return STEAMI_GAUGE_SWEEP;
    }
    return static_cast<int32_t>(lroundf(static_cast<float>(STEAMI_GAUGE_SWEEP) * ratio));
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

int32_t SteamiScreen::blockHeight(uint8_t f, bool hasUnit) {
    const int32_t cell = SteamiDisplay::CHAR_H * f;
    return hasUnit ? cell + cell / 3 + SteamiDisplay::CHAR_H * k() : cell;
}

void SteamiScreen::value(SteamiValue v, const char* unit, SteamiAt at, const char* label,
                         SteamiRgb c, uint8_t scale, int16_t yOffset) {
    // Formatting touches no hardware, so it comes before the auto-begin. The frame holds the 16 B
    // buffer and the run only (64 B on the target): placeValue() counts the glyphs and puts the
    // block, drawValueParts() cuts, places and draws.
    char text[16];
    Run t{text, 0, 0, Ink::VALUE, 1};
    v.format(text, sizeof text, _decimals);
    if (!widgetCanDraw()) {
        return;
    }
    // From the largest f down to 1 (the decision "Scale"). u(yOffset) is int32_t, so no int16_t
    // wraps (Review Focus 4).
    t.y = u(yOffset);
    t.f = glyphFactor(scale, k());
    while (!placeValue(t, at, unit)) {
        t.f--;
    }
    drawValueParts(t, c, unit, label, at);
}

void SteamiScreen::value(SteamiValue v, const char* unit, SteamiRgb c) {
    value(v, unit, SteamiAt::CENTER, nullptr, c);
}

SteamiScreen::Region SteamiScreen::valueRegion(const Run& value, SteamiAt at) {
    if (!isSide(at) && !isCorner(at)) {
        return {0, static_cast<int32_t>(width()) - 1};
    }
    // B2: the chord at the value's cell rows, so the region does not depend on the digits.
    const Region c = chord(value.y, value.y + SteamiDisplay::CHAR_H * value.f - 1);
    if (c.width() == 0) {
        return c;
    }
    if (isCorner(at)) {
        return {c.left + k(), c.right - k()};  // B9: the corner region
    }
    const int32_t gap = u(STEAMI_VALUE_SIDE_GAP);
    return at == SteamiAt::W ? Region{c.left + k(), centerX() - gap - 1}
                             : Region{centerX() + gap + 1, c.right - k()};
}

bool SteamiScreen::placeValue(Run& value, SteamiAt at, const char* unit) {
    const int32_t offset = value.y;
    value.glyphs = glyphCount(value.s);
    const int32_t vw = cellWidth(value.s, value.glyphs, Ink::VALUE, value.f);
    // N, S and the corners take resolve()'s row; CENTER, W and E centre the block on centerY() +
    // u(yOffset).
    if (at == SteamiAt::N || at == SteamiAt::S || isCorner(at)) {
        value.y = resolveY(at, vw, value.f);
    } else {
        const bool hasUnit = unit != nullptr && unit[0] != '\0';
        value.y = centerY() + offset - blockHeight(value.f, hasUnit) / 2;
    }
    bool fits = false;
    if (isSide(at)) {
        // B6: "88.8" sets the f of every narrower value, so two values side by side match.
        const int32_t widest = larger(vw, valueWidth(STEAMI_VALUE_REFERENCE, value.f));
        fits = widest <= valueRegion(value, at).width();
    } else if (isCorner(at)) {
        // 5.9 "Corner anchors": resolve()'s cell [x, x + vw) at this f lies inside the region.
        const Region room = valueRegion(value, at);
        const int32_t x = resolveX(at, vw, value.f);
        fits = room.width() > 0 && room.left <= x && x + vw - 1 <= room.right;
    } else {
        // The chord at the rows the value inks, less u(4); a value without ink always fits.
        fits = inkRows(value.s, value.glyphs, Ink::VALUE) == 0 ||
               vw + u(STEAMI_VALUE_FIT_MARGIN) <= inkChord(value).width();
    }
    if (fits || value.f == 1) {
        return true;
    }
    value.y = offset;
    return false;
}

void SteamiScreen::drawValueParts(Run& value, SteamiRgb c, const char* unit, const char* label,
                                  SteamiAt at) {
    // Label, value, unit: the drawing order of the decision "Label and unit of value()". A corner
    // label is centred on the value, so drawValuePart() asks fitValue() where the value goes.
    Run part{label, glyphCount(label), value.y - u(STEAMI_VALUE_LABEL_GAP) - 7 * k(), Ink::LABEL,
             k()};
    drawValuePart(part, value, at);
    drawGlyphs(value, fitValue(value, at), c);
    part = {unit, glyphCount(unit), value.y + SteamiDisplay::CHAR_H * value.f, Ink::TEXT, k()};
    drawValuePart(part, value, at);
}

int32_t SteamiScreen::fitValue(Run& value, SteamiAt at) {
    // Whole when it fits; at f 1 cut to the longest prefix that fits the same region. A W/E or
    // corner region lies inside the ink chord of any prefix (the chord at the cell rows is never
    // wider), so fitGlyphs() keeps the longest prefix no wider than the region and hands the
    // region back unchanged.
    const bool centred = !isSide(at) && !isCorner(at);
    Region room = valueRegion(value, at);
    value.glyphs = fitGlyphs(value, &room, centred ? u(STEAMI_VALUE_FIT_MARGIN) : 0);
    const int32_t w = cellWidth(value.s, value.glyphs, Ink::VALUE, value.f);
    if (isSide(at)) {
        return room.left + (room.width() - w) / 2;
    }
    if (isCorner(at)) {
        // Only x follows the kept prefix: the row and the region stay those of the whole text. A
        // value that fits at its f is already inside the region, so the clamp keeps it.
        return larger(room.left, smaller(resolveX(at, w, value.f), room.right - w + 1));
    }
    return centerX() - w / 2;
}

void SteamiScreen::drawValuePart(Run& part, Run& value, SteamiAt at) {
    const int32_t cx = centerX();
    const int32_t gap = u(STEAMI_VALUE_SIDE_GAP);
    // At W and E the half region at the part's own inked rows: its ink chord shrunk by k, on its
    // side of centerX(); at a corner that ink chord shrunk by k; elsewhere the whole chord.
    Region room{at == SteamiAt::E ? cx + gap + 1 : 0,
                at == SteamiAt::W ? cx - gap - 1 : static_cast<int32_t>(width()) - 1};
    part.shrink = isSide(at) || isCorner(at) ? k() : 0;
    part.glyphs = fitGlyphs(part, &room);
    const int32_t w = cellWidth(part.s, part.glyphs, part.ink, part.f);
    int32_t x = cx - w / 2;
    if (isSide(at)) {
        x = room.left + (room.width() - w) / 2;
    } else if (isCorner(at)) {
        // Centred on the value's centre, then clamped into the room of the kept prefix.
        const int32_t left = fitValue(value, at);
        const int32_t centre = left + cellWidth(value.s, value.glyphs, Ink::VALUE, value.f) / 2;
        x = larger(room.left, smaller(centre - w / 2, room.right - w + 1));
    }
    drawGlyphs(part, x, part.ink == Ink::LABEL ? STEAMI_GRAY : STEAMI_LIGHT);
}

void SteamiScreen::bar(float v, float maxV, int16_t yOffset, SteamiRgb c) {
    if (!widgetCanDraw()) {
        return;
    }
    // W - u(40) wide, centred, its top u(20) + u(yOffset) below centerY(). int32_t rows: no
    // int16_t wrap draws the bar elsewhere (Review Focus 4).
    const int32_t w = width() - u(STEAMI_BAR_SIDE);
    const int32_t x = centerX() - w / 2;
    const int32_t top = centerY() + u(STEAMI_BAR_DROP) + u(yOffset);
    barRows(x, x + w - 1, top, STEAMI_GRID_DARK);
    // The fill over the track, lifted on the grey panel so that it stands out (spec 5.10).
    barRows(x, x + filledColumns(w, v, maxV) - 1, top, lifted(c, STEAMI_GRID_DARK));
}

void SteamiScreen::bar(float v, float maxV, SteamiRgb c) {
    bar(v, maxV, 0, c);
}

void SteamiScreen::barRows(int32_t left, int32_t right, int32_t top, SteamiRgb c) {
    const int32_t from = larger(top, 0);
    const int32_t to = smaller(top + u(STEAMI_BAR_HEIGHT), height()) - 1;
    for (int32_t y = from; y <= to; y++) {
        // The chord of a row is {0, -1} outside the disc; on the centre row it reaches column
        // width(), one past the panel.
        const Region inside = chord(y, y);
        const int32_t x0 = larger(left, inside.left);
        const int32_t x1 = smaller(right, smaller(inside.right, width() - 1));
        if (x1 >= x0) {
            rect(static_cast<int16_t>(x0), static_cast<int16_t>(y),
                 static_cast<int16_t>(x1 - x0 + 1), 1, c, true);
        }
    }
}

void SteamiScreen::face(SteamiFace f, bool compact, SteamiRgb c) {
    // An f past LOVE (a cast) has no bitmap and draws nothing, as device.py's FACES.get; the
    // bitmap overload still begins the display first.
    const size_t i = static_cast<size_t>(f);
    face(i < sizeof(STEAMI_FACES) / sizeof(STEAMI_FACES[0]) ? STEAMI_FACES[i] : nullptr, compact,
         c);
}

void SteamiScreen::face(SteamiFace f, SteamiRgb c) {
    face(f, false, c);
}

void SteamiScreen::face(const uint8_t bitmap[8], bool compact, SteamiRgb c) {
    if (!widgetCanDraw() || bitmap == nullptr) {
        return;
    }
    // Cells of u(11), or u(8) when compact: device.py's W * 11 / 128 and W / 16 exactly. The grid
    // is centred, a compact one raised by half a cell (spec 5.9).
    const int32_t cell = u(compact ? STEAMI_FACE_COMPACT_CELL : STEAMI_FACE_CELL);
    const int32_t ox = centerX() - 4 * cell;
    const int32_t oy = centerY() - 4 * cell - (compact ? cell / 2 : 0);
    // One flat loop over the 64 bits, row i / 8 and column i % 8 (MSB = left): a nested row and
    // column loop around rect() takes more than the 64 B of stack of a widget.
    for (int32_t i = 0; i < 64; i++) {
        if ((bitmap[i / 8] & (0x80 >> (i % 8))) != 0) {
            rect(static_cast<int16_t>(ox + (i % 8) * cell),
                 static_cast<int16_t>(oy + (i / 8) * cell), static_cast<int16_t>(cell),
                 static_cast<int16_t>(cell), c, true);
        }
    }
}

int32_t SteamiScreen::polarX(int32_t d, int32_t deg) {
    return centerX() + steamiRoundQ14(d * steamiSinQ14(deg));  // |d| <= 32767: fits int32_t
}

int32_t SteamiScreen::polarY(int32_t d, int32_t deg) {
    return centerY() - steamiRoundQ14(d * steamiCosQ14(deg));
}

void SteamiScreen::ray(int32_t deg, int32_t from, int32_t to, SteamiRgb c) {
    strokeLine(static_cast<int16_t>(polarX(from, deg)), static_cast<int16_t>(polarY(from, deg)),
               static_cast<int16_t>(polarX(to, deg)), static_cast<int16_t>(polarY(to, deg)), k(),
               c);
}

void SteamiScreen::pointer(int32_t deg, int32_t length, int32_t halfWidth, SteamiRgb c) {
    // rnd is odd-symmetric, so the two base ends are mirror images about the centre.
    triangle(static_cast<int16_t>(polarX(length, deg)), static_cast<int16_t>(polarY(length, deg)),
             static_cast<int16_t>(polarX(halfWidth, deg + 90)),
             static_cast<int16_t>(polarY(halfWidth, deg + 90)),
             static_cast<int16_t>(polarX(halfWidth, deg - 90)),
             static_cast<int16_t>(polarY(halfWidth, deg - 90)), c, true);
}

void SteamiScreen::glyphsAt(const char* s, int32_t deg, int32_t d, SteamiRgb c) {
    // device.py's lx - tw // 2, ly - CHAR_H // 2, at factor k.
    const uint8_t f = k();
    SteamiDisplay::text(
        s, static_cast<int16_t>(polarX(d, deg) - cellWidth(s, glyphCount(s), Ink::TEXT, f) / 2),
        static_cast<int16_t>(polarY(d, deg) - SteamiDisplay::CHAR_H / 2 * f), c, f);
}

void SteamiScreen::watch(uint8_t h, uint8_t m, uint8_t s, SteamiRgb c) {
    if (!widgetCanDraw()) {
        return;
    }
    // device.py's drawing order: dial, ticks, numbers, hour hand, minute hand, second hand, pivot.
    const int32_t r = radius() - u(STEAMI_WATCH_INSET);
    strokeCircle(centerX(), centerY(), static_cast<int16_t>(r), k(), STEAMI_DARK);
    for (int32_t i = 0; i < 12; i++) {
        const bool major = i % 3 == 0;  // 12, 3, 6 and 9
        ray(30 * i, r - u(major ? STEAMI_WATCH_MAJOR_TICK : STEAMI_WATCH_MINOR_TICK), r,
            major ? STEAMI_LIGHT : STEAMI_GRAY);
    }
    const int32_t numbers = r - u(STEAMI_WATCH_NUMBERS);
    glyphsAt("12", 0, numbers, STEAMI_WHITE);
    glyphsAt("3", 90, numbers, STEAMI_WHITE);
    glyphsAt("6", 180, numbers, STEAMI_WHITE);
    glyphsAt("9", 270, numbers, STEAMI_WHITE);
    // Integer angles, no clamping (spec 5.9): an out-of-range time still gives a whole degree,
    // which the trigonometry normalises. The hand lengths R / 2, 3R / 4 and R * 85 / 100 of spec
    // 5.9's watch row truncate, as device.py's int().
    pointer((h % 12) * 30 + m / 2, r / 2, u(STEAMI_PIVOT), c);
    pointer(m * 6 + s / 10, 3 * r / 4, u(STEAMI_MINUTE_HALF_WIDTH), c);
    ray(s * 6, 0, r * 85 / 100, STEAMI_GRAY);
    circle(centerX(), centerY(), static_cast<int16_t>(u(STEAMI_PIVOT)), STEAMI_GRAY, true);
}

void SteamiScreen::compass(float headingDeg, SteamiRgb c) {
    if (!widgetCanDraw()) {
        return;
    }
    // A real rose (decision 2): every element at angle a is drawn at a - h. Drawing order of spec
    // 5.9's compass row: circles, ticks, letters, north half, south half, pivot. The inner circle
    // R * 7 / 10 and the needle R * 85 / 100 of that row truncate, as device.py's int().
    const int32_t h = roundedHeading(headingDeg);
    const int32_t r = radius() - u(STEAMI_COMPASS_INSET);
    strokeCircle(centerX(), centerY(), static_cast<int16_t>(r), k(), STEAMI_DARK);
    strokeCircle(centerX(), centerY(), static_cast<int16_t>(r * 7 / 10), k(), STEAMI_DARK);
    for (int32_t i = 0; i < 8; i++) {
        ray(45 * i - h, r - u(STEAMI_COMPASS_TICK), r, i % 2 == 0 ? STEAMI_LIGHT : STEAMI_DARK);
    }
    const int32_t letters = r + u(STEAMI_COMPASS_LETTERS);
    glyphsAt("N", -h, letters, STEAMI_WHITE);
    glyphsAt("E", 90 - h, letters, STEAMI_GRAY);
    glyphsAt("S", 180 - h, letters, STEAMI_GRAY);
    glyphsAt("W", 270 - h, letters, STEAMI_GRAY);
    if (!isnan(headingDeg) && !isinf(headingDeg)) {  // a heading that points nowhere: no needle
        pointer(-h, r * 85 / 100, u(STEAMI_PIVOT), c);
        pointer(180 - h, r * 85 / 100, u(STEAMI_PIVOT), STEAMI_DARK);
    }
    circle(centerX(), centerY(), static_cast<int16_t>(u(STEAMI_PIVOT)), STEAMI_GRAY, true);
}

void SteamiScreen::gauge(float v, float minV, float maxV, SteamiRgb c, uint8_t arcWidth) {
    if (!widgetCanDraw()) {
        return;
    }
    // Spec 5.9's gauge row: outer radius radius() - u(4); the width arcWidth in physical pixels,
    // or max(5, radius() / 9). Track, fill, min label, max label: device.py's drawing order.
    const int32_t outer = radius() - u(STEAMI_GAUGE_INSET);
    const uint8_t w =
        arcWidth != 0
            ? arcWidth
            : static_cast<uint8_t>(smaller(
                  larger(STEAMI_GAUGE_MIN_WIDTH, radius() / STEAMI_GAUGE_WIDTH_DIVISOR), 255));
    gaugeArc(outer, STEAMI_GAUGE_SWEEP, STEAMI_DARK, w);
    // The fill over the track, of the same width, lifted on the grey panel (spec 5.10).
    gaugeArc(outer, gaugeSweep(v, minV, maxV), lifted(c, STEAMI_DARK), w);
    const int32_t d = larger(0, outer - w - u(STEAMI_GAUGE_LABEL_GAP));
    gaugeLabel(minV, STEAMI_GAUGE_MIN_ANGLE, d);
    gaugeLabel(maxV, STEAMI_GAUGE_MAX_ANGLE, d);
}

void SteamiScreen::gaugeArc(int32_t outer, int32_t sweep, SteamiRgb c, uint8_t w) {
    arc(centerX(), centerY(), static_cast<int16_t>(outer), STEAMI_GAUGE_START,
        static_cast<int16_t>(sweep), c, w);
}

void SteamiScreen::gaugeLabel(float v, int16_t deg, int32_t d) {
    // The integer part, truncated toward zero as device.py's int() (-0.5 shows "0"); |v| < 1e9
    // keeps the float-to-long cast defined. The frame holds the 16 B buffer and the run only:
    // drawGaugeLabel() fits, places and draws.
    char text[16];
    if (fabsf(v) < 1e9f) {
        SteamiValue(static_cast<long>(v)).format(text, sizeof text, 0);
    } else {
        SteamiValue(NAN).format(text, sizeof text, 0);  // "--"
    }
    Run t{text, glyphCount(text), polarY(d, deg), Ink::LABEL, k(), k()};
    drawGaugeLabel(t, polarX(d, deg));
}

void SteamiScreen::drawGaugeLabel(Run& t, int32_t px) {
    // Centred on px by its cell width, then the 4a corner rule (B10): the longest prefix that fits
    // the chord at its inked rows shrunk by k (every label of at most 10 glyphs fits whole), slid
    // into that chord.
    Region room{0, static_cast<int32_t>(width()) - 1};
    t.glyphs = fitGlyphs(t, &room);
    const int32_t w = cellWidth(t.s, t.glyphs, Ink::LABEL, t.f);
    drawGlyphs(t, larger(room.left, smaller(px - w / 2, room.right - w + 1)), STEAMI_GRAY);
}
