// SPDX-License-Identifier: GPL-3.0-or-later

#include <unity.h>

#include "Wire.h"

TwoWire wire;

void setUp(void) {
    wire = TwoWire();
}

void tearDown(void) {}

void test_read_preloaded_register(void) {
    wire.setRegister(0x5F, 0x0F, 0xBC);

    wire.beginTransmission(0x5F);
    wire.write(0x0F);
    wire.endTransmission(false);

    wire.requestFrom(0x5F, 1);

    TEST_ASSERT_EQUAL(1, wire.available());
    TEST_ASSERT_EQUAL_HEX8(0xBC, wire.read());
}

void test_write_register(void) {
    wire.beginTransmission(0x5F);
    wire.write(0x20);
    wire.write(0x81);
    wire.endTransmission();

    TEST_ASSERT_EQUAL_HEX8(0x81, wire.getRegister(0x5F, 0x20));
}

void test_capture_write_operation(void) {
    wire.beginTransmission(0x5F);
    wire.write(0x10);
    wire.write(0xAA);
    wire.endTransmission();

    const auto& writes = wire.getWrites();

    TEST_ASSERT_EQUAL(1, writes.size());
    TEST_ASSERT_EQUAL_HEX8(0x5F, writes[0].address);
    TEST_ASSERT_EQUAL_HEX8(0x10, writes[0].reg);
    TEST_ASSERT_EQUAL_HEX8(0xAA, writes[0].value);
}

void test_read_multiple_bytes(void) {
    wire.setRegister(0x5F, 0x10, 0x01);
    wire.setRegister(0x5F, 0x11, 0x02);
    wire.setRegister(0x5F, 0x12, 0x03);

    wire.beginTransmission(0x5F);
    wire.write(0x10);
    wire.endTransmission(false);

    wire.requestFrom(0x5F, 3);

    TEST_ASSERT_EQUAL_HEX8(0x01, wire.read());
    TEST_ASSERT_EQUAL_HEX8(0x02, wire.read());
    TEST_ASSERT_EQUAL_HEX8(0x03, wire.read());
}

void test_addresses_are_isolated(void) {
    wire.setRegister(0x5F, 0x0F, 0xAA);
    wire.setRegister(0x6B, 0x0F, 0xBB);

    TEST_ASSERT_EQUAL_HEX8(0xAA, wire.getRegister(0x5F, 0x0F));
    TEST_ASSERT_EQUAL_HEX8(0xBB, wire.getRegister(0x6B, 0x0F));
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_read_preloaded_register);
    RUN_TEST(test_write_register);
    RUN_TEST(test_capture_write_operation);
    RUN_TEST(test_read_multiple_bytes);
    RUN_TEST(test_addresses_are_isolated);

    return UNITY_END();
}
