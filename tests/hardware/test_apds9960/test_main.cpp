// SPDX-License-Identifier: GPL-3.0-or-later

#include <APDS9960.h>
#include <Arduino.h>
#include <Wire.h>
#include <unity.h>

#include "driver_checks.h"

TwoWire internalI2C(I2C_INT_SDA, I2C_INT_SCL);
APDS9960 sensor(internalI2C);

void setUp(void) {
    sensor.begin();
}

void tearDown(void) {
    sensor.disableGestureSensor();
    sensor.disableLightSensor();
    sensor.disableProximitySensor();
    sensor.powerOff();
}

void test_apds9960_begin_succeeds(void) {
    check_begin(sensor);
}

void test_apds9960_device_id_is_supported(void) {
    uint8_t id = sensor.deviceId();
    bool supported =
        id == APDS9960_DEVICE_ID_1 || id == APDS9960_DEVICE_ID_2 || id == APDS9960_DEVICE_ID_3;
    TEST_ASSERT_TRUE_MESSAGE(supported, "unexpected APDS9960 device ID");
}

void test_apds9960_power_control_roundtrip(void) {
    sensor.powerOn();
    TEST_ASSERT_BITS_HIGH(APDS9960_ENABLE_PON, sensor.mode());

    sensor.powerOff();
    TEST_ASSERT_BITS_LOW(APDS9960_ENABLE_PON, sensor.mode());
}

void test_apds9960_light_mode_control(void) {
    sensor.enableLightSensor(false);
    TEST_ASSERT_BITS_HIGH(APDS9960_ENABLE_PON | APDS9960_ENABLE_AEN, sensor.mode());

    sensor.disableLightSensor();
    TEST_ASSERT_BITS_LOW(APDS9960_ENABLE_AEN, sensor.mode());
}

void test_apds9960_proximity_mode_control(void) {
    sensor.enableProximitySensor(false);
    TEST_ASSERT_BITS_HIGH(APDS9960_ENABLE_PON | APDS9960_ENABLE_PEN, sensor.mode());

    sensor.disableProximitySensor();
    TEST_ASSERT_BITS_LOW(APDS9960_ENABLE_PEN, sensor.mode());
}

void test_apds9960_gesture_mode_control(void) {
    sensor.enableGestureSensor(false);

    TEST_ASSERT_BITS_HIGH(
        APDS9960_ENABLE_PON | APDS9960_ENABLE_WEN | APDS9960_ENABLE_PEN | APDS9960_ENABLE_GEN,
        sensor.mode());
    TEST_ASSERT_TRUE(sensor.gestureModeEnabled());

    sensor.disableGestureSensor();
    TEST_ASSERT_BITS_LOW(APDS9960_ENABLE_GEN, sensor.mode());
    TEST_ASSERT_FALSE(sensor.gestureModeEnabled());
}

void test_apds9960_configuration_roundtrips(void) {
    sensor.setAmbientLightGain(APDS9960::AmbientLightGain::X4);
    sensor.setProximityGain(APDS9960::ProximityGain::X2);
    sensor.setLedDrive(APDS9960::LedDrive::MA_25);
    sensor.setLedBoost(APDS9960::LedBoost::PERCENT_100);
    sensor.setGestureGain(APDS9960::GestureGain::X1);
    sensor.setGestureLedDrive(APDS9960::LedDrive::MA_25);
    sensor.setGestureWaitTime(APDS9960::GestureWaitTime::MS_5_6);

    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(APDS9960::AmbientLightGain::X4),
                            static_cast<uint8_t>(sensor.ambientLightGain()));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(APDS9960::ProximityGain::X2),
                            static_cast<uint8_t>(sensor.proximityGain()));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(APDS9960::LedDrive::MA_25),
                            static_cast<uint8_t>(sensor.ledDrive()));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(APDS9960::LedBoost::PERCENT_100),
                            static_cast<uint8_t>(sensor.ledBoost()));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(APDS9960::GestureGain::X1),
                            static_cast<uint8_t>(sensor.gestureGain()));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(APDS9960::LedDrive::MA_25),
                            static_cast<uint8_t>(sensor.gestureLedDrive()));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(APDS9960::GestureWaitTime::MS_5_6),
                            static_cast<uint8_t>(sensor.gestureWaitTime()));
}

