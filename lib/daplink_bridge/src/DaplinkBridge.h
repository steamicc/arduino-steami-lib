// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <Arduino.h>
#include <Wire.h>

#include "daplink_bridge_const.h"

class DaplinkBridge {
   public:
    DaplinkBridge(TwoWire& wire = Wire, uint8_t address = DAPLINK_BRIDGE_DEFAULT_ADDR);

    // --- Lifecycle ---
    bool begin();
    uint8_t deviceId();

    // --- Status ---
    bool busy();

    // --- Config zone (1 KB persistent storage in F103 internal flash) ---
    bool clearConfig();
    bool writeConfig(const uint8_t* data, size_t length, uint16_t offset = 0);
    bool writeConfig(const char* str, uint16_t offset = 0);
    size_t readConfig(uint8_t* result, size_t maxLen);

    // --- Frame-level primitives (for sibling drivers) ---
    // sendCommand and readResponse expose the bridge's I2C protocol so
    // other drivers (daplink_flash, daplink_config, …) can compose their
    // own commands without re-implementing the framing or the busy/error
    // bookkeeping. They wait for the bridge to clear before issuing the
    // request, send `[cmd | payload...]` (or just `[cmd]`), wait again
    // for the device to finish, and check the error register.

    // Send `[cmd]` or `[cmd | payload...]`. Returns false on busy
    // timeout or when the device's error register is non-zero.
    bool sendCommand(uint8_t cmd, const uint8_t* payload = nullptr, size_t payloadLen = 0,
                     uint32_t timeoutMs = DAPLINK_BRIDGE_WRITE_TIMEOUT_MS);

    // Issue a command and read up to `maxLen` bytes back. Returns the
    // number of bytes actually read (zero on busy timeout).
    size_t readResponse(uint8_t cmd, uint8_t* buf, size_t maxLen,
                        uint32_t timeoutMs = DAPLINK_BRIDGE_READ_TIMEOUT_MS);

   private:
    TwoWire* _wire;
    uint8_t _address;

    // I2C helpers
    bool isPresent();
    uint8_t readReg(uint8_t reg);
    void readBlock(uint8_t reg, uint8_t* buf, uint8_t len);
    void writeFrame(const uint8_t* data, size_t len);

    // Status / error
    uint8_t status();
    uint8_t error();
    bool waitNotBusy(uint32_t timeoutMs);
};
