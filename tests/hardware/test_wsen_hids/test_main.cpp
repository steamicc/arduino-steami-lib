// SPDX-License-Identifier: GPL-3.0-or-later

/*
 * Hardware unit validation for WSEN-HIDS on real STeaMi silicon.
 *
 * The plausibility windows are intentionally broad:
 * - temperature: 0°C to 50°C covers typical indoor classroom/lab usage
 *   plus warm-board self-heating margin.
 * - humidity: 10% to 90% RH avoids false negatives from dry heated rooms
 *   or humid breath-adjacent environments while still catching nonsense data.
 */

#include <Arduino.h>
#include <Wire.h>
#include <WsenHids.h>
#include <math.h>
#include <unity.h>

#include "driver_checks.h"

TwoWire internalI2C(I2C_INT_SDA, I2C_INT_SCL);
WsenHids sensor(internalI2C);

// Direct register read straight off the wire — bypasses the driver so
// tests can assert what is actually present in the chip after a driver
// call, not what the driver's cached state thinks it wrote.
static uint8_t readRawRegister(uint8_t reg) {
    internalI2C.beginTransmission(WSEN_HIDS_DEFAULT_ADDRESS);
    internalI2C.write(reg);
    internalI2C.endTransmission(false);
    internalI2C.requestFrom(static_cast<uint8_t>(WSEN_HIDS_DEFAULT_ADDRESS),
                            static_cast<uint8_t>(1));
    return internalI2C.available() ? static_cast<uint8_t>(internalI2C.read()) : 0;
}

static void writeRawRegister(uint8_t reg, uint8_t value) {
    internalI2C.beginTransmission(WSEN_HIDS_DEFAULT_ADDRESS);
    internalI2C.write(reg);
    internalI2C.write(value);
    internalI2C.endTransmission();
}

// Unity invokes setUp() before every RUN_TEST. Re-initialising here keeps
// each test independent — otherwise reads following a skipped/failing
// test_wsen_hids_begin would compute against uninitialised calibration.
//
// setContinuous(1 Hz) leaves the part in steady-state ODR sampling so
// reads return real values immediately — same recipe as the native
// plausibility tests in test_wsen_hids. The dedicated
// `test_wsen_hids_read_auto_bringup_without_set_continuous` test below
// covers the path where the caller skips this explicit setContinuous.
void setUp(void) {
    sensor.begin();
    sensor.setContinuous(WSEN_HIDS_ODR_1_HZ);
}

void tearDown(void) {}

void test_wsen_hids_begin() {
    check_begin(sensor);
}

void test_wsen_hids_who_am_i() {
    check_who_am_i(sensor, WSEN_HIDS_WHO_AM_I_VALUE);
}

void test_wsen_hids_read_plausible_temperature() {
    check_read_plausible(sensor, &WsenHids::temperature, 0.0f, 50.0f);
}

void test_wsen_hids_read_plausible_humidity() {
    check_read_plausible(sensor, &WsenHids::humidity, 10.0f, 90.0f);
}

// Validates the auto-bring-up path inside read() on real silicon: after
// begin() the part is in power-down, and the caller never explicitly
// configures an ODR. read() should switch the chip to continuous at
// 12.5 Hz, wait for the first sample, and return plausible values
// without timing out. This is the user-facing guarantee that motivated
// switching read()'s auto-trigger from CTRL2.ONE_SHOT (broken on this
// silicon) to setContinuous(12.5 Hz).
void test_wsen_hids_read_auto_bringup_without_set_continuous() {
    // Re-init the sensor to start from a clean begin() — setUp() has
    // already run setContinuous, which would short-circuit the
    // bring-up path we want to exercise here.
    sensor.powerOff();
    TEST_ASSERT_TRUE(sensor.begin());

    auto r = sensor.read();

    TEST_ASSERT_FALSE_MESSAGE(isnan(r.temperature),
                              "read() auto-bring-up should not time out (temperature)");
    TEST_ASSERT_FALSE_MESSAGE(isnan(r.humidity),
                              "read() auto-bring-up should not time out (humidity)");
    TEST_ASSERT_GREATER_OR_EQUAL_FLOAT(0.0f, r.temperature);
    TEST_ASSERT_LESS_OR_EQUAL_FLOAT(50.0f, r.temperature);
    TEST_ASSERT_GREATER_OR_EQUAL_FLOAT(10.0f, r.humidity);
    TEST_ASSERT_LESS_OR_EQUAL_FLOAT(90.0f, r.humidity);
}

// Real-silicon counterpart of the native test_begin_resets_av_conf_to_default.
// On the STeaMi, the chip retains its register state across MCU resets,
// so begin() has to force AV_CONF back to the datasheet default
// regardless of whatever a previous sketch left behind.
void test_wsen_hids_begin_resets_av_conf_to_default() {
    // Plant a non-default value on the chip and confirm begin() overrides
    // it. 0x3F = max averaging is exactly the pathological state observed
    // during PR investigation.
    writeRawRegister(WSEN_HIDS_REG_AV_CONF, 0x3F);
    TEST_ASSERT_EQUAL_HEX8(0x3F, readRawRegister(WSEN_HIDS_REG_AV_CONF));

    TEST_ASSERT_TRUE(sensor.begin());

    TEST_ASSERT_EQUAL_HEX8(WSEN_HIDS_AV_CONF_DEFAULT, readRawRegister(WSEN_HIDS_REG_AV_CONF));
}

