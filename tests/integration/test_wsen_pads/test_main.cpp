// SPDX-License-Identifier: GPL-3.0-or-later

#include <Arduino.h>
#include <Wire.h>
#include <unity.h>

#include "WSEN_PADS.h"
#include "WSEN_PADS_const.h"

namespace {

constexpr uint32_t DATA_READY_TIMEOUT_MS = 2000;
constexpr uint8_t SAMPLE_COUNT = 10;

// WSEN-PADS operating ranges from the component specification.
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

void assertPlausible(const WSEN_PADS::ReadResult& reading) {
    TEST_ASSERT_FALSE_MESSAGE(isnan(reading.pressure), "Pressure reading is NaN");
    TEST_ASSERT_FALSE_MESSAGE(isnan(reading.temperature), "Temperature reading is NaN");

    TEST_ASSERT_FLOAT_WITHIN_MESSAGE((MAX_PRESSURE_HPA - MIN_PRESSURE_HPA) / 2.0f,
                                     (MAX_PRESSURE_HPA + MIN_PRESSURE_HPA) / 2.0f, reading.pressure,
                                     "Pressure is outside the WSEN-PADS operating range");

    TEST_ASSERT_FLOAT_WITHIN_MESSAGE((MAX_TEMPERATURE_C - MIN_TEMPERATURE_C) / 2.0f,
                                     (MAX_TEMPERATURE_C + MIN_TEMPERATURE_C) / 2.0f,
                                     reading.temperature,
                                     "Temperature is outside the WSEN-PADS operating range");
}

}  // namespace

void setUp(void) {
    sensor.powerOff();
    delay(20);
}

void tearDown(void) {
    sensor.powerOff();
}

void test_device_detection_and_id(void) {
    TEST_ASSERT_TRUE_MESSAGE(sensor.begin(), "WSEN-PADS was not detected on the internal I2C bus");
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(WSEN_PADS_DEVICE_ID, sensor.deviceId(),
                                   "Unexpected WSEN-PADS device ID");
}

void test_repeated_measurements_are_plausible(void) {
    TEST_ASSERT_TRUE_MESSAGE(sensor.begin(), "WSEN-PADS initialization failed");

    sensor.setContinuous(ODR_1_HZ);

    for (uint8_t sample = 0; sample < SAMPLE_COUNT; ++sample) {
        TEST_ASSERT_TRUE_MESSAGE(waitForDataReady(), "Timed out waiting for a WSEN-PADS sample");

        const WSEN_PADS::ReadResult reading = sensor.read();
        assertPlausible(reading);

        // At 1 Hz, wait before polling for the next acquisition.
        delay(1000);
    }
}

void test_power_off_on_cycle_restores_measurements(void) {
    TEST_ASSERT_TRUE_MESSAGE(sensor.begin(), "WSEN-PADS initialization failed");

    sensor.powerOn();
    TEST_ASSERT_TRUE_MESSAGE(waitForDataReady(), "No sample received before power-off");
    assertPlausible(sensor.read());

    sensor.powerOff();
    delay(100);

    sensor.powerOn();
    TEST_ASSERT_TRUE_MESSAGE(waitForDataReady(), "No sample received after power-on");
    assertPlausible(sensor.read());
}

void setup() {
    delay(2000);
    internalI2C.begin();

    UNITY_BEGIN();
    RUN_TEST(test_device_detection_and_id);
    RUN_TEST(test_repeated_measurements_are_plausible);
    RUN_TEST(test_power_off_on_cycle_restores_measurements);
    UNITY_END();
}

void loop() {}
