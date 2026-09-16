// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <Arduino.h>
#include <Wire.h>

#include "APDS9960_const.h"

class APDS9960 {
   public:
    enum class Mode : uint8_t {
        POWER = 0,
        AMBIENT_LIGHT = 1,
        PROXIMITY = 2,
        WAIT = 3,
        AMBIENT_LIGHT_INTERRUPT = 4,
        PROXIMITY_INTERRUPT = 5,
        GESTURE = 6,
        ALL = 7,
    };

    enum class Gesture : uint8_t {
        NONE = 0,
        LEFT,
        RIGHT,
        UP,
        DOWN,
        NEAR,
        FAR,
    };

    enum class LedDrive : uint8_t {
        MA_100 = 0,
        MA_50,
        MA_25,
        MA_12_5,
    };

    enum class ProximityGain : uint8_t {
        X1 = 0,
        X2,
        X4,
        X8,
    };

    enum class AmbientLightGain : uint8_t {
        X1 = 0,
        X4,
        X16,
        X64,
    };

    enum class GestureGain : uint8_t {
        X1 = 0,
        X2,
        X4,
        X8,
    };

    enum class LedBoost : uint8_t {
        PERCENT_100 = 0,
        PERCENT_150,
        PERCENT_200,
        PERCENT_300,
    };

    enum class GestureWaitTime : uint8_t {
        MS_0 = 0,
        MS_2_8,
        MS_5_6,
        MS_8_4,
        MS_14,
        MS_22_4,
        MS_30_8,
        MS_39_2,
    };

    APDS9960(TwoWire& wire = Wire, uint8_t address = APDS9960_DEFAULT_ADDRESS);

    bool begin();
    uint8_t deviceId();

    uint8_t status();
    uint8_t mode();
    bool setMode(Mode mode, bool enable = true);

    void powerOn();
    void powerOff();

    bool dataReady();
    bool lightReady();
    bool proximityReady();

    void enableLightSensor(bool interrupts = false);
    void disableLightSensor();
    void enableProximitySensor(bool interrupts = false);
    void disableProximitySensor();
    void enableGestureSensor(bool interrupts = false);
    void disableGestureSensor();

    bool ambientLight(uint16_t& value);
    bool redLight(uint16_t& value);
    bool greenLight(uint16_t& value);
    bool blueLight(uint16_t& value);
    bool proximity(uint8_t& value);

    bool gestureAvailable();
    Gesture readGesture(uint32_t timeoutMs = 1000);
    void resetGestureParameters();

    AmbientLightGain ambientLightGain();
    void setAmbientLightGain(AmbientLightGain gain);

    ProximityGain proximityGain();
    void setProximityGain(ProximityGain gain);

    LedDrive ledDrive();
    void setLedDrive(LedDrive drive);

    LedBoost ledBoost();
    void setLedBoost(LedBoost boost);

    bool proximityGainCompensationEnabled();
    void setProximityGainCompensation(bool enable);

    uint8_t proximityPhotodiodeMask();
    void setProximityPhotodiodeMask(uint8_t mask);

    uint8_t gestureEnterThreshold();
    void setGestureEnterThreshold(uint8_t threshold);

    uint8_t gestureExitThreshold();
    void setGestureExitThreshold(uint8_t threshold);

    GestureGain gestureGain();
    void setGestureGain(GestureGain gain);

    LedDrive gestureLedDrive();
    void setGestureLedDrive(LedDrive drive);

    GestureWaitTime gestureWaitTime();
    void setGestureWaitTime(GestureWaitTime waitTime);

    uint16_t lightInterruptLowThreshold();
    void setLightInterruptLowThreshold(uint16_t threshold);

    uint16_t lightInterruptHighThreshold();
    void setLightInterruptHighThreshold(uint16_t threshold);

    uint8_t proximityInterruptLowThreshold();
    void setProximityInterruptLowThreshold(uint8_t threshold);

    uint8_t proximityInterruptHighThreshold();
    void setProximityInterruptHighThreshold(uint8_t threshold);

    bool ambientLightInterruptEnabled();
    void setAmbientLightInterrupt(bool enable);

    bool proximityInterruptEnabled();
    void setProximityInterrupt(bool enable);

    bool gestureInterruptEnabled();
    void setGestureInterrupt(bool enable);

    void clearAmbientLightInterrupt();
    void clearProximityInterrupt();

    bool gestureModeEnabled();
    void setGestureMode(bool enable);

   private:
    struct GestureData {
        uint8_t up[APDS9960_GESTURE_DATASETS_MAX] = {};
        uint8_t down[APDS9960_GESTURE_DATASETS_MAX] = {};
        uint8_t left[APDS9960_GESTURE_DATASETS_MAX] = {};
        uint8_t right[APDS9960_GESTURE_DATASETS_MAX] = {};
        uint8_t index = 0;
        uint8_t total = 0;
    };

    TwoWire* _wire;
    uint8_t _address;

    GestureData _gestureData;
    int16_t _gestureUdDelta = 0;
    int16_t _gestureLrDelta = 0;
    int8_t _gestureUdCount = 0;
    int8_t _gestureLrCount = 0;
    uint8_t _gestureNearCount = 0;
    uint8_t _gestureFarCount = 0;
    Gesture _gestureMotion = Gesture::NONE;
    Gesture _gestureState = Gesture::NONE;

    bool validDeviceId(uint8_t id) const;
    bool waitForLight(uint32_t timeoutMs = APDS9960_DEFAULT_READY_TIMEOUT_MS);
    bool waitForProximity(uint32_t timeoutMs = APDS9960_DEFAULT_READY_TIMEOUT_MS);
    bool ensureLightEnabled();
    bool ensureProximityEnabled();

    bool readColorChannel(uint8_t lowRegister, uint16_t& value);
    bool processGestureData();
    bool decodeGesture();

    bool readReg(uint8_t reg, uint8_t& value);
    bool writeReg(uint8_t reg, uint8_t value);
    size_t readRegs(uint8_t reg, uint8_t* buffer, size_t length);
    void updateRegister(uint8_t reg, uint8_t mask, uint8_t value);
};
