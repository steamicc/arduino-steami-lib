// SPDX-License-Identifier: GPL-3.0-or-later

/*
 * Integration validation for HTS221 on real STeaMi silicon.
 *
 * Verifies:
 * - successful initialization,
 * - continuous 1 Hz acquisition cadence,
 * - plausible environmental values across time,
 * - readings are not frozen across the full capture window.
 */

#include <Arduino.h>
#include <HTS221.h>
#include <Wire.h>
#include <math.h>
#include <unity.h>

TwoWire internalI2C(I2C_INT_SDA, I2C_INT_SCL);
HTS221 sensor(internalI2C);

static constexpr int SAMPLE_COUNT = 10;
static constexpr float MIN_TEMP = 0.0f;
static constexpr float MAX_TEMP = 50.0f;
static constexpr float MIN_HUM = 10.0f;
static constexpr float MAX_HUM = 90.0f;
static constexpr unsigned long MIN_AVG_INTERVAL_MS = 800;
static constexpr unsigned long MAX_AVG_INTERVAL_MS = 1300;
static constexpr float CHANGE_EPSILON = 0.001f;

float temperatures[SAMPLE_COUNT];
float humidities[SAMPLE_COUNT];
unsigned long timestamps[SAMPLE_COUNT];

void wait_for_data_ready() {
    unsigned long start = millis();

    while (!sensor.dataReady()) {
        if (millis() - start > 3000) {
            TEST_FAIL_MESSAGE("Timeout waiting for HTS221 dataReady()");
        }
        delay(5);
    }
}

void test_hts221_continuous_runtime_behavior() {
    TEST_ASSERT_TRUE(sensor.begin());

    sensor.setContinuous(HTS221_ODR_1_HZ);
    delay(1200);

    for (int i = 0; i < SAMPLE_COUNT; i++) {
        wait_for_data_ready();

        timestamps[i] = millis();
        // Single read() call: cheaper (one I2C transaction instead of two)
        // and guarantees both channels come from the same conversion under
        // BDU.
        auto reading = sensor.read();
        temperatures[i] = reading.temperature;
        humidities[i] = reading.humidity;

        TEST_ASSERT_GREATER_OR_EQUAL_FLOAT(MIN_TEMP, temperatures[i]);
        TEST_ASSERT_LESS_OR_EQUAL_FLOAT(MAX_TEMP, temperatures[i]);
        TEST_ASSERT_GREATER_OR_EQUAL_FLOAT(MIN_HUM, humidities[i]);
        TEST_ASSERT_LESS_OR_EQUAL_FLOAT(MAX_HUM, humidities[i]);

        delay(900);
    }

    unsigned long interval_sum = 0;

    for (int i = 1; i < SAMPLE_COUNT; i++) {
        interval_sum += timestamps[i] - timestamps[i - 1];
    }

    unsigned long average_interval = interval_sum / (SAMPLE_COUNT - 1);

    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(MIN_AVG_INTERVAL_MS, average_interval);
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(MAX_AVG_INTERVAL_MS, average_interval);

    bool all_frozen = true;

    for (int i = 1; i < SAMPLE_COUNT; i++) {
        bool temp_same = fabsf(temperatures[i] - temperatures[0]) < CHANGE_EPSILON;
        bool hum_same = fabsf(humidities[i] - humidities[0]) < CHANGE_EPSILON;

        if (!(temp_same && hum_same)) {
            all_frozen = false;
            break;
        }
    }

    TEST_ASSERT_FALSE_MESSAGE(all_frozen, "HTS221 readings remained frozen over all samples");
}

void setup() {
    delay(2000);
    internalI2C.begin();

    UNITY_BEGIN();
    RUN_TEST(test_hts221_continuous_runtime_behavior);
    UNITY_END();
}

void loop() {}
