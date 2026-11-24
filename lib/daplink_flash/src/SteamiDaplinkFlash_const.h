// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <stddef.h>
#include <stdint.h>

// Commands of the DAPLink I2C bridge for the user file in the external
// flash (steamicc/DAPLink, source/board/steami/steami32.c).
constexpr uint8_t DAPLINK_FLASH_CMD_SET_FILENAME = 0x03;  // 11 argument bytes
constexpr uint8_t DAPLINK_FLASH_CMD_GET_FILENAME = 0x04;
constexpr uint8_t DAPLINK_FLASH_CMD_CLEAR_FLASH = 0x10;
constexpr uint8_t DAPLINK_FLASH_CMD_WRITE_DATA = 0x11;   // 31 argument bytes
constexpr uint8_t DAPLINK_FLASH_CMD_READ_SECTOR = 0x20;  // 2 argument bytes

// Error register bits.
constexpr uint8_t DAPLINK_FLASH_ERROR_BAD_PARAM = 0x01;
constexpr uint8_t DAPLINK_FLASH_ERROR_BAD_FILENAME = 0x02;
constexpr uint8_t DAPLINK_FLASH_ERROR_FILE_FULL = 0x04;

// The user file starts at the beginning of the 8 MB W25Q64 and ends at its
// first 0xFF byte; the last 4 KB hold the filename.
constexpr uint32_t DAPLINK_FLASH_FILE_SIZE = 8384512;
constexpr uint16_t DAPLINK_FLASH_SECTOR_SIZE = 256;
constexpr uint16_t DAPLINK_FLASH_MAX_SECTORS = DAPLINK_FLASH_FILE_SIZE / DAPLINK_FLASH_SECTOR_SIZE;

// 8.3 filename.
constexpr uint8_t DAPLINK_FLASH_FILENAME_LEN = 8;
constexpr uint8_t DAPLINK_FLASH_EXT_LEN = 3;

// WRITE_DATA always carries 31 argument bytes: [length | data | padding].
constexpr uint8_t DAPLINK_FLASH_WRITE_ARGS = 31;
constexpr uint8_t DAPLINK_FLASH_MAX_WRITE_CHUNK = DAPLINK_FLASH_WRITE_ARGS - 1;

// The F103 runs flash commands from its 30 ms main loop. GET_FILENAME and
// READ_SECTOR leave their answer in the bridge TX buffer, which a STATUS
// poll would replace: wait this long before reading it.
constexpr uint32_t DAPLINK_FLASH_RESPONSE_DELAY_MS = 100;

// Commands that wait for the flash. Before each append, the firmware scans
// the file for its end, which takes a few seconds on a full flash.
constexpr uint32_t DAPLINK_FLASH_TIMEOUT_MS = 10000;

// CLEAR_FLASH erases the file 64 KB at a time and blocks the F103 until it
// is done: about 20 s for a full flash, several minutes in the worst case.
constexpr uint32_t DAPLINK_FLASH_CLEAR_TIMEOUT_MS = 300000;
