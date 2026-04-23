// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <Arduino.h>
#include <Wire.h>

#include "WSEN_PADS_const.h"

class WSEN_PADS {
   public:
    WSEN_PADS(TwoWire& wire = Wire, uint8_t address = WSEN_PADS_I2C_DEFAULT_ADDR,
              float temp_gain = 1.0, float temp_offset = 0.0);

    bool begin();

    uint8_t deviceId();

    void reboot();

    void powerOff();
    void powerOn(uint8_t odr = ODR_1_HZ);
    void softReset();

    struct ReadResult {
        float pressure;
        float temperature;
    };
    float pressureHpa();
    float pressurePa();
    float pressureKpa();
    float temperature();
    ReadResult read();

    bool setContinuous(uint8_t odr = ODR_1_HZ, bool low_noise = false, bool low_pass = false,
                       bool low_pass_strong = false);

    void triggerOneShot(bool lowNoise = false);
    ReadResult readOneShot(bool lowNoise = false);

    void setTempOffset(float offset_c);
    bool calibrateTemperature(float refLow, float measuredLow, float refHigh, float measuredHigh);

    void enableLowPass(bool strong = false);
    void disableLowPass();

   private:
    TwoWire* _wire;
    uint8_t _address;
    float _temp_gain;
    float _temp_offset;

    bool isPresent();
    uint8_t readReg(uint8_t reg);
    void readBlock(uint8_t reg, uint8_t* buf, size_t len);
    void writeReg(uint8_t reg, uint8_t value);
    void updateReg(uint8_t reg, uint8_t mask, uint8_t value);

    static int32_t toSigned24(int32_t value);
    static int32_t toSigned16(uint16_t value);

    bool waitBoot(uint32_t timeout_ms = 20);
    bool checkDevice();
    void configureDefault();

    uint8_t status();
    bool pressureReady();
    bool temperatureReady();
    bool dataReady();

    bool isPowerDown();
    bool ensureData();
    int32_t pressureRaw();
    int32_t temperatureRaw();
};