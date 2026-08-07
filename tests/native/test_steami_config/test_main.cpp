// SPDX-License-Identifier: GPL-3.0-or-later

#include <unity.h>

#include "DaplinkBridge.h"
#include "SteamiConfig.h"
#include "Wire.h"

DaplinkBridge bridge(Wire);
SteamiConfig config(bridge);

struct FakeTemperatureSensor {
    float refLow = 0.0f;
    float measLow = 0.0f;
    float refHigh = 0.0f;
    float measHigh = 0.0f;
    bool called = false;

    void calibrateTemperature(float newRefLow, float newMeasLow, float newRefHigh,
                              float newMeasHigh) {
        refLow = newRefLow;
        measLow = newMeasLow;
        refHigh = newRefHigh;
        measHigh = newMeasHigh;
        called = true;
    }
};

struct FakeAccelerometer {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    bool called = false;

    void setAccelOffset(float newX, float newY, float newZ) {
        x = newX;
        y = newY;
        z = newZ;
        called = true;
    }
};

void setUp(void) {
    Wire = TwoWire();
    bridge = DaplinkBridge(Wire);
    config = SteamiConfig(bridge);
}

void tearDown(void) {}

void test_initial_config_is_empty(void) {
    int32_t revision = 0;
    String name;
    uint32_t bootCount = 0;
    TemperatureCalibration temperature;
    MagnetometerCalibration magnetometer;
    AccelerometerCalibration accelerometer;

    TEST_ASSERT_FALSE(config.boardRevision(revision));
    TEST_ASSERT_FALSE(config.boardName(name));
    TEST_ASSERT_FALSE(config.bootCount(bootCount));
    TEST_ASSERT_FALSE(config.getTemperatureCalibration("hts221", temperature));
    TEST_ASSERT_FALSE(config.getMagnetometerCalibration(magnetometer));
    TEST_ASSERT_FALSE(config.getAccelerometerCalibration(accelerometer));
}

void test_board_revision_set_get_and_clear(void) {
    int32_t revision = 0;

    config.setBoardRevision(3);
    TEST_ASSERT_TRUE(config.boardRevision(revision));
    TEST_ASSERT_EQUAL_INT32(3, revision);

    config.clearBoardRevision();
    TEST_ASSERT_FALSE(config.boardRevision(revision));
}

void test_board_name_set_get_and_clear(void) {
    String name;

    config.setBoardName("STeaMi-Test");
    TEST_ASSERT_TRUE(config.boardName(name));
    TEST_ASSERT_EQUAL_STRING("STeaMi-Test", name.c_str());

    config.clearBoardName();
    TEST_ASSERT_FALSE(config.boardName(name));
}

void test_temperature_calibration_supported_sensors(void) {
    const char* sensors[] = {
        "hts221", "lis2mdl", "ism330dl", "wsen_hids", "wsen_pads",
    };

    for (size_t i = 0; i < sizeof(sensors) / sizeof(sensors[0]); ++i) {
        const float gain = 1.0f + static_cast<float>(i) * 0.01f;
        const float offset = -0.5f + static_cast<float>(i) * 0.1f;

        TEST_ASSERT_TRUE(config.setTemperatureCalibration(sensors[i], gain, offset));

        TemperatureCalibration calibration;
        TEST_ASSERT_TRUE(config.getTemperatureCalibration(sensors[i], calibration));
        TEST_ASSERT_FLOAT_WITHIN(0.0001f, gain, calibration.gain);
        TEST_ASSERT_FLOAT_WITHIN(0.0001f, offset, calibration.offset);
    }
}

void test_temperature_calibration_rejects_unknown_sensor(void) {
    TemperatureCalibration calibration;

    TEST_ASSERT_FALSE(config.setTemperatureCalibration("unknown", 1.0f, 0.0f));
    TEST_ASSERT_FALSE(config.getTemperatureCalibration("unknown", calibration));
    TEST_ASSERT_FALSE(config.clearTemperatureCalibration("unknown"));
}

void test_temperature_calibration_clear(void) {
    TemperatureCalibration calibration;

    TEST_ASSERT_TRUE(config.setTemperatureCalibration("hts221", 1.02f, -0.3f));
    TEST_ASSERT_TRUE(config.clearTemperatureCalibration("hts221"));
    TEST_ASSERT_FALSE(config.getTemperatureCalibration("hts221", calibration));
}

void test_apply_temperature_calibration_uses_linear_hook(void) {
    FakeTemperatureSensor sensor;

    TEST_ASSERT_TRUE(config.setTemperatureCalibration("hts221", 1.25f, -0.5f));
    TEST_ASSERT_TRUE(config.applyTemperatureCalibration("hts221", sensor));

    TEST_ASSERT_TRUE(sensor.called);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, -0.5f, sensor.refLow);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.0f, sensor.measLow);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.75f, sensor.refHigh);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 1.0f, sensor.measHigh);
}

