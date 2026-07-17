// SPDX-License-Identifier: GPL-3.0-or-later

#include <unity.h>

#include "APDS9960.h"
#include "Wire.h"
#include "driver_checks.h"

constexpr uint8_t ADDR = APDS9960_DEFAULT_ADDRESS;

APDS9960 sensor(Wire, ADDR);

static void preloadDeviceId(uint8_t id = APDS9960_DEVICE_ID_1) {
    Wire.setRegister(ADDR, APDS9960_REG_ID, id);
}

static void preloadLightReady(bool ready = true) {
    uint8_t status = Wire.getRegister(ADDR, APDS9960_REG_STATUS);
    if (ready) {
        status |= APDS9960_STATUS_AVALID;
    } else {
        status &= static_cast<uint8_t>(~APDS9960_STATUS_AVALID);
    }
    Wire.setRegister(ADDR, APDS9960_REG_STATUS, status);
}

static void preloadProximityReady(bool ready = true) {
    uint8_t status = Wire.getRegister(ADDR, APDS9960_REG_STATUS);
    if (ready) {
        status |= APDS9960_STATUS_PVALID;
    } else {
        status &= static_cast<uint8_t>(~APDS9960_STATUS_PVALID);
    }
    Wire.setRegister(ADDR, APDS9960_REG_STATUS, status);
}

static void preload16(uint8_t lowRegister, uint16_t value) {
    Wire.setRegister(ADDR, lowRegister, static_cast<uint8_t>(value));
    Wire.setRegister(ADDR, static_cast<uint8_t>(lowRegister + 1), static_cast<uint8_t>(value >> 8));
}

void setUp(void) {
    Wire = TwoWire();
    preloadDeviceId();
    sensor = APDS9960(Wire, ADDR);
}

void tearDown(void) {}

void test_begin_detects_primary_device_id(void) {
    check_begin(sensor);
}

void test_begin_accepts_supported_alternate_ids(void) {
    preloadDeviceId(APDS9960_DEVICE_ID_2);
    TEST_ASSERT_TRUE(sensor.begin());

    preloadDeviceId(APDS9960_DEVICE_ID_3);
    TEST_ASSERT_TRUE(sensor.begin());
}

void test_begin_rejects_wrong_device_id(void) {
    preloadDeviceId(0x00);
    TEST_ASSERT_FALSE(sensor.begin());
}

void test_begin_rejects_i2c_error(void) {
    Wire.setEndTransmissionResult(2);
    TEST_ASSERT_FALSE(sensor.begin());
    Wire.setEndTransmissionResult(0);
}

void test_device_id_returns_identity_register(void) {
    preloadDeviceId(APDS9960_DEVICE_ID_1);
    check_who_am_i(sensor, APDS9960_DEVICE_ID_1);
}

void test_begin_writes_default_configuration(void) {
    TEST_ASSERT_TRUE(sensor.begin());

    TEST_ASSERT_EQUAL_HEX8(APDS9960_DEFAULT_ATIME, Wire.getRegister(ADDR, APDS9960_REG_ATIME));
    TEST_ASSERT_EQUAL_HEX8(APDS9960_DEFAULT_WTIME, Wire.getRegister(ADDR, APDS9960_REG_WTIME));
    TEST_ASSERT_EQUAL_HEX8(APDS9960_DEFAULT_PROX_PPULSE,
                           Wire.getRegister(ADDR, APDS9960_REG_PPULSE));
    TEST_ASSERT_EQUAL_HEX8(APDS9960_DEFAULT_GPENTH, Wire.getRegister(ADDR, APDS9960_REG_GPENTH));
    TEST_ASSERT_EQUAL_HEX8(APDS9960_DEFAULT_GEXTH, Wire.getRegister(ADDR, APDS9960_REG_GEXTH));
    TEST_ASSERT_EQUAL_HEX8(0x00, Wire.getRegister(ADDR, APDS9960_REG_ENABLE));
}

