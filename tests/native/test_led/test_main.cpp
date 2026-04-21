// SPDX-License-Identifier: GPL-3.0-or-later

#include <Arduino.h>
#include <unity.h>

#define LED_RED_PIN 1
#define LED_GREEN_PIN 2
#define LED_BLUE_PIN 3

void setUp(void) {
    gpioPinState().clear();
    gpioPinMode().clear();
}

void tearDown(void) {}

void test_led_pins_configured_output(void) {
    pinMode(LED_RED_PIN, OUTPUT);
    pinMode(LED_GREEN_PIN, OUTPUT);
    pinMode(LED_BLUE_PIN, OUTPUT);

    TEST_ASSERT_EQUAL(OUTPUT, gpioPinMode()[LED_RED_PIN]);
    TEST_ASSERT_EQUAL(OUTPUT, gpioPinMode()[LED_GREEN_PIN]);
    TEST_ASSERT_EQUAL(OUTPUT, gpioPinMode()[LED_BLUE_PIN]);
}

void test_led_write_sequence(void) {
    pinMode(LED_RED_PIN, OUTPUT);

    digitalWrite(LED_RED_PIN, HIGH);
    TEST_ASSERT_EQUAL(HIGH, gpioPinState()[LED_RED_PIN]);

    digitalWrite(LED_RED_PIN, LOW);
    TEST_ASSERT_EQUAL(LOW, gpioPinState()[LED_RED_PIN]);
}

void test_led_rgb_sequence(void) {
    pinMode(LED_RED_PIN, OUTPUT);
    pinMode(LED_GREEN_PIN, OUTPUT);
    pinMode(LED_BLUE_PIN, OUTPUT);

    digitalWrite(LED_RED_PIN, HIGH);
    TEST_ASSERT_EQUAL(HIGH, gpioPinState()[LED_RED_PIN]);

    digitalWrite(LED_GREEN_PIN, HIGH);
    TEST_ASSERT_EQUAL(HIGH, gpioPinState()[LED_GREEN_PIN]);

    digitalWrite(LED_BLUE_PIN, HIGH);
    TEST_ASSERT_EQUAL(HIGH, gpioPinState()[LED_BLUE_PIN]);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_led_pins_configured_output);
    RUN_TEST(test_led_write_sequence);
    RUN_TEST(test_led_rgb_sequence);
    return UNITY_END();
}
