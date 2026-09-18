// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <Arduino.h>
#include <Wire.h>

#include "ISM330DL_const.h"

class ISM330DL {
   public:
    enum class AccelOdr : uint8_t {
        POWER_DOWN = 0x00,
        HZ_12_5 = 0x01,
        HZ_26 = 0x02,
        HZ_52 = 0x03,
        HZ_104 = 0x04,
        HZ_208 = 0x05,
        HZ_416 = 0x06,
        HZ_833 = 0x07,
        HZ_1660 = 0x08,
    };

    enum class AccelScale : uint8_t {
        G_2 = 2,
        G_4 = 4,
        G_8 = 8,
        G_16 = 16,
    };

    enum class GyroOdr : uint8_t {
        POWER_DOWN = 0x00,
        HZ_12_5 = 0x01,
        HZ_26 = 0x02,
        HZ_52 = 0x03,
        HZ_104 = 0x04,
        HZ_208 = 0x05,
        HZ_416 = 0x06,
        HZ_833 = 0x07,
        HZ_1660 = 0x08,
    };

    enum class GyroScale : uint16_t {
        DPS_125 = 125,
        DPS_250 = 250,
        DPS_500 = 500,
        DPS_1000 = 1000,
        DPS_2000 = 2000,
    };

    enum class Orientation : uint8_t {
        SCREEN_DOWN,
        SCREEN_UP,
        TOP_EDGE_DOWN,
        BOTTOM_EDGE_DOWN,
        RIGHT_EDGE_DOWN,
        LEFT_EDGE_DOWN,
        MOVING,
    };

    enum class MotionType : uint8_t {
        TURNING_RIGHT,
        TURNING_LEFT,
        TILTING_LEFT,
        TILTING_RIGHT,
        TILTING_DOWN,
        TILTING_UP,
        STABLE,
    };

    struct RawVector {
        int16_t x;
        int16_t y;
        int16_t z;
    };

    struct Vector3 {
        float x;
        float y;
        float z;
    };

    struct Motion {
        MotionType type;
        float value;
    };

    explicit ISM330DL(TwoWire& wire = Wire, uint8_t address = ISM330DL_DEFAULT_ADDRESS);

    bool begin();
    uint8_t deviceId();
    bool isConnected();
    bool softReset();

    bool configureAccel(AccelOdr odr, AccelScale scale);
    bool configureGyro(GyroOdr odr, GyroScale scale);

    bool accelerationRaw(RawVector& value);
    bool gyroscopeRaw(RawVector& value);
    bool temperatureRaw(int16_t& value);

    bool accelerationG(Vector3& value);
    bool accelerationMs2(Vector3& value);
    bool gyroscopeDps(Vector3& value);
    bool gyroscopeRads(Vector3& value);
    bool temperature(float& value);

    void setAccelOffset(float x = 0.0F, float y = 0.0F, float z = 0.0F);
    Vector3 accelOffset() const;
    void setTemperatureOffset(float offsetC);
    bool calibrateTemperature(float refLow, float measuredLow, float refHigh, float measuredHigh);

    bool orientation(Orientation& value);
    bool motion(Motion& value);
    static const char* orientationToString(Orientation value);
    static const char* motionToString(MotionType value);

    uint8_t status();
    bool accelReady();
    bool gyroReady();
    bool temperatureReady();
    bool dataReady();

    bool powerOff();
    bool powerOn();

   private:
    TwoWire& _wire;
    uint8_t _address;

    AccelScale _accelScale;
    AccelOdr _accelOdr;
    GyroScale _gyroScale;
    GyroOdr _gyroOdr;

    Vector3 _accelOffset;
    float _tempGain;
    float _tempOffset;

    bool readReg(uint8_t reg, uint8_t& value);
    bool writeReg(uint8_t reg, uint8_t value);
    bool readBytes(uint8_t reg, uint8_t* buffer, size_t length);
    bool readInt16(uint8_t reg, int16_t& value);
    bool readVector(uint8_t reg, RawVector& value);
    bool isPowerDown(bool& value);
    bool ensureData();

    static bool accelOdrValid(AccelOdr odr);
    static bool gyroOdrValid(GyroOdr odr);
    static bool accelScaleBits(AccelScale scale, uint8_t& bits);
    static bool gyroScaleBits(GyroScale scale, uint8_t& bits, bool& fs125);
    static float accelSensitivityMg(AccelScale scale);
    static float gyroSensitivityMdps(GyroScale scale);
};
