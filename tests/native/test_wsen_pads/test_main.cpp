// SPDX-License-Identifier: GPL-3.0-or-later

#include <unity.h>

#include "WSEN_PADS.h"
#include "WSEN_PADS_const.h"
#include "Wire.h"

constexpr uint8_t ADDR = WSEN_PADS_I2C_DEFAULT_ADDR;

static void preloadDeviceId(bool valid = true) {
    Wire.setRegister(ADDR, REG_DEVICE_ID, valid ? WSEN_PADS_DEVICE_ID : 0x42);
}

static void preloadMeasurement(float tempC, float pressureHpa) {
    auto rawFromTemp = [](float t) -> int16_t { return static_cast<int16_t>(t / 0.01f); };
    auto rawPressur = [](float p) -> int32_t { return static_cast<int32_t>(p * 4096); };

    int16_t tempRaw = rawFromTemp(tempC);
    int32_t pressurRaw = rawPressur(pressureHpa);

    Wire.setRegister(ADDR, REG_DATA_P_XL, pressurRaw & 0xFF);
    Wire.setRegister(ADDR, REG_DATA_P_L, (pressurRaw >> 8) & 0xFF);
    Wire.setRegister(ADDR, REG_DATA_P_H, (pressurRaw >> 16) & 0xFF);

    Wire.setRegister(ADDR, REG_DATA_T_L, tempRaw & 0xFF);
    Wire.setRegister(ADDR, REG_DATA_T_H, (tempRaw >> 8) & 0xFF);

    Wire.setRegister(ADDR, REG_STATUS, STATUS_P_DA | STATUS_T_DA);
}

WSEN_PADS sensor;

void setUp() {
    Wire = TwoWire();
    preloadDeviceId();
    sensor = WSEN_PADS();
}

void tearDown(void) {}

void test_begin_detects_device(void) {
    TEST_ASSERT_TRUE(sensor.begin());
}

void test_begin_rejects_wrong_device_id(void) {
    preloadDeviceId(false);
    TEST_ASSERT_FALSE(sensor.begin());
}

void test_device_id_returns_correct_value(void) {
    preloadDeviceId(true);
    TEST_ASSERT_EQUAL_HEX8(WSEN_PADS_DEVICE_ID, sensor.deviceId());
}

void test_power_on_sets_ctrl1(void) {
    sensor.begin();
    sensor.powerOn();
    uint8_t ctrl1 = Wire.getRegister(ADDR, REG_CTRL_1);
    TEST_ASSERT_BITS_HIGH(CTRL1_BDU, ctrl1);
    TEST_ASSERT_NOT_EQUAL(0, ctrl1 & CTRL1_ODR_MASK);
}

void test_power_off_clears_ctrl1(void) {
    sensor.begin();
    sensor.powerOn();
    sensor.powerOff();
    uint8_t ctrl1 = Wire.getRegister(ADDR, REG_CTRL_1);
    TEST_ASSERT_EQUAL(0, ctrl1 & CTRL1_ODR_MASK);
}

void test_set_continuous_writes_expected_ctrl1(void) {
    sensor.begin();
    sensor.setContinuous();
    uint8_t ctrl1 = Wire.getRegister(ADDR, REG_CTRL_1);
    uint8_t expected = CTRL1_BDU | (ODR_1_HZ << CTRL1_ODR_SHIFT);
    TEST_ASSERT_EQUAL_HEX8(expected, ctrl1);
}

void test_trigger_one_shot_sets_ctrl2_one_shot(void) {
    sensor.begin();
    sensor.triggerOneShot();
    uint8_t ctrl2 = Wire.getRegister(ADDR, REG_CTRL_2);
    TEST_ASSERT_BITS_HIGH(CTRL2_ONE_SHOT, ctrl2);
}

void test_read_returns_converted_values(void) {
    sensor.begin();
    sensor.setContinuous(ODR_1_HZ);
    preloadMeasurement(23.0f, 1013.0f);

    auto r = sensor.read(false);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 23.0f, r.temperature);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 1013.0f, r.pressure);
}

void test_set_temp_offset_shifts_reading(void) {
    sensor.begin();
    sensor.setContinuous(ODR_1_HZ);
    preloadMeasurement(23.0f, 1013.0f);
    sensor.setTempOffset(1.5f);

    TEST_ASSERT_FLOAT_WITHIN(0.01f, 24.5f, sensor.temperature());
}

void test_calibrate_temperature_applies_correction(void) {
    sensor.begin();
    sensor.setContinuous(ODR_1_HZ);
    sensor.calibrateTemperature(1.0f, 0.0f, 22.0f, 20.0f);
    preloadMeasurement(20.0f, 1013.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 22.0f, sensor.temperature());
}

void test_reboot_writes_ctrl2_boot(void) {
    sensor.begin();
    Wire.clearWrites();
    Wire.setRegister(ADDR, REG_CTRL_2, 0);
    sensor.reboot();
    bool sawBootSet = false;
    for (const auto& w : Wire.getWrites()) {
        if (w.reg == REG_CTRL_2 && (w.value & CTRL2_BOOT)) {
            sawBootSet = true;
            break;
        }
    }
    TEST_ASSERT_TRUE(sawBootSet);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_begin_detects_device);
    RUN_TEST(test_begin_rejects_wrong_device_id);
    RUN_TEST(test_device_id_returns_correct_value);
    RUN_TEST(test_power_on_sets_ctrl1);
    RUN_TEST(test_power_off_clears_ctrl1);
    RUN_TEST(test_set_continuous_writes_expected_ctrl1);
    RUN_TEST(test_trigger_one_shot_sets_ctrl2_one_shot);
    RUN_TEST(test_read_returns_converted_values);
    RUN_TEST(test_set_temp_offset_shifts_reading);
    RUN_TEST(test_calibrate_temperature_applies_correction);
    RUN_TEST(test_reboot_writes_ctrl2_boot);
    return UNITY_END();
}