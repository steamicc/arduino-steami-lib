// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <Arduino.h>
#include <Wire.h>

#include "daplink_bridge_const.h"

class daplink_bridge {
   public:
    daplink_bridge(TwoWire& wire = Wire, uint8_t address = DAPLINK_BRIDGE_DEFAULT_ADDR);

    // Device identification
    uint8_t deviceId();

    // Status and error registers
    bool busy();

    // Config zone (1 KB persistent storage in F103 internal flash)
    bool clearConfig();
    bool writeConfig(const uint8_t* data, size_t length, uint16_t offset = 0);
    bool writeConfig(const char* data, uint16_t offset);
    size_t readConfig(uint8_t* result, size_t maxLen);

   private:
    TwoWire* _wire;
    uint8_t _address;
    uint8_t _buffer1;

    // Low level I2C
    uint8_t readReg(uint8_t reg);
    void readBlock(uint8_t reg, uint8_t* buf, uint8_t n);
    void writeReg(uint8_t reg, uint8_t data);
    void writeCmd(uint8_t cmd);
    void writeTo(uint8_t* data, size_t len);
    void readFrom(uint8_t* n, size_t len);

    // Status and error registers
    uint8_t status();
    uint8_t error();
    bool waitBusy(uint32_t timeout_ms = 30000);
};