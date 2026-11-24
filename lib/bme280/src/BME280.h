// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <Arduino.h>
#include <Wire.h>

#include "BME280_const.h"

// BME280 — Bosch pressure + humidity + temperature sensor, I2C.
//
// This is the sensor of the STeaMi V1c board: it replaces the WSEN-PADS,
// WSEN-HIDS pair of the V1a/V1b boards. It sits on the internal I2C bus at
// address 0x76. The API mirrors the rest of the STeaMi driver collection
// and the MicroPython driver (steamicc/micropython-steami-lib, lib/bme280).
class BME280 {
   public:
    struct ReadResult {
        float temperature;  // Celsius
        float humidity;     // %RH
        float pressure;     // hPa
    };

    BME280(TwoWire& wire = Wire, uint8_t address = BME280_DEFAULT_ADDRESS);

    // Probe the device, load the factory calibration and apply the default
    // configuration (oversampling x1, IIR filter off, sleep mode). Returns
    // false if the chip ID does not match BME280_CHIP_ID_VALUE or if the
    // chip never finishes copying its NVM.
    bool begin();

    // Chip ID (always 0x60 on a healthy BME280).
    uint8_t deviceId();

    // Software reset (power-on defaults), then reload the calibration and
    // re-apply the default configuration so the driver stays usable.
    void softReset();

    // Power control: powerOn() enters normal mode (continuous conversions at
    // the configured standby time), powerOff() enters sleep mode.
    void powerOn();
    void powerOff();

    // Status. The BME280 has no "new data" flag, only a single "measuring"
    // flag, so the per-channel methods all report the same as dataReady().
    // After triggerOneShot(), dataReady() turns true only once that
    // conversion is over (nominal time elapsed, chip back to sleep, flag
    // clear). Otherwise (normal mode, or no pending one-shot) it means "no
    // conversion in progress", not "new sample since the last read".
    bool dataReady();
    bool temperatureReady();
    bool pressureReady();
    bool humidityReady();

    // Single-channel reads. If the device is in sleep mode, the method
    // auto-triggers a forced (one-shot) conversion, waits for it and returns
    // the result. They return NAN on timeout, on an I2C error, when begin()
    // has not succeeded, or when the channel is disabled (oversampling set
    // to BME280_OSRS_SKIP).
    float temperature();
    float humidity();
    float pressureHpa();

    // Combined reading — one conversion, one burst read, consistent values.
    ReadResult read();

    // Normal mode with the given standby time between conversions
    // (BME280_STANDBY_0_5_MS .. BME280_STANDBY_20_MS).
    void setContinuous(uint8_t standby = BME280_STANDBY_1000_MS);

    // Start a forced conversion without blocking. Poll dataReady(), then call
    // read() / temperature() / ... — the next read uses that conversion
    // instead of starting a second one (and waits for it if it is not over).
    void triggerOneShot();

    // Trigger a forced conversion, wait for it and return all channels.
    ReadResult readOneShot();

    // Oversampling per channel (BME280_OSRS_SKIP .. BME280_OSRS_X16). Pass
    // BME280_OSRS_KEEP to leave a channel unchanged.
    void setOversampling(uint8_t temperature, uint8_t pressure, uint8_t humidity);

    // IIR filter coefficient (BME280_FILTER_OFF .. BME280_FILTER_16).
    void setIirFilter(uint8_t coefficient);

    // Standby time used in normal mode (BME280_STANDBY_0_5_MS ..).
    void setStandby(uint8_t standby);

    // Worst-case duration of one conversion for the current oversampling
    // settings, in milliseconds (rounded up).
    uint32_t measurementTimeMs();

    // Estimated altitude in meters from a fresh pressure reading, using the
    // international barometric formula. Pass the local sea-level pressure
    // (from a weather report) for a better estimate.
    float altitude(float seaLevelHpa = BME280_SEA_LEVEL_HPA);

    // Dew point in Celsius (Magnus formula) from a fresh reading.
    float dewPoint();

    // Pure helpers, usable on values already read with read().
    static float altitudeFromPressure(float pressureHpa, float seaLevelHpa = BME280_SEA_LEVEL_HPA);
    static float dewPointFrom(float temperatureC, float humidityPct);

    // Additive Celsius offset applied after the factory compensation.
    void setTemperatureOffset(float offset);

    // Two-point user calibration for temperature. refLow/refHigh are the
    // known reference values in Celsius; measLow/measHigh are what the
    // sensor reported at those references. Pressure and humidity are not
    // affected.
    void calibrateTemperature(float refLow, float measLow, float refHigh, float measHigh);

   private:
    // Stored as a pointer so the class stays default-assignable.
    TwoWire* _wire;
    uint8_t _address;

    // Factory trimming parameters (datasheet section 4.2.2), loaded by begin().
    uint16_t _digT1 = 0;
    int16_t _digT2 = 0;
    int16_t _digT3 = 0;
    uint16_t _digP1 = 0;
    int16_t _digP2 = 0;
    int16_t _digP3 = 0;
    int16_t _digP4 = 0;
    int16_t _digP5 = 0;
    int16_t _digP6 = 0;
    int16_t _digP7 = 0;
    int16_t _digP8 = 0;
    int16_t _digP9 = 0;
    uint8_t _digH1 = 0;
    int16_t _digH2 = 0;
    uint8_t _digH3 = 0;
    int16_t _digH4 = 0;
    int16_t _digH5 = 0;
    int8_t _digH6 = 0;

    // Fine temperature shared by the pressure and humidity compensation.
    int32_t _tFine = 0;

    // Set by a successful begin(): reads return NAN until then.
    bool _initialized = false;

    // True between triggerOneShot() and the read that consumes its result.
    bool _oneShotPending = false;
    // When the pending one-shot was triggered and how long it should take.
    uint32_t _oneShotStartMs = 0;
    uint32_t _oneShotDurationMs = 0;

    // User-applied temperature correction (on top of the factory compensation).
    float _tempOffset = 0.0f;
    float _tempUserSlope = 1.0f;
    float _tempUserIntercept = 0.0f;

    // readReg() returns 0 on a bus error; readRegs() returns false.
    uint8_t readReg(uint8_t reg);
    void writeReg(uint8_t reg, uint8_t value);
    bool readRegs(uint8_t reg, uint8_t* buf, size_t len);
    bool readStatus(uint8_t& value);

    bool waitBoot();
    bool loadCalibration();
    void configureDefault();
    void writeConfig(uint8_t mask, uint8_t shift, uint8_t value);
    bool isSleeping();
    bool conversionIdle();
    bool oneShotDone();
    bool waitMeasurement(bool settle = true);
    bool ensureData();
    ReadResult readData();

    int32_t compensateTemperature(int32_t rawTemp);
    uint32_t compensatePressure(int32_t rawPress);
    uint32_t compensateHumidity(int32_t rawHum);
};