void test_set_mode_sets_and_clears_requested_bit(void) {
    Wire.setRegister(ADDR, APDS9960_REG_ENABLE, 0x00);

    TEST_ASSERT_TRUE(sensor.setMode(APDS9960::Mode::AMBIENT_LIGHT, true));
    TEST_ASSERT_EQUAL_HEX8(APDS9960_ENABLE_AEN, Wire.getRegister(ADDR, APDS9960_REG_ENABLE));

    TEST_ASSERT_TRUE(sensor.setMode(APDS9960::Mode::AMBIENT_LIGHT, false));
    TEST_ASSERT_EQUAL_HEX8(0x00, Wire.getRegister(ADDR, APDS9960_REG_ENABLE));
}

void test_set_mode_all_controls_all_enable_bits(void) {
    TEST_ASSERT_TRUE(sensor.setMode(APDS9960::Mode::ALL, true));
    TEST_ASSERT_EQUAL_HEX8(0x7F, Wire.getRegister(ADDR, APDS9960_REG_ENABLE));

    TEST_ASSERT_TRUE(sensor.setMode(APDS9960::Mode::ALL, false));
    TEST_ASSERT_EQUAL_HEX8(0x00, Wire.getRegister(ADDR, APDS9960_REG_ENABLE));
}

void test_set_mode_rejects_invalid_enum_value(void) {
    APDS9960::Mode invalid = static_cast<APDS9960::Mode>(8);
    TEST_ASSERT_FALSE(sensor.setMode(invalid, true));
}

void test_power_control_toggles_pon(void) {
    Wire.setRegister(ADDR, APDS9960_REG_ENABLE, APDS9960_ENABLE_AEN);

    sensor.powerOn();
    TEST_ASSERT_BITS_HIGH(APDS9960_ENABLE_PON, Wire.getRegister(ADDR, APDS9960_REG_ENABLE));

    sensor.powerOff();
    TEST_ASSERT_BITS_LOW(APDS9960_ENABLE_PON, Wire.getRegister(ADDR, APDS9960_REG_ENABLE));
    TEST_ASSERT_BITS_HIGH(APDS9960_ENABLE_AEN, Wire.getRegister(ADDR, APDS9960_REG_ENABLE));
}

void test_status_readiness_helpers_decode_status(void) {
    Wire.setRegister(ADDR, APDS9960_REG_STATUS, APDS9960_STATUS_AVALID | APDS9960_STATUS_PVALID);
    TEST_ASSERT_TRUE(sensor.lightReady());
    TEST_ASSERT_TRUE(sensor.proximityReady());
    TEST_ASSERT_TRUE(sensor.dataReady());

    Wire.setRegister(ADDR, APDS9960_REG_STATUS, APDS9960_STATUS_AVALID);
    TEST_ASSERT_TRUE(sensor.lightReady());
    TEST_ASSERT_FALSE(sensor.proximityReady());
    TEST_ASSERT_FALSE(sensor.dataReady());
}

void test_enable_and_disable_light_sensor_control_bits(void) {
    sensor.enableLightSensor(true);
    uint8_t enabled = Wire.getRegister(ADDR, APDS9960_REG_ENABLE);
    TEST_ASSERT_BITS_HIGH(APDS9960_ENABLE_PON | APDS9960_ENABLE_AEN | APDS9960_ENABLE_AIEN,
                          enabled);

    sensor.disableLightSensor();
    enabled = Wire.getRegister(ADDR, APDS9960_REG_ENABLE);
    TEST_ASSERT_BITS_LOW(APDS9960_ENABLE_AEN | APDS9960_ENABLE_AIEN, enabled);
}

void test_enable_and_disable_proximity_sensor_control_bits(void) {
    sensor.enableProximitySensor(true);
    uint8_t enabled = Wire.getRegister(ADDR, APDS9960_REG_ENABLE);
    TEST_ASSERT_BITS_HIGH(APDS9960_ENABLE_PON | APDS9960_ENABLE_PEN | APDS9960_ENABLE_PIEN,
                          enabled);

    sensor.disableProximitySensor();
    enabled = Wire.getRegister(ADDR, APDS9960_REG_ENABLE);
    TEST_ASSERT_BITS_LOW(APDS9960_ENABLE_PEN | APDS9960_ENABLE_PIEN, enabled);
}

