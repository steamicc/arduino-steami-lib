// SPDX-License-Identifier: GPL-3.0-or-later

#include <Arduino.h>
#include <SSD1327.h>
#include <Wire.h>
#include <unity.h>

TwoWire internalI2C(I2C_INT_SDA, I2C_INT_SCL);
WS_OLED_128X128_I2C display(internalI2C);

void setUp(void) {
    display.begin();
}

void tearDown(void) {}

void test_ssd1327_begin(void) {
    TEST_ASSERT_TRUE(display.begin());
}

void test_ssd1327_fill_does_not_crash(void) {
    display.fill(7);
    display.show();
}

void test_ssd1327_show_does_not_crash(void) {
    display.show();
}

void test_ssd1327_pixel_does_not_crash(void) {
    display.pixel(0, 0, 0xF);
    display.pixel(127, 127, 0xF);
    display.show();
}

void test_ssd1327_fill_rect_does_not_crash(void) {
    display.fillRect(0, 0, 10, 10, 0xF);
    display.show();
}

void test_ssd1327_contrast_does_not_crash(void) {
    display.contrast(0x7F);
    display.contrast(0x00);
}

void test_ssd1327_invert_does_not_crash(void) {
    display.invert(1);
    display.invert(0);
}

void test_ssd1327_rotate_does_not_crash(void) {
    display.rotate(true);
    display.rotate(false);
}

void setup() {
    delay(2000);
    internalI2C.begin();

    UNITY_BEGIN();
    RUN_TEST(test_ssd1327_begin);
    RUN_TEST(test_ssd1327_fill_does_not_crash);
    RUN_TEST(test_ssd1327_show_does_not_crash);
    RUN_TEST(test_ssd1327_pixel_does_not_crash);
    RUN_TEST(test_ssd1327_fill_rect_does_not_crash);
    RUN_TEST(test_ssd1327_contrast_does_not_crash);
    RUN_TEST(test_ssd1327_invert_does_not_crash);
    RUN_TEST(test_ssd1327_rotate_does_not_crash);
    UNITY_END();
}

void loop() {}