void test_apds9960_threshold_roundtrips(void) {
    sensor.setLightInterruptLowThreshold(123);
    sensor.setLightInterruptHighThreshold(45678);
    sensor.setProximityInterruptLowThreshold(12);
    sensor.setProximityInterruptHighThreshold(210);
    sensor.setGestureEnterThreshold(50);
    sensor.setGestureExitThreshold(40);

    TEST_ASSERT_EQUAL_UINT16(123, sensor.lightInterruptLowThreshold());
    TEST_ASSERT_EQUAL_UINT16(45678, sensor.lightInterruptHighThreshold());
    TEST_ASSERT_EQUAL_UINT8(12, sensor.proximityInterruptLowThreshold());
    TEST_ASSERT_EQUAL_UINT8(210, sensor.proximityInterruptHighThreshold());
    TEST_ASSERT_EQUAL_UINT8(50, sensor.gestureEnterThreshold());
    TEST_ASSERT_EQUAL_UINT8(40, sensor.gestureExitThreshold());
}

void test_apds9960_reads_plausible_light_channels(void) {
    sensor.enableLightSensor(false);
    delay(150);

    uint16_t clear = 0;
    uint16_t red = 0;
    uint16_t green = 0;
    uint16_t blue = 0;

    TEST_ASSERT_TRUE_MESSAGE(sensor.ambientLight(clear), "clear read failed");
    TEST_ASSERT_TRUE_MESSAGE(sensor.redLight(red), "red read failed");
    TEST_ASSERT_TRUE_MESSAGE(sensor.greenLight(green), "green read failed");
    TEST_ASSERT_TRUE_MESSAGE(sensor.blueLight(blue), "blue read failed");

    // A completely dark environment may legitimately return zero. Successful
    // bool returns above validate that all four register pairs were read.
}

void test_apds9960_reads_proximity(void) {
    // Conservative optical settings reduce saturation on the STeaMi enclosure.
    sensor.enableProximitySensor(false);
    sensor.setLedDrive(APDS9960::LedDrive::MA_25);
    sensor.setLedBoost(APDS9960::LedBoost::PERCENT_100);
    sensor.setProximityGain(APDS9960::ProximityGain::X1);
    delay(50);

    uint8_t proximity = 0;
    TEST_ASSERT_TRUE_MESSAGE(sensor.proximity(proximity), "proximity read failed");
}

void test_apds9960_combined_data_ready_eventually(void) {
    sensor.enableLightSensor(false);
    sensor.enableProximitySensor(false);

    uint32_t started = millis();
    while (!sensor.dataReady() && millis() - started < 700) {
        delay(10);
    }

    TEST_ASSERT_TRUE_MESSAGE(sensor.dataReady(), "ALS and proximity did not become ready");
}

void test_apds9960_gesture_available_query_is_safe(void) {
    sensor.enableGestureSensor(false);
    delay(50);

    // No physical gesture is required. The test only validates that the
    // GSTATUS register can be queried on real hardware.
    sensor.gestureAvailable();
    TEST_ASSERT_BITS_HIGH(APDS9960_ENABLE_PON | APDS9960_ENABLE_GEN, sensor.mode());
}

void setup() {
    delay(2000);
    internalI2C.begin();

    UNITY_BEGIN();
    RUN_TEST(test_apds9960_begin_succeeds);
    RUN_TEST(test_apds9960_device_id_is_supported);
    RUN_TEST(test_apds9960_power_control_roundtrip);
    RUN_TEST(test_apds9960_light_mode_control);
    RUN_TEST(test_apds9960_proximity_mode_control);
    RUN_TEST(test_apds9960_gesture_mode_control);
    RUN_TEST(test_apds9960_configuration_roundtrips);
    RUN_TEST(test_apds9960_threshold_roundtrips);
    RUN_TEST(test_apds9960_reads_plausible_light_channels);
    RUN_TEST(test_apds9960_reads_proximity);
    RUN_TEST(test_apds9960_combined_data_ready_eventually);
    RUN_TEST(test_apds9960_gesture_available_query_is_safe);
    UNITY_END();
}

void loop() {}
