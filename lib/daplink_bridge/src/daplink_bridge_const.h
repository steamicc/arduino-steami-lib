// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <stdint.h>

// I2C address (7-bit) — 0x76 in 8-bit (CODAL convention)
constexpr uint8_t DAPLINK_BRIDGE_DEFAULT_ADDR = 0x3B;

// WHO_AM_I expected value
constexpr uint8_t DAPLINK_BRIDGE_WHO_AM_I_VAL = 0x4C;

// Commands — bridge level
constexpr uint8_t CMD_WHO_AM_I = 0x01;
constexpr uint8_t CMD_WRITE_CONFIG = 0x30;
constexpr uint8_t CMD_READ_CONFIG = 0x31;
constexpr uint8_t CMD_CLEAR_CONFIG = 0x32;

// Registers
constexpr uint8_t REG_STATUS = 0x80;
constexpr uint8_t REG_ERROR = 0x81;

// Status register bits
constexpr uint8_t STATUS_BUSY = 0x80;

// Error register bits
constexpr uint8_t ERROR_BAD_PARAM = 0x01;
constexpr uint8_t ERROR_CMD_FAILED = 0x80;

// Protocol limits
constexpr uint8_t MAX_WRITE_CHUNK = 30;
constexpr uint16_t SECTOR_SIZE = 256;
constexpr uint16_t CONFIG_SIZE = 1024;
