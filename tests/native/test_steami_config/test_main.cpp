// SPDX-License-Identifier: GPL-3.0-or-later

#include <unity.h>

#include "DaplinkBridge.h"
#include "FakeDaplinkBridge.h"
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

// ---------------------------------------------------------------------
// Persistence, against a model of the bridge firmware
// ---------------------------------------------------------------------

static FakeDaplinkBridge& attachFirmware() {
    static FakeDaplinkBridge firmware;
    firmware = FakeDaplinkBridge();
    Wire.attachDevice(DAPLINK_BRIDGE_DEFAULT_ADDR, &firmware);
    return firmware;
}

static String repeat(char c, size_t count) {
    String text;
    for (size_t i = 0; i < count; ++i) {
        text += c;
    }
    return text;
}

void test_save_then_load_round_trip(void) {
    attachFirmware();
    TEST_ASSERT_TRUE(config.begin());

    config.setBoardRevision(3);
    config.setBoardName("STeaMi \"C1\"");
    config.setTemperatureCalibration("wsen_hids", 1.02f, -0.75f);
    config.setMagnetometerCalibration(10.5f, -3.25f, 0.0f, 1.1f, 0.9f, 1.0f);
    config.setAccelerometerCalibration(0.01f, -0.02f, 0.03f);
    config.setBootCount(12);
    TEST_ASSERT_TRUE(config.save());

    SteamiConfig reloaded(bridge);
    TEST_ASSERT_TRUE(reloaded.load());

    int32_t revision = 0;
    String name;
    uint32_t bootCount = 0;
    TemperatureCalibration temperature;
    MagnetometerCalibration magnetometer;
    AccelerometerCalibration accelerometer;

    TEST_ASSERT_TRUE(reloaded.boardRevision(revision));
    TEST_ASSERT_EQUAL_INT32(3, revision);
    TEST_ASSERT_TRUE(reloaded.boardName(name));
    TEST_ASSERT_EQUAL_STRING("STeaMi \"C1\"", name.c_str());
    TEST_ASSERT_TRUE(reloaded.getTemperatureCalibration("wsen_hids", temperature));
    TEST_ASSERT_EQUAL_FLOAT(1.02f, temperature.gain);
    TEST_ASSERT_EQUAL_FLOAT(-0.75f, temperature.offset);
    TEST_ASSERT_FALSE(reloaded.getTemperatureCalibration("hts221", temperature));
    TEST_ASSERT_TRUE(reloaded.getMagnetometerCalibration(magnetometer));
    TEST_ASSERT_EQUAL_FLOAT(-3.25f, magnetometer.hardIronY);
    TEST_ASSERT_EQUAL_FLOAT(0.9f, magnetometer.softIronY);
    TEST_ASSERT_TRUE(reloaded.getAccelerometerCalibration(accelerometer));
    TEST_ASSERT_EQUAL_FLOAT(-0.02f, accelerometer.offsetY);
    TEST_ASSERT_TRUE(reloaded.bootCount(bootCount));
    TEST_ASSERT_EQUAL_UINT32(12, bootCount);
}

void test_load_reads_micropython_json(void) {
    // Same document as micropython-steami-lib's SteamiConfig.save(), so a
    // board calibrated from MicroPython keeps its settings under Arduino.
    FakeDaplinkBridge& firmware = attachFirmware();
    firmware.store(
        "{\"rev\":3,\"name\":\"STeaMi-01\",\"tc\":{\"hts\":{\"g\":1.0,\"o\":-0.5}},"
        "\"cm\":{\"hx\":12.5,\"hy\":-4.0,\"hz\":0.5,\"sx\":1.0,\"sy\":1.0,\"sz\":1.0},"
        "\"ca\":{\"ox\":0.01,\"oy\":0.0,\"oz\":-0.02},\"bc\":42}");

    TEST_ASSERT_TRUE(config.load());

    int32_t revision = 0;
    String name;
    uint32_t bootCount = 0;
    TemperatureCalibration temperature;
    MagnetometerCalibration magnetometer;

    TEST_ASSERT_TRUE(config.boardRevision(revision));
    TEST_ASSERT_EQUAL_INT32(3, revision);
    TEST_ASSERT_TRUE(config.boardName(name));
    TEST_ASSERT_EQUAL_STRING("STeaMi-01", name.c_str());
    TEST_ASSERT_TRUE(config.getTemperatureCalibration("hts221", temperature));
    TEST_ASSERT_EQUAL_FLOAT(-0.5f, temperature.offset);
    TEST_ASSERT_TRUE(config.getMagnetometerCalibration(magnetometer));
    TEST_ASSERT_EQUAL_FLOAT(12.5f, magnetometer.hardIronX);
    TEST_ASSERT_TRUE(config.bootCount(bootCount));
    TEST_ASSERT_EQUAL_UINT32(42, bootCount);
}

void test_save_and_load_config_longer_than_256_bytes(void) {
    attachFirmware();
    const String longName = repeat('n', 600);

    config.setBoardName(longName);
    config.setBootCount(7);
    TEST_ASSERT_TRUE(config.save());

    SteamiConfig reloaded(bridge);
    TEST_ASSERT_TRUE(reloaded.load());

    String name;
    uint32_t bootCount = 0;
    TEST_ASSERT_TRUE(reloaded.boardName(name));
    TEST_ASSERT_TRUE(name == longName);
    TEST_ASSERT_TRUE(reloaded.bootCount(bootCount));
    TEST_ASSERT_EQUAL_UINT32(7, bootCount);
}

void test_save_rejects_config_larger_than_zone(void) {
    FakeDaplinkBridge& firmware = attachFirmware();
    firmware.store("{\"bc\":1}");

    config.setBoardName(repeat('n', STEAMI_CONFIG_MAX_SIZE));
    TEST_ASSERT_FALSE(config.save());

    // The previous content is left untouched.
    TEST_ASSERT_EQUAL_STRING_LEN("{\"bc\":1}", reinterpret_cast<const char*>(firmware.zone.data()),
                                 8);
}

void test_load_rejects_truncated_json(void) {
    FakeDaplinkBridge& firmware = attachFirmware();
    firmware.store("{\"rev\":3,\"bc\":4");

    TEST_ASSERT_FALSE(config.load());

    int32_t revision = 0;
    TEST_ASSERT_FALSE(config.boardRevision(revision));
}

void test_load_of_empty_zone_gives_empty_config(void) {
    attachFirmware();
    config.setBootCount(5);

    TEST_ASSERT_TRUE(config.load());

    uint32_t bootCount = 0;
    TEST_ASSERT_FALSE(config.bootCount(bootCount));
}

void test_legacy_firmware_is_refused_without_locking_it_up(void) {
    FakeDaplinkBridge& firmware = attachFirmware();
    firmware.legacyFirmware = true;

    TEST_ASSERT_FALSE(config.begin());
    TEST_ASSERT_FALSE(config.load());

    config.setBootCount(1);
    TEST_ASSERT_FALSE(config.save());

    TEST_ASSERT_EQUAL_INT(0, firmware.configCommandsOnLegacy);
    TEST_ASSERT_FALSE(firmware.hung);
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
    RUN_TEST(test_save_then_load_round_trip);
    RUN_TEST(test_load_reads_micropython_json);
    RUN_TEST(test_save_and_load_config_longer_than_256_bytes);
    RUN_TEST(test_save_rejects_config_larger_than_zone);
    RUN_TEST(test_load_rejects_truncated_json);
    RUN_TEST(test_load_of_empty_zone_gives_empty_config);
    RUN_TEST(test_legacy_firmware_is_refused_without_locking_it_up);
    return UNITY_END();
}
