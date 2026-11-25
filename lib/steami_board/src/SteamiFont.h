// SPDX-License-Identifier: GPL-3.0-or-later
//
// The 8x8 and 5x7 bitmap fonts of the drawing layer, and the UTF-8 decoding
// that turns a string into glyph codes. A glyph code is 32..126 (ASCII) or
// STEAMI_GLYPH_DEGREE. Glyphs are stored by columns, bit 0 is the top row.

#pragma once

#include <stddef.h>
#include <stdint.h>

// Glyph code of the degree sign (U+00B0), in both fonts.
constexpr uint8_t STEAMI_GLYPH_DEGREE = 127;

// Decodes the next glyph of the UTF-8 string s and moves s past it. Returns a
// code 32..127, or 0 at the end of the string (and for nullptr), where s does
// not move. Accented French letters fold to their base letter; any other code
// point, control character or invalid byte gives '?'.
uint8_t steamiNextGlyph(const char*& s);

// Number of glyphs (not bytes) in s; 0 for nullptr.
size_t steamiGlyphCount(const char* s);

// The 8 column bytes of a glyph (8 rows); a code outside 32..127 gives '?'.
const uint8_t* steamiGlyph8x8(uint8_t code);

// The 5 column bytes of a glyph (7 rows); a code outside 32..127 gives '?'.
const uint8_t* steamiGlyph5x7(uint8_t code);
