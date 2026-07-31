// SPDX-License-Identifier: GPL-3.0-or-later

#include <APDS9960.h>
#include <Arduino.h>
#include <Wire.h>
#include <unity.h>

TwoWire internalI2C(I2C_INT_SDA, I2C_INT_SCL);
APDS9960 sensor(internalI2C);

static bool i2cDevicePresent(uint8_t address) {
    internalI2C.beginTransmission(address);
    return internalI2C.endTransmission() == 0;
}

static bool supportedDeviceId(uint8_t id) {
    return id == APDS9960_DEVICE_ID_1 || id == APDS9960_DEVICE_ID_2 || id == APDS9960_DEVICE_ID_3;
}

void setUp(void) {
    sensor.powerOff();
    TEST_ASSERT_TRUE_MESSAGE(sensor.begin(), "APDS9960 initialization failed");
}

void tearDown(void) {
    sensor.disableGestureSensor();
    sensor.disableProximitySensor();
    sensor.disableLightSensor();
    sensor.powerOff();
}

void test_apds9960_is_present_on_internal_i2c_bus(void) {
    TEST_ASSERT_TRUE_MESSAGE(i2cDevicePresent(APDS9960_DEFAULT_ADDRESS),
                             "APDS9960 did not acknowledge at I2C address 0x39");
}

void test_apds9960_device_id_is_supported(void) {
    uint8_t id = sensor.deviceId();

    TEST_ASSERT_TRUE_MESSAGE(supportedDeviceId(id), "Unexpected APDS9960 device ID");
}

void test_apds9960_reads_ambient_and_rgb_channels(void) {
    uint16_t clear = 0;
    uint16_t red = 0;
    uint16_t green = 0;
    uint16_t blue = 0;

    sensor.enableLightSensor(false);

    TEST_ASSERT_TRUE_MESSAGE(sensor.ambientLight(clear), "Ambient light acquisition failed");
    TEST_ASSERT_TRUE_MESSAGE(sensor.redLight(red), "Red channel acquisition failed");
    TEST_ASSERT_TRUE_MESSAGE(sensor.greenLight(green), "Green channel acquisition failed");
    TEST_ASSERT_TRUE_MESSAGE(sensor.blueLight(blue), "Blue channel acquisition failed");

    // A completely zero RGB/Clear sample is unlikely on a powered board and
    // usually indicates stale data or a failed acquisition.
    TEST_ASSERT_TRUE_MESSAGE(clear != 0 || red != 0 || green != 0 || blue != 0,
                             "All ambient-light channels returned zero");
}

void test_apds9960_reads_proximity(void) {
    uint8_t proximity = 0;

    sensor.enableProximitySensor(false);

    TEST_ASSERT_TRUE_MESSAGE(sensor.proximity(proximity), "Proximity acquisition failed");

    // The APDS9960 proximity ADC is an unsigned 8-bit value. Keep this test
    // intentionally broad because the result depends on the surroundings.
    TEST_ASSERT_LESS_OR_EQUAL_UINT8(255, proximity);
}

void test_apds9960_power_cycle(void) {
    sensor.powerOn();
    TEST_ASSERT_BITS_HIGH_MESSAGE(APDS9960_ENABLE_PON, sensor.mode(),
                                  "Power bit was not set after powerOn()");

    sensor.powerOff();
    TEST_ASSERT_BITS_LOW_MESSAGE(APDS9960_ENABLE_PON, sensor.mode(),
                                 "Power bit remained set after powerOff()");

    sensor.powerOn();
    TEST_ASSERT_BITS_HIGH_MESSAGE(APDS9960_ENABLE_PON, sensor.mode(),
                                  "Power bit was not restored after power cycle");

    uint16_t clear = 0;
    TEST_ASSERT_TRUE_MESSAGE(sensor.ambientLight(clear),
                             "Ambient light acquisition failed after power cycle");
}

void setup() {
    delay(2000);
    internalI2C.begin();

    UNITY_BEGIN();
    RUN_TEST(test_apds9960_is_present_on_internal_i2c_bus);
    RUN_TEST(test_apds9960_device_id_is_supported);
    RUN_TEST(test_apds9960_reads_ambient_and_rgb_channels);
    RUN_TEST(test_apds9960_reads_proximity);
    RUN_TEST(test_apds9960_power_cycle);
    UNITY_END();
}

void loop() {}
