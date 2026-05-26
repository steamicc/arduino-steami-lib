// SPDX-License-Identifier: GPL-3.0-or-later

/*
 * Hardware unit validation for the MCP23009E I/O expander on real
 * STeaMi silicon.
 *
 * The plausibility checks are non-destructive: each test re-resets the
 * expander and exercises a single register round-trip or a known idle
 * read. The D-PAD pull-up tests assume nobody is pressing the buttons
 * during the run — if they ever flap, set the board on a flat surface
 * before running `make test-hardware`.
 */

#include <Arduino.h>
#include <MCP23009E.h>
#include <Wire.h>
#include <unity.h>

TwoWire internalI2C(I2C_INT_SDA, I2C_INT_SCL);
MCP23009E expander(internalI2C, RST_EXPANDER, MCP23009_I2C_ADDR, INT_EXPANDER);

void setUp(void) {
    expander.begin();
}

void tearDown(void) {}

void test_mcp23009e_begin_succeeds() {
    TEST_ASSERT_TRUE(expander.begin());
}

// Write a known direction mask, read it back. Catches any I2C
// transport regression on the GPIO register family.
void test_mcp23009e_iodir_roundtrip() {
    expander.setIodir(0xA5);
    TEST_ASSERT_EQUAL_HEX8(0xA5, expander.getIodir());
    expander.setIodir(0x5A);
    TEST_ASSERT_EQUAL_HEX8(0x5A, expander.getIodir());
}

// Pull-up register round-trip. Confirms the chip honours pull-up
// configuration writes, which the D-PAD relies on.
void test_mcp23009e_gppu_roundtrip() {
    expander.setGppu(0xF0);
    TEST_ASSERT_EQUAL_HEX8(0xF0, expander.getGppu());
    expander.setGppu(0x0F);
    TEST_ASSERT_EQUAL_HEX8(0x0F, expander.getGppu());
}

// With the four D-PAD bits configured as input with pull-ups, an idle
// board (no button pressed) must read each of them HIGH. Validates
// both the input pull-up path and the GPIO register decoding.
void test_mcp23009e_dpad_idle_reads_high() {
    expander.setup(MCP23009_BTN_UP, MCP23009_DIR_INPUT, MCP23009_PULLUP);
    expander.setup(MCP23009_BTN_DOWN, MCP23009_DIR_INPUT, MCP23009_PULLUP);
    expander.setup(MCP23009_BTN_LEFT, MCP23009_DIR_INPUT, MCP23009_PULLUP);
    expander.setup(MCP23009_BTN_RIGHT, MCP23009_DIR_INPUT, MCP23009_PULLUP);

    TEST_ASSERT_EQUAL(MCP23009_LOGIC_HIGH, expander.getLevel(MCP23009_BTN_UP));
    TEST_ASSERT_EQUAL(MCP23009_LOGIC_HIGH, expander.getLevel(MCP23009_BTN_DOWN));
    TEST_ASSERT_EQUAL(MCP23009_LOGIC_HIGH, expander.getLevel(MCP23009_BTN_LEFT));
    TEST_ASSERT_EQUAL(MCP23009_LOGIC_HIGH, expander.getLevel(MCP23009_BTN_RIGHT));
}

// Output latch round-trip while the corresponding pins are configured
// as outputs. Confirms setLevel actually drives the silicon and the
// readback matches what we wrote.
void test_mcp23009e_set_level_roundtrip_on_outputs() {
    expander.setup(MCP23009_GPIO1, MCP23009_DIR_OUTPUT);
    expander.setup(MCP23009_GPIO2, MCP23009_DIR_OUTPUT);

    expander.setLevel(MCP23009_GPIO1, MCP23009_LOGIC_HIGH);
    expander.setLevel(MCP23009_GPIO2, MCP23009_LOGIC_LOW);
    TEST_ASSERT_EQUAL(MCP23009_LOGIC_HIGH, expander.getLevel(MCP23009_GPIO1));
    TEST_ASSERT_EQUAL(MCP23009_LOGIC_LOW, expander.getLevel(MCP23009_GPIO2));

    expander.setLevel(MCP23009_GPIO1, MCP23009_LOGIC_LOW);
    expander.setLevel(MCP23009_GPIO2, MCP23009_LOGIC_HIGH);
    TEST_ASSERT_EQUAL(MCP23009_LOGIC_LOW, expander.getLevel(MCP23009_GPIO1));
    TEST_ASSERT_EQUAL(MCP23009_LOGIC_HIGH, expander.getLevel(MCP23009_GPIO2));
}

void setup() {
    delay(2000);
    internalI2C.begin();

    UNITY_BEGIN();
    RUN_TEST(test_mcp23009e_begin_succeeds);
    RUN_TEST(test_mcp23009e_iodir_roundtrip);
    RUN_TEST(test_mcp23009e_gppu_roundtrip);
    RUN_TEST(test_mcp23009e_dpad_idle_reads_high);
    RUN_TEST(test_mcp23009e_set_level_roundtrip_on_outputs);
    UNITY_END();
}

void loop() {}
