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
#include <unity.h>

#include "driver_checks.h"

TwoWire internalI2C(I2C_INT_SDA, I2C_INT_SCL);
WsenHids sensor(internalI2C);

// Unity invokes setUp() before every RUN_TEST. Re-initialising here keeps
// each test independent — otherwise reads following a skipped/failing
// test_wsen_hids_begin would compute against uninitialised calibration.
//
// We don't stop at begin(): on real silicon, calling humidity() right
// after begin() routes through the driver's one-shot auto-trigger path.
// At default oversampling, the very first one-shot occasionally produces
// a slightly negative raw humidity (clamped to 0 by computeHumidity), so
// the plausibility window flags it. setContinuous(1 Hz) leaves the part
// in steady-state ODR sampling so reads return real values immediately —
// same recipe as the native plausibility tests in test_wsen_hids.
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

void setup() {
    delay(2000);
    internalI2C.begin();

    UNITY_BEGIN();
    RUN_TEST(test_wsen_hids_begin);
    RUN_TEST(test_wsen_hids_who_am_i);
    RUN_TEST(test_wsen_hids_read_plausible_temperature);
    RUN_TEST(test_wsen_hids_read_plausible_humidity);
    UNITY_END();
}

void loop() {}
