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
constexpr uint8_t DAPLINK_BRIDGE_CMD_READ_SECTOR = 0x20;
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

// Maximum payload bytes per WRITE_CONFIG frame. The firmware only runs
// WRITE_CONFIG once it has received exactly 31 argument bytes, so every
// frame is a fixed 32 bytes: `CMD + 2-byte offset + 1-byte length` plus a
// zero-padded 28-byte payload.
constexpr uint8_t DAPLINK_BRIDGE_MAX_WRITE_CHUNK = 28;

// READ_CONFIG loads this many bytes of the zone, starting at the requested
// offset, into the bridge TX buffer. Every I2C read restarts from the first
// byte of that buffer, and the offset must leave room for the whole block
// (offset + 256 <= 1 KB).
constexpr uint16_t DAPLINK_BRIDGE_READ_CONFIG_LEN = 256;

// Largest single I2C read the driver issues. STM32duino 2.x caps
// TwoWire::requestFrom() at 255 bytes, so readConfig() never asks for the
// full 256-byte block at once.
constexpr uint8_t DAPLINK_BRIDGE_MAX_READ_LEN = 255;

// Most argument bytes a command takes (WRITE_DATA, WRITE_CONFIG).
constexpr uint8_t DAPLINK_BRIDGE_MAX_ARGS = 31;

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

// How long a register read keeps retrying. The F103 answers reads from its
// 30 ms main loop, and it NACKs for about one loop after it starts sending
// a 256-byte block (READ_SECTOR, READ_CONFIG) that the host cut short.
constexpr uint32_t DAPLINK_BRIDGE_REG_TIMEOUT_MS = 100;

// READ_SECTOR keeps the bridge busy for about 100 ms while it reads the
// external flash.
constexpr uint32_t DAPLINK_BRIDGE_PROBE_TIMEOUT_MS = 500;

// Wait after the probe's READ_SECTOR before reading its result. The F103
// runs it on its next 30 ms tick and stays busy one more tick: this leaves
// a margin of a few ticks.
constexpr uint32_t DAPLINK_BRIDGE_PROBE_SETTLE_MS = 150;

// Settling delays after each config command, matching the MicroPython
// driver. The F103 runs config commands from its main loop. The answer to
// READ_CONFIG cannot be polled for, since reading STATUS replaces the
// bridge TX buffer: the driver waits for the block to be loaded instead.
constexpr uint32_t DAPLINK_BRIDGE_READ_CONFIG_DELAY_MS = 100;
constexpr uint32_t DAPLINK_BRIDGE_WRITE_CONFIG_DELAY_MS = 50;
constexpr uint32_t DAPLINK_BRIDGE_CLEAR_CONFIG_DELAY_MS = 100;
