// SPDX-License-Identifier: GPL-3.0-or-later

#include <Arduino.h>
#include <unity.h>

void test_buzzer_440_hz() {
    TEST_MESSAGE("Listen for a 440 Hz beep.");

    tone(SPEAKER, 440);
    delay(500);
    noTone(SPEAKER);

    TEST_PASS();
}

void test_buzzer_frequency_sweep() {
    TEST_MESSAGE("Listen for a rising frequency sweep.");

    for (unsigned int frequency = 200; frequency <= 2000; frequency += 100) {
        tone(SPEAKER, frequency);
        delay(100);
    }

    noTone(SPEAKER);

    TEST_PASS();
}

void setup() {
    delay(2000);

    UNITY_BEGIN();
    RUN_TEST(test_buzzer_440_hz);
    RUN_TEST(test_buzzer_frequency_sweep);
    UNITY_END();
}

void loop() {}
