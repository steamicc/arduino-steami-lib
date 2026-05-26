// SPDX-License-Identifier: GPL-3.0-or-later

/*
 * Hardware unit validation for WSEN-HIDS on real STeaMi silicon.
 *
 * The plausibility windows are intentionally broad:
 * - temperature: 0°C to 50°C covers typical indoor classroom/lab usage
 *   plus warm-board self-heating margin.
 * - humidity: 10% to 90% RH avoids false negatives from dry heated rooms
 *   or humid breath-adjacent environments while still catching nonsense data.
 */

#include <Arduino.h>
#include <Wire.h>
#include <WsenHids.h>
#include <math.h>
#include <unity.h>

#include "driver_checks.h"

TwoWire internalI2C(I2C_INT_SDA, I2C_INT_SCL);
WsenHids sensor(internalI2C);

// Unity invokes setUp() before every RUN_TEST. Re-initialising here keeps
// each test independent — otherwise reads following a skipped/failing
// test_wsen_hids_begin would compute against uninitialised calibration.
//
// setContinuous(1 Hz) leaves the part in steady-state ODR sampling so
// reads return real values immediately — same recipe as the native
// plausibility tests in test_wsen_hids. The dedicated
// `test_wsen_hids_read_auto_bringup_without_set_continuous` test below
// covers the path where the caller skips this explicit setContinuous.
void setUp(void) {
    sensor.begin();
    sensor.setContinuous(WSEN_HIDS_ODR_1_HZ);
}

void tearDown(void) {}

void test_wsen_hids_begin() {
    check_begin(sensor);
}

void test_wsen_hids_who_am_i() {
    check_who_am_i(sensor, WSEN_HIDS_WHO_AM_I_VALUE);
}

void test_wsen_hids_read_plausible_temperature() {
    check_read_plausible(sensor, &WsenHids::temperature, 0.0f, 50.0f);
}

void test_wsen_hids_read_plausible_humidity() {
    check_read_plausible(sensor, &WsenHids::humidity, 10.0f, 90.0f);
}

// Validates the auto-bring-up path inside read() on real silicon: after
// begin() the part is in power-down, and the caller never explicitly
// configures an ODR. read() should switch the chip to continuous at
// 12.5 Hz, wait for the first sample, and return plausible values
// without timing out. This is the user-facing guarantee that motivated
// switching read()'s auto-trigger from CTRL2.ONE_SHOT (broken on this
// silicon) to setContinuous(12.5 Hz).
void test_wsen_hids_read_auto_bringup_without_set_continuous() {
    // Re-init the sensor to start from a clean begin() — setUp() has
    // already run setContinuous, which would short-circuit the
    // bring-up path we want to exercise here.
    sensor.powerOff();
    TEST_ASSERT_TRUE(sensor.begin());

    auto r = sensor.read();

    TEST_ASSERT_FALSE_MESSAGE(isnan(r.temperature),
                              "read() auto-bring-up should not time out (temperature)");
    TEST_ASSERT_FALSE_MESSAGE(isnan(r.humidity),
                              "read() auto-bring-up should not time out (humidity)");
    TEST_ASSERT_GREATER_OR_EQUAL_FLOAT(0.0f, r.temperature);
    TEST_ASSERT_LESS_OR_EQUAL_FLOAT(50.0f, r.temperature);
    TEST_ASSERT_GREATER_OR_EQUAL_FLOAT(10.0f, r.humidity);
    TEST_ASSERT_LESS_OR_EQUAL_FLOAT(90.0f, r.humidity);
}

void setup() {
    delay(2000);
    internalI2C.begin();

    UNITY_BEGIN();
    RUN_TEST(test_wsen_hids_begin);
    RUN_TEST(test_wsen_hids_who_am_i);
    RUN_TEST(test_wsen_hids_read_plausible_temperature);
    RUN_TEST(test_wsen_hids_read_plausible_humidity);
    RUN_TEST(test_wsen_hids_read_auto_bringup_without_set_continuous);
    UNITY_END();
}

void loop() {}