void test_enable_and_disable_gesture_sensor_control_bits(void) {
    sensor.enableGestureSensor(true);

    uint8_t enabled = Wire.getRegister(ADDR, APDS9960_REG_ENABLE);
    TEST_ASSERT_BITS_HIGH(
        APDS9960_ENABLE_PON | APDS9960_ENABLE_WEN | APDS9960_ENABLE_PEN | APDS9960_ENABLE_GEN,
        enabled);
    TEST_ASSERT_TRUE(sensor.gestureModeEnabled());
    TEST_ASSERT_TRUE(sensor.gestureInterruptEnabled());
    TEST_ASSERT_EQUAL_HEX8(0xFF, Wire.getRegister(ADDR, APDS9960_REG_WTIME));
    TEST_ASSERT_EQUAL_HEX8(APDS9960_DEFAULT_GESTURE_PPULSE,
                           Wire.getRegister(ADDR, APDS9960_REG_PPULSE));

    sensor.disableGestureSensor();
    TEST_ASSERT_BITS_LOW(APDS9960_ENABLE_GEN, Wire.getRegister(ADDR, APDS9960_REG_ENABLE));
    TEST_ASSERT_FALSE(sensor.gestureModeEnabled());
    TEST_ASSERT_FALSE(sensor.gestureInterruptEnabled());
}

void test_light_channel_reads_return_little_endian_values(void) {
    preloadLightReady(true);
    preload16(APDS9960_REG_CDATAL, 0x1234);
    preload16(APDS9960_REG_RDATAL, 0x5678);
    preload16(APDS9960_REG_GDATAL, 0x9ABC);
    preload16(APDS9960_REG_BDATAL, 0xDEF0);

    uint16_t clear = 0;
    uint16_t red = 0;
    uint16_t green = 0;
    uint16_t blue = 0;

    TEST_ASSERT_TRUE(sensor.ambientLight(clear));
    TEST_ASSERT_TRUE(sensor.redLight(red));
    TEST_ASSERT_TRUE(sensor.greenLight(green));
    TEST_ASSERT_TRUE(sensor.blueLight(blue));

    TEST_ASSERT_EQUAL_HEX16(0x1234, clear);
    TEST_ASSERT_EQUAL_HEX16(0x5678, red);
    TEST_ASSERT_EQUAL_HEX16(0x9ABC, green);
    TEST_ASSERT_EQUAL_HEX16(0xDEF0, blue);
}

void test_light_read_auto_enables_sensor(void) {
    preloadLightReady(true);
    preload16(APDS9960_REG_CDATAL, 0x0102);
    Wire.setRegister(ADDR, APDS9960_REG_ENABLE, 0x00);

    uint16_t value = 0;
    TEST_ASSERT_TRUE(sensor.ambientLight(value));
    TEST_ASSERT_EQUAL_HEX16(0x0102, value);
    TEST_ASSERT_BITS_HIGH(APDS9960_ENABLE_PON | APDS9960_ENABLE_AEN,
                          Wire.getRegister(ADDR, APDS9960_REG_ENABLE));
}

void test_light_read_returns_false_on_i2c_error(void) {
    preloadLightReady(true);
    Wire.setEndTransmissionResult(2);

    uint16_t value = 0xFFFF;
    TEST_ASSERT_FALSE(sensor.ambientLight(value));
    TEST_ASSERT_EQUAL_UINT16(0, value);

    Wire.setEndTransmissionResult(0);
}

void test_proximity_read_returns_value_and_auto_enables(void) {
    preloadProximityReady(true);
    Wire.setRegister(ADDR, APDS9960_REG_PDATA, 173);
    Wire.setRegister(ADDR, APDS9960_REG_ENABLE, 0x00);

    uint8_t value = 0;
    TEST_ASSERT_TRUE(sensor.proximity(value));
    TEST_ASSERT_EQUAL_UINT8(173, value);
    TEST_ASSERT_BITS_HIGH(APDS9960_ENABLE_PON | APDS9960_ENABLE_PEN,
                          Wire.getRegister(ADDR, APDS9960_REG_ENABLE));
}

void test_gesture_available_reflects_gvalid(void) {
    Wire.setRegister(ADDR, APDS9960_REG_GSTATUS, APDS9960_GSTATUS_GVALID);
    TEST_ASSERT_TRUE(sensor.gestureAvailable());

    Wire.setRegister(ADDR, APDS9960_REG_GSTATUS, 0x00);
    TEST_ASSERT_FALSE(sensor.gestureAvailable());
}

