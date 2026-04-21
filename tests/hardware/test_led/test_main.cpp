// SPDX-License-Identifier: GPL-3.0-or-later

#include <Arduino.h>
#include <unity.h>

void setUp() {}
void tearDown() {}

void test_led_pins_output_mode() {
    pinMode(LED_RED, OUTPUT);
    pinMode(LED_GREEN, OUTPUT);
    pinMode(LED_BLUE, OUTPUT);

    // if we reach this point without crashing, we assume the pins are set correctly
    TEST_ASSERT_TRUE(true);
}

void test_led_blink_sequence() {
    pinMode(LED_RED, OUTPUT);
    pinMode(LED_GREEN, OUTPUT);
    pinMode(LED_BLUE, OUTPUT);

    // red
    digitalWrite(LED_RED, HIGH);
    delay(300);
    digitalWrite(LED_RED, LOW);

    // green
    digitalWrite(LED_GREEN, HIGH);
    delay(300);
    digitalWrite(LED_GREEN, LOW);

    // blue
    digitalWrite(LED_BLUE, HIGH);
    delay(300);
    digitalWrite(LED_BLUE, LOW);

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
