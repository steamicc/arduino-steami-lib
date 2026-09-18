// SPDX-License-Identifier: GPL-3.0-or-later

#include <Arduino.h>
#include <unity.h>

static void checkAnalogPin(uint32_t pin) {
    pinMode(pin, INPUT);

    int value = analogRead(pin);

    TEST_ASSERT_GREATER_OR_EQUAL_INT(0, value);
    TEST_ASSERT_LESS_OR_EQUAL_INT(4095, value);
}

static void checkDigitalPullupPin(uint32_t pin) {
    pinMode(pin, INPUT_PULLUP);
    delay(2);

    TEST_ASSERT_EQUAL(HIGH, digitalRead(pin));
}

void test_analog_expansion_pins() {
    analogReadResolution(12);

    checkAnalogPin(P0);
    checkAnalogPin(P1);
    checkAnalogPin(P2);
    checkAnalogPin(P3);
    checkAnalogPin(P4);
    checkAnalogPin(P10);
}

void test_digital_expansion_pins() {
    checkDigitalPullupPin(P6);
    checkDigitalPullupPin(P7);
    checkDigitalPullupPin(P8);
    checkDigitalPullupPin(P9);
    checkDigitalPullupPin(P12);
    checkDigitalPullupPin(P16);
}

void setup() {
    delay(2000);

    UNITY_BEGIN();
    RUN_TEST(test_analog_expansion_pins);
    RUN_TEST(test_digital_expansion_pins);
    UNITY_END();
}

void loop() {}
