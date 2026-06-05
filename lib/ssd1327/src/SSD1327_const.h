// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stdint.h>

// commands
constexpr uint8_t SET_COL_ADDR = 0x15;
constexpr uint8_t SET_SCROLL_DEACTIVATE = 0x2E;
constexpr uint8_t SET_ROW_ADDR = 0x75;
constexpr uint8_t SET_CONTRAST = 0x81;
constexpr uint8_t SET_SEG_REMAP = 0xA0;
constexpr uint8_t SET_DISP_START_LINE = 0xA1;
constexpr uint8_t SET_DISP_OFFSET = 0xA2;
constexpr uint8_t SET_DISP_MODE = 0xA4;
constexpr uint8_t SET_MUX_RATIO = 0xA8;
constexpr uint8_t SET_FN_SELECT_A = 0xAB;
constexpr uint8_t SET_DISP = 0xAE;
constexpr uint8_t SET_PHASE_LEN = 0xB1;
constexpr uint8_t SET_DISP_CLK_DIV = 0xB3;
constexpr uint8_t SET_SECOND_PRECHARGE = 0xB6;
constexpr uint8_t SET_GRAYSCALE_TABLE = 0xB8;
constexpr uint8_t SET_GRAYSCALE_LINEAR = 0xB9;
constexpr uint8_t SET_PRECHARGE = 0xBC;
constexpr uint8_t SET_VCOM_DESEL = 0xBE;
constexpr uint8_t SET_FN_SELECT_B = 0xD5;
constexpr uint8_t SET_COMMAND_LOCK = 0xFD;

// registers
constexpr uint8_t REG_CMD = 0x80;
constexpr uint8_t REG_DATA = 0x40;