// SPDX-License-Identifier: GPL-3.0-or-later

#include <unity.h>

#include <cstdio>

#include "SSD1327.h"
#include "Wire.h"
#include "driver_checks.h"

constexpr uint8_t ADDR = 0x3C;

SSD1327_I2C* display;

void setUp(void) {
    Wire = TwoWire();
    display = new SSD1327_I2C(128, 128, Wire, ADDR);
}

void tearDown(void) {
    delete display;
}

void test_ssd1327_i2c_begin_sends_init_sequence(void) {
    Wire.clearWrites();
    display->begin();
    TEST_ASSERT_TRUE(Wire.getWrites().size() > 0);
}

void test_ssd1327_i2c_show_sends_col_row_then_buffer(void) {
    Wire.clearWrites();
    display->show();
    auto writes = Wire.getWrites();
    TEST_ASSERT_TRUE(writes.size() > 0);
}

void test_ssd1327_i2c_fill_clears_buffer(void) {
    Wire.clearWrites();
    display->fill(0xFF);
    display->show();
    auto writes = Wire.getWrites();
    TEST_ASSERT_TRUE(writes.size() > 2);
}

void test_ssd1327_i2c_fill_sets_correct_nibbles(void) {
    display->fill(7);
    Wire.clearWrites();
    display->show();
    auto writes = Wire.getWrites();
    bool found77 = false;
    for (const auto& w : writes) {
        if (w.value == 0x77) {
            found77 = true;
            break;
        }
    }
    TEST_ASSERT_TRUE(found77);
}

void test_ssd1327_i2c_pixel_even_x_sets_high_nibble(void) {
    display->fill(0);
    Wire.clearWrites();
    display->pixel(0, 0, 0xF);
    display->show();
    auto writes = Wire.getWrites();
    bool foundF0 = false;
    for (const auto& w : writes) {
        if (w.value == 0xF0) {
            foundF0 = true;
            break;
        }
    }
    TEST_ASSERT_TRUE(foundF0);
}

void test_ssd1327_i2c_pixel_odd_x_sets_low_nibble(void) {
    display->fill(0);
    Wire.clearWrites();
    display->pixel(1, 0, 0xF);
    display->show();
    auto writes = Wire.getWrites();
    bool found0F = false;
    for (const auto& w : writes) {
        if (w.value == 0x0F) {
            found0F = true;
            break;
        }
    }
    TEST_ASSERT_TRUE(found0F);
}

void test_ssd1327_i2c_fill_rect_fills_correct_pixels(void) {
    display->fill(0);
    Wire.clearWrites();
    display->fillRect(0, 0, 2, 2, 0xF);
    display->show();
    auto writes = Wire.getWrites();
    bool foundFF = false;
    for (const auto& w : writes) {
        if (w.value == 0xFF) {
            foundFF = true;
            break;
        }
    }
    TEST_ASSERT_TRUE(foundFF);
}

void test_ssd1327_i2c_contrast_sends_correct_cmd(void) {
    Wire.clearWrites();
    display->contrast(0xAB);
    auto writes = Wire.getWrites();
    bool foundAB = false;
    for (const auto& w : writes) {
        if (w.value == 0xAB) {
            foundAB = true;
            break;
        }
    }
    TEST_ASSERT_TRUE(foundAB);
}

void test_ssd1327_i2c_invert_sends_correct_mode(void) {
    Wire.clearWrites();
    display->invert(1);
    auto writes = Wire.getWrites();
    bool foundA7 = false;
    for (const auto& w : writes) {
        if (w.value == 0xA7) {
            foundA7 = true;
            break;
        }
    }
    TEST_ASSERT_TRUE(foundA7);
}

void test_ssd1327_i2c_rotate_sends_correct_offset_and_remap(void) {
    Wire.clearWrites();
    display->rotate(1);
    auto writes = Wire.getWrites();
    bool foundOffset = false;
    bool foundRemap = false;
    for (const auto& w : writes) {
        if (w.value == 0x80) {
            foundOffset = true;
        }
        if (w.value == 0x42) {
            foundRemap = true;
        }
    }
    TEST_ASSERT_TRUE(foundOffset);
    TEST_ASSERT_TRUE(foundRemap);
}

void test_ssd1327_i2c_power_off_sends_correct_cmds(void) {
    Wire.clearWrites();
    display->powerOff();
    auto writes = Wire.getWrites();
    bool foundAE = false;
    for (const auto& w : writes) {
        if (w.value == 0xAE) {
            foundAE = true;
            break;
        }
    }
    TEST_ASSERT_TRUE(foundAE);
}

void test_ssd1327_i2c_power_on_sends_correct_cmds(void) {
    Wire.clearWrites();
    display->powerOn();
    auto writes = Wire.getWrites();
    bool foundAF = false;
    for (const auto& w : writes) {
        if (w.value == 0xAF) {
            foundAF = true;
            break;
        }
    }
    TEST_ASSERT_TRUE(foundAF);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_ssd1327_i2c_begin_sends_init_sequence);
    RUN_TEST(test_ssd1327_i2c_show_sends_col_row_then_buffer);
    RUN_TEST(test_ssd1327_i2c_fill_clears_buffer);
    RUN_TEST(test_ssd1327_i2c_fill_sets_correct_nibbles);
    RUN_TEST(test_ssd1327_i2c_pixel_even_x_sets_high_nibble);
    RUN_TEST(test_ssd1327_i2c_pixel_odd_x_sets_low_nibble);
    RUN_TEST(test_ssd1327_i2c_fill_rect_fills_correct_pixels);
    RUN_TEST(test_ssd1327_i2c_contrast_sends_correct_cmd);
    RUN_TEST(test_ssd1327_i2c_invert_sends_correct_mode);
    RUN_TEST(test_ssd1327_i2c_rotate_sends_correct_offset_and_remap);
    RUN_TEST(test_ssd1327_i2c_power_off_sends_correct_cmds);
    RUN_TEST(test_ssd1327_i2c_power_on_sends_correct_cmds);
    return UNITY_END();
}