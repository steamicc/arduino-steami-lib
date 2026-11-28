// SPDX-License-Identifier: GPL-3.0-or-later

#include <math.h>
#include <string.h>
#include <unity.h>

#include <new>

#include "SteamiISM330DL.h"
#include "Wire.h"

constexpr uint8_t ADDR = ISM330DL_DEFAULT_ADDRESS;

SteamiISM330DL sensor(Wire);

static void reconstructSensor() {
    sensor.~SteamiISM330DL();
    new (&sensor) SteamiISM330DL(Wire);
}

static void preloadWhoAmI(bool valid = true) {
    Wire.setRegister(ADDR, ISM330DL_REG_WHO_AM_I, valid ? ISM330DL_WHO_AM_I_VALUE : 0x42);
}

static void preloadPoweredOn() {
    Wire.setRegister(ADDR, ISM330DL_REG_CTRL1_XL, 0x40);
    Wire.setRegister(ADDR, ISM330DL_REG_CTRL2_G, 0x40);
}

static void preloadStatus(uint8_t value = ISM330DL_STATUS_ALL_READY) {
    Wire.setRegister(ADDR, ISM330DL_REG_STATUS, value);
}

static void preloadInt16(uint8_t reg, int16_t value) {
    const uint16_t raw = static_cast<uint16_t>(value);
    Wire.setRegister(ADDR, reg, raw & 0xFF);
    Wire.setRegister(ADDR, reg + 1, (raw >> 8) & 0xFF);
}

static void preloadVector(uint8_t reg, int16_t x, int16_t y, int16_t z) {
    preloadInt16(reg + 0, x);
    preloadInt16(reg + 2, y);
    preloadInt16(reg + 4, z);
}

static void preloadDefaultMeasurement() {
    preloadPoweredOn();
    preloadStatus();
    preloadVector(ISM330DL_REG_OUTX_L_XL, 1000, -2000, 16384);
    preloadVector(ISM330DL_REG_OUTX_L_G, 1000, -2000, 3000);
    preloadInt16(ISM330DL_REG_OUT_TEMP_L, 512);
}

void setUp(void) {
    Wire = TwoWire();
    reconstructSensor();
    preloadWhoAmI();
    preloadDefaultMeasurement();
}

void tearDown(void) {}

void test_device_id_returns_who_am_i(void) {
    TEST_ASSERT_EQUAL_HEX8(ISM330DL_WHO_AM_I_VALUE, sensor.deviceId());
}

void test_is_connected_accepts_expected_who_am_i(void) {
    TEST_ASSERT_TRUE(sensor.isConnected());
}

void test_is_connected_rejects_wrong_who_am_i(void) {
    preloadWhoAmI(false);
    TEST_ASSERT_FALSE(sensor.isConnected());
}

void test_begin_rejects_wrong_who_am_i(void) {
    preloadWhoAmI(false);
    TEST_ASSERT_FALSE(sensor.begin());
}

void test_configure_accel_writes_expected_register(void) {
    TEST_ASSERT_TRUE(
        sensor.configureAccel(SteamiISM330DL::AccelOdr::HZ_208, SteamiISM330DL::AccelScale::G_8));

    TEST_ASSERT_EQUAL_HEX8(0x5C, Wire.getRegister(ADDR, ISM330DL_REG_CTRL1_XL));
}

void test_configure_accel_supports_all_scales(void) {
    struct Case {
        SteamiISM330DL::AccelScale scale;
        uint8_t expected;
    };

    const Case cases[] = {
        {SteamiISM330DL::AccelScale::G_2, 0x40},
        {SteamiISM330DL::AccelScale::G_16, 0x44},
        {SteamiISM330DL::AccelScale::G_4, 0x48},
        {SteamiISM330DL::AccelScale::G_8, 0x4C},
    };

    for (const auto& item : cases) {
        TEST_ASSERT_TRUE(sensor.configureAccel(SteamiISM330DL::AccelOdr::HZ_104, item.scale));
        TEST_ASSERT_EQUAL_HEX8(item.expected, Wire.getRegister(ADDR, ISM330DL_REG_CTRL1_XL));
    }
}

void test_configure_accel_rejects_invalid_values(void) {
    const auto badOdr = static_cast<SteamiISM330DL::AccelOdr>(0x09);
    const auto badScale = static_cast<SteamiISM330DL::AccelScale>(3);

    TEST_ASSERT_FALSE(sensor.configureAccel(badOdr, SteamiISM330DL::AccelScale::G_2));
    TEST_ASSERT_FALSE(sensor.configureAccel(SteamiISM330DL::AccelOdr::HZ_104, badScale));
}

