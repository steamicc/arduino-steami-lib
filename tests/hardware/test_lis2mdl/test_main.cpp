// SPDX-License-Identifier: GPL-3.0-or-later

#include <Arduino.h>
#include <LIS2MDL.h>
#include <Wire.h>
#include <unity.h>

#include "driver_checks.h"

TwoWire internalI2C(I2C_INT_SDA, I2C_INT_SCL);
LIS2MDL sensor(internalI2C);

void setUp(void) {
    sensor.begin();
    sensor.setContinuous(10);
}

void tearDown(void) {}

void test_lis2mdl_begin() {
    check_begin(sensor);
}

void test_lis2mdl_who_am_i() {
    check_who_am_i(sensor, LIS2MDL_WHO_AM_I_VAL);
}

void test_lis2mdl_read_plausible_magnetic_field(void) {
    check_read_plausible(sensor, &LIS2MDL::magnitudeUt, 5.0f, 300.0f);
}

void test_lis2mdl_read_plausible_temperature(void) {
    check_read_plausible(sensor, &LIS2MDL::temperature, -40.0f, 95.0f);
}

void test_lis2mdl_magnitude_ut_plausible(void) {
    float mag1 = sensor.magnitudeUt();
    delay(50);
    float mag2 = sensor.magnitudeUt();
    delay(50);
    float mag3 = sensor.magnitudeUt();
    delay(50);

    float avgMag = (mag1 + mag2 + mag3) / 3.0f;
    float tolerance = avgMag * 0.1f;
    TEST_ASSERT_TRUE_MESSAGE(avgMag > 5.0f, "magnitude trop faible");
    TEST_ASSERT_TRUE_MESSAGE(fabs(mag1 - avgMag) < tolerance, "lecture 1 instable");
    TEST_ASSERT_TRUE_MESSAGE(fabs(mag2 - avgMag) < tolerance, "lecture 2 instable");
    TEST_ASSERT_TRUE_MESSAGE(fabs(mag3 - avgMag) < tolerance, "lecture 3 instable");
}

void test_lis2mdl_heading_flat_only_in_range(void) {
    check_read_plausible(sensor, &LIS2MDL::headingFlatOnly, 0.0f, 360.0f);
}

void setup() {
    delay(2000);
    internalI2C.begin();

    UNITY_BEGIN();
    RUN_TEST(test_lis2mdl_begin);
    RUN_TEST(test_lis2mdl_who_am_i);
    RUN_TEST(test_lis2mdl_read_plausible_magnetic_field);
    RUN_TEST(test_lis2mdl_read_plausible_temperature);
    RUN_TEST(test_lis2mdl_magnitude_ut_plausible);
    RUN_TEST(test_lis2mdl_heading_flat_only_in_range);
    UNITY_END();
}

void loop() {}