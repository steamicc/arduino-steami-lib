// SPDX-License-Identifier: GPL-3.0-or-later

// STeaMi V1c only: the GC9A01 round TFT replaces the V1b SSD1327 OLED.

#include <Arduino.h>
#include <SteamiGC9A01.h>
#include <unity.h>

#include "driver_checks.h"

SteamiGC9A01 display;

void setUp(void) {}

void tearDown(void) {}

void test_gc9a01_begin(void) {
    check_begin(display);
}

void test_gc9a01_fill_and_read_back(void) {
    display.fill(GC9A01_BLUE);
    TEST_ASSERT_EQUAL_HEX16(GC9A01_BLUE, display.pixel(0, 0));
    TEST_ASSERT_EQUAL_HEX16(GC9A01_BLUE, display.pixel(239, 239));
    display.show();
}

void test_gc9a01_show_duration(void) {
    const uint32_t start = millis();
    display.show();
    const uint32_t elapsed = millis() - start;
    char msg[48];
    snprintf(msg, sizeof(msg), "show() took %lu ms", static_cast<unsigned long>(elapsed));
    TEST_MESSAGE(msg);
    TEST_ASSERT_LESS_THAN_UINT32(500, elapsed);
}

void test_gc9a01_drawing_does_not_crash(void) {
    display.fill(GC9A01_BLACK);
    display.pixel(120, 120, GC9A01_WHITE);
    display.fillRect(60, 60, 40, 40, GC9A01_RED);
    display.line(0, 0, 239, 239, GC9A01_GREEN);
    display.text("STeaMi", 102, 116, GC9A01_YELLOW);
    display.scroll(4, 4);
    display.show();
}

void test_gc9a01_invert_does_not_crash(void) {
    display.invert(true);
    display.invert(false);
}

void test_gc9a01_rotate_does_not_crash(void) {
    display.rotate(true);
    display.rotate(false);
}

void test_gc9a01_power_cycle_does_not_crash(void) {
    display.powerOff();
    display.powerOn();
}

void setup() {
    delay(2000);

    UNITY_BEGIN();
    RUN_TEST(test_gc9a01_begin);
    RUN_TEST(test_gc9a01_fill_and_read_back);
    RUN_TEST(test_gc9a01_show_duration);
    RUN_TEST(test_gc9a01_drawing_does_not_crash);
    RUN_TEST(test_gc9a01_invert_does_not_crash);
    RUN_TEST(test_gc9a01_rotate_does_not_crash);
    RUN_TEST(test_gc9a01_power_cycle_does_not_crash);
    UNITY_END();
}

void loop() {}
