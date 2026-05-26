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
