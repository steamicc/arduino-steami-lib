// SPDX-License-Identifier: GPL-3.0-or-later
//
// Layout constants of SteamiScreen, in 128-px units (u() scales them to the panel) unless
// stated. Internal: the student never names them.

#pragma once

#include <stdint.h>

// The +4 of the N/S floor (2 cells + 4) and of the E/W margin (1 cell + 4), times k.
constexpr int16_t STEAMI_SCREEN_PAD = 4;

// TEXT-face cells that a title row is lowered for at most: the N/S row cap.
constexpr int16_t STEAMI_TITLE_CAP_CELLS = 13;

// The cell height of a TEXT or VALUE glyph at factor f is this times f on every panel (8x8 x f,
// and 8x16 x f / 2 on the 240 px panel): the unit of the vertical layout rules. Not scaled.
constexpr int16_t STEAMI_CELL_HEIGHT = 8;

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

// gauge(): the ring's outer radius radius() - u(4); its labels sit u(10) inside the ring.
constexpr int16_t STEAMI_GAUGE_INSET = 4;
constexpr int16_t STEAMI_GAUGE_LABEL_GAP = 10;

// gauge(), in degrees (0 = noon, clockwise), not scaled: the ring starts at 225 and sweeps 270,
// leaving the gap at the bottom; the min label sits at 233, the max label at 127.
constexpr int16_t STEAMI_GAUGE_START = 225;
constexpr int16_t STEAMI_GAUGE_SWEEP = 270;
constexpr int16_t STEAMI_GAUGE_MIN_ANGLE = 233;
constexpr int16_t STEAMI_GAUGE_MAX_ANGLE = 127;

// gauge(), in physical pixels: the default ring width is max(5, radius() / 9), 7 on 128, 13 on 240.
constexpr int16_t STEAMI_GAUGE_MIN_WIDTH = 5;
constexpr int16_t STEAMI_GAUGE_WIDTH_DIVISOR = 9;

// graph() (MicroPython's names): the box from x u(MARGIN + X_OFFSET) = u(21) and y u(38), w W -
// u(MARGIN) - u(21), h u(52); the current value's top row u(31); the dashed middle line, dashes of
// u(3) every u(3) + u(3); the axis labels u(2) inside the box.
constexpr int16_t STEAMI_GRAPH_MARGIN = 15;
constexpr int16_t STEAMI_GRAPH_X_OFFSET = 6;
constexpr int16_t STEAMI_GRAPH_Y = 38;
constexpr int16_t STEAMI_GRAPH_HEIGHT = 52;
constexpr int16_t STEAMI_GRAPH_VALUE_Y = 31;
constexpr int16_t STEAMI_GRAPH_DASH = 3;
constexpr int16_t STEAMI_GRAPH_GAP = 3;
constexpr int16_t STEAMI_GRAPH_LABEL_INSET = 2;
