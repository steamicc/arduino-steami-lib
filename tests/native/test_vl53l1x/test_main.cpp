// SPDX-License-Identifier: GPL-3.0-or-later

#include <math.h>
#include <unity.h>

#include "VL53L1X.h"
#include "Wire.h"
#include "driver_checks.h"

constexpr uint8_t ADDR = VL53L1X_I2C_DEFAULT_ADDR;

static void preloadWhoAmI(bool valid = true) {
    uint8_t msb = valid ? (uint8_t)(VL53L1X_DEVICE_ID >> 8) : 0x42;
    uint8_t lsb = valid ? (uint8_t)(VL53L1X_DEVICE_ID & 0xFF) : 0x42;
    Wire.setRegister(ADDR, 0x010F, msb);
    Wire.setRegister(ADDR, 0x0110, lsb);
}

static void preloadDataReady(bool ready = true) {
    Wire.setRegister(ADDR, (uint8_t)REG_GPIO_HV_MUX_CTRL, 0x00);
    Wire.setRegister(ADDR, (uint8_t)REG_GPIO_TIO_HV_STATUS,
                     ready ? GPIO_TIO_HV_STATUS_DATA_READY : 0x00);
}

static void preloadDistance(uint16_t distance) {
    Wire.setRegister(ADDR, (uint8_t)(REG_RESULT_RANGE_STATUS + RESULT_DISTANCE_MSB_OFFSET),
                     (uint8_t)(distance >> 8));
    Wire.setRegister(ADDR, (uint8_t)(REG_RESULT_RANGE_STATUS + RESULT_DISTANCE_LSB_OFFSET),
                     (uint8_t)(distance & 0xFF));
}

VL53L1X sensor;

void setUp() {
    Wire = TwoWire();
    sensor.begin();
}

void tearDown() {}

void test_begin_detects_device() {
    preloadWhoAmI(true);
    check_begin(sensor);
}

void test_device_id_returns_who_am_i() {
    preloadWhoAmI(true);
    VL53L1X testSensor;  // ← Créé ici
    testSensor.begin();
    check_who_am_i(sensor, VL53L1X_DEVICE_ID);
}

void test_begin_rejects_wrong_who_am_i() {
    preloadWhoAmI(false);
    TEST_ASSERT_FALSE(sensor.begin());
}

void test_reset_writes_assert_then_release(void) {
    preloadDataReady(false);
    TEST_ASSERT_FALSE(sensor.dataReady());
}

void test_power_off_writes_assert(void) {
    sensor.powerOff();
    preloadDataReady(false);
    TEST_ASSERT_FALSE(sensor.dataReady());
}

void test_power_on_writes_release(void) {
    sensor.powerOn();
    Wire.clearWrites();
    sensor.powerOn();

    bool sawRelease = false;
    for (const auto& w : Wire.getWrites()) {
        if (w.reg == (uint8_t)REG_SOFT_RESET && w.value == SOFT_RESET_RELEASE) {
            sawRelease = true;
        }
    }
    TEST_ASSERT_TRUE(sawRelease);
}

void test_start_ranging_writes_start_value(void) {
    sensor.begin();
    Wire.clearWrites();

    sensor.startRanging();

    bool sawStart = false;
    for (const auto& w : Wire.getWrites()) {
        if (w.reg == (uint8_t)REG_SYSTEM_START && w.value == RANGING_START) {
            sawStart = true;
        }
    }
    TEST_ASSERT_TRUE(sawStart);
}

void test_stop_ranging_writes_stop_value(void) {
    sensor.stopRanging();
    Wire.clearWrites();
    sensor.stopRanging();

    bool sawStop = false;
    for (const auto& w : Wire.getWrites()) {
        if (w.reg == (uint8_t)REG_SYSTEM_START && w.value == RANGING_STOP) {
            sawStop = true;
        }
    }
    TEST_ASSERT_TRUE(sawStop);
}

void test_data_ready_true_when_status_bit_set(void) {
    preloadDataReady(true);
    TEST_ASSERT_TRUE(sensor.dataReady());
}

void test_data_ready_false_when_status_bit_clear(void) {
    preloadDataReady(false);
    TEST_ASSERT_FALSE(sensor.dataReady());
}

void test_data_ready_polarity_inverted(void) {
    sensor.begin();
    Wire.setRegister(ADDR, (uint8_t)REG_GPIO_HV_MUX_CTRL, GPIO_HV_MUX_CTRL_POLARITY);
    Wire.setRegister(ADDR, (uint8_t)REG_GPIO_TIO_HV_STATUS, 0x00);

    TEST_ASSERT_TRUE(sensor.dataReady());
}

void test_distance_mm_returns_correct_value(void) {
    preloadDistance(1234);
    TEST_ASSERT_EQUAL_UINT16(1234, sensor.distanceMm());
}

void test_read_returns_same_as_distance_mm(void) {
    preloadDistance(5678);
    TEST_ASSERT_EQUAL_UINT16(5678, sensor.read());
}

void test_distance_mm_is_plausible(void) {
    preloadDistance(200);
    uint16_t dist = sensor.distanceMm();
    TEST_ASSERT_TRUE(dist > 0 && dist < 4000);
}

void test_distance_clears_interrupt_after_read(void) {
    preloadDataReady(true);
    sensor.read();
    uint8_t interruptClearVal = Wire.getRegister(ADDR, (uint8_t)REG_SYSTEM_INTERRUPT_CLEAR);
    TEST_ASSERT_EQUAL_UINT8(INTERRUPT_CLEAR, interruptClearVal);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_begin_detects_device);
    RUN_TEST(test_begin_rejects_wrong_who_am_i);
    RUN_TEST(test_device_id_returns_who_am_i);
    RUN_TEST(test_reset_writes_assert_then_release);
    RUN_TEST(test_power_off_writes_assert);
    RUN_TEST(test_power_on_writes_release);
    RUN_TEST(test_start_ranging_writes_start_value);
    RUN_TEST(test_stop_ranging_writes_stop_value);
    RUN_TEST(test_data_ready_true_when_status_bit_set);
    RUN_TEST(test_data_ready_false_when_status_bit_clear);
    RUN_TEST(test_data_ready_polarity_inverted);
    RUN_TEST(test_distance_mm_returns_correct_value);
    RUN_TEST(test_read_returns_same_as_distance_mm);
    RUN_TEST(test_distance_mm_is_plausible);
    RUN_TEST(test_distance_clears_interrupt_after_read);
    return UNITY_END();
}