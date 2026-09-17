// SPDX-License-Identifier: GPL-3.0-or-later

/*
 * Integration test for SteamiConfig persistence on a real STeaMi.
 *
 * WARNING: this test overwrites the DAPLink persistent configuration zone.
 *
 * It exercises repeated save -> clear RAM -> load cycles to verify that
 * configuration remains coherent across continued use. The serialized data is
 * deliberately kept below the current 255-byte Arduino/DAPLink read limit.
 *
 * The number of write cycles is intentionally small to avoid unnecessary flash
 * wear on the DAPLink STM32F103 config zone.
 */

#include <Arduino.h>
#include <DaplinkBridge.h>
#include <SteamiConfig.h>
#include <Wire.h>
#include <unity.h>

TwoWire internalI2C(I2C_INT_SDA, I2C_INT_SCL);
DaplinkBridge bridge(internalI2C);
SteamiConfig config(bridge);

static constexpr uint32_t CYCLE_COUNT = 3;
static constexpr float EXPECTED_GAIN = 1.015f;
static constexpr float EXPECTED_OFFSET = -0.35f;

void test_steami_config_repeated_persistence_cycles(void) {
    TEST_ASSERT_TRUE(config.begin());

    config.clear();
    config.setBoardRevision(3);
    config.setBoardName("STeaMi-INT");
    TEST_ASSERT_TRUE(config.setTemperatureCalibration("hts221", EXPECTED_GAIN, EXPECTED_OFFSET));
    config.setBootCount(0);

    for (uint32_t cycle = 1; cycle <= CYCLE_COUNT; ++cycle) {
        TEST_ASSERT_EQUAL_UINT32(cycle, config.incrementBootCount());
        TEST_ASSERT_TRUE_MESSAGE(config.save(), "save() failed during persistence cycle");

        // Simulate loss of all runtime state while keeping persistent storage.
        config.clear();

        TEST_ASSERT_TRUE_MESSAGE(config.load(), "load() failed during persistence cycle");

        int32_t revision = 0;
        String name;
        TemperatureCalibration calibration;
        uint32_t count = 0;

        TEST_ASSERT_TRUE(config.boardRevision(revision));
        TEST_ASSERT_EQUAL_INT32(3, revision);

        TEST_ASSERT_TRUE(config.boardName(name));
        TEST_ASSERT_EQUAL_STRING("STeaMi-INT", name.c_str());

        TEST_ASSERT_TRUE(config.getTemperatureCalibration("hts221", calibration));
        TEST_ASSERT_FLOAT_WITHIN(0.001f, EXPECTED_GAIN, calibration.gain);
        TEST_ASSERT_FLOAT_WITHIN(0.001f, EXPECTED_OFFSET, calibration.offset);

        TEST_ASSERT_TRUE(config.bootCount(count));
        TEST_ASSERT_EQUAL_UINT32(cycle, count);

        delay(250);
    }
}

void test_steami_config_persists_after_idle_period(void) {
    config.clear();
    config.setBoardName("STeaMi-IDLE");
    config.setBootCount(77);

    TEST_ASSERT_TRUE(config.save());

    // Exercise persistence across a longer idle period without touching the
    // config object or DAPLink storage.
    delay(3000);

    config.clear();
    TEST_ASSERT_TRUE(config.load());

    String name;
    uint32_t count = 0;

    TEST_ASSERT_TRUE(config.boardName(name));
    TEST_ASSERT_EQUAL_STRING("STeaMi-IDLE", name.c_str());

    TEST_ASSERT_TRUE(config.bootCount(count));
    TEST_ASSERT_EQUAL_UINT32(77, count);
}

void setup() {
    delay(2000);
    internalI2C.begin();

    UNITY_BEGIN();
    RUN_TEST(test_steami_config_repeated_persistence_cycles);
    RUN_TEST(test_steami_config_persists_after_idle_period);
    UNITY_END();
}

void loop() {}
