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
    // getFilename() now actuates via sendCommand() and then streams
    // the result from REG_RESPONSE, so stage the padded name+ext at
    // that register key.
    std::vector<uint8_t> raw(DAPLINK_FLASH_FILENAME_LEN + DAPLINK_FLASH_EXT_LEN, ' ');
    memcpy(raw.data(), "MYFILE", 6);
    memcpy(raw.data() + DAPLINK_FLASH_FILENAME_LEN, "BIN", 3);
    Wire.setResponse(ADDR, DAPLINK_BRIDGE_REG_RESPONSE, raw);

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
    // readSector() now actuates via sendCommand(CMD_READ_SECTOR, ...)
    // then streams the response from REG_RESPONSE, so stage the
    // sector payload at that key.
    std::vector<uint8_t> data(DAPLINK_FLASH_SECTOR_SIZE, 'A');
    data[10] = 0xFF;
    Wire.setResponse(ADDR, DAPLINK_BRIDGE_REG_RESPONSE, data);

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
    Wire.setResponse(ADDR, DAPLINK_BRIDGE_REG_RESPONSE, data);

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

// Regression: e6842dc — write() must return the bytes already in
// flash on a mid-stream chunk failure, not 0. Otherwise the caller
// can't distinguish "nothing written" from "prefix written, retry
// would duplicate" since flash writes are append-only and not atomic.
void test_write_returns_partial_offset_on_midstream_failure(void) {
    // Schedule REG_ERROR = CMD_FAILED to land right after the 2nd
    // multi-byte transaction (= the 2nd chunk's writeFrame). The
    // sendCommand wrapping that 2nd writeFrame then reads non-zero
    // from error() and returns false. The 1st chunk already
    // succeeded with REG_ERROR == 0, so write() must return one
    // chunk's worth, not zero.
    Wire.setRegisterAfterNWrites(2, DAPLINK_BRIDGE_REG_ERROR, DAPLINK_BRIDGE_ERROR_CMD_FAILED);

    uint8_t buf[3 * DAPLINK_FLASH_MAX_WRITE_CHUNK];
    memset(buf, 'X', sizeof(buf));
    size_t written = flash.write(buf, sizeof(buf));

    TEST_ASSERT_EQUAL(DAPLINK_FLASH_MAX_WRITE_CHUNK, written);
}

// Regression: 8b3b27f + e6842dc — readResponse used to recompute
// REG_RESPONSE + produced for every chunk, wrapping at 0xFF
// (0x82 + 0x7D == 0xFF) so multi-chunk reads silently lost data
// past the wrap. The fix emits REG_RESPONSE exactly once and lets
// the firmware cursor advance. Verify a full 256-byte sector
// round-trips byte-for-byte (the mock falls back to registers_
// for any key it doesn't have a queued response for, so a
// regression where the bridge re-selects mid-stream would either
// miss the response queue entirely or skip past it — both surface
// as a mismatch here).
void test_read_sector_streams_full_256_bytes_contiguously(void) {
    std::vector<uint8_t> sector(DAPLINK_FLASH_SECTOR_SIZE);
    for (size_t i = 0; i < sector.size(); ++i) {
        sector[i] = static_cast<uint8_t>(i & 0xFF);
    }
    Wire.setResponse(ADDR, DAPLINK_BRIDGE_REG_RESPONSE, sector);
    Wire.setRegister(ADDR, DAPLINK_BRIDGE_REG_ERROR, 0x00);

    uint8_t out[DAPLINK_FLASH_SECTOR_SIZE];
    TEST_ASSERT_TRUE(flash.readSector(0, out));
    for (size_t i = 0; i < DAPLINK_FLASH_SECTOR_SIZE; ++i) {
        TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(i & 0xFF), out[i]);
    }
}

// Regression: 9c6cc64 — setFilename now returns bool. Null name /
// null ext / bridge error all surface as false instead of being
// silently dropped.
void test_set_filename_rejects_null_name(void) {
    TEST_ASSERT_FALSE(flash.setFilename(nullptr, "TXT"));
}

void test_set_filename_rejects_null_ext(void) {
    TEST_ASSERT_FALSE(flash.setFilename("DATA", nullptr));
}

void test_set_filename_propagates_bridge_error(void) {
    Wire.setRegister(ADDR, DAPLINK_BRIDGE_REG_ERROR, DAPLINK_BRIDGE_ERROR_CMD_FAILED);
    TEST_ASSERT_FALSE(flash.setFilename("DATA", "TXT"));
}

void test_set_filename_returns_true_on_success(void) {
    TEST_ASSERT_TRUE(flash.setFilename("DATA", "TXT"));
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
    RUN_TEST(test_write_returns_partial_offset_on_midstream_failure);
    RUN_TEST(test_read_sector_streams_full_256_bytes_contiguously);
    RUN_TEST(test_set_filename_rejects_null_name);
    RUN_TEST(test_set_filename_rejects_null_ext);
    RUN_TEST(test_set_filename_propagates_bridge_error);
    RUN_TEST(test_set_filename_returns_true_on_success);
    return UNITY_END();
}
