// SPDX-License-Identifier: GPL-3.0-or-later

/*
 * Hardware unit validation for the ISM330DL on a real STeaMi board.
 *
 * The plausibility windows are intentionally broad:
 * - acceleration: each axis between -2.2 g and +2.2 g at the default ±2 g scale,
 * - gyroscope: each axis between -250 dps and +250 dps at the default ±250 dps scale,
 * - temperature: 0 °C to 60 °C for normal classroom/lab conditions,
 * - acceleration magnitude: approximately 1 g while the board is static.
 *
 * These tests validate atomic driver contracts on real hardware. Longer,
 * repeated-acquisition behaviour belongs in an integration suite.
 */

#include <Arduino.h>
#include <ISM330DL.h>
#include <Wire.h>
#include <math.h>
#include <unity.h>

#include "driver_checks.h"

TwoWire internalI2C(I2C_INT_SDA, I2C_INT_SCL);
ISM330DL sensor(internalI2C);

void setUp(void) {
    TEST_ASSERT_TRUE_MESSAGE(sensor.begin(), "ISM330DL begin() failed");
}

void tearDown(void) {}

void test_ism330dl_begin(void) {
    check_begin(sensor);
}

void test_ism330dl_who_am_i(void) {
    check_who_am_i(sensor, ISM330DL_WHO_AM_I_VALUE);
}

void test_ism330dl_acceleration_is_plausible(void) {
    ISM330DL::Vector3 accel{};
    TEST_ASSERT_TRUE_MESSAGE(sensor.accelerationG(accel), "Acceleration read failed");

    TEST_ASSERT_FLOAT_WITHIN(2.2F, 0.0F, accel.x);
    TEST_ASSERT_FLOAT_WITHIN(2.2F, 0.0F, accel.y);
    TEST_ASSERT_FLOAT_WITHIN(2.2F, 0.0F, accel.z);

    const float magnitude = sqrtf(accel.x * accel.x + accel.y * accel.y + accel.z * accel.z);
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.35F, 1.0F, magnitude,
                                     "Acceleration magnitude should be close to 1 g");
}

void test_ism330dl_gyroscope_is_plausible(void) {
    ISM330DL::Vector3 gyro{};
    TEST_ASSERT_TRUE_MESSAGE(sensor.gyroscopeDps(gyro), "Gyroscope read failed");

    TEST_ASSERT_FLOAT_WITHIN(250.0F, 0.0F, gyro.x);
    TEST_ASSERT_FLOAT_WITHIN(250.0F, 0.0F, gyro.y);
    TEST_ASSERT_FLOAT_WITHIN(250.0F, 0.0F, gyro.z);
}

void test_ism330dl_temperature_is_plausible(void) {
    float temperature = 0.0F;
    TEST_ASSERT_TRUE_MESSAGE(sensor.temperature(temperature), "Temperature read failed");
    TEST_ASSERT_TRUE_MESSAGE(temperature >= 0.0F && temperature <= 60.0F,
                             "Temperature outside plausible hardware range");
}

void test_ism330dl_status_register_is_readable(void) {
    const uint8_t currentStatus = sensor.status();

    TEST_ASSERT_BITS_HIGH_MESSAGE(ISM330DL_STATUS_XLDA | ISM330DL_STATUS_GDA, currentStatus,
                                  "Accelerometer and gyroscope data-ready bits should be set");
}

void test_ism330dl_orientation_returns_valid_value(void) {
    ISM330DL::Orientation orientation = ISM330DL::Orientation::MOVING;
    TEST_ASSERT_TRUE_MESSAGE(sensor.orientation(orientation), "Orientation read failed");

    const uint8_t raw = static_cast<uint8_t>(orientation);
    TEST_ASSERT_TRUE_MESSAGE(raw <= static_cast<uint8_t>(ISM330DL::Orientation::MOVING),
                             "Orientation enum value is invalid");

    TEST_ASSERT_NOT_NULL(ISM330DL::orientationToString(orientation));
}

void test_ism330dl_motion_returns_valid_value(void) {
    ISM330DL::Motion motion{};
    TEST_ASSERT_TRUE_MESSAGE(sensor.motion(motion), "Motion read failed");

    const uint8_t raw = static_cast<uint8_t>(motion.type);
    TEST_ASSERT_TRUE_MESSAGE(raw <= static_cast<uint8_t>(ISM330DL::MotionType::STABLE),
                             "Motion enum value is invalid");

    TEST_ASSERT_NOT_NULL(ISM330DL::motionToString(motion.type));
    TEST_ASSERT_TRUE_MESSAGE(motion.value >= 0.0F, "Motion magnitude must be non-negative");
}

void test_ism330dl_power_cycle_restores_measurements(void) {
    TEST_ASSERT_TRUE_MESSAGE(sensor.powerOff(), "powerOff() failed");

    delay(20);

    /*
     * STATUS data-ready bits may remain latched after entering power-down.
     * They do not indicate whether CTRL1_XL and CTRL2_G are powered down.
     *
     * A measurement request must automatically restore the previous
     * accelerometer and gyroscope configuration through ensureData().
     */
    ISM330DL::Vector3 accel{};
    ISM330DL::Vector3 gyro{};

    TEST_ASSERT_TRUE_MESSAGE(sensor.accelerationG(accel),
                             "Acceleration read failed after powerOff()");

    TEST_ASSERT_TRUE_MESSAGE(sensor.gyroscopeDps(gyro),
                             "Gyroscope read failed after automatic power restoration");

    const float magnitude = sqrtf(accel.x * accel.x + accel.y * accel.y + accel.z * accel.z);

    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(
        0.35F, 1.0F, magnitude,
        "Acceleration magnitude should remain close to 1 g after power cycle");
}

void setup() {
    delay(2000);
    internalI2C.begin();

    UNITY_BEGIN();
    RUN_TEST(test_ism330dl_begin);
    RUN_TEST(test_ism330dl_who_am_i);
    RUN_TEST(test_ism330dl_acceleration_is_plausible);
    RUN_TEST(test_ism330dl_gyroscope_is_plausible);
    RUN_TEST(test_ism330dl_temperature_is_plausible);
    RUN_TEST(test_ism330dl_status_register_is_readable);
    RUN_TEST(test_ism330dl_orientation_returns_valid_value);
    RUN_TEST(test_ism330dl_motion_returns_valid_value);
    RUN_TEST(test_ism330dl_power_cycle_restores_measurements);
    UNITY_END();
}

void loop() {}
