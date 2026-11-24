// SPDX-License-Identifier: GPL-3.0-or-later

/*
 * Hardware unit validation for the BME280 on a real STeaMi V1c.
 *
 * The plausibility windows are intentionally broad:
 * - temperature: 0 to 50 degC covers indoor use plus board self-heating.
 * - humidity: 5 to 95 %RH avoids false negatives in dry heated rooms.
 * - pressure: 800 to 1100 hPa covers every inhabited altitude.
 */

#include <Arduino.h>
#include <BME280.h>
#include <Wire.h>
#include <math.h>
#include <unity.h>

#include "driver_checks.h"

TwoWire internalI2C(I2C_INT_SDA, I2C_INT_SCL);
BME280 sensor(internalI2C);

// Direct register read straight off the wire — bypasses the driver so tests
// assert what is actually present in the chip.
static uint8_t readRawRegister(uint8_t reg) {
    internalI2C.beginTransmission(BME280_DEFAULT_ADDRESS);
    internalI2C.write(reg);
    internalI2C.endTransmission(false);
    internalI2C.requestFrom(static_cast<uint8_t>(BME280_DEFAULT_ADDRESS), static_cast<uint8_t>(1));
    return internalI2C.available() ? static_cast<uint8_t>(internalI2C.read()) : 0;
}

static void checkPlausible(const BME280::ReadResult& r) {
    TEST_ASSERT_FALSE_MESSAGE(isnan(r.temperature), "temperature is NaN");
    TEST_ASSERT_FALSE_MESSAGE(isnan(r.humidity), "humidity is NaN");
    TEST_ASSERT_FALSE_MESSAGE(isnan(r.pressure), "pressure is NaN");
    TEST_ASSERT_GREATER_OR_EQUAL_FLOAT(0.0f, r.temperature);
    TEST_ASSERT_LESS_OR_EQUAL_FLOAT(50.0f, r.temperature);
    TEST_ASSERT_GREATER_OR_EQUAL_FLOAT(5.0f, r.humidity);
    TEST_ASSERT_LESS_OR_EQUAL_FLOAT(95.0f, r.humidity);
    TEST_ASSERT_GREATER_OR_EQUAL_FLOAT(800.0f, r.pressure);
    TEST_ASSERT_LESS_OR_EQUAL_FLOAT(1100.0f, r.pressure);
}

// begin() restores the default configuration, so every test starts from
// sleep mode, oversampling x1, filter off.
void setUp(void) {
    sensor.begin();
}

void tearDown(void) {}

void test_bme280_begin() {
    check_begin(sensor);
}

void test_bme280_chip_id() {
    check_who_am_i(sensor, BME280_CHIP_ID_VALUE);
}

void test_bme280_begin_leaves_chip_in_sleep_mode() {
    TEST_ASSERT_EQUAL_HEX8(BME280_MODE_SLEEP,
                           readRawRegister(BME280_REG_CTRL_MEAS) & BME280_MODE_MASK);
}

void test_bme280_read_plausible_temperature() {
    check_read_plausible(sensor, &BME280::temperature, 0.0f, 50.0f);
}

void test_bme280_read_plausible_humidity() {
    check_read_plausible(sensor, &BME280::humidity, 5.0f, 95.0f);
}

void test_bme280_read_plausible_pressure() {
    check_read_plausible(sensor, &BME280::pressureHpa, 800.0f, 1100.0f);
}

// From sleep mode, read() must auto-trigger a forced conversion.
void test_bme280_read_auto_triggers_from_sleep() {
    checkPlausible(sensor.read());
}

void test_bme280_read_one_shot() {
    checkPlausible(sensor.readOneShot());
    // Forced mode returns to sleep by itself.
    delay(20);
    TEST_ASSERT_EQUAL_HEX8(BME280_MODE_SLEEP,
                           readRawRegister(BME280_REG_CTRL_MEAS) & BME280_MODE_MASK);
}

void test_bme280_trigger_one_shot_then_poll_data_ready() {
    sensor.triggerOneShot();
    uint32_t start = millis();
    while (!sensor.dataReady() && millis() - start < 200) {
        delay(1);
    }
    TEST_ASSERT_TRUE(sensor.dataReady());
    checkPlausible(sensor.read());
}

