// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stdint.h>

#include "SteamiBoard.h"
#include "SteamiDisplayBackend.h"
#include "SteamiRgb.h"
#include "SteamiTypes.h"

struct SteamiFontFace;  // SteamiFont.h

/// Drawing on the board's screen, in the physical pixels of the panel: origin top-left, x to the
/// right, y down. Every object of the layer draws into the one backend of its board. The first
/// drawing call begins the display if the sketch did not; after a failure every drawing call and
/// show() does nothing and error() says why. Everything is clipped to the panel before the backend
/// is called.
class SteamiDisplay {
   public:
    /// Stores the address of the board only; touches no hardware.
    explicit constexpr SteamiDisplay(SteamiBoard& board = Steami) noexcept
        : _board(&board), _who("SteamiDisplay") {}

    /// Begins the display once and keeps the result. False with one message on an unknown board
    /// (E30) or when the colour screen does not fit in RAM (E31). Idempotent.
    bool begin();

    bool isAvailable() const { return _status == SteamiStatus::OK; }
    SteamiStatus status() const { return _status; }

    /// "" when OK, "not started", "unknown board" or "no memory".
    const char* error() const;

    /// Geometry of the panel; valid before begin(), 0 or false without a backend.
    bool isColor();
    int16_t width();
    int16_t height();
    int16_t centerX();
    int16_t centerY();
    int16_t radius();

    void clear(SteamiRgb c = STEAMI_BLACK);
    void pixel(int16_t x, int16_t y, SteamiRgb c = STEAMI_WHITE);
    void line(int16_t x1, int16_t y1, int16_t x2, int16_t y2, SteamiRgb c = STEAMI_WHITE);
    void hline(int16_t x, int16_t y, int16_t w, SteamiRgb c = STEAMI_WHITE);
    void vline(int16_t x, int16_t y, int16_t h, SteamiRgb c = STEAMI_WHITE);
    void rect(int16_t x, int16_t y, int16_t w, int16_t h, SteamiRgb c = STEAMI_WHITE,
              bool fill = false);
    void circle(int16_t cx, int16_t cy, int16_t r, SteamiRgb c = STEAMI_WHITE, bool fill = false);
    void triangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2,
                  SteamiRgb c = STEAMI_WHITE, bool fill = false);

    /// Ring of outer radius r and radial thickness width; degrees, 0 = noon (12 o'clock),
    /// clockwise, the same convention as compass() and watch().
    void arc(int16_t cx, int16_t cy, int16_t r, int16_t startDeg, int16_t sweepDeg,
             SteamiRgb c = STEAMI_WHITE, uint8_t width = 1);

    /// s with the top-left corner of its first cell at (x, y), in the panel's font drawn `scale`
    /// times bigger: 8x8 on the 128 px screen, 8x16 with accents on the 240 px one. Each glyph
    /// advances 8 * scale; scale 0 counts as 1.
    void text(const char* s, int16_t x, int16_t y, SteamiRgb c = STEAMI_WHITE, uint8_t scale = 1);

    /// Width in pixels of s: glyphs, not bytes.
    int16_t textWidth(const char* s, uint8_t scale = 1) const;

    /// Height in pixels of a line of text() on the screen in use: 8 * scale on the 128 px screen,
    /// 16 * scale on the 240 px one; scale 0 counts as 1. Valid before begin(); 8 * scale without
    /// a screen.
    int16_t textHeight(uint8_t scale = 1);

    void show();
    void rotate(bool upsideDown);
    void invert(bool on);
    void powerOn();
    void powerOff();

    /// The board's backend, built but not begun; nullptr without one.
    SteamiDisplayBackend* backend();

    static constexpr uint8_t CHAR_W = 8, CHAR_H = 8;

   protected:  // for SteamiScreen, not for the student
    /// `who` is the name in messages ("SteamiScreen"); it must outlive the object (a literal).
    constexpr SteamiDisplay(SteamiBoard& board, const char* who) noexcept
        : _board(&board), _who(who) {}

    /// s in small glyphs `scale` times, advancing 6 * scale: 5x7 on the 128 px screen, 6x12 with
    /// accents on the 240 px one.
    void textSmall(const char* s, int16_t x, int16_t y, SteamiRgb c, uint8_t scale = 1);
    int16_t textSmallWidth(const char* s, uint8_t scale = 1) const;

    /// True when the panel draws the Spleen faces: (width() + 64) / 128 == 2, the 240 px screen
    /// (a 200 px panel too); false on 128 and 96 px panels and without a backend.
    bool spleenPanel();

    /// The column window [firstColumn, firstColumn + columnCount) of a glyph stored by columns
    /// (bit 0 = top row, `rows` rows), its left edge at x; each horizontal run of set bits of a
    /// row is one span(). Scale 0 counts as 1. 4a's value text uses (g, 2, 4, 8) for '.' and ':'.
    void glyph(const uint8_t* columns, uint8_t firstColumn, uint8_t columnCount, uint8_t rows,
               int32_t x, int32_t y, SteamiRgb c, uint8_t scale);

    /// The column window [firstColumn, firstColumn + columnCount) of a Spleen glyph of `face`
    /// (`bits`: its rows, top to bottom, MSB = leftmost column), the window's left edge at x; each
    /// run of set bits of a font row is one span() of n x n blocks. With bold, a run [a, b] spans
    /// [a * n, (b + 2) * n - 1] from the window's left edge: a one-column double strike. n 0
    /// counts as 1; nullptr bits draw nothing.
    void faceGlyph(const SteamiFontFace& face, const uint8_t* bits, uint8_t firstColumn,
                   uint8_t columnCount, int32_t x, int32_t y, SteamiRgb c, uint8_t n, bool bold);

    void strokeLine(int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint8_t w, SteamiRgb c);
    void strokeCircle(int16_t cx, int16_t cy, int16_t r, uint8_t w, SteamiRgb c);

   private:
    /// The backend when drawing is allowed (begins on first use), nullptr otherwise.
    SteamiDisplayBackend* ready();

    /// One pixel, dropped when off the panel.
    void dot(SteamiDisplayBackend* b, int32_t x, int32_t y, uint16_t c);

    /// A w x h rectangle intersected with the panel; nothing when the result is empty.
    void span(SteamiDisplayBackend* b, int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c);

    /// The one Bresenham walker of line() and strokeLine(): the endpoints in canonical order, a
    /// w x w square brush (top-left at x - (w-1)/2, y - (w-1)/2: centred on the path, even widths
    /// extend right and down) through span(), or dot() when w is 1.
    void walk(SteamiDisplayBackend* b, int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint8_t w,
              uint16_t c);

    /// The glyph loop of text() and textSmall() off a Spleen panel: 8x8 glyphs advancing 8
    /// columns, or 5x7 glyphs advancing 6, accents folded.
    void drawText(const char* s, int16_t x, int16_t y, SteamiRgb c, uint8_t scale, bool small);

    /// The glyph loop of text() and textSmall() on a Spleen panel: the full set `face` `scale`
    /// times, accents drawn, advancing face.w * scale; it stops past the right edge.
    void drawFaceText(const char* s, int16_t x, int16_t y, SteamiRgb c, uint8_t scale,
                      const SteamiFontFace& face);

    SteamiBoard* _board;
    const char* _who;
    SteamiStatus _status = SteamiStatus::NOT_STARTED;
};