void test_configure_gyro_writes_expected_register(void) {
    TEST_ASSERT_TRUE(
        sensor.configureGyro(SteamiISM330DL::GyroOdr::HZ_416, SteamiISM330DL::GyroScale::DPS_1000));

    TEST_ASSERT_EQUAL_HEX8(0x68, Wire.getRegister(ADDR, ISM330DL_REG_CTRL2_G));
}

void test_configure_gyro_supports_all_scales(void) {
    struct Case {
        SteamiISM330DL::GyroScale scale;
        uint8_t expected;
    };

    const Case cases[] = {
        {SteamiISM330DL::GyroScale::DPS_125, 0x42},  {SteamiISM330DL::GyroScale::DPS_250, 0x40},
        {SteamiISM330DL::GyroScale::DPS_500, 0x44},  {SteamiISM330DL::GyroScale::DPS_1000, 0x48},
        {SteamiISM330DL::GyroScale::DPS_2000, 0x4C},
    };

    for (const auto& item : cases) {
        TEST_ASSERT_TRUE(sensor.configureGyro(SteamiISM330DL::GyroOdr::HZ_104, item.scale));
        TEST_ASSERT_EQUAL_HEX8(item.expected, Wire.getRegister(ADDR, ISM330DL_REG_CTRL2_G));
    }
}

void test_configure_gyro_rejects_invalid_values(void) {
    const auto badOdr = static_cast<SteamiISM330DL::GyroOdr>(0x09);
    const auto badScale = static_cast<SteamiISM330DL::GyroScale>(333);

    TEST_ASSERT_FALSE(sensor.configureGyro(badOdr, SteamiISM330DL::GyroScale::DPS_250));
    TEST_ASSERT_FALSE(sensor.configureGyro(SteamiISM330DL::GyroOdr::HZ_104, badScale));
}

void test_raw_measurements_decode_little_endian_signed_values(void) {
    SteamiISM330DL::RawVector accel{};
    SteamiISM330DL::RawVector gyro{};
    int16_t temperature = 0;

    TEST_ASSERT_TRUE(sensor.accelerationRaw(accel));
    TEST_ASSERT_TRUE(sensor.gyroscopeRaw(gyro));
    TEST_ASSERT_TRUE(sensor.temperatureRaw(temperature));

    TEST_ASSERT_EQUAL_INT16(1000, accel.x);
    TEST_ASSERT_EQUAL_INT16(-2000, accel.y);
    TEST_ASSERT_EQUAL_INT16(16384, accel.z);

    TEST_ASSERT_EQUAL_INT16(1000, gyro.x);
    TEST_ASSERT_EQUAL_INT16(-2000, gyro.y);
    TEST_ASSERT_EQUAL_INT16(3000, gyro.z);

    TEST_ASSERT_EQUAL_INT16(512, temperature);
}

void test_acceleration_g_uses_selected_scale(void) {
    sensor.configureAccel(SteamiISM330DL::AccelOdr::HZ_104, SteamiISM330DL::AccelScale::G_4);
    preloadVector(ISM330DL_REG_OUTX_L_XL, 1000, -2000, 4000);

    SteamiISM330DL::Vector3 value{};
    TEST_ASSERT_TRUE(sensor.accelerationG(value));

    TEST_ASSERT_FLOAT_WITHIN(0.0001F, 0.122F, value.x);
    TEST_ASSERT_FLOAT_WITHIN(0.0001F, -0.244F, value.y);
    TEST_ASSERT_FLOAT_WITHIN(0.0001F, 0.488F, value.z);
}

void test_acceleration_ms2_converts_from_g(void) {
    sensor.configureAccel(SteamiISM330DL::AccelOdr::HZ_104, SteamiISM330DL::AccelScale::G_2);
    preloadVector(ISM330DL_REG_OUTX_L_XL, 1000, 0, -1000);

    SteamiISM330DL::Vector3 value{};
    TEST_ASSERT_TRUE(sensor.accelerationMs2(value));

    const float expected = 0.061F * ISM330DL_STANDARD_GRAVITY;
    TEST_ASSERT_FLOAT_WITHIN(0.0001F, expected, value.x);
    TEST_ASSERT_FLOAT_WITHIN(0.0001F, 0.0F, value.y);
    TEST_ASSERT_FLOAT_WITHIN(0.0001F, -expected, value.z);
}

