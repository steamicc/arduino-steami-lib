// SPDX-License-Identifier: GPL-3.0-or-later

#include <Arduino.h>
#include <Wire.h>
#include <math.h>
#include <unity.h>

#include "WSEN_PADS.h"
#include "WSEN_PADS_const.h"
#include "driver_checks.h"

namespace {

constexpr uint32_t DATA_READY_TIMEOUT_MS = 2000;

// Physical operating ranges of the WSEN-PADS.
constexpr float MIN_PRESSURE_HPA = 260.0f;
constexpr float MAX_PRESSURE_HPA = 1260.0f;
constexpr float MIN_TEMPERATURE_C = -40.0f;
constexpr float MAX_TEMPERATURE_C = 85.0f;

TwoWire internalI2C(I2C_INT_SDA, I2C_INT_SCL);
WSEN_PADS sensor(internalI2C);

bool waitForDataReady(uint32_t timeoutMs = DATA_READY_TIMEOUT_MS) {
    const uint32_t start = millis();

    while ((millis() - start) < timeoutMs) {
        if (sensor.dataReady()) {
            return true;
        }
        delay(10);
    }

    return false;
}

void assertPlausibleReading(const WSEN_PADS::ReadResult& reading) {
    TEST_ASSERT_FALSE_MESSAGE(isnan(reading.pressure), "Pressure reading is NaN");
    TEST_ASSERT_FALSE_MESSAGE(isnan(reading.temperature), "Temperature reading is NaN");

    TEST_ASSERT_GREATER_OR_EQUAL_FLOAT_MESSAGE(MIN_PRESSURE_HPA, reading.pressure,
                                               "Pressure is below the sensor operating range");
    TEST_ASSERT_LESS_OR_EQUAL_FLOAT_MESSAGE(MAX_PRESSURE_HPA, reading.pressure,
                                            "Pressure is above the sensor operating range");

    TEST_ASSERT_GREATER_OR_EQUAL_FLOAT_MESSAGE(MIN_TEMPERATURE_C, reading.temperature,
                                               "Temperature is below the sensor operating range");
    TEST_ASSERT_LESS_OR_EQUAL_FLOAT_MESSAGE(MAX_TEMPERATURE_C, reading.temperature,
                                            "Temperature is above the sensor operating range");
}

}  // namespace

void setUp(void) {
    sensor.begin();
}

void tearDown(void) {
    sensor.powerOff();
}

void test_wsen_pads_begin() {
    check_begin(sensor);
}

void test_wsen_pads_device_id() {
    check_who_am_i(sensor, WSEN_PADS_DEVICE_ID);
}

void test_wsen_pads_read_plausible_measurements() {
    sensor.powerOn();

    TEST_ASSERT_TRUE_MESSAGE(waitForDataReady(), "Timed out waiting for WSEN-PADS data");

    const WSEN_PADS::ReadResult reading = sensor.read();
    assertPlausibleReading(reading);
}

void test_wsen_pads_power_cycle() {
    sensor.powerOn();

    TEST_ASSERT_TRUE_MESSAGE(waitForDataReady(), "No measurement available before power-off");
    assertPlausibleReading(sensor.read());

    sensor.powerOff();
    delay(100);

    sensor.powerOn();

    TEST_ASSERT_TRUE_MESSAGE(waitForDataReady(), "No measurement available after power-on");
    assertPlausibleReading(sensor.read());
}

void setup() {
    delay(2000);
    internalI2C.begin();

    UNITY_BEGIN();
    RUN_TEST(test_wsen_pads_begin);
    RUN_TEST(test_wsen_pads_device_id);
    RUN_TEST(test_wsen_pads_read_plausible_measurements);
    RUN_TEST(test_wsen_pads_power_cycle);
    UNITY_END();
}

void loop() {}
