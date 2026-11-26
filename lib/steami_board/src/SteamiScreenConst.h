// SPDX-License-Identifier: GPL-3.0-or-later
//
// Layout constants of SteamiScreen, in 128-px units (u() scales them to the panel) unless
// stated. Internal: the student never names them.

#pragma once

#include <stdint.h>

// The +4 of the N/S floor (2 cells + 4) and of the E/W margin (1 cell + 4), times k.
constexpr int16_t STEAMI_SCREEN_PAD = 4;

// Cells of 8x8 glyphs (times k) that a title row is lowered for at most: the N/S row cap.
constexpr int16_t STEAMI_TITLE_CAP_CELLS = 13;

// subtitle(): one line starts this far below the S row; several lines are this pitch apart.
constexpr int16_t STEAMI_SUBTITLE_DROP = 8;
constexpr int16_t STEAMI_SUBTITLE_PITCH = 11;

// value(): gap between the label and the value; what CENTER, N and S keep off the available
// width; gap between a W/E half region and centerX().
constexpr int16_t STEAMI_VALUE_LABEL_GAP = 4;
constexpr int16_t STEAMI_VALUE_FIT_MARGIN = 4;
constexpr int16_t STEAMI_VALUE_SIDE_GAP = 3;

// The widest value that keeps the same glyph factor at W and E (B6).
constexpr const char STEAMI_VALUE_REFERENCE[] = "88.8";

// bar(): width W - u(40), height u(8), top u(20) below centerY().
constexpr int16_t STEAMI_BAR_SIDE = 40;
constexpr int16_t STEAMI_BAR_HEIGHT = 8;
constexpr int16_t STEAMI_BAR_DROP = 20;

// Grey levels (0..15) between a foreground and its background on the grey panel.
constexpr uint8_t STEAMI_MIN_CONTRAST = 4;

// face(): the 8x8 bitmaps of SteamiFace, in its order (const.py's FACES): rows top to bottom,
// MSB = left column. One 48 B copy for the whole program (C++17 inline variable).
inline constexpr uint8_t STEAMI_FACES[6][8] = {
    {0x00, 0x24, 0x24, 0x00, 0x00, 0x42, 0x3C, 0x00},   // HAPPY
    {0x00, 0x24, 0x24, 0x00, 0x00, 0x3C, 0x42, 0x00},   // SAD
    {0x00, 0x24, 0x24, 0x00, 0x18, 0x24, 0x24, 0x18},   // SURPRISED
    {0x00, 0x00, 0x66, 0x00, 0x00, 0x18, 0x18, 0x00},   // SLEEPING
    {0x00, 0x42, 0x24, 0x24, 0x00, 0x3C, 0x42, 0x00},   // ANGRY
    {0x00, 0x66, 0xFF, 0xFF, 0x7E, 0x3C, 0x18, 0x00}};  // LOVE

// face(): the cell of a full face (88 px of 128) and of a compact one (64 px of 128).
constexpr int16_t STEAMI_FACE_CELL = 11;
constexpr int16_t STEAMI_FACE_COMPACT_CELL = 8;

// The pivot of the watch and the compass: its radius, which is also the half width of the hour
// hand and of the compass needle.
constexpr int16_t STEAMI_PIVOT = 3;

// watch(): the dial at radius() - u(8); the ticks at 3, 6, 9 and 12 start u(8) inside the dial,
// the others u(5); the numbers sit u(15) inside it. The minute hand is u(2) wide on each side.
constexpr int16_t STEAMI_WATCH_INSET = 8;
constexpr int16_t STEAMI_WATCH_MAJOR_TICK = 8;
constexpr int16_t STEAMI_WATCH_MINOR_TICK = 5;
constexpr int16_t STEAMI_WATCH_NUMBERS = 15;
constexpr int16_t STEAMI_MINUTE_HALF_WIDTH = 2;

// compass(): the rose at radius() - u(12); its eight ticks run u(6) inward from the rose; the
// letters N, E, S and W are centred u(5) outside it.
constexpr int16_t STEAMI_COMPASS_INSET = 12;
constexpr int16_t STEAMI_COMPASS_TICK = 6;
constexpr int16_t STEAMI_COMPASS_LETTERS = 5;