void test_accel_offset_is_stored_and_applied(void) {
    sensor.configureAccel(SteamiISM330DL::AccelOdr::HZ_104, SteamiISM330DL::AccelScale::G_2);
    preloadVector(ISM330DL_REG_OUTX_L_XL, 1000, 2000, 3000);
    sensor.setAccelOffset(0.010F, 0.020F, 0.030F);

    const SteamiISM330DL::Vector3 offset = sensor.accelOffset();
    TEST_ASSERT_FLOAT_WITHIN(0.0001F, 0.010F, offset.x);
    TEST_ASSERT_FLOAT_WITHIN(0.0001F, 0.020F, offset.y);
    TEST_ASSERT_FLOAT_WITHIN(0.0001F, 0.030F, offset.z);

    SteamiISM330DL::Vector3 value{};
    TEST_ASSERT_TRUE(sensor.accelerationG(value));
    TEST_ASSERT_FLOAT_WITHIN(0.0001F, 0.051F, value.x);
    TEST_ASSERT_FLOAT_WITHIN(0.0001F, 0.102F, value.y);
    TEST_ASSERT_FLOAT_WITHIN(0.0001F, 0.153F, value.z);
}

void test_gyroscope_dps_uses_selected_scale(void) {
    sensor.configureGyro(SteamiISM330DL::GyroOdr::HZ_104, SteamiISM330DL::GyroScale::DPS_500);
    preloadVector(ISM330DL_REG_OUTX_L_G, 1000, -2000, 3000);

    SteamiISM330DL::Vector3 value{};
    TEST_ASSERT_TRUE(sensor.gyroscopeDps(value));

    TEST_ASSERT_FLOAT_WITHIN(0.001F, 17.5F, value.x);
    TEST_ASSERT_FLOAT_WITHIN(0.001F, -35.0F, value.y);
    TEST_ASSERT_FLOAT_WITHIN(0.001F, 52.5F, value.z);
}

void test_gyroscope_rads_converts_from_dps(void) {
    sensor.configureGyro(SteamiISM330DL::GyroOdr::HZ_104, SteamiISM330DL::GyroScale::DPS_250);
    preloadVector(ISM330DL_REG_OUTX_L_G, 1000, 0, -1000);

    SteamiISM330DL::Vector3 value{};
    TEST_ASSERT_TRUE(sensor.gyroscopeRads(value));

    const float expected = 8.75F * ISM330DL_DEG_TO_RAD;
    TEST_ASSERT_FLOAT_WITHIN(0.0001F, expected, value.x);
    TEST_ASSERT_FLOAT_WITHIN(0.0001F, 0.0F, value.y);
    TEST_ASSERT_FLOAT_WITHIN(0.0001F, -expected, value.z);
}

void test_temperature_conversion(void) {
    preloadInt16(ISM330DL_REG_OUT_TEMP_L, 512);

    float value = 0.0F;
    TEST_ASSERT_TRUE(sensor.temperature(value));
    TEST_ASSERT_FLOAT_WITHIN(0.001F, 27.0F, value);
}

void test_temperature_offset_shifts_reading(void) {
    preloadInt16(ISM330DL_REG_OUT_TEMP_L, 512);
    sensor.setTemperatureOffset(-1.5F);

    float value = 0.0F;
    TEST_ASSERT_TRUE(sensor.temperature(value));
    TEST_ASSERT_FLOAT_WITHIN(0.001F, 25.5F, value);
}

void test_temperature_two_point_calibration(void) {
    preloadInt16(ISM330DL_REG_OUT_TEMP_L, 0);  // Factory value = 25 °C.
    TEST_ASSERT_TRUE(sensor.calibrateTemperature(1.0F, 0.0F, 22.0F, 20.0F));

    float value = 0.0F;
    TEST_ASSERT_TRUE(sensor.temperature(value));
    TEST_ASSERT_FLOAT_WITHIN(0.001F, 27.25F, value);
}

void test_temperature_calibration_rejects_equal_measured_points(void) {
    // The refused call keeps the previous correction, gain included (MicroPython
    // raises ValueError with the same effect).
    preloadInt16(ISM330DL_REG_OUT_TEMP_L, 0);  // Factory value = 25 °C.
    TEST_ASSERT_TRUE(sensor.calibrateTemperature(1.0F, 0.0F, 22.0F, 20.0F));  // gain 1.05
    TEST_ASSERT_FALSE(sensor.calibrateTemperature(0.0F, 10.0F, 20.0F, 10.0F));

    float value = 0.0F;
    TEST_ASSERT_TRUE(sensor.temperature(value));
    // 1.05 * 25 + 1.0 = 27.25 (gain reset to 1: 26.0, identity: 25.0)
    TEST_ASSERT_FLOAT_WITHIN(0.001F, 27.25F, value);
}

