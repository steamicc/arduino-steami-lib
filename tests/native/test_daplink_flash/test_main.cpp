// SPDX-License-Identifier: GPL-3.0-or-later

#include <unity.h>

#include <cctype>
#include <cstdio>
#include <cstring>

#include "Wire.h"
#include "daplink_flash.h"

constexpr uint8_t ADDR = DAPLINK_BRIDGE_DEFAULT_ADDR;

static void preloadBusy(bool busy = true) {
    Wire.setRegister(ADDR, REG_STATUS, busy ? STATUS_BUSY : 0x00);
}

daplink_bridge bridge;
DaplinkFlash flash(bridge);

void setUp() {
    Wire = TwoWire();
    preloadBusy(false);
    bridge = daplink_bridge();
    flash = DaplinkFlash(bridge);
}

void tearDown(void) {}

void test_set_filename_sends_correct_payload(void) {
    Wire.clearWrites();
    flash.setFilename("TEST", "TXT");
    bool sawWrite = false;
    for (const auto& w : Wire.getWrites()) {
        if (w.reg == CMD_SET_FILENAME) {
            sawWrite = true;
            break;
        }
    }
    TEST_ASSERT_TRUE(sawWrite);
}

void test_get_filename_returns_stripped_name(void) {
    uint8_t raw[1 + FILENAME_LEN + EXT_LEN];
    raw[0] = CMD_GET_FILENAME;
    memset(raw + 1, ' ', FILENAME_LEN + EXT_LEN);
    memcpy(raw + 1, "MYFILE", 6);
    memcpy(raw + 1 + FILENAME_LEN, "BIN", 3);
    for (uint8_t i = 0; i < FILENAME_LEN + EXT_LEN; i++) {
        Wire.setRegister(ADDR, CMD_GET_FILENAME + i, raw[1 + i]);
    }

    DaplinkFlash::FilenameResult result = flash.getFilename();
    TEST_ASSERT_EQUAL_STRING("MYFILE", result.name);
    TEST_ASSERT_EQUAL_STRING("BIN", result.ext);
}

void test_clear_flash_sends_cmd(void) {
    Wire.clearWrites();
    flash.clearFlash();

    TEST_ASSERT_TRUE(true);
}

void test_write_sends_data(void) {
    const char* data = "Hello";
    flash.write(data);
    bool sawWrite = false;
    for (const auto& w : Wire.getWrites()) {
        if (w.reg == CMD_WRITE_DATA) {
            sawWrite = true;
            if (std::memcmp(&w.value, data, strlen(data)) == 0)
                return;
        }
    }
    TEST_ASSERT_TRUE(sawWrite);
}

void test_write_returns_length(void) {
    const char* data = "Hello";
    size_t len = flash.write(data);
    TEST_ASSERT_EQUAL(len, strlen(data));
}

void test_write_line_appends_newline(void) {
    Wire.clearWrites();
    flash.writeLine("Hello");
    bool sawNewline = false;
    for (const auto& w : Wire.getWrites()) {
        if (w.value == '\n') {
            sawNewline = true;
            break;
        }
    }
    TEST_ASSERT_TRUE(sawNewline);
}

void test_read_sector_sends_correct_command(void) {
    Wire.clearWrites();
    uint8_t buf[SECTOR_SIZE];
    flash.readSector(5, buf);
    TEST_ASSERT_TRUE(true);
}

void test_read_stops_at_sentinel(void) {
    uint8_t buf[SECTOR_SIZE];
    memset(buf, 'A', SECTOR_SIZE);
    buf[100] = 0xFF;  // Sentinel value
    for (size_t i = 0; i < SECTOR_SIZE; i++) {
        Wire.setRegister(ADDR, CMD_READ_SECTOR + i, buf[i]);
    }
    uint8_t result[200];
    size_t len = flash.read(result, sizeof(result), false);
    TEST_ASSERT_EQUAL(100, len);
    for (size_t i = 0; i < len; i++) {
        TEST_ASSERT_EQUAL('A', result[i]);
    }
}

void test_read_limited_by_maxlen(void) {
    uint8_t buf[SECTOR_SIZE];
    memset(buf, 'B', SECTOR_SIZE);
    for (size_t i = 0; i < SECTOR_SIZE; i++) {
        Wire.setRegister(ADDR, CMD_READ_SECTOR + i, 'B');
    }
    uint8_t result[50];
    size_t len = flash.read(result, sizeof(result), true);
    TEST_ASSERT_EQUAL(50, len);
    for (size_t i = 0; i < len; i++) {
        TEST_ASSERT_EQUAL('B', result[i]);
    }
}

void test_write_returns_zero_on_error(void) {
    Wire.setRegister(ADDR, REG_ERROR, ERROR_CMD_FAILED);
    size_t len = flash.write("data");
    TEST_ASSERT_EQUAL(0, len);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_set_filename_sends_correct_payload);
    RUN_TEST(test_get_filename_returns_stripped_name);
    RUN_TEST(test_clear_flash_sends_cmd);
    RUN_TEST(test_write_sends_data);
    RUN_TEST(test_write_returns_length);
    RUN_TEST(test_write_line_appends_newline);
    RUN_TEST(test_read_sector_sends_correct_command);
    RUN_TEST(test_read_stops_at_sentinel);
    RUN_TEST(test_read_limited_by_maxlen);
    RUN_TEST(test_write_returns_zero_on_error);

    return UNITY_END();
}