void test_bme280_continuous_mode() {
    sensor.setContinuous(BME280_STANDBY_62_5_MS);
    TEST_ASSERT_EQUAL_HEX8(BME280_MODE_NORMAL,
                           readRawRegister(BME280_REG_CTRL_MEAS) & BME280_MODE_MASK);
    TEST_ASSERT_EQUAL_HEX8(
        BME280_STANDBY_62_5_MS,
        (readRawRegister(BME280_REG_CONFIG) >> BME280_STANDBY_SHIFT) & BME280_STANDBY_MASK);
    delay(200);
    checkPlausible(sensor.read());
    sensor.powerOff();
    TEST_ASSERT_EQUAL_HEX8(BME280_MODE_SLEEP,
                           readRawRegister(BME280_REG_CTRL_MEAS) & BME280_MODE_MASK);
}

void test_bme280_oversampling_and_filter_reach_the_chip() {
    sensor.setOversampling(BME280_OSRS_X2, BME280_OSRS_X16, BME280_OSRS_X4);
    sensor.setIirFilter(BME280_FILTER_8);
    TEST_ASSERT_EQUAL_HEX8(BME280_OSRS_X4, readRawRegister(BME280_REG_CTRL_HUM) & 0x07);
    TEST_ASSERT_EQUAL_HEX8((BME280_OSRS_X2 << 5) | (BME280_OSRS_X16 << 2),
                           readRawRegister(BME280_REG_CTRL_MEAS) & 0xFC);
    TEST_ASSERT_EQUAL_HEX8(
        BME280_FILTER_8,
        (readRawRegister(BME280_REG_CONFIG) >> BME280_FILTER_SHIFT) & BME280_FILTER_MASK);
    checkPlausible(sensor.read());
}

void test_bme280_altitude_is_plausible() {
    float altitude = sensor.altitude();
    TEST_ASSERT_FALSE(isnan(altitude));
    TEST_ASSERT_GREATER_OR_EQUAL_FLOAT(-500.0f, altitude);
    TEST_ASSERT_LESS_OR_EQUAL_FLOAT(3000.0f, altitude);
}

void test_bme280_dew_point_is_below_temperature() {
    auto r = sensor.read();
    float dewPoint = BME280::dewPointFrom(r.temperature, r.humidity);
    TEST_ASSERT_FALSE(isnan(dewPoint));
    TEST_ASSERT_LESS_OR_EQUAL_FLOAT(r.temperature + 0.5f, dewPoint);
}

void test_bme280_temperature_offset_applies() {
    sensor.setTemperatureOffset(0.0f);
    float base = sensor.temperature();
    sensor.setTemperatureOffset(5.0f);
    float shifted = sensor.temperature();
    TEST_ASSERT_FALSE(isnan(base));
    TEST_ASSERT_FALSE(isnan(shifted));
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 5.0f, shifted - base);
}

void test_bme280_soft_reset_keeps_chip_functional() {
    sensor.softReset();
    TEST_ASSERT_EQUAL_HEX8(BME280_CHIP_ID_VALUE, sensor.deviceId());
    checkPlausible(sensor.read());
}

void setup() {
    delay(2000);
    internalI2C.begin();

    UNITY_BEGIN();
    RUN_TEST(test_bme280_begin);
    RUN_TEST(test_bme280_chip_id);
    RUN_TEST(test_bme280_begin_leaves_chip_in_sleep_mode);
    RUN_TEST(test_bme280_read_plausible_temperature);
    RUN_TEST(test_bme280_read_plausible_humidity);
    RUN_TEST(test_bme280_read_plausible_pressure);
    RUN_TEST(test_bme280_read_auto_triggers_from_sleep);
    RUN_TEST(test_bme280_read_one_shot);
    RUN_TEST(test_bme280_trigger_one_shot_then_poll_data_ready);
    RUN_TEST(test_bme280_continuous_mode);
    RUN_TEST(test_bme280_oversampling_and_filter_reach_the_chip);
    RUN_TEST(test_bme280_altitude_is_plausible);
    RUN_TEST(test_bme280_dew_point_is_below_temperature);
    RUN_TEST(test_bme280_temperature_offset_applies);
    RUN_TEST(test_bme280_soft_reset_keeps_chip_functional);
    UNITY_END();
}

void loop() {}