void test_apply_temperature_calibration_returns_false_when_missing(void) {
    FakeTemperatureSensor sensor;

    TEST_ASSERT_FALSE(config.applyTemperatureCalibration("hts221", sensor));
    TEST_ASSERT_FALSE(sensor.called);
}

void test_magnetometer_calibration_set_get_and_clear(void) {
    config.setMagnetometerCalibration(12.3f, -5.1f, 0.8f, 1.01f, 0.98f, 1.0f);

    MagnetometerCalibration calibration;
    TEST_ASSERT_TRUE(config.getMagnetometerCalibration(calibration));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 12.3f, calibration.hardIronX);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, -5.1f, calibration.hardIronY);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.8f, calibration.hardIronZ);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 1.01f, calibration.softIronX);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.98f, calibration.softIronY);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 1.0f, calibration.softIronZ);

    config.clearMagnetometerCalibration();
    TEST_ASSERT_FALSE(config.getMagnetometerCalibration(calibration));
}

void test_accelerometer_calibration_set_get_and_clear(void) {
    config.setAccelerometerCalibration(0.01f, -0.02f, 0.03f);

    AccelerometerCalibration calibration;
    TEST_ASSERT_TRUE(config.getAccelerometerCalibration(calibration));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.01f, calibration.offsetX);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, -0.02f, calibration.offsetY);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.03f, calibration.offsetZ);

    config.clearAccelerometerCalibration();
    TEST_ASSERT_FALSE(config.getAccelerometerCalibration(calibration));
}

void test_apply_accelerometer_calibration_uses_driver_hook(void) {
    FakeAccelerometer sensor;

    config.setAccelerometerCalibration(0.01f, -0.02f, 0.03f);
    TEST_ASSERT_TRUE(config.applyAccelerometerCalibration(sensor));

    TEST_ASSERT_TRUE(sensor.called);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.01f, sensor.x);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, -0.02f, sensor.y);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 0.03f, sensor.z);
}

void test_apply_accelerometer_calibration_returns_false_when_missing(void) {
    FakeAccelerometer sensor;

    TEST_ASSERT_FALSE(config.applyAccelerometerCalibration(sensor));
    TEST_ASSERT_FALSE(sensor.called);
}

void test_boot_counter_set_get_and_increment(void) {
    uint32_t count = 0;

    TEST_ASSERT_FALSE(config.bootCount(count));

    TEST_ASSERT_EQUAL_UINT32(1, config.incrementBootCount());
    TEST_ASSERT_TRUE(config.bootCount(count));
    TEST_ASSERT_EQUAL_UINT32(1, count);

    config.setBootCount(41);
    TEST_ASSERT_TRUE(config.bootCount(count));
    TEST_ASSERT_EQUAL_UINT32(41, count);

    TEST_ASSERT_EQUAL_UINT32(42, config.incrementBootCount());
    TEST_ASSERT_TRUE(config.bootCount(count));
    TEST_ASSERT_EQUAL_UINT32(42, count);
}

void test_clear_resets_all_in_memory_values(void) {
    config.setBoardRevision(3);
    config.setBoardName("STeaMi");
    config.setTemperatureCalibration("hts221", 1.01f, -0.5f);
    config.setMagnetometerCalibration(1.0f, 2.0f, 3.0f, 1.1f, 1.2f, 1.3f);
    config.setAccelerometerCalibration(0.1f, 0.2f, 0.3f);
    config.setBootCount(7);

    config.clear();

    int32_t revision = 0;
    String name;
    uint32_t bootCount = 0;
    TemperatureCalibration temperature;
    MagnetometerCalibration magnetometer;
    AccelerometerCalibration accelerometer;

    TEST_ASSERT_FALSE(config.boardRevision(revision));
    TEST_ASSERT_FALSE(config.boardName(name));
    TEST_ASSERT_FALSE(config.bootCount(bootCount));
    TEST_ASSERT_FALSE(config.getTemperatureCalibration("hts221", temperature));
    TEST_ASSERT_FALSE(config.getMagnetometerCalibration(magnetometer));
    TEST_ASSERT_FALSE(config.getAccelerometerCalibration(accelerometer));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_initial_config_is_empty);
    RUN_TEST(test_board_revision_set_get_and_clear);
    RUN_TEST(test_board_name_set_get_and_clear);
    RUN_TEST(test_temperature_calibration_supported_sensors);
    RUN_TEST(test_temperature_calibration_rejects_unknown_sensor);
    RUN_TEST(test_temperature_calibration_clear);
    RUN_TEST(test_apply_temperature_calibration_uses_linear_hook);
    RUN_TEST(test_apply_temperature_calibration_returns_false_when_missing);
    RUN_TEST(test_magnetometer_calibration_set_get_and_clear);
    RUN_TEST(test_accelerometer_calibration_set_get_and_clear);
    RUN_TEST(test_apply_accelerometer_calibration_uses_driver_hook);
    RUN_TEST(test_apply_accelerometer_calibration_returns_false_when_missing);
    RUN_TEST(test_boot_counter_set_get_and_increment);
    RUN_TEST(test_clear_resets_all_in_memory_values);
    return UNITY_END();
}
