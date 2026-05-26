// SPDX-License-Identifier: GPL-3.0-or-later

/*
 * Integration validation for DaplinkFlash on real STeaMi silicon.
 *
 * **DESTRUCTIVE** — clearFlash() wipes the entire partition. Run this
 * suite only when there's nothing to keep on the DAPLink flash.
 *
 * Exercises the full life-cycle on real hardware:
 *  - bridge + flash bring-up,
 *  - clearFlash() actually returns true on a healthy bridge,
 *  - setFilename() roundtrips through getFilename(),
 *  - writeLine() lands bytes that come back via readUntilSentinel(),
 *  - readSector() reads back a known byte pattern.
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

void test_daplink_flash_full_cycle() {
    TEST_ASSERT_TRUE_MESSAGE(flash.begin(), "flash.begin() must succeed");

    TEST_ASSERT_TRUE_MESSAGE(flash.clearFlash(),
                             "clearFlash() must propagate a true on a healthy bridge");

    // Set a known 8.3 filename and roundtrip it back through the bridge.
    flash.setFilename("STEATEST", "TXT");

    auto filename = flash.getFilename();
    TEST_ASSERT_EQUAL_STRING_MESSAGE("STEATEST", filename.name,
                                     "filename read-back should match what was written");
    TEST_ASSERT_EQUAL_STRING_MESSAGE("TXT", filename.ext,
                                     "extension read-back should match what was written");

    // Write a deterministic line, then read it back through the
    // sentinel-terminated path. Expect the line + a trailing newline.
    const char* payload = "STeaMi DaplinkFlash hardware integration smoke";
    size_t written = flash.writeLine(payload);
    TEST_ASSERT_EQUAL_MESSAGE(strlen(payload) + 1, written,
                              "writeLine() must report payload + '\\n' bytes written");

    uint8_t readback[128];
    memset(readback, 0, sizeof(readback));
    size_t readLen = flash.readUntilSentinel(readback, sizeof(readback));
    TEST_ASSERT_EQUAL_MESSAGE(strlen(payload) + 1, readLen,
                              "readUntilSentinel() length should match what writeLine wrote");

    TEST_ASSERT_EQUAL_MEMORY_MESSAGE(payload, readback, strlen(payload),
                                     "readUntilSentinel payload bytes should match");
    TEST_ASSERT_EQUAL_MESSAGE('\n', readback[strlen(payload)],
                              "readUntilSentinel should preserve the trailing newline");
}

void test_daplink_flash_read_sector_returns_known_pattern() {
    TEST_ASSERT_TRUE(flash.begin());
    TEST_ASSERT_TRUE(flash.clearFlash());

    flash.setFilename("PATTERN", "BIN");

    // Push 16 bytes of a known pattern (0xA5).
    uint8_t pattern[16];
    memset(pattern, 0xA5, sizeof(pattern));
    size_t written = flash.write(pattern, sizeof(pattern));
    TEST_ASSERT_EQUAL(sizeof(pattern), written);

    uint8_t sector[DAPLINK_FLASH_SECTOR_SIZE];
    TEST_ASSERT_TRUE_MESSAGE(flash.readSector(0, sector),
                             "readSector(0) must succeed after a write");

    for (size_t i = 0; i < sizeof(pattern); ++i) {
        TEST_ASSERT_EQUAL_MESSAGE(0xA5, sector[i], "sector byte should match the written pattern");
    }
    // Bytes past the written length should be the unused-flash sentinel
    // (0xFF), confirming the partition was actually cleared first.
    TEST_ASSERT_EQUAL_MESSAGE(0xFF, sector[sizeof(pattern)],
                              "tail of the sector should be the 0xFF sentinel");
}

void setup() {
    delay(2000);
    internalI2C.begin();

    UNITY_BEGIN();
    RUN_TEST(test_daplink_flash_full_cycle);
    RUN_TEST(test_daplink_flash_read_sector_returns_known_pattern);
    UNITY_END();
}

void loop() {}
