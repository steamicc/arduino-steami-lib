// SPDX-License-Identifier: GPL-3.0-or-later

#include <unity.h>

#include <cstring>

#include "DaplinkBridge.h"
#include "FakeDaplinkBridge.h"
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

// ---------------------------------------------------------------------
// Config zone protocol, against a model of the bridge firmware
// ---------------------------------------------------------------------

static FakeDaplinkBridge& attachFirmware() {
    static FakeDaplinkBridge firmware;
    firmware = FakeDaplinkBridge();
    Wire.attachDevice(ADDR, &firmware);
    return firmware;
}

static void fillPattern(uint8_t* buf, size_t len) {
    for (size_t i = 0; i < len; ++i) {
        buf[i] = static_cast<uint8_t>('A' + (i % 26));
    }
}

void test_write_config_sends_fixed_32_byte_frames(void) {
    FakeDaplinkBridge& firmware = attachFirmware();
    bridge.begin();

    TEST_ASSERT_TRUE(bridge.writeConfig("hello", 0));

    // The firmware only runs WRITE_CONFIG after 31 argument bytes: a
    // shorter frame would be silently ignored.
    TEST_ASSERT_EQUAL_INT(1, firmware.writeFrameLengths.size());
    TEST_ASSERT_EQUAL_INT(32, firmware.writeFrameLengths[0]);
    TEST_ASSERT_EQUAL_INT(0, firmware.ignoredFrames);
    TEST_ASSERT_EQUAL_STRING_LEN("hello", reinterpret_cast<const char*>(firmware.zone.data()), 5);
    TEST_ASSERT_EQUAL_HEX8(0xFF, firmware.zone[5]);
}

void test_write_config_chunks_long_payload(void) {
    FakeDaplinkBridge& firmware = attachFirmware();
    bridge.begin();

    constexpr size_t kPayloadLen = DAPLINK_BRIDGE_MAX_WRITE_CHUNK + 5;
    uint8_t payload[kPayloadLen];
    fillPattern(payload, kPayloadLen);

    TEST_ASSERT_TRUE(bridge.writeConfig(payload, kPayloadLen, 10));

    TEST_ASSERT_EQUAL_INT(2, firmware.writeFrameLengths.size());
    TEST_ASSERT_EQUAL_HEX8_ARRAY(payload, firmware.zone.data() + 10, kPayloadLen);
    TEST_ASSERT_EQUAL_HEX8(0xFF, firmware.zone[10 + kPayloadLen]);
}

void test_write_config_returns_false_on_device_error(void) {
    FakeDaplinkBridge& firmware = attachFirmware();
    bridge.begin();
    firmware.error = DAPLINK_BRIDGE_ERROR_CMD_FAILED;
    TEST_ASSERT_FALSE(bridge.writeConfig("x", 0));
}

void test_read_config_returns_data_until_first_0xff(void) {
    FakeDaplinkBridge& firmware = attachFirmware();
    bridge.begin();
    firmware.store("hello");

    uint8_t buf[32] = {0};
    TEST_ASSERT_EQUAL_INT(5, bridge.readConfig(buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_STRING_LEN("hello", reinterpret_cast<char*>(buf), 5);
}

void test_read_config_returns_zero_on_empty_zone(void) {
    attachFirmware();
    bridge.begin();

    uint8_t buf[32] = {0};
    TEST_ASSERT_EQUAL_INT(0, bridge.readConfig(buf, sizeof(buf)));
}

void test_read_config_truncates_to_max_len(void) {
    FakeDaplinkBridge& firmware = attachFirmware();
    bridge.begin();
    firmware.store("aaaaaaaaaa");

    uint8_t buf[3] = {0};
    TEST_ASSERT_EQUAL_INT(3, bridge.readConfig(buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_HEX8('a', buf[0]);
    TEST_ASSERT_EQUAL_HEX8('a', buf[2]);
}

void test_read_config_reads_past_256_bytes(void) {
    // Regression: each I2C read restarts at the first byte of the bridge
    // TX buffer, so a second read of the same READ_CONFIG block returned
    // byte 0 again instead of byte 255.
    FakeDaplinkBridge& firmware = attachFirmware();
    bridge.begin();
    fillPattern(firmware.zone.data(), 600);

    uint8_t buf[DAPLINK_BRIDGE_CONFIG_SIZE] = {0};
    TEST_ASSERT_EQUAL_INT(600, bridge.readConfig(buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_HEX8_ARRAY(firmware.zone.data(), buf, 600);
    TEST_ASSERT_LESS_OR_EQUAL(DAPLINK_BRIDGE_MAX_READ_LEN, firmware.largestRead);
}

void test_read_config_reaches_end_of_zone(void) {
    // Only offsets up to CONFIG_SIZE - 256 are valid: the tail of the zone
    // is read from the last block, never from an out-of-range offset.
    FakeDaplinkBridge& firmware = attachFirmware();
    bridge.begin();
    fillPattern(firmware.zone.data(), DAPLINK_BRIDGE_CONFIG_SIZE);

    uint8_t buf[DAPLINK_BRIDGE_CONFIG_SIZE] = {0};
    TEST_ASSERT_EQUAL_INT(DAPLINK_BRIDGE_CONFIG_SIZE - 1, bridge.readConfig(buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_HEX8_ARRAY(firmware.zone.data(), buf, DAPLINK_BRIDGE_CONFIG_SIZE - 1);
    TEST_ASSERT_EQUAL_HEX8(0, firmware.error);
}

void test_config_round_trip_of_1000_bytes(void) {
    attachFirmware();
    bridge.begin();

    uint8_t payload[1000];
    fillPattern(payload, sizeof(payload));
    TEST_ASSERT_TRUE(bridge.writeConfig(payload, sizeof(payload), 0));

    uint8_t buf[DAPLINK_BRIDGE_CONFIG_SIZE] = {0};
    TEST_ASSERT_EQUAL_INT(sizeof(payload), bridge.readConfig(buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_HEX8_ARRAY(payload, buf, sizeof(payload));
}

void test_read_config_returns_zero_on_i2c_error(void) {
    FakeDaplinkBridge& firmware = attachFirmware();
    bridge.begin();
    firmware.store("hello");
    Wire.setEndTransmissionResult(2);

    uint8_t buf[32] = {0};
    TEST_ASSERT_EQUAL_INT(0, bridge.readConfig(buf, sizeof(buf)));
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
    RUN_TEST(test_write_config_sends_fixed_32_byte_frames);
    RUN_TEST(test_write_config_chunks_long_payload);
    RUN_TEST(test_write_config_returns_false_on_device_error);
    RUN_TEST(test_read_config_returns_data_until_first_0xff);
    RUN_TEST(test_read_config_returns_zero_on_empty_zone);
    RUN_TEST(test_read_config_truncates_to_max_len);
    RUN_TEST(test_read_config_reads_past_256_bytes);
    RUN_TEST(test_read_config_reaches_end_of_zone);
    RUN_TEST(test_config_round_trip_of_1000_bytes);
    RUN_TEST(test_read_config_returns_zero_on_i2c_error);
    return UNITY_END();
}
