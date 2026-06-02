// SPDX-License-Identifier: GPL-3.0-or-later

#include <Arduino.h>
#include <VL53L1X.h>
#include <Wire.h>
#include <unity.h>

#include "driver_checks.h"

TwoWire internalI2C(I2C_INT_SDA, I2C_INT_SCL);
VL53L1X sensor(internalI2C);

void setUp(void) {
    sensor.begin();
}

void tearDown(void) {}

void test_vl53l1x_begin() {
    check_begin(sensor);
}

void test_vl53l1x_device_id(void) {
    check_who_am_i(sensor, VL53L1X_DEVICE_ID);
}

void test_vl53l1x_read_plausible_distance(void) {
    check_read_plausible(sensor, &VL53L1X::distanceMm, (uint16_t)1, (uint16_t)4000);
}

void setup() {
    delay(2000);
    internalI2C.begin();

    UNITY_BEGIN();
    RUN_TEST(test_vl53l1x_begin);
    RUN_TEST(test_vl53l1x_device_id);
    RUN_TEST(test_vl53l1x_read_plausible_distance);
    UNITY_END();
}

void loop() {}