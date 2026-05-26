// SPDX-License-Identifier: GPL-3.0-or-later

#include <unity.h>

#include <cstring>

#include "DaplinkBridge.h"
#include "Wire.h"
#include "driver_checks.h"

constexpr uint8_t ADDR = DAPLINK_BRIDGE_DEFAULT_ADDR;

static void preloadDeviceId(bool valid = true) {
    Wire.setRegister(ADDR, DAPLINK_BRIDGE_CMD_WHO_AM_I, valid ? DAPLINK_BRIDGE_WHO_AM_I : 0x42);
}

static void preloadStatus(bool busy) {
    Wire.setRegister(ADDR, DAPLINK_BRIDGE_REG_STATUS, busy ? DAPLINK_BRIDGE_STATUS_BUSY : 0x00);
}

static void preloadError(uint8_t value) {
    Wire.setRegister(ADDR, DAPLINK_BRIDGE_REG_ERROR, value);
}

DaplinkBridge bridge;

void setUp(void) {
    Wire = TwoWire();
    preloadDeviceId();
    preloadStatus(false);
    preloadError(0);
    bridge = DaplinkBridge();
}

void tearDown(void) {}

void test_begin_detects_device(void) {
    check_begin(bridge);
}

void test_begin_rejects_wrong_who_am_i(void) {
    preloadDeviceId(false);
    TEST_ASSERT_FALSE(bridge.begin());
}

void test_device_id_returns_who_am_i(void) {
    check_who_am_i(bridge, DAPLINK_BRIDGE_WHO_AM_I);
}

void test_busy_reflects_status_register(void) {
    preloadStatus(true);
    TEST_ASSERT_TRUE(bridge.busy());

    preloadStatus(false);
    TEST_ASSERT_FALSE(bridge.busy());
}

void test_clear_config_returns_true_on_no_error(void) {
    // The Wire mock only records writes whose transmission carries a
    // payload byte (reg + value). A single-byte command write (just
    // `[CMD_CLEAR_CONFIG]`) leaves no entry in getWrites(), so we
    // exercise the negative path (test below) to prove the driver
    // really sends the command and reads back the error register.
    bridge.begin();
    TEST_ASSERT_TRUE(bridge.clearConfig());
}

void test_clear_config_returns_false_on_device_error(void) {
    bridge.begin();
    preloadError(DAPLINK_BRIDGE_ERROR_CMD_FAILED);
    TEST_ASSERT_FALSE(bridge.clearConfig());
}

void test_write_config_rejects_offset_at_or_past_end(void) {
    bridge.begin();
    const uint8_t payload[1] = {0x42};
    TEST_ASSERT_FALSE(bridge.writeConfig(payload, 1, DAPLINK_BRIDGE_CONFIG_SIZE));
}

void test_write_config_rejects_payload_overflowing_zone(void) {
    bridge.begin();
    const uint8_t payload[2] = {0x01, 0x02};
    // Last valid offset is CONFIG_SIZE - 1; writing 2 bytes there overflows.
    TEST_ASSERT_FALSE(
        bridge.writeConfig(payload, 2, static_cast<uint16_t>(DAPLINK_BRIDGE_CONFIG_SIZE - 1)));
}

void test_write_config_writes_framed_payload(void) {
    bridge.begin();
    Wire.clearWrites();

    const char* payload = "hello";
    TEST_ASSERT_TRUE(bridge.writeConfig(payload, 0));

    // The frame is `[CMD_WRITE_CONFIG, off_hi, off_lo, len, h, e, l, l, o]`.
    // The Wire mock treats the first byte as the register address and
    // records one WriteOp per subsequent byte (so 8 entries for a 9-byte
    // frame).
    const auto& writes = Wire.getWrites();
    TEST_ASSERT_EQUAL_INT(8, writes.size());
    TEST_ASSERT_EQUAL_HEX8(0x00, writes[0].value);  // offset hi
    TEST_ASSERT_EQUAL_HEX8(0x00, writes[1].value);  // offset lo
    TEST_ASSERT_EQUAL_HEX8(0x05, writes[2].value);  // chunk length
    TEST_ASSERT_EQUAL_HEX8('h', writes[3].value);
    TEST_ASSERT_EQUAL_HEX8('e', writes[4].value);
    TEST_ASSERT_EQUAL_HEX8('l', writes[5].value);
    TEST_ASSERT_EQUAL_HEX8('l', writes[6].value);
    TEST_ASSERT_EQUAL_HEX8('o', writes[7].value);
}

