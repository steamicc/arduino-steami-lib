// SPDX-License-Identifier: GPL-3.0-or-later

#include <unity.h>

#include <cstring>

#include "FakeDaplinkBridge.h"
#include "SteamiDaplinkBridge.h"
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

SteamiDaplinkBridge bridge;

void setUp(void) {
    Wire = TwoWire();
    preloadDeviceId();
    preloadStatus(false);
    preloadError(0);
    bridge = SteamiDaplinkBridge();
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
    firmware.failWrites = true;
    TEST_ASSERT_FALSE(bridge.writeConfig("x", 0));
}

void test_clear_config_erases_zone(void) {
    FakeDaplinkBridge& firmware = attachFirmware();
    bridge.begin();
    firmware.store("hello");

    TEST_ASSERT_TRUE(bridge.clearConfig());
    TEST_ASSERT_EQUAL_HEX8(0xFF, firmware.zone[0]);
}

void test_clear_config_returns_false_on_device_error(void) {
    FakeDaplinkBridge& firmware = attachFirmware();
    bridge.begin();
    firmware.failWrites = true;
    TEST_ASSERT_FALSE(bridge.clearConfig());
}

// ---------------------------------------------------------------------
// Firmwares without the config zone
// ---------------------------------------------------------------------

void test_has_config_zone_on_current_firmware(void) {
    attachFirmware();
    bridge.begin();
    TEST_ASSERT_TRUE(bridge.hasConfigZone());
}

void test_config_zone_probe_runs_once(void) {
    FakeDaplinkBridge& firmware = attachFirmware();
    bridge.begin();
    firmware.store("hello");

    uint8_t buf[8] = {0};
    TEST_ASSERT_TRUE(bridge.hasConfigZone());
    TEST_ASSERT_EQUAL_INT(5, bridge.readConfig(buf, sizeof(buf)));
    TEST_ASSERT_TRUE(bridge.writeConfig("x", 10));
    TEST_ASSERT_EQUAL_INT(1, firmware.readSectorCommands);
}

void test_stale_device_error_does_not_hide_config_zone(void) {
    // The probe reads (and so clears) the error register before asking
    // for the sector: a stale error must not pass for an old firmware.
    FakeDaplinkBridge& firmware = attachFirmware();
    bridge.begin();
    firmware.error = DAPLINK_BRIDGE_ERROR_CMD_FAILED;
    TEST_ASSERT_TRUE(bridge.hasConfigZone());
}

void test_legacy_firmware_has_no_config_zone(void) {
    FakeDaplinkBridge& firmware = attachFirmware();
    firmware.legacyFirmware = true;
    TEST_ASSERT_TRUE(bridge.begin());
    TEST_ASSERT_FALSE(bridge.hasConfigZone());
}

void test_legacy_firmware_never_receives_config_commands(void) {
    // Sending READ_CONFIG or WRITE_CONFIG to these firmwares locks the
    // F103 up until the board is unplugged.
    FakeDaplinkBridge& firmware = attachFirmware();
    firmware.legacyFirmware = true;
    bridge.begin();

    uint8_t buf[32] = {0};
    TEST_ASSERT_EQUAL_INT(0, bridge.readConfig(buf, sizeof(buf)));
    TEST_ASSERT_FALSE(bridge.writeConfig("hello", 0));
    TEST_ASSERT_FALSE(bridge.clearConfig());

    TEST_ASSERT_EQUAL_INT(0, firmware.configCommandsOnLegacy);
    TEST_ASSERT_FALSE(firmware.hung);
    TEST_ASSERT_EQUAL_HEX8(DAPLINK_BRIDGE_WHO_AM_I, bridge.deviceId());
}

