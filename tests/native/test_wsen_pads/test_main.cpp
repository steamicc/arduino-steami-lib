// SPDX-License-Identifier: GPL-3.0-or-later

#include <unity.h>

#include "SteamiWsenPads.h"
#include "SteamiWsenPads_const.h"
#include "Wire.h"
#include "driver_checks.h"
#include "steami_fake_chips.h"

constexpr uint8_t ADDR = WSEN_PADS_I2C_DEFAULT_ADDR;

static void preloadDeviceId(bool valid = true) {
    steamiFakeWsenPadsId(Wire, valid);
}

// Force the device to look "powered on" so reads don't auto-trigger a
// one-shot. Tests that exercise the auto-trigger path explicitly clear
// CTRL_1 first.
static void preloadPoweredOn() {
    Wire.setRegister(ADDR, REG_CTRL_1, CTRL1_BDU | (ODR_1_HZ << CTRL1_ODR_SHIFT));
}

static void preloadMeasurement(float tempC, float pressureHpa) {
    steamiFakeWsenPadsMeasurement(Wire, tempC, pressureHpa);
}

SteamiWsenPads sensor;

void setUp() {
    Wire = TwoWire();
    preloadDeviceId();
    sensor = SteamiWsenPads();
}

void tearDown(void) {}

void test_begin_detects_device(void) {
    check_begin(sensor);
}

void test_begin_rejects_wrong_device_id(void) {
    preloadDeviceId(false);
    TEST_ASSERT_FALSE(sensor.begin());
}

void test_device_id_returns_correct_value(void) {
    check_who_am_i(sensor, WSEN_PADS_DEVICE_ID);
}

void test_power_on_sets_ctrl1(void) {
    sensor.begin();
    sensor.powerOn();
    uint8_t ctrl1 = Wire.getRegister(ADDR, REG_CTRL_1);
    TEST_ASSERT_BITS_HIGH(CTRL1_BDU, ctrl1);
    TEST_ASSERT_NOT_EQUAL(0, ctrl1 & CTRL1_ODR_MASK);
}

void test_power_off_clears_ctrl1_odr(void) {
    sensor.begin();
    sensor.powerOn();
    sensor.powerOff();
    uint8_t ctrl1 = Wire.getRegister(ADDR, REG_CTRL_1);
    TEST_ASSERT_EQUAL(0, ctrl1 & CTRL1_ODR_MASK);
}

void test_set_continuous_writes_expected_ctrl1(void) {
    sensor.begin();
    sensor.setContinuous(ODR_1_HZ);
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

void test_trigger_one_shot_powers_down_first(void) {
    // ONE_SHOT only fires from ODR=000. Verify the trigger forces the
    // power-down before flipping the bit.
    sensor.begin();
    sensor.setContinuous(ODR_25_HZ);
    sensor.triggerOneShot();
    uint8_t ctrl1 = Wire.getRegister(ADDR, REG_CTRL_1);
    TEST_ASSERT_EQUAL(0, ctrl1 & CTRL1_ODR_MASK);
}

void test_data_ready_reflects_status_register(void) {
    sensor.begin();
    Wire.setRegister(ADDR, REG_STATUS, 0);
    TEST_ASSERT_FALSE(sensor.dataReady());
    TEST_ASSERT_FALSE(sensor.pressureReady());
    TEST_ASSERT_FALSE(sensor.temperatureReady());

    Wire.setRegister(ADDR, REG_STATUS, STATUS_P_DA);
    TEST_ASSERT_FALSE(sensor.dataReady());
    TEST_ASSERT_TRUE(sensor.pressureReady());
    TEST_ASSERT_FALSE(sensor.temperatureReady());

    Wire.setRegister(ADDR, REG_STATUS, STATUS_P_DA | STATUS_T_DA);
    TEST_ASSERT_TRUE(sensor.dataReady());
    TEST_ASSERT_TRUE(sensor.pressureReady());
    TEST_ASSERT_TRUE(sensor.temperatureReady());
}

void test_read_returns_converted_values(void) {
    sensor.begin();
    sensor.setContinuous(ODR_1_HZ);
    preloadMeasurement(23.0f, 1013.0f);

    auto r = sensor.read();
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 23.0f, r.temperature);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 1013.0f, r.pressure);
}

void test_pressure_hpa_returns_nan_on_powered_down_timeout(void) {
    // begin() leaves the part in power-down. With STATUS reporting no
    // data ready, the auto-trigger path times out and pressureHpa()
    // must surface NaN — not silently return 0 hPa.
    sensor.begin();
    Wire.setRegister(ADDR, REG_STATUS, 0);
    TEST_ASSERT_TRUE(isnan(sensor.pressureHpa()));
}

