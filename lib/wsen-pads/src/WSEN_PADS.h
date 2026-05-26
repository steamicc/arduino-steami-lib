// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <Arduino.h>
#include <Wire.h>

#include "WSEN_PADS_const.h"

class WSEN_PADS {
   public:
    WSEN_PADS(TwoWire& wire = Wire, uint8_t address = WSEN_PADS_I2C_DEFAULT_ADDR);

    // --- Lifecycle ---
    bool begin();
    uint8_t deviceId();
    void softReset();
    void reboot();
    void powerOn();
    void powerOff();

    // --- Reading ---
    struct ReadResult {
        float pressure;     // hPa
        float temperature;  // °C, factory + user calibration applied
    };
    float pressureHpa();
    float temperature();
    ReadResult read();

    // --- Status ---
    bool dataReady();
    bool pressureReady();
    bool temperatureReady();

    // --- Modes ---
    void setContinuous(uint8_t odr);
    void triggerOneShot();
    ReadResult readOneShot();

    // --- Calibration ---
    // setTemperatureOffset() and calibrateTemperature() are independent:
    // the offset stacks on top of the two-point user slope/intercept.
    void setTemperatureOffset(float offset);
    void calibrateTemperature(float refLow, float measLow, float refHigh, float measHigh);

    // --- Driver-specific tuning ---
    // Both LOW_NOISE_EN and the LPF bits are layered on the running CTRL
    // registers, so callers can flip them without losing the current ODR.
    // LOW_NOISE_EN is only valid when ODR is 1, 10, 25, 50 or 75 Hz —
    // see the WSEN-PADS datasheet (rev. 1.5) §6.1.
    void enableLowPass(bool strong = false);
    void disableLowPass();
    void enableLowNoise();
    void disableLowNoise();

   private:
    TwoWire* _wire;
    uint8_t _address;
    float _tempUserSlope = 1.0f;   // two-point user calibration slope
    float _tempUserOffset = 0.0f;  // two-point user calibration intercept
    float _tempOffset = 0.0f;      // additive offset on top of user calibration

    // I2C helpers
    bool isPresent();
    uint8_t readReg(uint8_t reg);
    void readBlock(uint8_t reg, uint8_t* buf, size_t len);
    void writeReg(uint8_t reg, uint8_t value);
    void updateReg(uint8_t reg, uint8_t mask, uint8_t value);

    // Boot helpers
    bool waitBoot(uint32_t timeoutMs = 20);
    void configureDefault();

    // Data path
    bool isPoweredOn();
    bool waitForDataReady(uint32_t timeoutMs = 100);
    int32_t pressureRaw();     // returns INT32_MIN on failure
    int32_t temperatureRaw();  // returns INT32_MIN on failure
    ReadResult readSensorData();

    uint8_t status();

    static int32_t toSigned24(uint32_t value);
    static int32_t toSigned16(uint16_t value);
};
