// SPDX-License-Identifier: GPL-3.0-or-later

/*
 * Hardware unit tests for SteamiConfig on a real STeaMi.
 *
 * These tests write to the DAPLink persistent configuration zone. The
 * zone content is saved before the first test and written back after the
 * last one, so the board keeps its own configuration.
 */

#include <Arduino.h>
#include <DaplinkBridge.h>
#include <SteamiConfig.h>
#include <Wire.h>
#include <unity.h>

#include "config_zone_backup.h"

TwoWire internalI2C(I2C_INT_SDA, I2C_INT_SCL);
DaplinkBridge bridge(internalI2C);
SteamiConfig config(bridge);
ConfigZoneBackup backup;

void setUp(void) {
    config.clear();
}

void tearDown(void) {}

void test_steami_config_begin(void) {
    TEST_ASSERT_TRUE(config.begin());
}

void test_steami_config_board_info_round_trip(void) {
    config.setBoardRevision(3);
    config.setBoardName("STeaMi-HW");

    TEST_ASSERT_TRUE(config.save());

    config.clear();
    TEST_ASSERT_TRUE(config.load());

    int32_t revision = 0;
    String name;

    TEST_ASSERT_TRUE(config.boardRevision(revision));
    TEST_ASSERT_EQUAL_INT32(3, revision);

    TEST_ASSERT_TRUE(config.boardName(name));
    TEST_ASSERT_EQUAL_STRING("STeaMi-HW", name.c_str());
}

void test_steami_config_temperature_calibration_round_trip(void) {
    TEST_ASSERT_TRUE(config.setTemperatureCalibration("hts221", 1.01f, -0.5f));
    TEST_ASSERT_TRUE(config.save());

    config.clear();
    TEST_ASSERT_TRUE(config.load());

    TemperatureCalibration calibration;
    TEST_ASSERT_TRUE(config.getTemperatureCalibration("hts221", calibration));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.01f, calibration.gain);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -0.5f, calibration.offset);
}

void test_steami_config_boot_counter_round_trip(void) {
    config.setBootCount(41);
    TEST_ASSERT_TRUE(config.save());

    config.clear();
    TEST_ASSERT_TRUE(config.load());

    uint32_t count = 0;
    TEST_ASSERT_TRUE(config.bootCount(count));
    TEST_ASSERT_EQUAL_UINT32(41, count);

    TEST_ASSERT_EQUAL_UINT32(42, config.incrementBootCount());
    TEST_ASSERT_TRUE(config.save());

    config.clear();
    TEST_ASSERT_TRUE(config.load());
    TEST_ASSERT_TRUE(config.bootCount(count));
    TEST_ASSERT_EQUAL_UINT32(42, count);
}

void test_steami_config_mixed_small_config_round_trip(void) {
    config.setBoardRevision(4);
    config.setBoardName("STeaMi");
    TEST_ASSERT_TRUE(config.setTemperatureCalibration("hts221", 1.02f, -0.25f));
    config.setBootCount(9);

    TEST_ASSERT_TRUE(config.save());

    config.clear();
    TEST_ASSERT_TRUE(config.load());

    int32_t revision = 0;
    String name;
    TemperatureCalibration calibration;
    uint32_t count = 0;

    TEST_ASSERT_TRUE(config.boardRevision(revision));
    TEST_ASSERT_EQUAL_INT32(4, revision);

    TEST_ASSERT_TRUE(config.boardName(name));
    TEST_ASSERT_EQUAL_STRING("STeaMi", name.c_str());

    TEST_ASSERT_TRUE(config.getTemperatureCalibration("hts221", calibration));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.02f, calibration.gain);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -0.25f, calibration.offset);

    TEST_ASSERT_TRUE(config.bootCount(count));
    TEST_ASSERT_EQUAL_UINT32(9, count);
}

void test_steami_config_long_config_round_trip(void) {
    // Longer than one 256-byte READ_CONFIG block.
    String name;
    for (int i = 0; i < 400; ++i) {
        name += static_cast<char>('a' + (i % 26));
    }
    config.setBoardName(name);
    config.setBootCount(1234);

    TEST_ASSERT_TRUE(config.save());

    config.clear();
    TEST_ASSERT_TRUE(config.load());

    String loaded;
    uint32_t count = 0;
    TEST_ASSERT_TRUE(config.boardName(loaded));
    TEST_ASSERT_EQUAL_STRING(name.c_str(), loaded.c_str());
    TEST_ASSERT_TRUE(config.bootCount(count));
    TEST_ASSERT_EQUAL_UINT32(1234, count);
}

void test_steami_config_restore_board_config(void) {
    TEST_ASSERT_TRUE(backup.restore(bridge));
}

void setup() {
    delay(2000);
    internalI2C.begin();
    bridge.begin();
    backup.take(bridge);

    UNITY_BEGIN();
    RUN_TEST(test_steami_config_begin);
    RUN_TEST(test_steami_config_board_info_round_trip);
    RUN_TEST(test_steami_config_temperature_calibration_round_trip);
    RUN_TEST(test_steami_config_boot_counter_round_trip);
    RUN_TEST(test_steami_config_mixed_small_config_round_trip);
    RUN_TEST(test_steami_config_long_config_round_trip);
    RUN_TEST(test_steami_config_restore_board_config);
    UNITY_END();
}

void loop() {}
