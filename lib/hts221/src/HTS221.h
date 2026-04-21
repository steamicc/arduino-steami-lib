// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <Arduino.h>
#include <Wire.h>

#include "HTS221_const.h"

// HTS221 — ST capacitive humidity + temperature sensor, I2C.
//
// The public API intentionally mirrors the rest of the STeaMi driver
// collection (begin / deviceId / powerOn / powerOff / softReset / reboot /
// setContinuous / triggerOneShot / temperature / humidity / read) so
// consumers learn one shape and reuse it across sensors.
class HTS221 {
   public:
    struct ReadResult {
        float temperature;  // Celsius
        float humidity;     // %RH
    };

    HTS221(TwoWire& wire = Wire, uint8_t address = HTS221_DEFAULT_ADDRESS);

    // Probe the device and load calibration. Returns false if WHO_AM_I
    // does not match HTS221_WHO_AM_I_VALUE.
    bool begin();

    // WHO_AM_I value (always 0xBC on a healthy HTS221).
    uint8_t deviceId();

    // Software reset: reload the factory trimming from non-volatile memory.
    void reboot();

    // Software-visible reset equivalent to reboot() — the part has no
    // dedicated reset register beyond BOOT, so softReset() and reboot()
    // share the same mechanism. softReset() is kept for API symmetry
    // with other drivers in the collection.
    void softReset();

    // Power control via CTRL_REG1.PD.
    void powerOn();
    void powerOff();

    // Data-ready bits from STATUS_REG.
    bool dataReady();
    bool temperatureReady();
    bool humidityReady();

    // Single-channel reads. If the device is powered down, the method
    // auto-triggers a one-shot measurement, waits for dataReady(), and
    // returns the result — the caller doesn't need to manage modes.
    float temperature();
    float humidity();

    // Combined reading — preferred when both channels are needed to keep
    // them consistent (single auto-trigger, single poll).
    ReadResult read();

    // Continuous mode at the requested ODR (HTS221_ODR_1_HZ, _7_HZ, or
    // _12_5_HZ). BDU is set so no register is updated mid-read.
    void setContinuous(uint8_t odr);

    // Trigger a single measurement without blocking. Pair with
    // temperature() / humidity() after polling dataReady(), or use
    // readOneShot() for the bundled flow.
    void triggerOneShot();

    // Trigger a one-shot measurement, wait for dataReady() with a timeout,
    // and return both channels.
    ReadResult readOneShot();

    // Additive Celsius offset applied after the factory calibration.
    void setTemperatureOffset(float offset);

    // Two-point user calibration for temperature. refLow/refHigh are the
    // known reference values in Celsius; measLow/measHigh are what the
    // sensor reported at those references. Subsequent temperature()
    // calls apply the correction on top of the factory calibration.
    void calibrateTemperature(float refLow, float measLow, float refHigh, float measHigh);

   private:
    // Stored as a pointer (not a reference) so the class is default-assignable
    // — useful for tests that reconstruct the sensor between fixtures. The
    // public constructor still takes a TwoWire& per CLAUDE.md convention.
    TwoWire* _wire;
    uint8_t _address;

    // Cached factory calibration, loaded by begin().
    float _tempSlope = 0.0f;
    float _tempIntercept = 0.0f;
    float _humSlope = 0.0f;
    float _humIntercept = 0.0f;

    // User-applied correction (on top of the factory calibration).
    float _tempOffset = 0.0f;
    float _tempUserSlope = 1.0f;
    float _tempUserIntercept = 0.0f;

    uint8_t status();
    uint8_t readReg(uint8_t reg);
    void writeReg(uint8_t reg, uint8_t value);
    void readRegs(uint8_t reg, uint8_t* buf, size_t len);

    void loadCalibration();
    bool isPoweredOn();
    bool waitForDataReady(uint32_t timeoutMs = 100);
    float computeTemperature(int16_t raw);
    float computeHumidity(int16_t raw);
};
