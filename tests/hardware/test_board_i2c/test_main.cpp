// SPDX-License-Identifier: GPL-3.0-or-later

#include <Arduino.h>
#include <Wire.h>
#include <unity.h>

TwoWire internalI2C(I2C_INT_SDA, I2C_INT_SCL);

static bool deviceResponds(uint8_t address) {
    internalI2C.beginTransmission(address);
    return internalI2C.endTransmission() == 0;
}

void test_expected_i2c_devices_respond() {
    // MCP23009E must be released from reset before scanning.
    pinMode(RST_EXPANDER, OUTPUT);
    digitalWrite(RST_EXPANDER, HIGH);
    delay(10);

    const uint8_t expected[] = {
        0x1E,  // LIS2MDL
        0x20,  // MCP23009E
        0x29,  // VL53L1X
        0x39,  // APDS9960
        0x3B,  // DAPLink bridge
        0x5D,  // WSEN-PADS
        0x5F,  // HTS221 / WSEN-HIDS
        0x6B,  // ISM330DL
    };

    for (uint8_t address : expected) {
        TEST_ASSERT_TRUE_MESSAGE(deviceResponds(address), "Expected I2C device did not respond");
    }
}

void test_no_unexpected_i2c_devices() {
    const uint8_t known[] = {
        0x1E, 0x20, 0x29, 0x39, 0x3B,
        0x55,  // BQ27441 - optional, battery dependent
        0x5D, 0x5F, 0x6B,
    };

    for (uint8_t address = 1; address < 0x7F; address++) {
        if (!deviceResponds(address)) {
            continue;
        }

        bool recognized = false;

        for (uint8_t knownAddress : known) {
            if (address == knownAddress) {
                recognized = true;
                break;
            }
        }

        TEST_ASSERT_TRUE_MESSAGE(recognized, "Unexpected I2C device detected");
    }
}

void setup() {
    delay(2000);

    internalI2C.begin();

    UNITY_BEGIN();
    RUN_TEST(test_expected_i2c_devices_respond);
    RUN_TEST(test_no_unexpected_i2c_devices);
    UNITY_END();
}

void loop() {}
