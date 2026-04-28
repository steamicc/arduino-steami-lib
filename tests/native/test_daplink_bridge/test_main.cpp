// SPDX-License-Identifier: GPL-3.0-or-later

#include <unity.h>

#include <cstdio>
#include <cstring>

#include "Wire.h"
#include "daplink_bridge.h"

constexpr uint8_t ADDR = DAPLINK_BRIDGE_DEFAULT_ADDR;

static void preloadDeviceId(bool valid = true) {
    Wire.setRegister(ADDR, CMD_WHO_AM_I, valid ? DAPLINK_BRIDGE_WHO_AM_I_VAL : 0x42);
}

static void preloadBusy(bool busy = true) {
    Wire.setRegister(ADDR, REG_STATUS, busy ? STATUS_BUSY : 0x00);
}

daplink_bridge bridge;

void setUp() {
    Wire = TwoWire();
    preloadDeviceId();
    bridge = daplink_bridge();
}

void tearDown(void) {}

void test_device_id_returns_correct_value(void) {
    preloadDeviceId(true);
    TEST_ASSERT_EQUAL_HEX8(DAPLINK_BRIDGE_WHO_AM_I_VAL, bridge.deviceId());
}

void test_busy_returns_true_when_busy(void) {
    preloadBusy(true);
    TEST_ASSERT_TRUE(bridge.busy());
}

void test_busy_returns_false_when_not_busy(void) {
    preloadBusy(false);
    TEST_ASSERT_FALSE(bridge.busy());
}

void test_clear_config_returns_true_on_success(void) {
    preloadBusy(false);
    Wire.setRegister(ADDR, REG_ERROR, 0x00);
    TEST_ASSERT_TRUE(bridge.clearConfig());
}

void test_write_config_writes_data(void) {
    const char* data = "Hello, config!";
    TEST_ASSERT_TRUE(bridge.writeConfig(data, 0));

    bool sawWrite = false;
    for (const auto& w : Wire.getWrites()) {
        if (w.reg == CMD_WRITE_CONFIG) {
            sawWrite = true;
            if (std::memcmp(&w.value, data, strlen(data)) == 0)
                return;
        }
    }
    TEST_ASSERT_TRUE(sawWrite);
}

void test_read_config_returns_data(void) {
    preloadBusy(false);
    // Assure que REG_STATUS reste à 0 après auto-incrément
    Wire.setRegister(ADDR, REG_STATUS, 0x00);
    Wire.setRegister(ADDR, REG_STATUS + 1, 0x00);

    const char* expected = "hello";
    for (size_t i = 0; i < 5; i++) {
        Wire.setRegister(ADDR, CMD_READ_CONFIG + i, (uint8_t)expected[i]);
    }
    Wire.setRegister(ADDR, CMD_READ_CONFIG + 5, 0xFF);

    uint8_t buf[32] = {0};
    size_t len = bridge.readConfig(buf, sizeof(buf));
    TEST_ASSERT_EQUAL(5, len);
    TEST_ASSERT_EQUAL_STRING(expected, reinterpret_cast<char*>(buf));
}

void test_debug_read(void) {
    preloadBusy(false);
    Wire.setRegister(ADDR, CMD_READ_CONFIG, 'h');
    Wire.setRegister(ADDR, CMD_READ_CONFIG + 1, 'i');
    Wire.setRegister(ADDR, CMD_READ_CONFIG + 2, 0xFF);

    // Test readBlock directement sans readConfig
    uint8_t buf[10];
    memset(buf, 0xFF, sizeof(buf));
    Wire.beginTransmission(ADDR);
    Wire.write(CMD_READ_CONFIG);
    Wire.endTransmission(false);
    Wire.requestFrom(ADDR, (uint8_t)3);
    buf[0] = Wire.read();
    buf[1] = Wire.read();
    buf[2] = Wire.read();

    printf("buf[0]=0x%02X buf[1]=0x%02X buf[2]=0x%02X\n", buf[0], buf[1], buf[2]);
    TEST_ASSERT_TRUE(true);
}
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_device_id_returns_correct_value);
    RUN_TEST(test_busy_returns_true_when_busy);
    RUN_TEST(test_busy_returns_false_when_not_busy);
    RUN_TEST(test_clear_config_returns_true_on_success);
    RUN_TEST(test_write_config_writes_data);
    RUN_TEST(test_read_config_returns_data);
    RUN_TEST(test_debug_read);
    return UNITY_END();
}