void test_write_config_chunked_when_payload_exceeds_max_chunk(void) {
    bridge.begin();
    Wire.clearWrites();

    constexpr size_t kPayloadLen = DAPLINK_BRIDGE_MAX_WRITE_CHUNK + 5;
    uint8_t payload[kPayloadLen];
    for (size_t i = 0; i < kPayloadLen; ++i) {
        payload[i] = static_cast<uint8_t>(i & 0xFF);
    }
    TEST_ASSERT_TRUE(bridge.writeConfig(payload, kPayloadLen, 0));

    // Two frames: one MAX_WRITE_CHUNK long, one 5 bytes long.
    int chunkLengthsSeen = 0;
    for (const auto& w : Wire.getWrites()) {
        // The third byte of each frame (offset 0, 1, 2, 3, ...) at register
        // CMD_WRITE_CONFIG + 3 carries the chunk length. Two frames means
        // the mock writes that "register" twice across the whole call.
        if (w.reg == DAPLINK_BRIDGE_CMD_WRITE_CONFIG + 3) {
            ++chunkLengthsSeen;
        }
    }
    TEST_ASSERT_EQUAL_INT(2, chunkLengthsSeen);
}

void test_write_config_returns_false_on_device_error(void) {
    bridge.begin();
    preloadError(DAPLINK_BRIDGE_ERROR_CMD_FAILED);
    TEST_ASSERT_FALSE(bridge.writeConfig("x", 0));
}

void test_read_config_returns_data_until_first_0xff(void) {
    bridge.begin();

    // The Wire mock auto-increments the register pointer on requestFrom,
    // so loading consecutive registers under CMD_READ_CONFIG mimics a
    // bridge that streams 5 payload bytes then 0xFF as end-of-data.
    const char* expected = "hello";
    for (size_t i = 0; i < 5; ++i) {
        Wire.setRegister(ADDR, DAPLINK_BRIDGE_CMD_READ_CONFIG + i,
                         static_cast<uint8_t>(expected[i]));
    }
    Wire.setRegister(ADDR, DAPLINK_BRIDGE_CMD_READ_CONFIG + 5, 0xFF);

    uint8_t buf[32] = {0};
    size_t len = bridge.readConfig(buf, sizeof(buf));
    TEST_ASSERT_EQUAL_INT(5, len);
    TEST_ASSERT_EQUAL_STRING_LEN(expected, reinterpret_cast<char*>(buf), 5);
}

void test_read_config_returns_zero_on_immediate_0xff(void) {
    bridge.begin();
    Wire.setRegister(ADDR, DAPLINK_BRIDGE_CMD_READ_CONFIG, 0xFF);

    uint8_t buf[32] = {0};
    TEST_ASSERT_EQUAL_INT(0, bridge.readConfig(buf, sizeof(buf)));
}

void test_read_config_truncates_to_max_len(void) {
    bridge.begin();
    // Fill enough registers that readConfig would otherwise keep reading.
    for (size_t i = 0; i < 10; ++i) {
        Wire.setRegister(ADDR, DAPLINK_BRIDGE_CMD_READ_CONFIG + i, 'a');
    }
    // No 0xFF sentinel: readConfig must stop at maxLen.
    uint8_t buf[3] = {0};
    TEST_ASSERT_EQUAL_INT(3, bridge.readConfig(buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_HEX8('a', buf[0]);
    TEST_ASSERT_EQUAL_HEX8('a', buf[1]);
    TEST_ASSERT_EQUAL_HEX8('a', buf[2]);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_begin_detects_device);
    RUN_TEST(test_begin_rejects_wrong_who_am_i);
    RUN_TEST(test_device_id_returns_who_am_i);
    RUN_TEST(test_busy_reflects_status_register);
    RUN_TEST(test_clear_config_returns_true_on_no_error);
    RUN_TEST(test_clear_config_returns_false_on_device_error);
    RUN_TEST(test_write_config_rejects_offset_at_or_past_end);
    RUN_TEST(test_write_config_rejects_payload_overflowing_zone);
    RUN_TEST(test_write_config_writes_framed_payload);
    RUN_TEST(test_write_config_chunked_when_payload_exceeds_max_chunk);
    RUN_TEST(test_write_config_returns_false_on_device_error);
    RUN_TEST(test_read_config_returns_data_until_first_0xff);
    RUN_TEST(test_read_config_returns_zero_on_immediate_0xff);
    RUN_TEST(test_read_config_truncates_to_max_len);
    return UNITY_END();
}
