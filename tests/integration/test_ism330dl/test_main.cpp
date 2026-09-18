// SPDX-License-Identifier: GPL-3.0-or-later

#include <Arduino.h>
#include <Wire.h>
#include <math.h>
#include <unity.h>

#include "ISM330DL.h"
#include "ISM330DL_const.h"

namespace {

constexpr uint32_t DATA_READY_TIMEOUT_MS = 1000;
constexpr uint8_t SAMPLE_COUNT = 20;
constexpr uint32_t SAMPLE_INTERVAL_MS = 20;

// begin() configures the accelerometer to +/-2 g and the gyroscope to
// +/-250 dps. A small margin is allowed for conversion and sensor tolerances.
constexpr float MAX_ACCEL_G = 2.1F;
constexpr float MAX_GYRO_DPS = 260.0F;

// Broad physical range suitable for an on-board plausibility check.
constexpr float MIN_TEMPERATURE_C = -40.0F;
constexpr float MAX_TEMPERATURE_C = 85.0F;

constexpr float ACCEL_CHANGE_EPSILON_G = 0.0001F;
constexpr float GYRO_CHANGE_EPSILON_DPS = 0.001F;

TwoWire internalI2C(I2C_INT_SDA, I2C_INT_SCL);
ISM330DL sensor(internalI2C);

bool waitForDataReady(uint32_t timeoutMs = DATA_READY_TIMEOUT_MS) {
    const uint32_t start = millis();

    while ((millis() - start) < timeoutMs) {
        if (sensor.dataReady()) {
            return true;
        }
        delay(1);
    }

    return false;
}

void assertFiniteVector(const ISM330DL::Vector3& value, const char* message) {
    TEST_ASSERT_TRUE_MESSAGE(isfinite(value.x), message);
    TEST_ASSERT_TRUE_MESSAGE(isfinite(value.y), message);
    TEST_ASSERT_TRUE_MESSAGE(isfinite(value.z), message);
}

void assertPlausibleAcceleration(const ISM330DL::Vector3& acceleration) {
    assertFiniteVector(acceleration, "Acceleration contains a non-finite value");

    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(MAX_ACCEL_G, 0.0F, acceleration.x,
                                     "Acceleration X is outside the configured range");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(MAX_ACCEL_G, 0.0F, acceleration.y,
                                     "Acceleration Y is outside the configured range");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(MAX_ACCEL_G, 0.0F, acceleration.z,
                                     "Acceleration Z is outside the configured range");
}

void assertPlausibleGyroscope(const ISM330DL::Vector3& gyroscope) {
    assertFiniteVector(gyroscope, "Gyroscope contains a non-finite value");

    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(MAX_GYRO_DPS, 0.0F, gyroscope.x,
                                     "Gyroscope X is outside the configured range");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(MAX_GYRO_DPS, 0.0F, gyroscope.y,
                                     "Gyroscope Y is outside the configured range");
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(MAX_GYRO_DPS, 0.0F, gyroscope.z,
                                     "Gyroscope Z is outside the configured range");
}

void assertPlausibleTemperature(float temperature) {
    TEST_ASSERT_TRUE_MESSAGE(isfinite(temperature), "Temperature is not finite");

    TEST_ASSERT_GREATER_OR_EQUAL_FLOAT_MESSAGE(MIN_TEMPERATURE_C, temperature,
                                               "Temperature is below the plausible range");
    TEST_ASSERT_LESS_OR_EQUAL_FLOAT_MESSAGE(MAX_TEMPERATURE_C, temperature,
                                            "Temperature is above the plausible range");
}

bool vectorChanged(const ISM330DL::Vector3& previous, const ISM330DL::Vector3& current,
                   float epsilon) {
    return fabsf(current.x - previous.x) > epsilon || fabsf(current.y - previous.y) > epsilon ||
           fabsf(current.z - previous.z) > epsilon;
}

void readAndValidateSample(ISM330DL::Vector3& acceleration, ISM330DL::Vector3& gyroscope,
                           float& temperature) {
    TEST_ASSERT_TRUE_MESSAGE(waitForDataReady(), "Timed out waiting for ISM330DL data");

    TEST_ASSERT_TRUE_MESSAGE(sensor.accelerationG(acceleration), "Failed to read acceleration");
    TEST_ASSERT_TRUE_MESSAGE(sensor.gyroscopeDps(gyroscope), "Failed to read gyroscope");
    TEST_ASSERT_TRUE_MESSAGE(sensor.temperature(temperature), "Failed to read temperature");

    assertPlausibleAcceleration(acceleration);
    assertPlausibleGyroscope(gyroscope);
    assertPlausibleTemperature(temperature);
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
    TEST_ASSERT_TRUE_MESSAGE(sensor.begin(), "ISM330DL was not detected on the internal I2C bus");
    TEST_ASSERT_TRUE_MESSAGE(sensor.isConnected(), "ISM330DL does not respond on I2C");
    TEST_ASSERT_EQUAL_HEX8_MESSAGE(ISM330DL_WHO_AM_I_VALUE, sensor.deviceId(),
                                   "Unexpected ISM330DL device ID");
}

void test_repeated_measurements_are_plausible_and_not_frozen(void) {
    TEST_ASSERT_TRUE_MESSAGE(sensor.begin(), "ISM330DL initialization failed");

    ISM330DL::Vector3 previousAcceleration{};
    ISM330DL::Vector3 previousGyroscope{};
    float temperature = 0.0F;

    readAndValidateSample(previousAcceleration, previousGyroscope, temperature);

    bool accelerationChanged = false;
    bool gyroscopeChanged = false;

    for (uint8_t sample = 1; sample < SAMPLE_COUNT; ++sample) {
        delay(SAMPLE_INTERVAL_MS);

        ISM330DL::Vector3 acceleration{};
        ISM330DL::Vector3 gyroscope{};

        readAndValidateSample(acceleration, gyroscope, temperature);

        accelerationChanged |=
            vectorChanged(previousAcceleration, acceleration, ACCEL_CHANGE_EPSILON_G);
        gyroscopeChanged |= vectorChanged(previousGyroscope, gyroscope, GYRO_CHANGE_EPSILON_DPS);

        previousAcceleration = acceleration;
        previousGyroscope = gyroscope;
    }

    TEST_ASSERT_TRUE_MESSAGE(
        accelerationChanged || gyroscopeChanged,
        "All inertial samples were identical; measurements may be frozen or stale");
}

void test_power_off_on_cycle_restores_measurements(void) {
    TEST_ASSERT_TRUE_MESSAGE(sensor.begin(), "ISM330DL initialization failed");

    ISM330DL::Vector3 acceleration{};
    ISM330DL::Vector3 gyroscope{};
    float temperature = 0.0F;

    readAndValidateSample(acceleration, gyroscope, temperature);

    TEST_ASSERT_TRUE_MESSAGE(sensor.powerOff(), "Failed to power off ISM330DL");
    delay(100);

    TEST_ASSERT_TRUE_MESSAGE(sensor.powerOn(), "Failed to power on ISM330DL");
    readAndValidateSample(acceleration, gyroscope, temperature);
}

void setup() {
    delay(2000);
    internalI2C.begin();

    UNITY_BEGIN();
    RUN_TEST(test_device_detection_and_id);
    RUN_TEST(test_repeated_measurements_are_plausible_and_not_frozen);
    RUN_TEST(test_power_off_on_cycle_restores_measurements);
    UNITY_END();
}

void loop() {}
