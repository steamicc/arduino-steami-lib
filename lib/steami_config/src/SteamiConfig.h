// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <Arduino.h>
#include <DaplinkBridge.h>

#include "SteamiConfig_const.h"

struct TemperatureCalibration {
    float gain = 1.0f;
    float offset = 0.0f;
};

struct MagnetometerCalibration {
    float hardIronX = 0.0f;
    float hardIronY = 0.0f;
    float hardIronZ = 0.0f;
    float softIronX = 1.0f;
    float softIronY = 1.0f;
    float softIronZ = 1.0f;
};

struct AccelerometerCalibration {
    float offsetX = 0.0f;
    float offsetY = 0.0f;
    float offsetZ = 0.0f;
};

class SteamiConfig {
   public:
    explicit SteamiConfig(DaplinkBridge& bridge);

    // --- Lifecycle / persistence ---
    bool begin();
    bool load();
    bool save();
    void clear();

    // --- Board info ---
    void setBoardRevision(int32_t revision);
    bool boardRevision(int32_t& revision) const;
    void clearBoardRevision();

    void setBoardName(const String& name);
    bool boardName(String& name) const;
    void clearBoardName();

    // --- Temperature calibration ---
    bool setTemperatureCalibration(const char* sensor, float gain = 1.0f, float offset = 0.0f);
    bool getTemperatureCalibration(const char* sensor, TemperatureCalibration& calibration) const;
    bool clearTemperatureCalibration(const char* sensor);

    template <typename Sensor>
    bool applyTemperatureCalibration(const char* sensor, Sensor& instance) const {
        TemperatureCalibration calibration;
        if (!getTemperatureCalibration(sensor, calibration)) {
            return false;
        }

        // Project temperature drivers expose a two-point linear calibration hook.
        // Choosing measured points 0 and 1 reproduces:
        // corrected = measured * gain + offset.
        instance.calibrateTemperature(calibration.offset, 0.0f,
                                      calibration.gain + calibration.offset, 1.0f);
        return true;
    }

    // --- Magnetometer calibration ---
    void setMagnetometerCalibration(float hardIronX = 0.0f, float hardIronY = 0.0f,
                                    float hardIronZ = 0.0f, float softIronX = 1.0f,
                                    float softIronY = 1.0f, float softIronZ = 1.0f);
    bool getMagnetometerCalibration(MagnetometerCalibration& calibration) const;
    void clearMagnetometerCalibration();

    // --- Accelerometer calibration ---
    void setAccelerometerCalibration(float offsetX = 0.0f, float offsetY = 0.0f,
                                     float offsetZ = 0.0f);
    bool getAccelerometerCalibration(AccelerometerCalibration& calibration) const;
    void clearAccelerometerCalibration();

    template <typename Sensor>
    bool applyAccelerometerCalibration(Sensor& instance) const {
        AccelerometerCalibration calibration;
        if (!getAccelerometerCalibration(calibration)) {
            return false;
        }

        instance.setAccelOffset(calibration.offsetX, calibration.offsetY, calibration.offsetZ);
        return true;
    }

    // --- Boot counter ---
    void setBootCount(uint32_t count);
    bool bootCount(uint32_t& count) const;
    uint32_t incrementBootCount();

   private:
    struct TemperatureEntry {
        bool present = false;
        TemperatureCalibration calibration;
    };

    DaplinkBridge* _bridge;

    bool _hasBoardRevision = false;
    int32_t _boardRevision = 0;

    bool _hasBoardName = false;
    String _boardName;

    TemperatureEntry _temperature[STEAMI_CONFIG_SENSOR_COUNT];

    bool _hasMagnetometerCalibration = false;
    MagnetometerCalibration _magnetometerCalibration;

    bool _hasAccelerometerCalibration = false;
    AccelerometerCalibration _accelerometerCalibration;

    bool _hasBootCount = false;
    uint32_t _bootCount = 0;

    int8_t sensorIndex(const char* sensor) const;
    const char* sensorShortKey(uint8_t index) const;

    bool parseJson(const char* json);
    bool serializeJson(String& json) const;

    static const char* skipWhitespace(const char* cursor);
    static const char* findValue(const char* json, const char* key);
    static bool extractObject(const char* json, const char* key, String& object);
    static bool parseFloatValue(const char* json, const char* key, float& value);
    static bool parseIntValue(const char* json, const char* key, int32_t& value);
    static bool parseUintValue(const char* json, const char* key, uint32_t& value);
    static bool parseStringValue(const char* json, const char* key, String& value);
    static void appendEscapedString(String& json, const String& value);
};
