// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "SteamiDisplay.h"
#include "SteamiScreenConst.h"
#include "SteamiValue.h"

/// Where a widget sits on the round screen: the eight compass points and the centre.
enum class SteamiAt : uint8_t { N, NE, E, SE, S, SW, W, NW, CENTER };

/// The six pixel-art faces of face().
enum class SteamiFace : uint8_t { HAPPY, SAD, SURPRISED, SLEEPING, ANGRY, LOVE };

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

    /// v at `at`, formatted with setDecimals() ("--" for NAN), in c; its unit below in LIGHT and
    /// its label above in small GRAY letters. The glyphs shrink from `scale` until the value
    /// fits the circle, then a value still too wide is cut. At W and E every value no wider than
    /// "88.8" keeps the same size, so two values side by side match. yOffset moves a CENTER, W or
    /// E value down (up when negative), in pixels of the 128 px screen.
    void value(SteamiValue v, const char* unit = nullptr, SteamiAt at = SteamiAt::CENTER,
               const char* label = nullptr, SteamiRgb c = STEAMI_WHITE, uint8_t scale = 2,
               int16_t yOffset = 0);
    void value(SteamiValue v, const char* unit, SteamiRgb c);

    /// A horizontal bar below the centre: a GRID_DARK track, filled in c from the left in
    /// proportion to v / maxV (v is clamped to [0, maxV]). NAN, or a maxV not above 0, leaves it
    /// empty. yOffset moves it down (up when negative), in pixels of the 128 px screen; any part
    /// outside the circle is cut off.
    void bar(float v, float maxV = 100, int16_t yOffset = 0, SteamiRgb c = STEAMI_LIGHT);
    void bar(float v, float maxV, SteamiRgb c);

    /// A pixel-art face in c, centred: its 8x8 bitmap drawn as filled squares of 11 px on the
    /// 128 px screen. A compact face has 8 px squares and sits half a square higher. A value of f
    /// past LOVE draws nothing.
    void face(SteamiFace f, bool compact = false, SteamiRgb c = STEAMI_LIGHT);
    void face(SteamiFace f, SteamiRgb c);
    /// The same with your own 8x8 bitmap: rows top to bottom, the most significant bit on the
    /// left. nullptr draws nothing.
    void face(const uint8_t bitmap[8], bool compact = false, SteamiRgb c = STEAMI_LIGHT);

    /// An analog watch at h:m:s: a DARK dial with 12 ticks, the numbers 12, 3, 6 and 9 in WHITE,
    /// the hour and minute hands in c, a GRAY second hand and pivot. The hour hand moves with the
    /// minutes and the minute hand with the seconds, in whole degrees. A time past its range is
    /// drawn as the angles it gives: watch(25, 0, 0) shows 1 o'clock.
    void watch(uint8_t h, uint8_t m, uint8_t s = 0, SteamiRgb c = STEAMI_LIGHT);

    /// A compass rose turned so that its N points to real north: headingDeg is the direction the
    /// top of the screen points to, in degrees clockwise from north (0 = north), rounded to a whole
    /// degree. Two DARK circles, 8 ticks, the letters N (WHITE), E, S and W (GRAY), upright; a
    /// needle whose north half in c points to N, its south half DARK; a GRAY pivot. At heading 90
    /// the N is on the left. NAN (no sensor) or an infinite heading draws the rose at 0 and the
    /// pivot, with no needle.
    void compass(float headingDeg, SteamiRgb c = STEAMI_LIGHT);

    /// A round gauge near the edge: a DARK ring of 270 degrees with its gap at the bottom, filled
    /// in c clockwise from the lower left in proportion to (v - minV) / (maxV - minV), clamped to
    /// [0, 1]: with minV above maxV, the fill grows as v falls. The integer parts of minV and maxV
    /// ("--" from 1e9) are written in small GRAY letters inside the two ends. NAN, or minV equal
    /// to maxV, leaves it empty. arcWidth is the ring's width in pixels of this panel, 0 for the
    /// default. No value and no unit: draw them with value().
    void gauge(float v, float minV = 0, float maxV = 100, SteamiRgb c = STEAMI_LIGHT,
               uint8_t arcWidth = 0);

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
    /// string in one Run and hands it to the kernel by reference, which keeps the kernel calls'
    /// stack arguments few and each widget frame within 64 B.
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

    /// Height of a value block at factor f: 8f + 8f/3 + 8k with a unit (the unit hangs 8f/3 below
    /// the value), 8f without. A unit is a non-empty string, as device.py's `if unit:`.
    int32_t blockHeight(uint8_t f, bool hasUnit);

    /// The columns a value run may take at its factor and row, from the chord at its cell rows (so
    /// they do not depend on the digits; empty when those rows leave the circle): at W and E its
    /// half region, [left edge + k, centerX() - u(3)) and its mirror image; at NE, NW, SE and SW
    /// the corner region, that chord shrunk by k at each end; elsewhere the whole panel.
    Region valueRegion(const Run& value, SteamiAt at);

    /// Draws a value run whose f and top row value() chose: its label (5x7 x k, GRAY, its top
    /// u(4) + 7k above the value), the value in c as fitValue() cuts and places it, and its unit
    /// (8x8 x k, LIGHT, its top 8f below the value's). Label and unit are cut to the chord at their
    /// own inked rows: at W and E also to the half region taken at those rows, and centred in what
    /// remains; at a corner shrunk by k, centred on the value's centre, then clamped into it;
    /// elsewhere centred on centerX().
    void drawValueParts(Run& value, SteamiRgb c, const char* unit, const char* label, SteamiAt at);

    /// fg, or on the grey panel a grey STEAMI_MIN_CONTRAST levels above bg when fg is darker.
    SteamiRgb lifted(SteamiRgb fg, SteamiRgb bg);

    /// Glyphs (not bytes) of s; 0 for nullptr.
    static size_t glyphCount(const char* s);

    /// The point at distance d from the centre and angle deg (integer degrees, 0 = noon,
    /// clockwise): (centerX() + rnd(d sin deg), centerY() - rnd(d cos deg)), rnd rounding Q14 to
    /// nearest with halves away from zero (spec 5.6). Two scalars, so that a widget's point never
    /// goes through memory. |d| <= 32767.
    int32_t polarX(int32_t d, int32_t deg);
    int32_t polarY(int32_t d, int32_t deg);

   private:
    /// The entry of every widget: begins the display on first use; false when it draws nothing.
    bool widgetCanDraw();

    /// chord() over the rows set in `rows` (bit 0 = top row) of glyphs drawn at row y, factor f;
    /// {0, width() - 1} when rows is 0.
    Region rowsChord(uint8_t rows, int32_t y, uint8_t f);

    /// True when every inked line of the block, its first line's top at row y and the next ones
    /// u(STEAMI_SUBTITLE_PITCH) lower, fits whole.
    bool linesFit(const char* const* lines, int32_t count, int32_t y);

    /// Puts the value run's block at its f: counts the glyphs of value.s, then sets value.y to the
    /// block's top, on resolve()'s row at N, S and the corners, elsewhere centred on centerY() +
    /// value.y (value.y holds u(yOffset) on entry; blockHeight() with a non-empty unit). Returns
    /// true when the value fits there or f is 1; otherwise false, value.y back to u(yOffset). It
    /// fits when, at W and E, max(its width, the width of "88.8") fits valueRegion(); at a corner
    /// when resolve()'s cell lies inside valueRegion(); elsewhere when its width plus u(4) fits the
    /// chord at the rows it inks (a value without ink always fits).
    bool placeValue(Run& value, SteamiAt at, const char* unit);

    /// Cuts the value run placed by placeValue() to the longest prefix that fits valueRegion()
    /// (less u(4) at CENTER, N and S), whole whenever it fit there, and returns the left column of
    /// what it keeps: centred in the region at W and E; at a corner resolve()'s x for the kept
    /// width, clamped into the region; elsewhere centred on centerX(). Cutting a cut run again
    /// keeps it unchanged.
    int32_t fitValue(Run& value, SteamiAt at);

    /// Cuts a label (LABEL ink, drawn GRAY) or unit (TEXT ink, drawn LIGHT) run of the value run
    /// and draws it as drawValueParts() says.
    void drawValuePart(Run& part, Run& value, SteamiAt at);

    /// A stroke k() px wide on the ray at deg, from distance `from` to distance `to`: a tick, the
    /// second hand.
    void ray(int32_t deg, int32_t from, int32_t to, SteamiRgb c);

    /// A filled triangle in c: its tip at distance `length` on deg, its base from halfWidth on
    /// deg + 90 to halfWidth on deg - 90 (mirror images about the centre): a hand, a needle half.
    void pointer(int32_t deg, int32_t length, int32_t halfWidth, SteamiRgb c);

    /// s in 8x8 glyphs at factor k(), its cell centred on the point at distance d and angle deg,
    /// never cut: the callers keep every cell inside the circle.
    void glyphsAt(const char* s, int32_t deg, int32_t d, SteamiRgb c);

    /// The ring of gauge(): arc() of outer radius `outer` and width w from STEAMI_GAUGE_START,
    /// `sweep` degrees long (nothing for 0); the track and the fill.
    void gaugeArc(int32_t outer, int32_t sweep, SteamiRgb c, uint8_t w);

    /// One label of gauge(): v's integer part, truncated toward zero ("--" when |v| >= 1e9, NAN
    /// included), in 5x7 x k, its top row on the point at distance d and angle deg.
    void gaugeLabel(float v, int16_t deg, int32_t d);

    /// Draws the label run of gaugeLabel() in GRAY, centred on column px by its cell width, then
    /// cut to the longest prefix that fits the chord at its inked rows shrunk by t.shrink, and
    /// slid into that chord (the corner rule).
    void drawGaugeLabel(Run& t, int32_t px);

    /// Fills columns [left, right] of the u(STEAMI_BAR_HEIGHT) rows from row top in c, each row
    /// cut to the panel and to the chord at that row; a row with nothing left draws nothing.
    void barRows(int32_t left, int32_t right, int32_t top, SteamiRgb c);

    uint8_t _decimals = 1;
};
