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

    /// The first `glyphs` glyphs of s, drawn with `ink` at factor f, the top of their cells on row
    /// y; fitGlyphs() keeps `shrink` columns free at each end of the ink chord. A widget keeps its
    /// string in one Run and hands it to the kernel by reference: no kernel call of a widget
    /// passes arguments on the stack, which keeps each widget frame within 64 B.
    struct Run {
        const char* s;
        size_t glyphs;
        int32_t y;
        Ink ink;
        uint8_t f;
        uint8_t shrink = 0;
    };

    /// A length in 128-px units on this panel: v * width() / 128, truncated toward zero.
    int32_t u(int32_t v128);

    /// Glyph factor and stroke width of the panel: (width() + 64) / 128, at least 1.
    uint8_t k();

    /// The columns inside the circle on every row from top to bottom: the narrower of the chords
    /// of the two end rows. Empty when either row is outside the circle.
    Region chord(int32_t top, int32_t bottom);

    /// The rows (bit 0 = top row) inked by the first `glyphs` glyphs of s; LABEL has 7 rows.
    static uint8_t inkRows(const char* s, size_t glyphs, Ink ink);

    /// chord() over the rows the run inks; {0, width() - 1} when it inks no row (callers treat
    /// no ink as fitting).
    Region inkChord(const Run& t);

    /// The advance of the first `glyphs` glyphs of s at factor f.
    static int32_t cellWidth(const char* s, size_t glyphs, Ink ink, uint8_t f);

    /// The advance of the whole value text s at factor f.
    static int32_t valueWidth(const char* s, uint8_t f);

    /// The longest prefix of the run, in glyphs (at most t.glyphs), whose cell width plus pad fits
    /// inside its ink chord shrunk by t.shrink at each end and intersected with *room (the whole
    /// panel when room is nullptr); 0 when none fits. On return *room holds the columns of the
    /// kept prefix: its ink chord shrunk by t.shrink, intersected with the room it was given
    /// ({0, width() - 1} stands for the chord of a prefix without ink).
    size_t fitGlyphs(const Run& t, Region* room = nullptr, int32_t pad = 0);

    /// The N/S row margin for a text tw px wide at factor f: the safe margin of the circle, at
    /// least the floor of two cells plus the pad, at most the 13-cell cap.
    int32_t marginNs(int32_t tw, uint8_t f);

    /// The E/W column margin at factor f: one cell plus the pad.
    int32_t marginEw(uint8_t f);

    /// The left column and the top row of a cell tw px wide and 8f high at the anchor. Two
    /// scalars, so that a widget's anchor never goes through memory.
    int32_t resolveX(SteamiAt at, int32_t tw, uint8_t f);
    int32_t resolveY(SteamiAt at, int32_t tw, uint8_t f);

    /// The top-left corner of that cell: {resolveX(), resolveY()}.
    Point resolve(SteamiAt at, int32_t tw, uint8_t f) {
        return {resolveX(at, tw, f), resolveY(at, tw, f)};
    }

    /// Draws the run (fewer glyphs if s ends first), its first cell's left column at x.
    void drawGlyphs(const Run& t, int32_t x, SteamiRgb c);

    /// fg, or on the grey panel a grey STEAMI_MIN_CONTRAST levels above bg when fg is darker.
    SteamiRgb lifted(SteamiRgb fg, SteamiRgb bg);

    /// Glyphs (not bytes) of s; 0 for nullptr.
    static size_t glyphCount(const char* s);

   private:
    /// The entry of every widget: begins the display on first use; false when it draws nothing.
    bool widgetCanDraw();

    /// chord() over the rows set in `rows` (bit 0 = top row) of glyphs drawn at row y, factor f;
    /// {0, width() - 1} when rows is 0.
    Region rowsChord(uint8_t rows, int32_t y, uint8_t f);

    /// True when every inked line of the block, its first line's top at row y and the next ones
    /// u(STEAMI_SUBTITLE_PITCH) lower, fits whole.
    bool linesFit(const char* const* lines, int32_t count, int32_t y);

    uint8_t _decimals = 1;
};
