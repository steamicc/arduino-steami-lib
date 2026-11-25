// SPDX-License-Identifier: GPL-3.0-or-later

/*
 * Hardware unit tests for the motion, light and button classes (accelerometer, gyroscope,
 * magnetometer, compass, light, colour, gesture, buttons) on a real STeaMi. Nothing is written to
 * the board.
 *
 * Keep the board still and flat, screen up, away from magnets and metal. Leave the buttons alone
 * and keep your hands away from the sensors during the run. The same file runs on
 * every release: the checks that only hold on a V1c are ignored elsewhere, and the APDS-9960
 * checks flip on a V1a or V1b.
 */

#include <Arduino.h>
#include <SteamiAccelerometer.h>
#include <SteamiBoard.h>
#include <SteamiButtons.h>
#include <SteamiColor.h>
#include <SteamiCompass.h>
#include <SteamiGesture.h>
#include <SteamiGyroscope.h>
#include <SteamiLight.h>
#include <SteamiMagnetometer.h>
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

void test_accelerometer_reads_one_g_flat(void) {
    SteamiAccelerometer accelerometer;
    TEST_ASSERT_TRUE(accelerometer.begin());
    TEST_ASSERT_TRUE(accelerometer.isAvailable());
    TEST_ASSERT_TRUE(accelerometer.status() == SteamiStatus::OK);
    TEST_ASSERT_EQUAL_STRING("", accelerometer.error());
    TEST_ASSERT_EQUAL_STRING("ISM330DL", accelerometer.chip());

    delay(100);
    const SteamiVector a = accelerometer.read();
    TEST_ASSERT_FALSE(isnan(a.x));
    TEST_ASSERT_FLOAT_WITHIN(0.2f, -1.0f, a.z);  // -1.2..-0.8 g: flat, screen up
    TEST_ASSERT_FLOAT_WITHIN(0.2f, 1.0f, accelerometer.strength());
    TEST_ASSERT_FALSE(accelerometer.isShaken());
}

void test_gyroscope_is_still(void) {
    SteamiGyroscope gyroscope;
    TEST_ASSERT_TRUE(gyroscope.begin());
    delay(100);
    const SteamiVector g = gyroscope.read();
    TEST_ASSERT_FALSE(isnan(g.x));
    const float norm = sqrtf(g.x * g.x + g.y * g.y + g.z * g.z);
    TEST_ASSERT_TRUE(norm < 10.0f);  // degrees per second
}

void test_magnetometer_measures_the_earth_field(void) {
    SteamiMagnetometer magnetometer;
    TEST_ASSERT_TRUE(magnetometer.begin());
    TEST_ASSERT_EQUAL_STRING("LIS2MDL", magnetometer.chip());
    delay(100);
    const float strength = magnetometer.strength();
    TEST_ASSERT_FALSE(isnan(strength));
    // An uncalibrated board includes its own hard-iron field (about 85 uT on X on a V1c), so the
    // bound is wide: the sensor range is +-5000 uT.
    TEST_ASSERT_TRUE(strength >= 10.0f && strength <= 1000.0f);
}

void test_compass_heading_is_in_range(void) {
    SteamiCompass compass;
    TEST_ASSERT_TRUE(compass.begin());
    delay(100);
    const float heading = compass.heading();
    TEST_ASSERT_FALSE(isnan(heading));
    TEST_ASSERT_TRUE(heading >= 0.0f && heading < 360.0f);
    TEST_ASSERT_TRUE(strlen(compass.direction()) > 0);
}

void test_shared_drivers_are_built_once(void) {
    SteamiAccelerometer accelerometer;
    SteamiGyroscope gyroscope;
    SteamiMagnetometer magnetometer;
    SteamiCompass compass;
    TEST_ASSERT_TRUE(accelerometer.begin());
    TEST_ASSERT_TRUE(gyroscope.begin());
    TEST_ASSERT_TRUE(magnetometer.begin());
    TEST_ASSERT_TRUE(compass.begin());
    TEST_ASSERT_TRUE(Steami.built(SteamiChip::ISM330DL));
    TEST_ASSERT_TRUE(Steami.built(SteamiChip::LIS2MDL));
}

void test_apds_classes_follow_the_release(void) {
    SteamiLight light;
    SteamiColor color;
    SteamiGesture gesture;
    if (Steami.isV1c()) {
        TEST_ASSERT_FALSE(light.begin());
        TEST_ASSERT_FALSE(color.begin());
        TEST_ASSERT_FALSE(gesture.begin());
        TEST_ASSERT_TRUE(light.status() == SteamiStatus::NOT_ON_THIS_BOARD);
        TEST_ASSERT_TRUE(endsWith(Steami.lastError(), "(E20)"));
        TEST_ASSERT_EQUAL_UINT16(0, light.read());
        uint16_t r = 7;
        uint16_t g = 7;
        uint16_t b = 7;
        TEST_ASSERT_FALSE(color.read(r, g, b));
        TEST_ASSERT_TRUE(gesture.read() == SteamiGesture::NONE);
        return;
    }
    // V1a or V1b: the APDS-9960 answers, so the three classes begin.
    TEST_ASSERT_TRUE(light.begin());
    TEST_ASSERT_TRUE(color.begin());
    TEST_ASSERT_TRUE(gesture.begin());
    TEST_ASSERT_EQUAL_STRING("APDS-9960", light.chip());
    uint16_t r = 0;
    uint16_t g = 0;
    uint16_t b = 0;
    TEST_ASSERT_TRUE(color.read(r, g, b));
    TEST_ASSERT_TRUE(gesture.read() == SteamiGesture::NONE);
}

void test_v1c_has_no_apds_chip(void) {
    SteamiLight light;
    light.begin();
    if (!Steami.isV1c()) {
        TEST_IGNORE_MESSAGE("V1c only: the APDS-9960 is absent from a V1c.");
        return;
    }
    TEST_ASSERT_FALSE(Steami.built(SteamiChip::APDS9960));
}

void test_buttons_begin_and_read_released(void) {
    SteamiButtons buttons;
    TEST_ASSERT_TRUE(buttons.begin());
    TEST_ASSERT_TRUE(buttons.isAvailable());
    TEST_ASSERT_EQUAL_STRING("MCP23009E", buttons.chip());
    TEST_ASSERT_FALSE(buttons.isPressed(SteamiButton::NONE));

    if (!Steami.isV1c()) {
        TEST_IGNORE_MESSAGE("V1c only: the idle level of the buttons is checked on a V1c.");
        return;
    }
    // Nobody presses a button during the test.
    TEST_ASSERT_FALSE(buttons.wasPressed(SteamiButton::A));
    TEST_ASSERT_FALSE(buttons.wasPressed(SteamiButton::B));
    TEST_ASSERT_FALSE(buttons.wasPressed(SteamiButton::MENU));
    TEST_ASSERT_TRUE(buttons.waitForPress(100) == SteamiButton::NONE);
}

void setup() {
    delay(2000);

    UNITY_BEGIN();
    RUN_TEST(test_accelerometer_reads_one_g_flat);
    RUN_TEST(test_gyroscope_is_still);
    RUN_TEST(test_magnetometer_measures_the_earth_field);
    RUN_TEST(test_compass_heading_is_in_range);
    RUN_TEST(test_shared_drivers_are_built_once);
    RUN_TEST(test_apds_classes_follow_the_release);
    RUN_TEST(test_v1c_has_no_apds_chip);
    RUN_TEST(test_buttons_begin_and_read_released);
    UNITY_END();
}

void loop() {}
