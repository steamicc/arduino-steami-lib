// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <Arduino.h>
#include <Wire.h>

#include "SteamiWsenHids_const.h"

// WSEN-HIDS — Würth Elektronik capacitive humidity + temperature sensor, I2C.
//
// The public API intentionally mirrors the rest of the STeaMi driver
// collection (begin / deviceId / powerOn / powerOff / softReset / reboot /
// setContinuous / triggerOneShot / temperature / humidity / read) so
// consumers learn one shape and reuse it across sensors.
class SteamiWsenHids {
   public:
    struct ReadResult {
        float temperature;  // Celsius
        float humidity;     // %RH
    };

    SteamiWsenHids(TwoWire& wire = Wire, uint8_t address = WSEN_HIDS_DEFAULT_ADDRESS);

    // Probe the device and load calibration. Returns false if WHO_AM_I
    // does not match WSEN_HIDS_WHO_AM_I_VALUE.
    bool begin();

    // WHO_AM_I value (always 0xBC on a healthy WSEN-HIDS).
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

    // Single-channel reads. If the device is not in continuous mode
    // (power-down, or ODR = one-shot), the method brings it up in
    // continuous mode at 12.5 Hz, waits for dataReady() (200 ms at most)
    // and returns the first sample; the chip then stays in continuous
    // mode. NaN when no sample arrives.
    float temperature();
    float humidity();

    // Combined reading — preferred when both channels are needed to keep
    // them consistent (single bring-up, single poll).
    ReadResult read();

    // Continuous mode at the requested ODR (WSEN_HIDS_ODR_1_HZ, _7_HZ, or
    // _12_5_HZ). BDU is set (unless enableBdu(false)) so no register is
    // updated mid-read.
    void setContinuous(uint8_t odr);

    // Block Data Update (CTRL_REG1.BDU), on by default. The choice is
    // applied at once and kept by powerOn(), setContinuous(),
    // setOneShotMode() and triggerOneShot().
    void enableBdu(bool enabled = true);

    // Active with ODR = one-shot (CTRL_REG1 PD = 1, ODR = 00): no conversion
    // runs until triggerOneShot(). Same as set_one_shot_mode() in
    // MicroPython.
    void setOneShotMode();

    // Integrated heater (CTRL_REG2.HEATER, bit 1), read-modify-write so the
    // other CTRL_REG2 bits are kept. It warms the sensor by several degrees:
    // switch it off and let the chip cool down before measuring.
    void enableHeater(bool enabled = true);

    // setOneShotMode() then CTRL_REG2.ONE_SHOT, without waiting (the heater
    // bit is kept). On the STeaMi V1a/V1b this conversion does not run
    // (steamicc/micropython-steami-lib#425): use readOneShot() or
    // setContinuous() to get data.
    void triggerOneShot();

    // One measurement: the chip is woken up in continuous mode at 12.5 Hz
    // for one sample (200 ms at most), then CTRL_REG1 is put back as it was
    // (power-down after begin()), also on timeout. CTRL_REG2 is not touched.
    // {NaN, NaN} when no sample arrives.
    ReadResult readOneShot();

    // Number of internal samples averaged per output. Pass register
    // values from AV_CONF (0..7 for humidity, 0..7 for temperature, see
    // WSEN-HIDS datasheet table 18).
    void setAveraging(uint8_t humidityAvg, uint8_t temperatureAvg);

    // Celsius offset applied after the factory calibration. It replaces any
    // two-point calibration: the gain goes back to 1.
    void setTemperatureOffset(float offset);

    // Two-point user calibration for temperature. refLow/refHigh are the
    // known reference values in Celsius; measLow/measHigh are what the
    // sensor reported at those references, without calibration. Replaces
    // any earlier offset. False when measLow == measHigh: the calibration
    // is then left as it was.
    bool calibrateTemperature(float refLow, float measLow, float refHigh, float measHigh);

   private:
    // Stored as a pointer (not a reference) so the class is default-assignable
    // — useful for tests that reconstruct the sensor between fixtures. The
    // public constructor still takes a TwoWire& to keep the usual Arduino
    // I2C interface at the call site.
    TwoWire* _wire;
    uint8_t _address;

    // Cached factory calibration, loaded by begin().
    float _tempSlope = 0.0f;
    float _tempIntercept = 0.0f;
    float _humSlope = 0.0f;
    float _humIntercept = 0.0f;

    // User temperature correction, gain * factory + offset.
    float _tempGain = 1.0f;
    float _tempOffset = 0.0f;

    // CTRL_REG1.BDU value written by every mode change (enableBdu()).
    uint8_t _bdu = WSEN_HIDS_CTRL1_BDU;

    uint8_t status();
    uint8_t readReg(uint8_t reg);
    void writeReg(uint8_t reg, uint8_t value);
    void readRegs(uint8_t reg, uint8_t* buf, size_t len);

    void loadCalibration();
    // PD = 1 and ODR != one-shot: a conversion runs periodically.
    bool isContinuous();
    ReadResult readOutputs();
    // 200 ms: more than two periods at 12.5 Hz, as the MicroPython
    // DEFAULT_ONE_SHOT_TIMEOUT_MS.
    bool waitForDataReady(uint32_t timeoutMs = 200);
    float computeTemperature(int16_t raw);
    float computeHumidity(int16_t raw);
};
