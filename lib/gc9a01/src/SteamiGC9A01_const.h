// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stdint.h>

// Every name carries a GC9A01_ prefix: the STeaMi package umbrella header
// includes all drivers at once, so names must not clash with SSD1327_const.h
// or with third-party GC9A01 libraries.

// panel geometry
constexpr uint16_t GC9A01_WIDTH = 240;
constexpr uint16_t GC9A01_HEIGHT = 240;

// "no pin" value for the optional reset line of GC9A01_SPI
constexpr uint8_t GC9A01_PIN_NONE = 0xFF;

// standard commands (MIPI DCS)
constexpr uint8_t GC9A01_CMD_SWRESET = 0x01;
constexpr uint8_t GC9A01_CMD_SLPIN = 0x10;
constexpr uint8_t GC9A01_CMD_SLPOUT = 0x11;
constexpr uint8_t GC9A01_CMD_INVOFF = 0x20;
constexpr uint8_t GC9A01_CMD_INVON = 0x21;
constexpr uint8_t GC9A01_CMD_DISPOFF = 0x28;
constexpr uint8_t GC9A01_CMD_DISPON = 0x29;
constexpr uint8_t GC9A01_CMD_CASET = 0x2A;
constexpr uint8_t GC9A01_CMD_RASET = 0x2B;
constexpr uint8_t GC9A01_CMD_RAMWR = 0x2C;
constexpr uint8_t GC9A01_CMD_MADCTL = 0x36;
constexpr uint8_t GC9A01_CMD_COLMOD = 0x3A;

// GC9A01 specific commands
constexpr uint8_t GC9A01_CMD_DISP_FUNC_CTRL = 0xB6;
constexpr uint8_t GC9A01_CMD_POWER2 = 0xC3;
constexpr uint8_t GC9A01_CMD_POWER3 = 0xC4;
constexpr uint8_t GC9A01_CMD_POWER4 = 0xC9;
constexpr uint8_t GC9A01_CMD_FRAME_RATE = 0xE8;
constexpr uint8_t GC9A01_CMD_GAMMA1 = 0xF0;
constexpr uint8_t GC9A01_CMD_GAMMA2 = 0xF1;
constexpr uint8_t GC9A01_CMD_GAMMA3 = 0xF2;
constexpr uint8_t GC9A01_CMD_GAMMA4 = 0xF3;
constexpr uint8_t GC9A01_CMD_INREGEN1 = 0xFE;
constexpr uint8_t GC9A01_CMD_INREGEN2 = 0xEF;

// COLMOD value: 16 bits per pixel (RGB565)
constexpr uint8_t GC9A01_COLMOD_RGB565 = 0x05;

// MADCTL (memory access control) bits
constexpr uint8_t GC9A01_MAD_MY = 0x80;
constexpr uint8_t GC9A01_MAD_MX = 0x40;
constexpr uint8_t GC9A01_MAD_MV = 0x20;
constexpr uint8_t GC9A01_MAD_ML = 0x10;
constexpr uint8_t GC9A01_MAD_BGR = 0x08;
constexpr uint8_t GC9A01_MAD_MH = 0x04;

// MADCTL values used by rotate(): normal orientation and 180° rotation
constexpr uint8_t GC9A01_ORIENT_NORMAL = GC9A01_MAD_BGR;
constexpr uint8_t GC9A01_ORIENT_ROTATED = GC9A01_MAD_MY | GC9A01_MAD_MX | GC9A01_MAD_BGR;

// delays (milliseconds)
constexpr uint16_t GC9A01_RESET_DELAY_MS = 120;
constexpr uint16_t GC9A01_SLEEP_DELAY_MS = 120;

// RGB565 colours
constexpr uint16_t GC9A01_BLACK = 0x0000;
constexpr uint16_t GC9A01_NAVY = 0x000F;
constexpr uint16_t GC9A01_DARKGREEN = 0x03E0;
constexpr uint16_t GC9A01_PURPLE = 0x780F;
constexpr uint16_t GC9A01_DARKGREY = 0x4208;
constexpr uint16_t GC9A01_GREY = 0x8410;
constexpr uint16_t GC9A01_LIGHTGREY = 0xC618;
constexpr uint16_t GC9A01_BLUE = 0x001F;
constexpr uint16_t GC9A01_GREEN = 0x07E0;
constexpr uint16_t GC9A01_CYAN = 0x07FF;
constexpr uint16_t GC9A01_RED = 0xF800;
constexpr uint16_t GC9A01_MAGENTA = 0xF81F;
constexpr uint16_t GC9A01_ORANGE = 0xFD20;
constexpr uint16_t GC9A01_YELLOW = 0xFFE0;
constexpr uint16_t GC9A01_PINK = 0xFC18;
constexpr uint16_t GC9A01_WHITE = 0xFFFF;
