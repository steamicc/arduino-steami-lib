// SPDX-License-Identifier: GPL-3.0-or-later

#include <unity.h>

#include <cctype>
#include <cstdio>
#include <cstring>
#include <vector>

#include "Wire.h"
#include "daplink_flash.h"

constexpr uint8_t ADDR = DAPLINK_BRIDGE_DEFAULT_ADDR;

static void preloadBusy(bool busy = true) {
    Wire.setRegister(ADDR, DAPLINK_BRIDGE_REG_STATUS, busy ? DAPLINK_BRIDGE_STATUS_BUSY : 0x00);
}

DaplinkBridge bridge;
DaplinkFlash flash(bridge);

void setUp() {
    Wire = TwoWire();
    Wire.setRegister(ADDR, DAPLINK_BRIDGE_CMD_WHO_AM_I, DAPLINK_BRIDGE_WHO_AM_I);
    Wire.setRegister(ADDR, DAPLINK_BRIDGE_REG_ERROR, 0x00);
    preloadBusy(false);
    bridge = DaplinkBridge();
    flash = DaplinkFlash(bridge);
}

void tearDown(void) {}

void test_begin_detects_device(void) {
    TEST_ASSERT_TRUE(flash.begin());
}

void test_begin_rejects_wrong_who_am_i(void) {
    Wire.setRegister(ADDR, DAPLINK_BRIDGE_CMD_WHO_AM_I, 0x42);
    TEST_ASSERT_FALSE(flash.begin());
}

void test_set_filename_sends_correct_payload(void) {
    Wire.clearWrites();
    flash.setFilename("TEST", "TXT");
    bool foundCmd = false;
    bool payloadCorrect = false;

    for (const auto& w : Wire.getWrites()) {
        if (w.reg == DAPLINK_FLASH_CMD_SET_FILENAME) {
            foundCmd = true;
            break;
        }
    }

    if (foundCmd) {
        const auto& writes = Wire.getWrites();
        if (writes.size() >= 11) {
            payloadCorrect =
                (writes[0].value == 'T') && (writes[1].value == 'E') && (writes[2].value == 'S') &&
                (writes[3].value == 'T') && (writes[4].value == ' ') && (writes[5].value == ' ') &&
                (writes[6].value == ' ') && (writes[7].value == ' ') && (writes[8].value == 'T') &&
                (writes[9].value == 'X') && (writes[10].value == 'T');
        }
    }

    TEST_ASSERT_TRUE(foundCmd);
    TEST_ASSERT_TRUE(payloadCorrect);
}

void test_get_filename_returns_stripped_name(void) {
    // readResponse(cmd, ...) writes `cmd` as the register pointer, then
    // streams the response back. Stage the padded name+ext via the
    // response queue rather than the register map so the data survives
    // any command-side payload writes.
    std::vector<uint8_t> raw(DAPLINK_FLASH_FILENAME_LEN + DAPLINK_FLASH_EXT_LEN, ' ');
    memcpy(raw.data(), "MYFILE", 6);
    memcpy(raw.data() + DAPLINK_FLASH_FILENAME_LEN, "BIN", 3);
    Wire.setResponse(ADDR, DAPLINK_FLASH_CMD_GET_FILENAME, raw);

    DaplinkFlash::FilenameResult result = flash.getFilename();
    TEST_ASSERT_EQUAL_STRING("MYFILE", result.name);
    TEST_ASSERT_EQUAL_STRING("BIN", result.ext);
}

void test_clear_flash_sends_cmd(void) {
    Wire.clearWrites();
    Wire.clearCommands();
    bool ok = flash.clearFlash();

    bool sawCmd = false;
    for (const auto& cmd : Wire.getCommands()) {
        if (cmd.cmd == DAPLINK_FLASH_CMD_CLEAR_FLASH) {
            sawCmd = true;
            break;
        }
    }

    TEST_ASSERT_TRUE(sawCmd);
    TEST_ASSERT_TRUE_MESSAGE(ok, "clearFlash() should return true on a clean bridge response");
}