void test_status_helpers_reflect_individual_bits(void) {
    preloadStatus(0);
    TEST_ASSERT_FALSE(sensor.accelReady());
    TEST_ASSERT_FALSE(sensor.gyroReady());
    TEST_ASSERT_FALSE(sensor.temperatureReady());
    TEST_ASSERT_FALSE(sensor.dataReady());

    preloadStatus(ISM330DL_STATUS_XLDA);
    TEST_ASSERT_TRUE(sensor.accelReady());
    TEST_ASSERT_FALSE(sensor.dataReady());

    preloadStatus(ISM330DL_STATUS_XLDA | ISM330DL_STATUS_GDA);
    TEST_ASSERT_TRUE(sensor.accelReady());
    TEST_ASSERT_TRUE(sensor.gyroReady());
    TEST_ASSERT_FALSE(sensor.temperatureReady());
    TEST_ASSERT_FALSE(sensor.dataReady());

    preloadStatus(ISM330DL_STATUS_ALL_READY);
    TEST_ASSERT_TRUE(sensor.dataReady());
}

void test_power_off_clears_accel_and_gyro_ctrl_registers(void) {
    TEST_ASSERT_TRUE(sensor.powerOff());
    TEST_ASSERT_EQUAL_HEX8(0x00, Wire.getRegister(ADDR, ISM330DL_REG_CTRL1_XL));
    TEST_ASSERT_EQUAL_HEX8(0x00, Wire.getRegister(ADDR, ISM330DL_REG_CTRL2_G));
}

void test_power_on_restores_last_non_power_down_configuration(void) {
    TEST_ASSERT_TRUE(
        sensor.configureAccel(SteamiISM330DL::AccelOdr::HZ_208, SteamiISM330DL::AccelScale::G_8));
    TEST_ASSERT_TRUE(
        sensor.configureGyro(SteamiISM330DL::GyroOdr::HZ_416, SteamiISM330DL::GyroScale::DPS_1000));
    TEST_ASSERT_TRUE(sensor.powerOff());
    TEST_ASSERT_TRUE(sensor.powerOn());

    TEST_ASSERT_EQUAL_HEX8(0x5C, Wire.getRegister(ADDR, ISM330DL_REG_CTRL1_XL));
    TEST_ASSERT_EQUAL_HEX8(0x68, Wire.getRegister(ADDR, ISM330DL_REG_CTRL2_G));
}

void test_read_after_power_off_restores_configuration(void) {
    TEST_ASSERT_TRUE(sensor.powerOff());
    preloadStatus(ISM330DL_STATUS_ALL_READY);
    preloadVector(ISM330DL_REG_OUTX_L_XL, 1000, 0, 0);

    SteamiISM330DL::RawVector value{};
    TEST_ASSERT_TRUE(sensor.accelerationRaw(value));
    TEST_ASSERT_EQUAL_INT16(1000, value.x);
    TEST_ASSERT_EQUAL_HEX8(0x40, Wire.getRegister(ADDR, ISM330DL_REG_CTRL1_XL));
    TEST_ASSERT_EQUAL_HEX8(0x40, Wire.getRegister(ADDR, ISM330DL_REG_CTRL2_G));
}

void test_orientation_detects_each_static_axis(void) {
    struct Case {
        int16_t x;
        int16_t y;
        int16_t z;
        SteamiISM330DL::Orientation expected;
    };

    // At ±2 g, approximately 1 g = 16384 LSB.
    const Case cases[] = {
        {0, 0, 16384, SteamiISM330DL::Orientation::SCREEN_DOWN},
        {0, 0, -16384, SteamiISM330DL::Orientation::SCREEN_UP},
        {16384, 0, 0, SteamiISM330DL::Orientation::TOP_EDGE_DOWN},
        {-16384, 0, 0, SteamiISM330DL::Orientation::BOTTOM_EDGE_DOWN},
        {0, 16384, 0, SteamiISM330DL::Orientation::RIGHT_EDGE_DOWN},
        {0, -16384, 0, SteamiISM330DL::Orientation::LEFT_EDGE_DOWN},
        {100, 100, 100, SteamiISM330DL::Orientation::MOVING},
    };

    for (const auto& item : cases) {
        preloadVector(ISM330DL_REG_OUTX_L_XL, item.x, item.y, item.z);
        SteamiISM330DL::Orientation value = SteamiISM330DL::Orientation::MOVING;
        TEST_ASSERT_TRUE(sensor.orientation(value));
        TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(item.expected), static_cast<uint8_t>(value));
    }
}

