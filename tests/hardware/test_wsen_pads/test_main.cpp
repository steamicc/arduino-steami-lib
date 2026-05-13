// SPDX-License-Identifier: GPL-3.0-or-later

/*
 * Hardware unit validation for WSEN-PADS on real STeaMi silicon.
 *
 * The plausibility windows are intentionally broad:
 * - pressure: 300-1100 hPa covers typical altitude range
 * - temperature: -40°C to 85°C covers component operating range
 */

#include <Arduino.h>
#include <WSEN_PADS.h>
#include <Wire.h>
#include <unity.h>

#include "driver_checks.h"

TwoWire internalI2C(I2C_INT_SDA, I2C_INT_SCL);
WSEN_PADS sensor(internalI2C);

void setUp(void) {
    sensor.begin();
    sensor.setContinuous(ODR_1_HZ);  // Même approche que HTS221
}

void tearDown(void) {}

void test_wsen_pads_begin() {
    check_begin(sensor);
}

void test_wsen_pads_who_am_i() {
    check_who_am_i(sensor, WSEN_PADS_DEVICE_ID);
}

void test_wsen_pads_read_plausible_pressure() {
    check_read_plausible(sensor, &WSEN_PADS::pressureHpa, 300.0f, 1100.0f);
}

void test_wsen_pads_read_plausible_temperature() {
    check_read_plausible(sensor, &WSEN_PADS::temperature, -40.0f, 85.0f);
}

void test_wsen_pads_read_result() {
    WSEN_PADS::ReadResult result = sensor.read();
    TEST_ASSERT_GREATER_OR_EQUAL(300.0f, result.pressure);
    TEST_ASSERT_LESS_OR_EQUAL(1100.0f, result.pressure);
    TEST_ASSERT_GREATER_OR_EQUAL(-40.0f, result.temperature);
    TEST_ASSERT_LESS_OR_EQUAL(85.0f, result.temperature);
}

void setup() {
    delay(2000);
    internalI2C.begin();

    UNITY_BEGIN();
    RUN_TEST(test_wsen_pads_begin);
    RUN_TEST(test_wsen_pads_who_am_i);
    RUN_TEST(test_wsen_pads_read_plausible_pressure);
    RUN_TEST(test_wsen_pads_read_plausible_temperature);
    RUN_TEST(test_wsen_pads_read_result);
    UNITY_END();
}

void loop() {}