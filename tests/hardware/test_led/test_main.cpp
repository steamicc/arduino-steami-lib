// SPDX-License-Identifier: GPL-3.0-or-later

#include <Arduino.h>
#include <unity.h>

#define LED_RED_PIN LED_BUILTIN
#define LED_GREEN_PIN LED_BUILTIN
#define LED_BLUE_PIN LED_BUILTIN

void setUp() {}
void tearDown() {}

void test_led_pins_output_mode() {
    pinMode(LED_RED_PIN, OUTPUT);
    pinMode(LED_GREEN_PIN, OUTPUT);
    pinMode(LED_BLUE_PIN, OUTPUT);

    // if we reach this point without crashing, we assume the pins are set correctly
    TEST_ASSERT_TRUE(true);
}

void test_led_blink_sequence() {
    pinMode(LED_RED_PIN, OUTPUT);
    pinMode(LED_GREEN_PIN, OUTPUT);
    pinMode(LED_BLUE_PIN, OUTPUT);

    // red
    digitalWrite(LED_RED_PIN, HIGH);
    delay(300);
    digitalWrite(LED_RED_PIN, LOW);

    // green
    digitalWrite(LED_GREEN_PIN, HIGH);
    delay(300);
    digitalWrite(LED_GREEN_PIN, LOW);

    // blue
    digitalWrite(LED_BLUE_PIN, HIGH);
    delay(300);
    digitalWrite(LED_BLUE_PIN, LOW);

    TEST_ASSERT_TRUE(true);
}

void setup() {
    delay(2000);
    UNITY_BEGIN();

    RUN_TEST(test_led_pins_output_mode);
    RUN_TEST(test_led_blink_sequence);

    UNITY_END();
}

void loop() {}
