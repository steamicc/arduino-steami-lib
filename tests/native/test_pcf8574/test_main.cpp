// SPDX-License-Identifier: GPL-3.0-or-later

#include "PCF8574.h"
#include "Wire.h"
#include <unity.h>

constexpr uint8_t ADDR = 0x38;

static void preloadPins(uint8_t reg, uint8_t value) {
    Wire.setRegister(ADDR, reg, value);
}

PCF8574 expander(ADDR, &Wire);

void setUp(void) {
    Wire = TwoWire();
    expander = PCF8574(ADDR, &Wire);
}

void tearDown(void) {}

void test_begin_detects_device(void) {
    TEST_ASSERT_TRUE(expander.begin());
}

void test_begin_rejects_wrong_address(void) {
    Wire.setEndTransmissionResult(2);
    TEST_ASSERT_FALSE(expander.begin());
}

void test_is_connected_returns_true_when_device_present(void) {
    TEST_ASSERT_TRUE(expander.isConnected());
}

void test_is_connected_returns_false_when_device_absent(void) {
    Wire.setEndTransmissionResult(2);
    TEST_ASSERT_FALSE(expander.isConnected());
}

void test_write8_sets_all_pins(void) {
    expander.begin(0xFF);
    expander.write8(0xAB);
    TEST_ASSERT_EQUAL_HEX8(0xAB, expander.valueOut());
}

void test_write_single_pin_high(void) {
    expander.begin(0x00);
    Wire.clearWrites();
    expander.write(3, HIGH);
    TEST_ASSERT_EQUAL_HEX8(0x08, expander.valueOut());
}

void test_write_single_pin_low(void) {
    expander.begin(0xFF);
    Wire.clearWrites();
    expander.write(3, LOW);
    TEST_ASSERT_EQUAL_HEX8(0xF7, expander.valueOut());
}

void test_write_preserves_other_pins(void) {
    expander.begin(0b10101010);
    expander.write(0, HIGH);
    TEST_ASSERT_EQUAL_HEX8(0b10101011, expander.valueOut());
    expander.write(1, LOW);
    TEST_ASSERT_EQUAL_HEX8(0b10101001, expander.valueOut());
}

void test_read8_returns_all_pins(void) {
    expander.begin(0xFF);
    preloadPins(0xFF, 0xC3);
    uint8_t result = expander.read8();
    TEST_ASSERT_EQUAL_HEX8(0xC3, result);
    TEST_ASSERT_EQUAL_HEX8(0xC3, expander.value());
}

void test_read_single_pin_high(void) {
    expander.begin(0xFF);
    preloadPins(0xFF, 0b00001000);
    TEST_ASSERT_EQUAL_UINT8(1, expander.read(3));
}

void test_read_single_pin_low(void) {
    expander.begin();
    preloadPins(0xFF, 0b11110111);
    TEST_ASSERT_EQUAL_UINT8(0, expander.read(3));
}

void test_write_pin_out_of_range_sets_error(void) {
    expander.begin();
    expander.write(8, HIGH);
    TEST_ASSERT_EQUAL_INT(PCF8574_PIN_ERROR, expander.lastError());
}

void test_read_pin_out_of_range_sets_error(void) {
    expander.begin();
    expander.read(8);
    TEST_ASSERT_EQUAL_INT(PCF8574_PIN_ERROR, expander.lastError());
}

void test_last_error_resets_after_read(void) {
    expander.begin();
    expander.write(8, HIGH);
    expander.lastError();
    TEST_ASSERT_EQUAL_INT(PCF8574_OK, expander.lastError());
}

void test_toggle_inverts_pin(void) {
    expander.begin(0x00);
    expander.toggle(4);
    TEST_ASSERT_EQUAL_HEX8(0x10, expander.valueOut());
    expander.toggle(4);
    TEST_ASSERT_EQUAL_HEX8(0x00, expander.valueOut());
}

void test_toggle_mask_inverts_selected_pins(void) {
    expander.begin(0b00001111);
    expander.toggleMask(0b11111111);
    TEST_ASSERT_EQUAL_HEX8(0b11110000, expander.valueOut());
}

void test_shift_right(void) {
    expander.begin(0b10000000);
    expander.shiftRight(1);
    TEST_ASSERT_EQUAL_HEX8(0b01000000, expander.valueOut());
}

void test_shift_left(void) {
    expander.begin(0b00000001);
    expander.shiftLeft(1);
    TEST_ASSERT_EQUAL_HEX8(0b00000010, expander.valueOut());
}

void test_shift_right_by_8_clears_all(void) {
    expander.begin(0xFF);
    expander.shiftRight(8);
    TEST_ASSERT_EQUAL_HEX8(0x00, expander.valueOut());
}

void test_shift_left_by_8_clears_all(void) {
    expander.begin(0xFF);
    expander.shiftLeft(8);
    TEST_ASSERT_EQUAL_HEX8(0x00, expander.valueOut());
}

void test_rotate_right(void) {
    expander.begin(0b00000001);
    expander.rotateRight(1);
    TEST_ASSERT_EQUAL_HEX8(0b10000000, expander.valueOut());
}

void test_rotate_left(void) {
    expander.begin(0b10000000);
    expander.rotateLeft(1);
    TEST_ASSERT_EQUAL_HEX8(0b00000001, expander.valueOut());
}

void test_select_sets_only_one_pin_high(void) {
    expander.begin();
    expander.select(2);
    TEST_ASSERT_EQUAL_HEX8(0b00000100, expander.valueOut());
}

void test_select_none_clears_all_pins(void) {
    expander.begin(0xFF);
    expander.selectNone();
    TEST_ASSERT_EQUAL_HEX8(0x00, expander.valueOut());
}

void test_select_all_sets_all_pins(void) {
    expander.begin(0x00);
    expander.selectAll();
    TEST_ASSERT_EQUAL_HEX8(0xFF, expander.valueOut());
}

void test_select_n_sets_pins_0_to_n(void) {
    expander.begin(0x00);
    expander.selectN(2);  // pins 0, 1, 2 → 0b00000111
    TEST_ASSERT_EQUAL_HEX8(0b00000111, expander.valueOut());
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_begin_detects_device);
    RUN_TEST(test_begin_rejects_wrong_address);
    RUN_TEST(test_is_connected_returns_true_when_device_present);
    RUN_TEST(test_is_connected_returns_false_when_device_absent);
    RUN_TEST(test_write8_sets_all_pins);
    RUN_TEST(test_write_single_pin_high);
    RUN_TEST(test_write_single_pin_low);
    RUN_TEST(test_write_preserves_other_pins);
    RUN_TEST(test_read8_returns_all_pins);
    RUN_TEST(test_read_single_pin_high);
    RUN_TEST(test_read_single_pin_low);
    RUN_TEST(test_write_pin_out_of_range_sets_error);
    RUN_TEST(test_read_pin_out_of_range_sets_error);
    RUN_TEST(test_last_error_resets_after_read);
    RUN_TEST(test_toggle_inverts_pin);
    RUN_TEST(test_toggle_mask_inverts_selected_pins);
    RUN_TEST(test_shift_right);
    RUN_TEST(test_shift_left);
    RUN_TEST(test_shift_right_by_8_clears_all);
    RUN_TEST(test_shift_left_by_8_clears_all);
    RUN_TEST(test_rotate_right);
    RUN_TEST(test_rotate_left);
    RUN_TEST(test_select_sets_only_one_pin_high);
    RUN_TEST(test_select_none_clears_all_pins);
    RUN_TEST(test_select_all_sets_all_pins);
    RUN_TEST(test_select_n_sets_pins_0_to_n);
    return UNITY_END();
}