void test_temperature_returns_nan_on_powered_down_timeout(void) {
    sensor.begin();
    Wire.setRegister(ADDR, REG_STATUS, 0);
    TEST_ASSERT_TRUE(isnan(sensor.temperature()));
}

void test_set_temperature_offset_shifts_reading(void) {
    sensor.begin();
    sensor.setContinuous(ODR_1_HZ);
    preloadMeasurement(23.0f, 1013.0f);
    sensor.setTemperatureOffset(1.5f);

    TEST_ASSERT_FLOAT_WITHIN(0.01f, 24.5f, sensor.temperature());
}

void test_calibrate_temperature_applies_correction(void) {
    sensor.begin();
    sensor.setContinuous(ODR_1_HZ);
    sensor.calibrateTemperature(1.0f, 0.0f, 22.0f, 20.0f);
    preloadMeasurement(20.0f, 1013.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 22.0f, sensor.temperature());
}

void test_calibrate_temperature_returns_true(void) {
    sensor.begin();
    TEST_ASSERT_TRUE(sensor.calibrateTemperature(1.0f, 0.0f, 22.0f, 20.0f));
}

void test_calibrate_temperature_with_equal_points_keeps_calibration(void) {
    // Same contract as ISM330DL, LIS2MDL and MicroPython (ValueError there).
    sensor.begin();
    sensor.setContinuous(ODR_1_HZ);
    sensor.setTemperatureOffset(1.5f);
    TEST_ASSERT_FALSE(sensor.calibrateTemperature(0.0f, 5.0f, 100.0f, 5.0f));
    preloadMeasurement(23.0f, 1013.0f);

    TEST_ASSERT_FLOAT_WITHIN(0.01f, 24.5f, sensor.temperature());
}

void test_set_temperature_offset_replaces_two_point_calibration(void) {
    // One calibration at a time, as in MicroPython: the offset resets the
    // gain to 1 and calibrateTemperature() replaces the offset.
    sensor.begin();
    sensor.setContinuous(ODR_1_HZ);
    sensor.calibrateTemperature(1.0f, 0.0f, 22.0f, 20.0f);  // gain 1.05, offset 1.0
    sensor.setTemperatureOffset(0.5f);
    preloadMeasurement(20.0f, 1013.0f);

    // 1.0 * 20 + 0.5 = 20.5 (it was 22.5 when the offset stacked)
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 20.5f, sensor.temperature());
}

void test_calibrate_temperature_replaces_offset(void) {
    sensor.begin();
    sensor.setContinuous(ODR_1_HZ);
    sensor.setTemperatureOffset(0.5f);
    sensor.calibrateTemperature(1.0f, 0.0f, 22.0f, 20.0f);
    preloadMeasurement(20.0f, 1013.0f);

    TEST_ASSERT_FLOAT_WITHIN(0.01f, 22.0f, sensor.temperature());
}

void test_read_applies_the_same_calibration(void) {
    sensor.begin();
    sensor.setContinuous(ODR_1_HZ);
    sensor.calibrateTemperature(1.0f, 0.0f, 22.0f, 20.0f);
    sensor.setTemperatureOffset(0.5f);
    preloadMeasurement(20.0f, 1013.0f);

    TEST_ASSERT_FLOAT_WITHIN(0.01f, 20.5f, sensor.read().temperature);
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
    RUN_TEST(test_power_off_clears_ctrl1_odr);
    RUN_TEST(test_set_continuous_writes_expected_ctrl1);
    RUN_TEST(test_trigger_one_shot_sets_ctrl2_one_shot);
    RUN_TEST(test_trigger_one_shot_powers_down_first);
    RUN_TEST(test_data_ready_reflects_status_register);
    RUN_TEST(test_read_returns_converted_values);
    RUN_TEST(test_pressure_hpa_returns_nan_on_powered_down_timeout);
    RUN_TEST(test_temperature_returns_nan_on_powered_down_timeout);
    RUN_TEST(test_set_temperature_offset_shifts_reading);
    RUN_TEST(test_calibrate_temperature_applies_correction);
    RUN_TEST(test_calibrate_temperature_returns_true);
    RUN_TEST(test_calibrate_temperature_with_equal_points_keeps_calibration);
    RUN_TEST(test_set_temperature_offset_replaces_two_point_calibration);
    RUN_TEST(test_calibrate_temperature_replaces_offset);
    RUN_TEST(test_read_applies_the_same_calibration);
    RUN_TEST(test_reboot_writes_ctrl2_boot);
    return UNITY_END();
}
