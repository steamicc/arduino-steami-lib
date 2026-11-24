// SPDX-License-Identifier: GPL-3.0-or-later

/*
 * Hardware unit tests for SteamiDaplinkFlash on a real STeaMi.
 *
 * These tests rename, write and erase the user file of the DAPLink
 * external flash. The file (up to 4 KB) and its name are saved before the
 * first test and written back after the last one. A larger file is left
 * alone and the suite fails at once: copy it from the STeaMi drive and
 * erase it first.
 *
 * Reading needs a DAPLink firmware from 2026-03 or later (READ_SECTOR with
 * a 2-byte sector number).
 */

#include <Arduino.h>
#include <SteamiDaplinkBridge.h>
#include <SteamiDaplinkFlash.h>
#include <Wire.h>
#include <string.h>
#include <unity.h>

TwoWire internalI2C(I2C_INT_SDA, I2C_INT_SCL);
SteamiDaplinkBridge bridge(internalI2C);
SteamiDaplinkFlash flash(bridge);

static constexpr size_t kBackupSize = 4096;
static uint8_t savedFile[kBackupSize + 1];
static size_t savedLength = 0;
static SteamiDaplinkFlash::Filename savedName;
static bool hadName = false;

static uint8_t buf[1024];

// Printable text of `length` bytes, never 0xFF.
static void fillPattern(char* out, size_t length) {
    for (size_t i = 0; i < length; ++i) {
        out[i] = static_cast<char>('A' + i % 26);
    }
    out[length] = '\0';
}

void setUp(void) {}

void tearDown(void) {}

void test_daplink_flash_begin(void) {
    TEST_ASSERT_TRUE(flash.begin());
}

void test_daplink_flash_clear_empties_file(void) {
    TEST_ASSERT_EQUAL_INT(5, flash.write("hello"));
    TEST_ASSERT_TRUE(flash.clearFlash());
    TEST_ASSERT_EQUAL_INT(0, flash.read(buf, sizeof(buf)));
}

void test_daplink_flash_filename_round_trip(void) {
    TEST_ASSERT_TRUE(flash.setFilename("steami", "tst"));

    SteamiDaplinkFlash::Filename name;
    TEST_ASSERT_TRUE(flash.getFilename(name));
    TEST_ASSERT_EQUAL_STRING("STEAMI", name.name);
    TEST_ASSERT_EQUAL_STRING("TST", name.ext);
}

void test_daplink_flash_write_read_across_sectors(void) {
    TEST_ASSERT_TRUE(flash.clearFlash());

    static char text[601];
    fillPattern(text, 600);
    TEST_ASSERT_EQUAL_INT(600, flash.write(text));

    TEST_ASSERT_EQUAL_INT(600, flash.read(buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_MEMORY(text, buf, 600);
}

void test_daplink_flash_read_sector_matches_file(void) {
    TEST_ASSERT_TRUE(flash.clearFlash());
    static char text[301];
    fillPattern(text, 300);
    TEST_ASSERT_EQUAL_INT(300, flash.write(text));

    uint8_t sector[DAPLINK_FLASH_SECTOR_SIZE];
    TEST_ASSERT_TRUE(flash.readSector(1, sector));
    TEST_ASSERT_EQUAL_MEMORY(text + 256, sector, 44);
    TEST_ASSERT_EQUAL_HEX8(0xFF, sector[44]);
}

void test_daplink_flash_write_line_appends(void) {
    TEST_ASSERT_TRUE(flash.clearFlash());
    TEST_ASSERT_EQUAL_INT(4, flash.writeLine("1,2"));
    TEST_ASSERT_EQUAL_INT(4, flash.writeLine("3,4"));

    TEST_ASSERT_EQUAL_INT(8, flash.read(buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_MEMORY("1,2\n3,4\n", buf, 8);
}

void test_daplink_flash_restore_user_file(void) {
    TEST_ASSERT_TRUE(flash.clearFlash());
    if (hadName) {
        TEST_ASSERT_TRUE(flash.setFilename(savedName.name, savedName.ext));
    }
    TEST_ASSERT_EQUAL_INT(savedLength, flash.write(savedFile, savedLength));
}

void test_daplink_flash_user_file_fits_backup(void) {
    TEST_FAIL_MESSAGE(
        "User file larger than 4 KB: copy it from the STeaMi drive and erase it first.");
}

void setup() {
    delay(2000);
    internalI2C.begin();
    flash.begin();

    UNITY_BEGIN();
    hadName = flash.getFilename(savedName);
    savedLength = flash.read(savedFile, sizeof(savedFile));
    if (savedLength > kBackupSize) {
        RUN_TEST(test_daplink_flash_user_file_fits_backup);
        UNITY_END();
        return;
    }

    RUN_TEST(test_daplink_flash_begin);
    RUN_TEST(test_daplink_flash_clear_empties_file);
    RUN_TEST(test_daplink_flash_filename_round_trip);
    RUN_TEST(test_daplink_flash_write_read_across_sectors);
    RUN_TEST(test_daplink_flash_read_sector_matches_file);
    RUN_TEST(test_daplink_flash_write_line_appends);
    RUN_TEST(test_daplink_flash_restore_user_file);
    UNITY_END();
}

void loop() {}
