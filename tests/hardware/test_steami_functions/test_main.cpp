// SPDX-License-Identifier: GPL-3.0-or-later

/*
 * Hardware unit tests for the function classes (temperature, humidity, pressure, distance,
 * battery) on a real STeaMi. Nothing is written to the board.
 *
 * The same file runs on every release: the checks that only hold on a V1c are ignored
 * elsewhere. The battery may be unplugged, so both outcomes of the battery test pass.
 */

#include <Arduino.h>
#include <SteamiBattery.h>
#include <SteamiBoard.h>
#include <SteamiDistance.h>
#include <SteamiHumidity.h>
#include <SteamiPressure.h>
#include <SteamiTemperature.h>
#include <math.h>
#include <string.h>
#include <unity.h>

void setUp(void) {}

void tearDown(void) {
    Steami.end();
}

static bool endsWith(const char* text, const char* suffix) {
    const size_t n = strlen(text);
    const size_t m = strlen(suffix);
    return n >= m && strcmp(text + n - m, suffix) == 0;
}

void test_temperature_reads_a_plausible_value(void) {
    SteamiTemperature temperature;
    TEST_ASSERT_TRUE(temperature.begin());
    TEST_ASSERT_TRUE(temperature.isAvailable());
    TEST_ASSERT_TRUE(temperature.status() == SteamiStatus::OK);
    TEST_ASSERT_EQUAL_STRING("", temperature.error());

    const float value = temperature.read();
    TEST_ASSERT_FALSE(isnan(value));
    TEST_ASSERT_FLOAT_WITHIN(25.0f, 25.0f, value);  // 0..50 degC
}

void test_humidity_reads_a_plausible_value(void) {
    SteamiHumidity humidity;
    TEST_ASSERT_TRUE(humidity.begin());
    const float value = humidity.read();
    TEST_ASSERT_FALSE(isnan(value));
    TEST_ASSERT_FLOAT_WITHIN(45.0f, 55.0f, value);  // 10..100 %RH
}

void test_pressure_reads_a_plausible_value(void) {
    SteamiPressure pressure;
    TEST_ASSERT_TRUE(pressure.begin());
    const float value = pressure.read();
    TEST_ASSERT_FALSE(isnan(value));
    TEST_ASSERT_FLOAT_WITHIN(100.0f, 1000.0f, value);  // 900..1100 hPa

    const float altitude = pressure.altitude();
    TEST_ASSERT_FALSE(isnan(altitude));
    TEST_ASSERT_FLOAT_WITHIN(2000.0f, 0.0f, altitude);
}

void test_v1c_builds_the_bme280_only(void) {
    SteamiTemperature temperature;
    SteamiHumidity humidity;
    SteamiPressure pressure;
    TEST_ASSERT_TRUE(temperature.begin());
    TEST_ASSERT_TRUE(humidity.begin());
    TEST_ASSERT_TRUE(pressure.begin());

    if (!Steami.isV1c()) {
        TEST_IGNORE_MESSAGE("V1c only: the BME280 serves these functions on a V1c.");
        return;
    }
    TEST_ASSERT_TRUE(Steami.built(SteamiChip::BME280));
    TEST_ASSERT_FALSE(Steami.built(SteamiChip::WSEN_HIDS));
    TEST_ASSERT_FALSE(Steami.built(SteamiChip::WSEN_PADS));
}

void test_calibration_follows_the_board_config(void) {
    SteamiTemperature temperature;
    TEST_ASSERT_TRUE(temperature.begin());

    if (!Steami.isV1c()) {
        TEST_IGNORE_MESSAGE("V1c only: the BME280 calibration is checked on a V1c.");
        return;
    }
    // Without a config entry for the BME280, nothing is calibrated; with one, it is applied.
    TemperatureCalibration calibration;
    const SteamiConfig* config = Steami.config();
    const bool configured =
        config != nullptr && config->getTemperatureCalibration("bme280", calibration);
    TEST_ASSERT_EQUAL(configured, Steami.calibrated(SteamiChip::BME280));
}

void test_distance_is_unavailable_or_in_range(void) {
    SteamiDistance distance;
    distance.begin();
    const int mm = distance.read();
    TEST_ASSERT_TRUE(mm == -1 || (mm >= 1 && mm <= 4000));

    const float cm = distance.cm();
    TEST_ASSERT_TRUE(isnan(cm) || cm < 0.0f || (cm >= 0.1f && cm <= 400.0f));
}

void test_battery_begins_or_reports_e10(void) {
    SteamiBattery battery;
    if (battery.begin()) {
        TEST_ASSERT_TRUE(battery.isAvailable());
        TEST_ASSERT_TRUE(battery.chip() != nullptr);
        const float volts = battery.voltage();
        // A gauge that reads 0 mV gives NAN: nothing to range-check then.
        if (!isnan(volts)) {
            TEST_ASSERT_FLOAT_WITHIN(0.65f, 3.65f, volts);  // 3.0..4.3 V
        }
        TEST_ASSERT_TRUE(battery.percent() <= 100);
        TEST_ASSERT_EQUAL(battery.current() > 0, battery.isCharging());
    } else {
        TEST_ASSERT_FALSE(battery.isAvailable());
        TEST_ASSERT_TRUE(endsWith(Steami.lastError(), "(E10)"));
        TEST_ASSERT_EQUAL_UINT8(0, battery.percent());
        TEST_ASSERT_TRUE(isnan(battery.voltage()));
        TEST_ASSERT_FALSE(battery.isCharging());
    }
}

void setup() {
    delay(2000);

    UNITY_BEGIN();
    RUN_TEST(test_temperature_reads_a_plausible_value);
    RUN_TEST(test_humidity_reads_a_plausible_value);
    RUN_TEST(test_pressure_reads_a_plausible_value);
    RUN_TEST(test_v1c_builds_the_bme280_only);
    RUN_TEST(test_calibration_follows_the_board_config);
    RUN_TEST(test_distance_is_unavailable_or_in_range);
    RUN_TEST(test_battery_begins_or_reports_e10);
    UNITY_END();
}

void loop() {}
