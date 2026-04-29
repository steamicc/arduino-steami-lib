// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <stdint.h>

// ============================================================================
// I2C addressing
// ============================================================================

// 7-bit I2C address. The DAPLink chip uses 0x76 in 8-bit format (CODAL
// convention), which is 0x3B in 7-bit.
constexpr uint8_t DAPLINK_BRIDGE_DEFAULT_ADDR = 0x3B;

// Expected WHO_AM_I value.
constexpr uint8_t DAPLINK_BRIDGE_WHO_AM_I = 0x4C;

// ============================================================================
// Command codes
// ============================================================================

constexpr uint8_t DAPLINK_BRIDGE_CMD_WHO_AM_I = 0x01;
constexpr uint8_t DAPLINK_BRIDGE_CMD_WRITE_CONFIG = 0x30;
constexpr uint8_t DAPLINK_BRIDGE_CMD_READ_CONFIG = 0x31;
constexpr uint8_t DAPLINK_BRIDGE_CMD_CLEAR_CONFIG = 0x32;

// ============================================================================
// Status / error register addresses
// ============================================================================

constexpr uint8_t DAPLINK_BRIDGE_REG_STATUS = 0x80;
constexpr uint8_t DAPLINK_BRIDGE_REG_ERROR = 0x81;

// ============================================================================
// Status register bits
// ============================================================================

constexpr uint8_t DAPLINK_BRIDGE_STATUS_BUSY = 0x80;

// ============================================================================
// Error register bits
// ============================================================================

constexpr uint8_t DAPLINK_BRIDGE_ERROR_BAD_PARAM = 0x01;
constexpr uint8_t DAPLINK_BRIDGE_ERROR_CMD_FAILED = 0x80;

// ============================================================================
// Protocol limits
// ============================================================================

// Maximum payload bytes per WRITE_CONFIG frame. The bridge accepts
// `CMD + 2-byte offset + 1-byte length + payload` and the F103 buffers
// it up to ~32 bytes; 30 leaves headroom for the 4-byte header.
constexpr uint8_t DAPLINK_BRIDGE_MAX_WRITE_CHUNK = 30;

// Maximum payload bytes per READ_CONFIG frame. Arduino Wire's default
// receive buffer is 32 bytes, so we cap reads at 16 to leave room for
// future protocol extensions and to keep latency predictable.
constexpr uint8_t DAPLINK_BRIDGE_MAX_READ_CHUNK = 16;

// 1 KB persistent config zone (in F103 internal flash).
constexpr uint16_t DAPLINK_BRIDGE_CONFIG_SIZE = 1024;

// ============================================================================
// Timing helpers
// ============================================================================

// Worst-case erase of the 1 KB config zone in the F103 flash.
constexpr uint32_t DAPLINK_BRIDGE_CLEAR_TIMEOUT_MS = 1000;

// Worst-case write of one chunk into flash.
constexpr uint32_t DAPLINK_BRIDGE_WRITE_TIMEOUT_MS = 100;

// Worst-case read latency.
constexpr uint32_t DAPLINK_BRIDGE_READ_TIMEOUT_MS = 50;