// Power-down → read() must recover via the auto-bring-up path. Validates
// the low-power sleep / on-demand wake pattern on real silicon.
void test_wsen_hids_powerOff_then_read_recovers() {
    auto r1 = sensor.read();
    TEST_ASSERT_FALSE_MESSAGE(isnan(r1.temperature), "baseline read failed");

    sensor.powerOff();
    delay(50);
    TEST_ASSERT_BITS_LOW(WSEN_HIDS_CTRL1_PD, readRawRegister(WSEN_HIDS_REG_CTRL1));

    auto r2 = sensor.read();

    TEST_ASSERT_FALSE_MESSAGE(isnan(r2.temperature),
                              "post-powerOff read should recover (temperature)");
    TEST_ASSERT_FALSE_MESSAGE(isnan(r2.humidity), "post-powerOff read should recover (humidity)");
    TEST_ASSERT_BITS_HIGH(WSEN_HIDS_CTRL1_PD, readRawRegister(WSEN_HIDS_REG_CTRL1));
}

// setTemperatureOffset() must shift subsequent readings by the configured
// amount. Done back-to-back so ambient drift between the two reads is
// well under the asserted tolerance.
void test_wsen_hids_setTemperatureOffset_applies() {
    sensor.setTemperatureOffset(0.0f);
    float tBase = sensor.temperature();
    TEST_ASSERT_FALSE(isnan(tBase));

    sensor.setTemperatureOffset(5.0f);
    float tShifted = sensor.temperature();
    TEST_ASSERT_FALSE(isnan(tShifted));

    TEST_ASSERT_FLOAT_WITHIN(0.5f, 5.0f, tShifted - tBase);
}

// reboot() polls CTRL2.BOOT until it self-clears (max 100 ms). Validate
// on real silicon that the BOOT mechanism completes within that window
// and the chip stays functional afterwards.
void test_wsen_hids_reboot_clears_boot_and_keeps_chip_functional() {
    sensor.reboot();

    TEST_ASSERT_BITS_LOW_MESSAGE(WSEN_HIDS_CTRL2_BOOT, readRawRegister(WSEN_HIDS_REG_CTRL2),
                                 "BOOT did not self-clear within reboot() poll window");

    // Re-arm continuous mode and confirm reads still come back valid.
    sensor.setContinuous(WSEN_HIDS_ODR_1_HZ);
    delay(1200);
    auto r = sensor.read();
    TEST_ASSERT_FALSE(isnan(r.temperature));
    TEST_ASSERT_FALSE(isnan(r.humidity));
}

// setAveraging() at a non-default code must (a) actually land in AV_CONF
// and (b) leave the chip producing plausible data. Catches mask/shift
// regressions and verifies the chip honours new AV_CONF on the fly.
void test_wsen_hids_setAveraging_takes_effect() {
    // AVGH code 1 = 8 humidity samples; AVGT code 1 = 4 temperature samples.
    sensor.setAveraging(0x01, 0x01);

    uint8_t expected = static_cast<uint8_t>((0x01 << WSEN_HIDS_AV_CONF_AVGT_SHIFT) |
                                            (0x01 & WSEN_HIDS_AV_CONF_AVGH_MASK));
    TEST_ASSERT_EQUAL_HEX8(expected, readRawRegister(WSEN_HIDS_REG_AV_CONF));

    delay(1200);
    auto r = sensor.read();
    TEST_ASSERT_GREATER_OR_EQUAL_FLOAT(0.0f, r.temperature);
    TEST_ASSERT_LESS_OR_EQUAL_FLOAT(50.0f, r.temperature);
    TEST_ASSERT_GREATER_OR_EQUAL_FLOAT(10.0f, r.humidity);
    TEST_ASSERT_LESS_OR_EQUAL_FLOAT(90.0f, r.humidity);
}

// Documents the observed limitation: triggerOneShot() on this silicon
// does not reliably fire a conversion (see
// steamicc/micropython-steami-lib#425). What we DO guarantee is that an
// explicit one-shot attempt does not break the chip — once the caller
// recovers via setContinuous(), reads come back to life.
void test_wsen_hids_triggerOneShot_is_recoverable() {
    sensor.powerOff();

    sensor.triggerOneShot();
    delay(200);
    // We intentionally do NOT assert the outcome of the one-shot — it
    // typically times out on this silicon. The point is the recovery
    // path below.

    sensor.setContinuous(WSEN_HIDS_ODR_1_HZ);
    delay(1200);
    auto r = sensor.read();
    TEST_ASSERT_FALSE_MESSAGE(isnan(r.temperature),
                              "chip should recover via continuous mode after one-shot attempt");
    TEST_ASSERT_FALSE_MESSAGE(isnan(r.humidity),
                              "chip should recover via continuous mode after one-shot attempt");
}

void setup() {
    delay(2000);
    internalI2C.begin();

    UNITY_BEGIN();
    RUN_TEST(test_wsen_hids_begin);
    RUN_TEST(test_wsen_hids_who_am_i);
    RUN_TEST(test_wsen_hids_read_plausible_temperature);
    RUN_TEST(test_wsen_hids_read_plausible_humidity);
    RUN_TEST(test_wsen_hids_read_auto_bringup_without_set_continuous);
    RUN_TEST(test_wsen_hids_begin_resets_av_conf_to_default);
    RUN_TEST(test_wsen_hids_powerOff_then_read_recovers);
    RUN_TEST(test_wsen_hids_setTemperatureOffset_applies);
    RUN_TEST(test_wsen_hids_reboot_clears_boot_and_keeps_chip_functional);
    RUN_TEST(test_wsen_hids_setAveraging_takes_effect);
    RUN_TEST(test_wsen_hids_triggerOneShot_is_recoverable);
    UNITY_END();
}

void loop() {}
