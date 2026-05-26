// SPDX-License-Identifier: GPL-3.0-or-later

/*
 * Hardware unit validation for DaplinkFlash on real STeaMi silicon.
 *
 * Non-destructive checks only: this suite probes the driver / bridge
 * round-trip without touching flash data. The full destructive cycle
 * (clearFlash + setFilename + writeLine + readUntilSentinel) lives in
 * tests/integration/test_daplink_flash/ — running it wipes the
 * partition.
 */

#include <Arduino.h>
#include <DaplinkBridge.h>
#include <Wire.h>
#include <string.h>
#include <unity.h>

#include "daplink_flash.h"

TwoWire internalI2C(I2C_INT_SDA, I2C_INT_SCL);
DaplinkBridge bridge(internalI2C);
DaplinkFlash flash(bridge);

void setUp(void) {
    // begin() on the flash driver re-probes the bridge, so we can call
    // it before every test without any extra plumbing.
    flash.begin();
}

void tearDown(void) {}

void test_daplink_flash_begin() {
    TEST_ASSERT_TRUE_MESSAGE(flash.begin(), "flash.begin() must succeed on a wired STeaMi");
}

void test_daplink_flash_get_filename_returns_printable_chars() {
    // Read the current filename. Whatever it contains, every byte that
    // survives the trailing-space trim should be a printable ASCII
    // character — non-printable garbage would mean the read framing or
    // the readResponse byte count is broken.
    auto current = flash.getFilename();

    for (size_t i = 0; i < strlen(current.name); ++i) {
        unsigned char c = static_cast<unsigned char>(current.name[i]);
        TEST_ASSERT_TRUE_MESSAGE(c >= 0x20 && c < 0x7F, "filename contains non-printable byte");
    }
    for (size_t i = 0; i < strlen(current.ext); ++i) {
        unsigned char c = static_cast<unsigned char>(current.ext[i]);
        TEST_ASSERT_TRUE_MESSAGE(c >= 0x20 && c < 0x7F, "extension contains non-printable byte");
    }
}

void setup() {
    delay(2000);
    internalI2C.begin();

    UNITY_BEGIN();
    RUN_TEST(test_daplink_flash_begin);
    RUN_TEST(test_daplink_flash_get_filename_returns_printable_chars);
    UNITY_END();
}

void loop() {}
