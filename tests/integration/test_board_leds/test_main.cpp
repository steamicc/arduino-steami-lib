// SPDX-License-Identifier: GPL-3.0-or-later

#include <Arduino.h>
#include <unity.h>

static void setRgb(uint8_t red, uint8_t green, uint8_t blue) {
    digitalWrite(LED_RED, red);
    digitalWrite(LED_GREEN, green);
    digitalWrite(LED_BLUE, blue);
}

void test_rgb_led_cycle() {
    TEST_MESSAGE("Observe RGB LED: RED -> GREEN -> BLUE -> WHITE.");

    setRgb(HIGH, LOW, LOW);
    TEST_ASSERT_EQUAL(HIGH, digitalRead(LED_RED));
    delay(1000);

    setRgb(LOW, HIGH, LOW);
    TEST_ASSERT_EQUAL(HIGH, digitalRead(LED_GREEN));
    delay(1000);

    setRgb(LOW, LOW, HIGH);
    TEST_ASSERT_EQUAL(HIGH, digitalRead(LED_BLUE));
    delay(1000);

    setRgb(HIGH, HIGH, HIGH);
    delay(1000);

    setRgb(LOW, LOW, LOW);
}

void test_ble_led_cycle() {
    TEST_MESSAGE("Observe BLE LED: it should turn ON for one second.");

    digitalWrite(LED_BLE, HIGH);
    TEST_ASSERT_EQUAL(HIGH, digitalRead(LED_BLE));

    delay(1000);

    digitalWrite(LED_BLE, LOW);
    TEST_ASSERT_EQUAL(LOW, digitalRead(LED_BLE));
}

void setup() {
    delay(2000);

    pinMode(LED_RED, OUTPUT);
    pinMode(LED_GREEN, OUTPUT);
    pinMode(LED_BLUE, OUTPUT);
    pinMode(LED_BLE, OUTPUT);

    setRgb(LOW, LOW, LOW);
    digitalWrite(LED_BLE, LOW);

    UNITY_BEGIN();
    RUN_TEST(test_rgb_led_cycle);
    RUN_TEST(test_ble_led_cycle);
    UNITY_END();
}

void loop() {}
