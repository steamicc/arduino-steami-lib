// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <Arduino.h>
#include <Wire.h>

#include "VL53L1X_const.h"

class VL53L1X {
   public:
    VL53L1X(TwoWire& wire = Wire, uint8_t address = VL53L1X_I2C_DEFAULT_ADDR);

    bool begin();
    uint16_t deviceId();
    void reset();
    void powerOff();
    void powerOn();
    void startRanging();
    void stopRanging();
    bool dataReady();
    uint16_t distanceMm();
    uint16_t read();

   private:
    TwoWire& _wire;
    uint8_t _address;
    static const uint8_t DEFAULT_CONFIG[];

    void writeReg(uint16_t reg, uint8_t value);
    void writeReg16(uint16_t reg, uint16_t value);
    void writeRegBytes(uint16_t reg, const uint8_t* data, size_t len);
    uint8_t readReg(uint16_t reg);
    uint16_t readReg16(uint16_t reg);
    void clearInterrupt();
    bool ensureData();
};