// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "SteamiDisplay.h"
#include "SteamiScreenConst.h"
#include "SteamiValue.h"

/// Where a widget sits on the round screen: the eight compass points and the centre.
enum class SteamiAt : uint8_t { N, NE, E, SE, S, SW, W, NW, CENTER };

/// The layout layer over SteamiDisplay: every widget computes its place from width() in
/// integers, the same rules on the 128 px grey panel and the 240 px colour panel, and draws
/// through the inherited API only. Nothing a widget draws leaves the visible circle.
class SteamiScreen : public SteamiDisplay {
   public:
    /// Stores the address of the board only; touches no hardware.
    explicit constexpr SteamiScreen(SteamiBoard& board = Steami) noexcept
        : SteamiDisplay(board, "SteamiScreen") {}

    /// How many 8x8 characters of text fit across the panel; 0 without a screen.
    uint8_t maxChars();

    /// Digits after the '.' of float values, 1 by default, at most STEAMI_VALUE_MAX_DECIMALS.
    void setDecimals(uint8_t d);

    /// A title at the top of the screen, GRAY by default. A long title moves down until it fits,
    /// never lower than the row of 13 characters, and is cut there.
    void title(const char* text, SteamiRgb c = STEAMI_GRAY);

    /// One to three centred lines at the bottom of the screen, DARK by default; the list ends at
    /// the first nullptr and "" keeps its place. The block rises until every line fits, then a
    /// line that still does not fit is cut.
    void subtitle(const char* l1, const char* l2 = nullptr, const char* l3 = nullptr,
                  SteamiRgb c = STEAMI_DARK);
    void subtitle(const char* l1, SteamiRgb c);
    void subtitle(const char* l1, const char* l2, SteamiRgb c);

    /// s at a compass point or at the centre, 8x8 glyphs at `scale`; cut to the longest part that
    /// fits inside the circle. text(s, x, y, ...) below draws in pixels and is never cut.
    void text(const char* s, SteamiAt at = SteamiAt::CENTER, SteamiRgb c = STEAMI_WHITE,
              uint8_t scale = 1);
    using SteamiDisplay::text;

   protected:  // the layout kernel of the widgets, not for the student
    /// A run of columns [left, right], inclusive; empty when right < left.
    struct Region {
        int32_t left;
        int32_t right;
        int32_t width() const;
    };

    struct Point {
        int32_t x;
        int32_t y;
    };

    /// How a string is drawn: 8x8 advancing 8; 5x7 advancing 6; value text, 8x8 with '.' and ':'
    /// drawn from the column window (2, 4) and advancing 4.
    enum class Ink : uint8_t { TEXT, LABEL, VALUE };

    /// A length in 128-px units on this panel: v * width() / 128, truncated toward zero.
    int32_t u(int32_t v128);

    /// Glyph factor and stroke width of the panel: (width() + 64) / 128, at least 1.
    uint8_t k();

    /// The columns inside the circle on every row from top to bottom: the narrower of the chords
    /// of the two end rows. Empty when either row is outside the circle.
    Region chord(int32_t top, int32_t bottom);

    /// The rows (bit 0 = top row) inked by the first `glyphs` glyphs of s; LABEL has 7 rows.
    static uint8_t inkRows(const char* s, size_t glyphs, Ink ink);

    /// chord() over the inked rows of the first `glyphs` glyphs drawn at row y, factor f;
    /// {0, width() - 1} when they ink no row (callers treat no ink as fitting).
    Region inkChord(const char* s, size_t glyphs, Ink ink, int32_t y, uint8_t f);

    /// The advance of the first `glyphs` glyphs of s at factor f.
    static int32_t cellWidth(const char* s, size_t glyphs, Ink ink, uint8_t f);

    /// The advance of the whole value text s at factor f.
    static int32_t valueWidth(const char* s, uint8_t f);

    /// The longest prefix of s, in glyphs, whose cell width plus pad fits inside the ink chord at
    /// row y shrunk by `shrink` at each end and intersected with limit. 0 when none fits.
    size_t fitGlyphs(const char* s, Ink ink, uint8_t f, int32_t y, Region limit, uint8_t shrink,
                     int32_t pad);

    /// The N/S row margin for a text tw px wide at factor f: the safe margin of the circle, at
    /// least the floor of two cells plus the pad, at most the 13-cell cap.
    int32_t marginNs(int32_t tw, uint8_t f);

    /// The E/W column margin at factor f: one cell plus the pad.
    int32_t marginEw(uint8_t f);

    /// The top-left corner of a cell tw px wide and 8f high at the anchor.
    Point resolve(SteamiAt at, int32_t tw, uint8_t f);

    /// Draws the first `glyphs` glyphs of s (fewer if s ends first) with its top-left at (x, y).
    void drawGlyphs(const char* s, size_t glyphs, Ink ink, int32_t x, int32_t y, SteamiRgb c,
                    uint8_t f);

    /// fg, or on the grey panel a grey STEAMI_MIN_CONTRAST levels above bg when fg is darker.
    SteamiRgb lifted(SteamiRgb fg, SteamiRgb bg);

    /// Glyphs (not bytes) of s; 0 for nullptr.
    static size_t glyphCount(const char* s);

   private:
    /// The entry of every widget: begins the display on first use; false when it draws nothing.
    bool widgetCanDraw();

    /// True when every inked line of the block, its first line's top at row y, fits whole.
    bool linesFit(const char* const* lines, int32_t count, int32_t y, int32_t pitch);

    uint8_t _decimals = 1;
};
