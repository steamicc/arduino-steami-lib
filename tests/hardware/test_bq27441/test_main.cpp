// SPDX-License-Identifier: GPL-3.0-or-later

/*
 * Hardware unit validation for the BQ27441 fuel gauge on real STeaMi
 * silicon.
 *
 * The plausibility windows mirror the MicroPython sister project
 * (tests/scenarios/bq27441.yaml) — they're wide enough to pass on a
 * board with any reasonable Li-Po state and tight enough to flag a
 * bus / register-decoding regression.
 */

// WARNING! don’t forget to connect a battery to the card before starting the test

#include <Arduino.h>
#include <BQ27441.h>
#include <Wire.h>
#include <unity.h>

#include "driver_checks.h"

TwoWire internalI2C(I2C_INT_SDA, I2C_INT_SCL);
BQ27441 sensor(internalI2C);

void setUp(void) {
    sensor.begin();
}

void tearDown(void) {}

void test_bq27441_begin() {
    check_begin(sensor);
}

void test_bq27441_device_id() {
    check_who_am_i(sensor, BQ27441_DEVICE_ID);
}

void test_bq27441_voltage_in_plausible_range() {
    check_read_plausible(sensor, &BQ27441::voltageMv, static_cast<uint16_t>(3000),
                         static_cast<uint16_t>(4300));
}

void test_bq27441_state_of_charge_in_valid_range() {
    check_read_plausible(sensor, &BQ27441::stateOfCharge, static_cast<uint8_t>(0),
                         static_cast<uint8_t>(100));
}

void test_bq27441_state_of_health_in_valid_range() {
    check_read_plausible(sensor, &BQ27441::stateOfHealth, static_cast<uint8_t>(0),
                         static_cast<uint8_t>(100));
}

void test_bq27441_capacity_remaining_positive() {
    check_read_plausible(sensor, &BQ27441::capacityRemaining, static_cast<uint16_t>(0),
                         static_cast<uint16_t>(2000));
}

void test_bq27441_capacity_full_positive() {
    check_read_plausible(sensor, &BQ27441::capacityFull, static_cast<uint16_t>(100),
                         static_cast<uint16_t>(2000));
}

void test_bq27441_battery_temperature_in_plausible_range() {
    // 0–50 °C covers typical indoor classroom/lab usage plus warm-board
    // self-heating margin, matching the env-sensor suites.
    float t = sensor.temperature();
    TEST_ASSERT_GREATER_OR_EQUAL_FLOAT(0.0f, t);
    TEST_ASSERT_LESS_OR_EQUAL_FLOAT(50.0f, t);
}

void setup() {
    delay(2000);
    internalI2C.begin();

    UNITY_BEGIN();
    RUN_TEST(test_bq27441_begin);
    RUN_TEST(test_bq27441_device_id);
    RUN_TEST(test_bq27441_voltage_in_plausible_range);
    RUN_TEST(test_bq27441_state_of_charge_in_valid_range);
    RUN_TEST(test_bq27441_state_of_health_in_valid_range);
    RUN_TEST(test_bq27441_capacity_remaining_positive);
    RUN_TEST(test_bq27441_capacity_full_positive);
    RUN_TEST(test_bq27441_battery_temperature_in_plausible_range);
    UNITY_END();
}

void loop() {}