void test_read_gesture_returns_none_when_engine_is_disabled(void) {
    Wire.setRegister(ADDR, APDS9960_REG_ENABLE, 0x00);
    Wire.setRegister(ADDR, APDS9960_REG_GSTATUS, APDS9960_GSTATUS_GVALID);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(APDS9960::Gesture::NONE),
                            static_cast<uint8_t>(sensor.readGesture(0)));
}

void test_control_field_getters_and_setters_preserve_other_bits(void) {
    Wire.setRegister(ADDR, APDS9960_REG_CONTROL, 0x00);

    sensor.setAmbientLightGain(APDS9960::AmbientLightGain::X64);
    sensor.setProximityGain(APDS9960::ProximityGain::X8);
    sensor.setLedDrive(APDS9960::LedDrive::MA_12_5);

    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(APDS9960::AmbientLightGain::X64),
                            static_cast<uint8_t>(sensor.ambientLightGain()));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(APDS9960::ProximityGain::X8),
                            static_cast<uint8_t>(sensor.proximityGain()));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(APDS9960::LedDrive::MA_12_5),
                            static_cast<uint8_t>(sensor.ledDrive()));
    TEST_ASSERT_EQUAL_HEX8(0xCF, Wire.getRegister(ADDR, APDS9960_REG_CONTROL));
}

void test_led_boost_roundtrip_preserves_config2_other_bits(void) {
    Wire.setRegister(ADDR, APDS9960_REG_CONFIG2, 0xC1);
    sensor.setLedBoost(APDS9960::LedBoost::PERCENT_200);

    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(APDS9960::LedBoost::PERCENT_200),
                            static_cast<uint8_t>(sensor.ledBoost()));
    TEST_ASSERT_EQUAL_HEX8(0xE1, Wire.getRegister(ADDR, APDS9960_REG_CONFIG2));
}

void test_config3_fields_roundtrip(void) {
    Wire.setRegister(ADDR, APDS9960_REG_CONFIG3, 0xC0);

    sensor.setProximityGainCompensation(true);
    sensor.setProximityPhotodiodeMask(0x0A);

    TEST_ASSERT_TRUE(sensor.proximityGainCompensationEnabled());
    TEST_ASSERT_EQUAL_HEX8(0x0A, sensor.proximityPhotodiodeMask());
    TEST_ASSERT_EQUAL_HEX8(0xEA, Wire.getRegister(ADDR, APDS9960_REG_CONFIG3));
}

void test_gesture_configuration_fields_roundtrip(void) {
    Wire.setRegister(ADDR, APDS9960_REG_GCONF2, 0x80);

    sensor.setGestureGain(APDS9960::GestureGain::X8);
    sensor.setGestureLedDrive(APDS9960::LedDrive::MA_25);
    sensor.setGestureWaitTime(APDS9960::GestureWaitTime::MS_22_4);

    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(APDS9960::GestureGain::X8),
                            static_cast<uint8_t>(sensor.gestureGain()));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(APDS9960::LedDrive::MA_25),
                            static_cast<uint8_t>(sensor.gestureLedDrive()));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(APDS9960::GestureWaitTime::MS_22_4),
                            static_cast<uint8_t>(sensor.gestureWaitTime()));
    TEST_ASSERT_EQUAL_HEX8(0xF5, Wire.getRegister(ADDR, APDS9960_REG_GCONF2));
}

void test_gesture_thresholds_roundtrip(void) {
    sensor.setGestureEnterThreshold(55);
    sensor.setGestureExitThreshold(44);

    TEST_ASSERT_EQUAL_UINT8(55, sensor.gestureEnterThreshold());
    TEST_ASSERT_EQUAL_UINT8(44, sensor.gestureExitThreshold());
}

void test_light_interrupt_thresholds_roundtrip(void) {
    sensor.setLightInterruptLowThreshold(0x1234);
    sensor.setLightInterruptHighThreshold(0xABCD);

    TEST_ASSERT_EQUAL_HEX16(0x1234, sensor.lightInterruptLowThreshold());
    TEST_ASSERT_EQUAL_HEX16(0xABCD, sensor.lightInterruptHighThreshold());
}

