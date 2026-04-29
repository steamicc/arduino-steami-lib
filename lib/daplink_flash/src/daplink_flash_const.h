// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <stdint.h>

// Commands — flash level
constexpr uint8_t CMD_SET_FILENAME = 0x03;
constexpr uint8_t CMD_GET_FILENAME = 0x04;
constexpr uint8_t CMD_CLEAR_FLASH = 0x10;
constexpr uint8_t CMD_WRITE_DATA = 0x11;
constexpr uint8_t CMD_READ_SECTOR = 0x20;

// Protocol limits
// constexpr uint8_t MAX_WRITE_CHUNK = 30;
// constexpr uint16_t SECTOR_SIZE = 256;
constexpr uint16_t MAX_SECTORS = 32768;
constexpr uint8_t FILENAME_LEN = 8;
constexpr uint8_t EXT_LEN = 3;