void test_clear_flash_returns_false_on_device_error(void) {
    Wire.setRegister(ADDR, DAPLINK_BRIDGE_REG_ERROR, DAPLINK_BRIDGE_ERROR_CMD_FAILED);

    TEST_ASSERT_FALSE(flash.clearFlash());
}

void test_write_sends_data(void) {
    const char* data = "Hello";
    Wire.clearWrites();
    flash.write(data);
    bool sawWrite = false;
    for (const auto& w : Wire.getWrites()) {
        if (w.reg == DAPLINK_FLASH_CMD_WRITE_DATA) {
            sawWrite = true;
            break;
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
    uint8_t buf[DAPLINK_FLASH_SECTOR_SIZE];
    flash.readSector(0x1234, buf);

    const auto& writes = Wire.getWrites();
    bool foundAndCorrect = false;

    for (size_t i = 0; i < writes.size(); ++i) {
        if (writes[i].reg == DAPLINK_FLASH_CMD_READ_SECTOR) {
            if (i + 1 < writes.size()) {
                uint8_t sectorHi = writes[i].value;
                uint8_t sectorLo = writes[i + 1].value;
                if (sectorHi == 0x12 && sectorLo == 0x34) {
                    foundAndCorrect = true;
                }
            }
            break;
        }
    }

    TEST_ASSERT_TRUE(foundAndCorrect);
}

void test_read_stops_at_sentinel(void) {
    // readSector(...) does sendCommand(CMD_READ_SECTOR, payload) then
    // readResponse(CMD_READ_SECTOR, ...). Stage the sector payload via
    // the response queue so the sentinel byte survives the
    // sendCommand-side payload writes targeting the same cmd offsets.
    std::vector<uint8_t> data(DAPLINK_FLASH_SECTOR_SIZE, 'A');
    data[10] = 0xFF;
    Wire.setResponse(ADDR, DAPLINK_FLASH_CMD_READ_SECTOR, data);

    Wire.setRegister(ADDR, DAPLINK_BRIDGE_REG_ERROR, 0x00);

    uint8_t result[20];
    size_t len = flash.readUntilSentinel(result, sizeof(result));
    TEST_ASSERT_EQUAL(10, len);
    for (size_t i = 0; i < len; i++) {
        TEST_ASSERT_EQUAL('A', result[i]);
    }
}

void test_read_limited_by_maxlen(void) {
    std::vector<uint8_t> data(DAPLINK_FLASH_SECTOR_SIZE, 'B');
    Wire.setResponse(ADDR, DAPLINK_FLASH_CMD_READ_SECTOR, data);

    Wire.setRegister(ADDR, DAPLINK_BRIDGE_REG_ERROR, 0x00);

    uint8_t result[20];
    size_t len = flash.readUntilSentinel(result, sizeof(result));
    TEST_ASSERT_EQUAL(sizeof(result), len);
    for (size_t i = 0; i < len; i++) {
        TEST_ASSERT_EQUAL('B', result[i]);
    }
}

void test_write_returns_zero_on_error(void) {
    Wire.setRegister(ADDR, DAPLINK_BRIDGE_REG_ERROR, DAPLINK_BRIDGE_ERROR_CMD_FAILED);
    size_t len = flash.write("data");
    TEST_ASSERT_EQUAL(0, len);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_begin_detects_device);
    RUN_TEST(test_begin_rejects_wrong_who_am_i);
    RUN_TEST(test_set_filename_sends_correct_payload);
    RUN_TEST(test_get_filename_returns_stripped_name);
    RUN_TEST(test_clear_flash_sends_cmd);
    RUN_TEST(test_clear_flash_returns_false_on_device_error);
    RUN_TEST(test_write_sends_data);
    RUN_TEST(test_write_returns_length);
    RUN_TEST(test_write_line_appends_newline);
    RUN_TEST(test_read_sector_sends_correct_command);
    RUN_TEST(test_read_stops_at_sentinel);
    RUN_TEST(test_read_limited_by_maxlen);
    RUN_TEST(test_write_returns_zero_on_error);
    return UNITY_END();
}