void test_config_zone_probe_retried_after_i2c_error(void) {
    attachFirmware();
    bridge.begin();

    Wire.setEndTransmissionResult(2);
    TEST_ASSERT_FALSE(bridge.hasConfigZone());

    Wire.setEndTransmissionResult(0);
    TEST_ASSERT_TRUE(bridge.hasConfigZone());
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

void test_busy_when_bridge_does_not_answer(void) {
    // A silent bridge must not look idle, or the next command would race
    // the one it is still running.
    FakeDaplinkBridge& firmware = attachFirmware();
    firmware.hung = true;
    TEST_ASSERT_TRUE(bridge.busy());
}

void test_device_id_retries_while_bridge_nacks(void) {
    FakeDaplinkBridge& firmware = attachFirmware();
    firmware.nackAfterReadSector = 5;
    TEST_ASSERT_TRUE(bridge.hasConfigZone());
    TEST_ASSERT_EQUAL_HEX8(DAPLINK_BRIDGE_WHO_AM_I, bridge.deviceId());
}

void test_config_zone_found_when_bridge_nacks_after_probe(void) {
    FakeDaplinkBridge& firmware = attachFirmware();
    firmware.nackAfterReadSector = 10;
    bridge.begin();
    TEST_ASSERT_TRUE(bridge.hasConfigZone());
}

void test_legacy_firmware_detected_when_bridge_nacks_after_probe(void) {
    FakeDaplinkBridge& firmware = attachFirmware();
    firmware.legacyFirmware = true;
    firmware.nackAfterReadSector = 10;
    bridge.begin();
    TEST_ASSERT_FALSE(bridge.hasConfigZone());
}

void test_silent_bridge_never_reads_as_config_zone(void) {
    // Regression: failed reads used to return 0, which the probe took for
    // "no error", so a legacy bridge that stopped answering was reported
    // as having a config zone and then sent READ_CONFIG, which locks it up.
    FakeDaplinkBridge& firmware = attachFirmware();
    firmware.legacyFirmware = true;
    firmware.nackAfterReadSector = 100000;
    bridge.begin();

    TEST_ASSERT_FALSE(bridge.hasConfigZone());
    uint8_t buf[16] = {0};
    TEST_ASSERT_EQUAL_INT(0, bridge.readConfig(buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_INT(0, firmware.configCommandsOnLegacy);
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
    RUN_TEST(test_write_config_rejects_offset_at_or_past_end);
    RUN_TEST(test_write_config_rejects_payload_overflowing_zone);
    RUN_TEST(test_write_config_sends_fixed_32_byte_frames);
    RUN_TEST(test_write_config_chunks_long_payload);
    RUN_TEST(test_write_config_returns_false_on_device_error);
    RUN_TEST(test_clear_config_erases_zone);
    RUN_TEST(test_clear_config_returns_false_on_device_error);
    RUN_TEST(test_has_config_zone_on_current_firmware);
    RUN_TEST(test_config_zone_probe_runs_once);
    RUN_TEST(test_stale_device_error_does_not_hide_config_zone);
    RUN_TEST(test_legacy_firmware_has_no_config_zone);
    RUN_TEST(test_legacy_firmware_never_receives_config_commands);
    RUN_TEST(test_config_zone_probe_retried_after_i2c_error);
    RUN_TEST(test_read_config_returns_data_until_first_0xff);
    RUN_TEST(test_read_config_returns_zero_on_empty_zone);
    RUN_TEST(test_read_config_truncates_to_max_len);
    RUN_TEST(test_read_config_reads_past_256_bytes);
    RUN_TEST(test_read_config_reaches_end_of_zone);
    RUN_TEST(test_config_round_trip_of_1000_bytes);
    RUN_TEST(test_busy_when_bridge_does_not_answer);
    RUN_TEST(test_device_id_retries_while_bridge_nacks);
    RUN_TEST(test_config_zone_found_when_bridge_nacks_after_probe);
    RUN_TEST(test_legacy_firmware_detected_when_bridge_nacks_after_probe);
    RUN_TEST(test_silent_bridge_never_reads_as_config_zone);
    RUN_TEST(test_read_config_returns_zero_on_i2c_error);
    return UNITY_END();
}
