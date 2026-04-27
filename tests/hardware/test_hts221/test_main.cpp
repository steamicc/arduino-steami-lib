// SPDX-License-Identifier: GPL-3.0-or-later

/*
 * Hardware unit validation for HTS221 on real STeaMi silicon.
 *
 * The plausibility windows are intentionally broad:
 * - temperature: 0°C to 50°C covers typical indoor classroom/lab usage
 *   plus warm-board self-heating margin.
 * - humidity: 10% to 90% RH avoids false negatives from dry heated rooms
 *   or humid breath-adjacent environments while still catching nonsense data.
 */

#include <Arduino.h>
#include <HTS221.h>
#include <Wire.h>
#include <unity.h>

#include "driver_checks.h"

TwoWire internalI2C(I2C_INT_SDA, I2C_INT_SCL);
HTS221 sensor(internalI2C);

void test_hts221_begin() {
    check_begin(sensor);
}

void test_hts221_who_am_i() {
    check_who_am_i(sensor, HTS221_WHO_AM_I_VALUE);
}

void test_hts221_read_plausible_temperature() {
    check_read_plausible(sensor, &HTS221::temperature, 0.0f, 50.0f);
}

void test_hts221_read_plausible_humidity() {
    check_read_plausible(sensor, &HTS221::humidity, 10.0f, 90.0f);
}

void setup() {
    delay(2000);
    internalI2C.begin();

    UNITY_BEGIN();
    RUN_TEST(test_hts221_begin);
    RUN_TEST(test_hts221_who_am_i);
    RUN_TEST(test_hts221_read_plausible_temperature);
    RUN_TEST(test_hts221_read_plausible_humidity);
    UNITY_END();
}

void loop() {}
