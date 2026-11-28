// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <Arduino.h>
#include <Wire.h>

#include "SteamiLIS2MDL_const.h"

struct Vec3i {
    int16_t x, y, z;
};
struct Vec3f {
    float x, y, z;
};

struct CalibrationQuality {
    float meanX, meanY, meanZ;
    float stdX, stdY, stdZ;
    float rMeanXY;
    float rStdXY;
    float anisotropyXY;
};

// Hard-iron offsets and soft-iron scales, in the setCalibrateStep() order.
struct MagCalibration {
    float xOff, yOff, zOff;
    float xScale, yScale, zScale;
};

using MagneticField = Vec3i;
using HwOffsets = Vec3i;
using MagneticFieldUt = Vec3f;
using CalibratedField = Vec3f;

struct ReadAll {
    MagneticField raw;
    MagneticFieldUt ut;
    CalibratedField cal;
    float tempC;
    uint8_t status;
};

class SteamiLIS2MDL {
   public:
    float xOff = 0.0f;
    float yOff = 0.0f;
    float zOff = 0.0f;
    float xScale = 1.0f;
    float yScale = 1.0f;
    float zScale = 1.0f;

    SteamiLIS2MDL(TwoWire& wire = Wire, uint8_t address = LIS2MDL_I2C_ADDR, uint8_t odrHz = 10,
                  bool tempComp = true, bool lowPower = false, bool drdyEnable = false);

    bool begin();
    void setMode(const char* mode);
    void setOdr(int hz);
    void setContinuous(uint8_t hz = 10);
    void triggerOneShot();
    MagneticField readOneShot();
    void setLowPower(bool enabled);
    void setLowPass(bool enabled);
    void setOffsetCancellation(bool enabled, bool oneShot = false);
    void setBdu(bool enabled = true);
    void setEndianness(bool bigEndian);
    void useSpi4wire(bool enable);
    void setHeadingOffset(float deg);
    void setDeclination(float deg);
    void setHwOffsets(int16_t x, int16_t y, int16_t z);

    // Six output bytes in one I2C burst; {0, 0, 0} on a bus error or a data-ready timeout.
    MagneticField magneticField();
    // Same reading; false on a bus error or a data-ready timeout (value is then {0, 0, 0}).
    bool magneticField(MagneticField& value);
    MagneticField magneticFieldRaw();
    MagneticFieldUt magneticFieldUt();
    CalibratedField calibratedField();
    float magnitudeUt();
    uint8_t status();
    bool dataReady();
    uint8_t readIntSource();
    int16_t readTemperatureRaw();
    float temperature();
    void setTemperatureOffset(float offset);
    // Former name of setTemperatureOffset() (#56), kept for the whole 1.x
    // series and removed in 2.0.0.
    [[deprecated("use setTemperatureOffset(); setTempOffset() goes away in 2.0.0")]]
    void setTempOffset(float offset);
    bool calibrateTemperature(float refLow, float measuredLow, float refHigh, float measuredHigh);
    uint8_t deviceId();
    HwOffsets readHwOffsets();
    void readRegisters(uint8_t startAddr, uint8_t length, uint8_t* buffer);
    ReadAll readAll();

    void setCalibrateStep(float xoff, float yoff, float zoff, float xscale, float yscale,
                          float zscale);
    void calibrateMinmax2d(uint16_t samples = 300, uint16_t delayMs = 20);
    void calibrateMinmax3d(uint16_t samples = 600, uint16_t delayMs = 20);
    CalibratedField calibrateApply(float x, float y, float z);
    CalibrationQuality calibrateQuality(uint16_t samplesCheck = 200, uint16_t delayMs = 10);
    void calibrateReset();
    // Current offsets and scales, as read_calibration() in MicroPython.
    MagCalibration readCalibration() const;
    void calibrateStep();

    void setHeadingFilter(float alpha);
    float headingFromVectors(float x, float y, float z, bool calibrated = true);
    float headingFlatOnly();
    float headingWithTiltCompensation(Vec3f (*readAccel)());
    const char* directionLabel(float angle = -1.0f);
    const char* getMode();
    void powerOff();
    void powerOn(const char* mode = "continuous");
    void softReset(uint16_t waitMs = 10);
    void reboot(uint16_t waitMs = 10);
    bool isIdle();

   private:
    TwoWire* _wire;
    uint8_t _address;
    uint8_t _odrHz;
    bool _tempComp;
    bool _lowPower;
    bool _drdyEnable;

    uint8_t _writeBuffer[1] = {0};
    uint8_t _readBuffer[1] = {0};

    float _tempGain = 1.0f;
    float _tempOffset = 0.0f;
    float _tempBaseOffset = static_cast<float>(LIS2MDL_TEMP_OFFSET);

    float _headingOffsetDeg = 0.0f;
    float _declinationDeg = 0.0f;

    float _hfAlpha = 0.0f;
    float _hfCos = 0.0f;
    float _hfSin = 0.0f;

    static constexpr float MAG_LSB_TO_UT = 0.15f;

    void writeReg(uint8_t reg, uint8_t data);
    void write16(uint8_t regL, int16_t value);
    uint8_t readReg(uint8_t reg);
    int16_t read16(uint8_t reg);
    bool readBytes(uint8_t reg, uint8_t* buffer, uint8_t length);
    bool ensureData();
    int16_t toInt16(uint16_t v);
    float normalizeDeg(float a);
    float applyHeadingOffsets(float angleDeg);
    float filterHeading(float angleDeg);
};