void test_motion_classifies_rotation_and_tilt(void) {
    struct Case {
        int16_t x;
        int16_t y;
        int16_t z;
        SteamiISM330DL::MotionType expected;
    };

    // Default sensitivity is 8.75 mdps/LSB. 2000 LSB = 17.5 dps.
    const Case cases[] = {
        {0, 0, 2000, SteamiISM330DL::MotionType::TURNING_RIGHT},
        {0, 0, -2000, SteamiISM330DL::MotionType::TURNING_LEFT},
        {2000, 0, 0, SteamiISM330DL::MotionType::TILTING_LEFT},
        {-2000, 0, 0, SteamiISM330DL::MotionType::TILTING_RIGHT},
        {0, 2000, 0, SteamiISM330DL::MotionType::TILTING_DOWN},
        {0, -2000, 0, SteamiISM330DL::MotionType::TILTING_UP},
        {100, 100, 100, SteamiISM330DL::MotionType::STABLE},
    };

    for (const auto& item : cases) {
        preloadVector(ISM330DL_REG_OUTX_L_G, item.x, item.y, item.z);
        SteamiISM330DL::Motion value{};
        TEST_ASSERT_TRUE(sensor.motion(value));
        TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(item.expected),
                                static_cast<uint8_t>(value.type));
    }
}

void test_orientation_and_motion_string_helpers(void) {
    TEST_ASSERT_EQUAL_STRING(
        "SCREEN_UP", SteamiISM330DL::orientationToString(SteamiISM330DL::Orientation::SCREEN_UP));
    TEST_ASSERT_EQUAL_STRING(
        "MOVING", SteamiISM330DL::orientationToString(SteamiISM330DL::Orientation::MOVING));
    TEST_ASSERT_EQUAL_STRING(
        "TURNING_RIGHT", SteamiISM330DL::motionToString(SteamiISM330DL::MotionType::TURNING_RIGHT));
    TEST_ASSERT_EQUAL_STRING("STABLE",
                             SteamiISM330DL::motionToString(SteamiISM330DL::MotionType::STABLE));
}

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_device_id_returns_who_am_i);
    RUN_TEST(test_is_connected_accepts_expected_who_am_i);
    RUN_TEST(test_is_connected_rejects_wrong_who_am_i);
    RUN_TEST(test_begin_rejects_wrong_who_am_i);

    RUN_TEST(test_configure_accel_writes_expected_register);
    RUN_TEST(test_configure_accel_supports_all_scales);
    RUN_TEST(test_configure_accel_rejects_invalid_values);
    RUN_TEST(test_configure_gyro_writes_expected_register);
    RUN_TEST(test_configure_gyro_supports_all_scales);
    RUN_TEST(test_configure_gyro_rejects_invalid_values);

    RUN_TEST(test_raw_measurements_decode_little_endian_signed_values);
    RUN_TEST(test_acceleration_g_uses_selected_scale);
    RUN_TEST(test_acceleration_ms2_converts_from_g);
    RUN_TEST(test_accel_offset_is_stored_and_applied);
    RUN_TEST(test_gyroscope_dps_uses_selected_scale);
    RUN_TEST(test_gyroscope_rads_converts_from_dps);
    RUN_TEST(test_temperature_conversion);
    RUN_TEST(test_temperature_offset_shifts_reading);
    RUN_TEST(test_temperature_two_point_calibration);
    RUN_TEST(test_temperature_calibration_rejects_equal_measured_points);

    RUN_TEST(test_status_helpers_reflect_individual_bits);
    RUN_TEST(test_power_off_clears_accel_and_gyro_ctrl_registers);
    RUN_TEST(test_power_on_restores_last_non_power_down_configuration);
    RUN_TEST(test_read_after_power_off_restores_configuration);

    RUN_TEST(test_orientation_detects_each_static_axis);
    RUN_TEST(test_motion_classifies_rotation_and_tilt);
    RUN_TEST(test_orientation_and_motion_string_helpers);

    return UNITY_END();
}