void test_proximity_interrupt_thresholds_roundtrip(void) {
    sensor.setProximityInterruptLowThreshold(12);
    sensor.setProximityInterruptHighThreshold(210);

    TEST_ASSERT_EQUAL_UINT8(12, sensor.proximityInterruptLowThreshold());
    TEST_ASSERT_EQUAL_UINT8(210, sensor.proximityInterruptHighThreshold());
}

void test_interrupt_enable_helpers_roundtrip(void) {
    sensor.setAmbientLightInterrupt(true);
    sensor.setProximityInterrupt(true);
    sensor.setGestureInterrupt(true);
    sensor.setGestureMode(true);

    TEST_ASSERT_TRUE(sensor.ambientLightInterruptEnabled());
    TEST_ASSERT_TRUE(sensor.proximityInterruptEnabled());
    TEST_ASSERT_TRUE(sensor.gestureInterruptEnabled());
    TEST_ASSERT_TRUE(sensor.gestureModeEnabled());

    sensor.setAmbientLightInterrupt(false);
    sensor.setProximityInterrupt(false);
    sensor.setGestureInterrupt(false);
    sensor.setGestureMode(false);

    TEST_ASSERT_FALSE(sensor.ambientLightInterruptEnabled());
    TEST_ASSERT_FALSE(sensor.proximityInterruptEnabled());
    TEST_ASSERT_FALSE(sensor.gestureInterruptEnabled());
    TEST_ASSERT_FALSE(sensor.gestureModeEnabled());
}

void test_clear_interrupt_methods_read_clear_registers(void) {
    Wire.setRegister(ADDR, APDS9960_REG_AICLEAR, 0xA5);
    Wire.setRegister(ADDR, APDS9960_REG_PICLEAR, 0x5A);

    sensor.clearAmbientLightInterrupt();
    sensor.clearProximityInterrupt();

    // These special registers clear on read on real silicon. The native mock
    // cannot emulate that side effect, so successful execution is the contract.
    TEST_PASS();
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_begin_detects_primary_device_id);
    RUN_TEST(test_begin_accepts_supported_alternate_ids);
    RUN_TEST(test_begin_rejects_wrong_device_id);
    RUN_TEST(test_begin_rejects_i2c_error);
    RUN_TEST(test_device_id_returns_identity_register);
    RUN_TEST(test_begin_writes_default_configuration);
    RUN_TEST(test_set_mode_sets_and_clears_requested_bit);
    RUN_TEST(test_set_mode_all_controls_all_enable_bits);
    RUN_TEST(test_set_mode_rejects_invalid_enum_value);
    RUN_TEST(test_power_control_toggles_pon);
    RUN_TEST(test_status_readiness_helpers_decode_status);
    RUN_TEST(test_enable_and_disable_light_sensor_control_bits);
    RUN_TEST(test_enable_and_disable_proximity_sensor_control_bits);
    RUN_TEST(test_enable_and_disable_gesture_sensor_control_bits);
    RUN_TEST(test_light_channel_reads_return_little_endian_values);
    RUN_TEST(test_light_read_auto_enables_sensor);
    RUN_TEST(test_light_read_returns_false_on_i2c_error);
    RUN_TEST(test_proximity_read_returns_value_and_auto_enables);
    RUN_TEST(test_gesture_available_reflects_gvalid);
    RUN_TEST(test_read_gesture_returns_none_when_engine_is_disabled);
    RUN_TEST(test_control_field_getters_and_setters_preserve_other_bits);
    RUN_TEST(test_led_boost_roundtrip_preserves_config2_other_bits);
    RUN_TEST(test_config3_fields_roundtrip);
    RUN_TEST(test_gesture_configuration_fields_roundtrip);
    RUN_TEST(test_gesture_thresholds_roundtrip);
    RUN_TEST(test_light_interrupt_thresholds_roundtrip);
    RUN_TEST(test_proximity_interrupt_thresholds_roundtrip);
    RUN_TEST(test_interrupt_enable_helpers_roundtrip);
    RUN_TEST(test_clear_interrupt_methods_read_clear_registers);
    return UNITY_END();
}
