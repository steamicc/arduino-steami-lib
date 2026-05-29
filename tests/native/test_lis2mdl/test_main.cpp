// SPDX-License-Identifier: GPL-3.0-or-later

#include <math.h>
#include <unity.h>

#include "LIS2MDL.h"
#include "Wire.h"
#include "driver_checks.h"

constexpr uint8_t ADDR = LIS2MDL_I2C_ADDR;
constexpr uint8_t WHO_AM_I_VAL = LIS2MDL_WHO_AM_I_VAL;

static void preloadWhoAmI(bool valid = true) {
    Wire.setRegister(ADDR, LIS2MDL_WHO_AM_I, valid ? LIS2MDL_WHO_AM_I_VAL : 0x00);
}

LIS2MDL sensor;

void setUp(void) {
    Wire = TwoWire();
    preloadWhoAmI(true);
    sensor = LIS2MDL(Wire, ADDR);
}

void tearDown(void) {}

void test_begin_detects_device(void) {
    check_begin(sensor);
}

void test_begin_rejects_wrong_who_am_i(void) {
    preloadWhoAmI(false);
    TEST_ASSERT_FALSE(sensor.begin());
}

void test_device_id_returns_who_am_i(void) {
    check_who_am_i(sensor, LIS2MDL_WHO_AM_I_VAL);
}

void test_power_on_sets_continuous_mode(void) {
    sensor.begin();
    sensor.powerOn();
    uint8_t cfgA = Wire.getRegister(ADDR, LIS2MDL_CFG_REG_A);
    TEST_ASSERT_EQUAL_HEX8(0b00, cfgA & 0b11);
}

void test_power_off_sets_idle_mode(void) {
    sensor.begin();
    sensor.powerOff();
    uint8_t cfgA = Wire.getRegister(ADDR, LIS2MDL_CFG_REG_A);
    TEST_ASSERT_EQUAL_HEX8(0b11, cfgA & 0b11);
}

void test_set_continuous_writes_expected_cfg_reg_a(void) {
    sensor.begin();
    sensor.setContinuous(50);
    uint8_t cfgA = Wire.getRegister(ADDR, LIS2MDL_CFG_REG_A);
    TEST_ASSERT_EQUAL_HEX8(0b10 << 2, cfgA & (0b11 << 2));
}

void test_set_odr_writes_expected_bits(void) {
    sensor.begin();
    sensor.setOdr(100);
    uint8_t cfgA = Wire.getRegister(ADDR, LIS2MDL_CFG_REG_A);
    TEST_ASSERT_EQUAL_HEX8(0b11 << 2, cfgA & (0b11 << 2));
}

void test_trigger_one_shot_sets_single_mode(void) {
    sensor.begin();
    sensor.triggerOneShot();
    uint8_t cfgA = Wire.getRegister(ADDR, LIS2MDL_CFG_REG_A);
    TEST_ASSERT_EQUAL_HEX8(0b01, cfgA & 0b11);
}

void test_data_ready_reflects_status_register(void) {
    sensor.begin();
    Wire.setRegister(ADDR, LIS2MDL_STATUS_REG, 0b1000);
    TEST_ASSERT_TRUE(sensor.dataReady());
    Wire.setRegister(ADDR, LIS2MDL_STATUS_REG, 0b0000);
    TEST_ASSERT_FALSE(sensor.dataReady());
}

void test_magnetic_field_returns_signed_values(void) {
    sensor.begin();
    Wire.setRegister(ADDR, LIS2MDL_OUTX_L_REG, 0x34);
    Wire.setRegister(ADDR, LIS2MDL_OUTX_H_REG, 0x12);
    Wire.setRegister(ADDR, LIS2MDL_OUTY_L_REG, 0x78);
    Wire.setRegister(ADDR, LIS2MDL_OUTY_H_REG, 0x56);
    Wire.setRegister(ADDR, LIS2MDL_OUTZ_L_REG, 0xBC);
    Wire.setRegister(ADDR, LIS2MDL_OUTZ_H_REG, 0x9A);

    MagneticField field = sensor.magneticField();
    TEST_ASSERT_EQUAL_INT16(0x1234, field.x);
    TEST_ASSERT_EQUAL_INT16(0x5678, field.y);
    TEST_ASSERT_EQUAL_INT16((int16_t)0x9ABC, field.z);
}

void test_magnetic_field_ut_applies_lsb_conversion(void) {
    sensor.begin();
    Wire.setRegister(ADDR, LIS2MDL_OUTX_L_REG, 0x34);
    Wire.setRegister(ADDR, LIS2MDL_OUTX_H_REG, 0x12);

    MagneticFieldUt field = sensor.magneticFieldUt();
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.15f * 0x1234, field.x);
}

void test_calibrated_field_applies_offset_and_scale(void) {
    sensor.begin();
    sensor.setCalibrateStep(10.0f, 20.0f, 30.0f, 2.0f, 3.0f, 4.0f);
    Wire.setRegister(ADDR, LIS2MDL_OUTX_L_REG, 0x34);
    Wire.setRegister(ADDR, LIS2MDL_OUTX_H_REG, 0x12);
    Wire.setRegister(ADDR, LIS2MDL_OUTY_L_REG, 0x78);
    Wire.setRegister(ADDR, LIS2MDL_OUTY_H_REG, 0x56);
    Wire.setRegister(ADDR, LIS2MDL_OUTZ_L_REG, 0xBC);
    Wire.setRegister(ADDR, LIS2MDL_OUTZ_H_REG, 0x9A);

    CalibratedField field = sensor.calibratedField();
    TEST_ASSERT_FLOAT_WITHIN(0.01f, (0x1234 - 10.0f) / 2.0f, field.x);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, (0x5678 - 20.0f) / 3.0f, field.y);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, ((int16_t)0x9ABC - 30.0f) / 4.0f, field.z);
}

void test_magnitude_ut_is_positive(void) {
    sensor.begin();
    Wire.setRegister(ADDR, LIS2MDL_OUTX_L_REG, 0x34);
    Wire.setRegister(ADDR, LIS2MDL_OUTX_H_REG, 0x12);
    Wire.setRegister(ADDR, LIS2MDL_OUTY_L_REG, 0x78);
    Wire.setRegister(ADDR, LIS2MDL_OUTY_H_REG, 0x56);
    Wire.setRegister(ADDR, LIS2MDL_OUTZ_L_REG, 0xBC);
    Wire.setRegister(ADDR, LIS2MDL_OUTZ_H_REG, 0x9A);

    float mag = sensor.magnitudeUt();
    TEST_ASSERT_TRUE(mag > 0.0f);
}

void test_read_all_returns_all_channels(void) {
    sensor.begin();
    Wire.setRegister(ADDR, LIS2MDL_STATUS_REG, 0b1000);
    Wire.setRegister(ADDR, LIS2MDL_OUTX_L_REG, 0x34);
    Wire.setRegister(ADDR, LIS2MDL_OUTX_H_REG, 0x12);
    Wire.setRegister(ADDR, LIS2MDL_OUTY_L_REG, 0x78);
    Wire.setRegister(ADDR, LIS2MDL_OUTY_H_REG, 0x56);
    Wire.setRegister(ADDR, LIS2MDL_OUTZ_L_REG, 0xBC);
    Wire.setRegister(ADDR, LIS2MDL_OUTZ_H_REG, 0x9A);
    Wire.setRegister(ADDR, LIS2MDL_TEMP_OUT_L_REG, 0x00);
    Wire.setRegister(ADDR, LIS2MDL_TEMP_OUT_H_REG, 0x00);

    ReadAll all = sensor.readAll();
    TEST_ASSERT_EQUAL_INT16(0x1234, all.raw.x);
    TEST_ASSERT_EQUAL_INT16(0x5678, all.raw.y);
    TEST_ASSERT_EQUAL_INT16((int16_t)0x9ABC, all.raw.z);
    TEST_ASSERT_TRUE(all.status == 0b1000);
}

void test_read_one_shot_returns_magnetic_field(void) {
    sensor.begin();
    Wire.setRegister(ADDR, LIS2MDL_STATUS_REG, 0b1000);
    Wire.setRegister(ADDR, LIS2MDL_OUTX_L_REG, 0x34);
    Wire.setRegister(ADDR, LIS2MDL_OUTX_H_REG, 0x12);
    Wire.setRegister(ADDR, LIS2MDL_OUTY_L_REG, 0x78);
    Wire.setRegister(ADDR, LIS2MDL_OUTY_H_REG, 0x56);
    Wire.setRegister(ADDR, LIS2MDL_OUTZ_L_REG, 0xBC);
    Wire.setRegister(ADDR, LIS2MDL_OUTZ_H_REG, 0x9A);

    MagneticField field = sensor.readOneShot();
    TEST_ASSERT_EQUAL_INT16(0x1234, field.x);
    TEST_ASSERT_EQUAL_INT16(0x5678, field.y);
    TEST_ASSERT_EQUAL_INT16((int16_t)0x9ABC, field.z);
}

void test_read_one_shot_returns_zeros_on_timeout(void) {
    sensor.begin();
    Wire.setRegister(ADDR, LIS2MDL_STATUS_REG, 0b0000);
    Wire.setRegister(ADDR, LIS2MDL_OUTX_L_REG, 0x34);
    Wire.setRegister(ADDR, LIS2MDL_OUTX_H_REG, 0x12);
    Wire.setRegister(ADDR, LIS2MDL_OUTY_L_REG, 0x78);
    Wire.setRegister(ADDR, LIS2MDL_OUTY_H_REG, 0x56);
    Wire.setRegister(ADDR, LIS2MDL_OUTZ_L_REG, 0xBC);
    Wire.setRegister(ADDR, LIS2MDL_OUTZ_H_REG, 0x9A);

    MagneticField field = sensor.readOneShot();
    TEST_ASSERT_EQUAL_INT16(0x0000, field.x);
    TEST_ASSERT_EQUAL_INT16(0x0000, field.y);
    TEST_ASSERT_EQUAL_INT16(0x0000, field.z);
}

void test_ensure_data_returns_false_on_timeout(void) {
    sensor.begin();
    sensor.setMode("powerdown");
    Wire.setRegister(ADDR, LIS2MDL_STATUS_REG, 0b0000);
    MagneticField field = sensor.magneticField();

    TEST_ASSERT_EQUAL_INT16(0x0000, field.x);
    TEST_ASSERT_EQUAL_INT16(0x0000, field.y);
    TEST_ASSERT_EQUAL_INT16(0x0000, field.z);
}

void test_temperature_raw_is_signed(void) {
    sensor.begin();
    Wire.setRegister(ADDR, LIS2MDL_TEMP_OUT_L_REG, 0x34);
    Wire.setRegister(ADDR, LIS2MDL_TEMP_OUT_H_REG, 0x12);

    int16_t tempRaw = sensor.readTemperatureRaw();
    TEST_ASSERT_EQUAL_INT16(0x1234, tempRaw);
}

void test_set_temp_offset_shifts_reading(void) {
    sensor.begin();
    Wire.setRegister(ADDR, LIS2MDL_TEMP_OUT_L_REG, 0x00);
    Wire.setRegister(ADDR, LIS2MDL_TEMP_OUT_H_REG, 0x00);
    Wire.setRegister(ADDR, LIS2MDL_STATUS_REG, 0b1000);
    float baseTemp = sensor.temperature();

    sensor.setTempOffset(10.0f);
    float tempC = sensor.temperature();
    TEST_ASSERT_FLOAT_WITHIN(0.01f, baseTemp + 10.0f, tempC);
}

void test_calibrate_temperature_applies_two_point_correction(void) {
    sensor.begin();
    Wire.setRegister(ADDR, LIS2MDL_TEMP_OUT_L_REG, 0x00);
    Wire.setRegister(ADDR, LIS2MDL_TEMP_OUT_H_REG, 0x00);

    sensor.calibrateTemperature(21.0f, 20.0f, 27.0f, 25.0f);

    float tempC = sensor.temperature();
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 27.0f, tempC);
}

void test_set_low_power_sets_bit4_cfg_reg_a(void) {
    sensor.begin();
    sensor.setLowPower(true);
    uint8_t cfgA = Wire.getRegister(ADDR, LIS2MDL_CFG_REG_A);
    TEST_ASSERT_EQUAL_HEX8(0b1 << 4, cfgA & (0b1 << 4));
}

void test_set_low_pass_sets_bit0_cfg_reg_b(void) {
    sensor.begin();
    sensor.setLowPass(true);
    uint8_t cfgB = Wire.getRegister(ADDR, LIS2MDL_CFG_REG_B);
    TEST_ASSERT_EQUAL_HEX8(0b1, cfgB & 0b1);
}

void test_set_offset_cancellation_sets_bits_cfg_reg_b(void) {
    sensor.begin();
    sensor.setOffsetCancellation(true, false);
    uint8_t cfgB = Wire.getRegister(ADDR, LIS2MDL_CFG_REG_B);
    TEST_ASSERT_EQUAL_HEX8(0b10, cfgB & 0b110);

    sensor.setOffsetCancellation(true, true);
    cfgB = Wire.getRegister(ADDR, LIS2MDL_CFG_REG_B);
    TEST_ASSERT_EQUAL_HEX8(0b110, cfgB & 0b110);
}

void test_set_bdu_sets_bit4_cfg_reg_c(void) {
    sensor.begin();
    sensor.setBdu(true);
    uint8_t cfgC = Wire.getRegister(ADDR, LIS2MDL_CFG_REG_C);
    TEST_ASSERT_EQUAL_HEX8(0b1 << 4, cfgC & (0b1 << 4));
}

void test_set_hw_offsets_writes_offset_registers(void) {
    sensor.begin();
    sensor.setHwOffsets(0x12, 0x34, 0x56);
    uint8_t xOffsetL = Wire.getRegister(ADDR, LIS2MDL_OFFSET_X_REG_L);
    uint8_t xOffsetH = Wire.getRegister(ADDR, LIS2MDL_OFFSET_X_REG_H);
    uint8_t yOffsetL = Wire.getRegister(ADDR, LIS2MDL_OFFSET_Y_REG_L);
    uint8_t yOffsetH = Wire.getRegister(ADDR, LIS2MDL_OFFSET_Y_REG_H);
    uint8_t zOffsetL = Wire.getRegister(ADDR, LIS2MDL_OFFSET_Z_REG_L);
    uint8_t zOffsetH = Wire.getRegister(ADDR, LIS2MDL_OFFSET_Z_REG_H);

    TEST_ASSERT_EQUAL_HEX8(0x12, xOffsetL);
    TEST_ASSERT_EQUAL_HEX8(0x00, xOffsetH);
    TEST_ASSERT_EQUAL_HEX8(0x34, yOffsetL);
    TEST_ASSERT_EQUAL_HEX8(0x00, yOffsetH);
    TEST_ASSERT_EQUAL_HEX8(0x56, zOffsetL);
    TEST_ASSERT_EQUAL_HEX8(0x00, zOffsetH);
}

void test_read_hw_offsets_returns_written_values(void) {
    sensor.begin();
    Wire.setRegister(ADDR, LIS2MDL_OFFSET_X_REG_L, 0x12);
    Wire.setRegister(ADDR, LIS2MDL_OFFSET_Y_REG_L, 0x34);
    Wire.setRegister(ADDR, LIS2MDL_OFFSET_Z_REG_L, 0x56);

    HwOffsets offsets = sensor.readHwOffsets();
    TEST_ASSERT_EQUAL_HEX8(0x12, offsets.x);
    TEST_ASSERT_EQUAL_HEX8(0x34, offsets.y);
    TEST_ASSERT_EQUAL_HEX8(0x56, offsets.z);
}

void test_calibrate_reset_clears_offsets_and_scales(void) {
    sensor.begin();
    sensor.setCalibrateStep(10.0f, 20.0f, 30.0f, 2.0f, 3.0f, 4.0f);
    Wire.setRegister(ADDR, LIS2MDL_OUTX_L_REG, 0x34);
    Wire.setRegister(ADDR, LIS2MDL_OUTX_H_REG, 0x12);
    Wire.setRegister(ADDR, LIS2MDL_STATUS_REG, 0b1000);

    sensor.calibrateReset();

    CalibratedField field = sensor.calibratedField();
    TEST_ASSERT_FLOAT_WITHIN(0.01f, (float)0x1234, field.x);
}

void test_calibrate_apply_normalizes_values(void) {
    sensor.begin();
    sensor.setCalibrateStep(10.0f, 20.0f, 30.0f, 2.0f, 3.0f, 4.0f);
    Wire.setRegister(ADDR, LIS2MDL_OUTX_L_REG, 0x34);
    Wire.setRegister(ADDR, LIS2MDL_OUTX_H_REG, 0x12);
    Wire.setRegister(ADDR, LIS2MDL_OUTY_L_REG, 0x78);
    Wire.setRegister(ADDR, LIS2MDL_OUTY_H_REG, 0x56);
    Wire.setRegister(ADDR, LIS2MDL_OUTZ_L_REG, 0xBC);
    Wire.setRegister(ADDR, LIS2MDL_OUTZ_H_REG, 0x9A);

    CalibratedField field = sensor.calibrateApply(0x1234, 0x5678, (int16_t)0x9ABC);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, (0x1234 - 10.0f) / 2.0f, field.x);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, (0x5678 - 20.0f) / 3.0f, field.y);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, ((int16_t)0x9ABC - 30.0f) / 4.0f, field.z);
}

void test_heading_flat_only_returns_angle_in_0_360(void) {
    sensor.begin();
    Wire.setRegister(ADDR, LIS2MDL_OUTX_L_REG, 0x34);
    Wire.setRegister(ADDR, LIS2MDL_OUTX_H_REG, 0x12);
    Wire.setRegister(ADDR, LIS2MDL_OUTY_L_REG, 0x78);
    Wire.setRegister(ADDR, LIS2MDL_OUTY_H_REG, 0x56);
    Wire.setRegister(ADDR, LIS2MDL_OUTZ_L_REG, 0xBC);
    Wire.setRegister(ADDR, LIS2MDL_OUTZ_H_REG, 0x9A);

    float heading = sensor.headingFlatOnly();
    TEST_ASSERT_TRUE(heading >= 0.0f && heading < 360.0f);
}

void test_heading_filter_smooths_angle(void) {
    sensor.begin();
    sensor.setHeadingFilter(0.5f);
    Wire.setRegister(ADDR, LIS2MDL_OUTX_L_REG, 0x34);
    Wire.setRegister(ADDR, LIS2MDL_OUTX_H_REG, 0x12);
    Wire.setRegister(ADDR, LIS2MDL_OUTY_L_REG, 0x78);
    Wire.setRegister(ADDR, LIS2MDL_OUTY_H_REG, 0x56);
    Wire.setRegister(ADDR, LIS2MDL_OUTZ_L_REG, 0xBC);
    Wire.setRegister(ADDR, LIS2MDL_OUTZ_H_REG, 0x9A);

    float heading1 = sensor.headingFlatOnly();
    float heading2 = sensor.headingFlatOnly();
    TEST_ASSERT_FLOAT_WITHIN(0.01f, heading1, heading2);
}

void test_normalize_deg_wraps_negative_angles(void) {
    sensor.begin();
    sensor.setHeadingOffset(-180.0f);
    float heading = sensor.headingFromVectors(0.0f, 1.0f, 0.0f, false);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 270.0f, heading);
    sensor.setHeadingOffset(0.0f);
}

void test_direction_label_returns_correct_cardinal(void) {
    sensor.begin();
    const char* label = sensor.directionLabel(45.0f);
    TEST_ASSERT_EQUAL_STRING("NE", label);

    label = sensor.directionLabel(135.0f);
    TEST_ASSERT_EQUAL_STRING("SE", label);

    label = sensor.directionLabel(225.0f);
    TEST_ASSERT_EQUAL_STRING("SW", label);

    label = sensor.directionLabel(315.0f);
    TEST_ASSERT_EQUAL_STRING("NW", label);
}

void test_soft_reset_writes_bit5_cfg_reg_a(void) {
    sensor.begin();
    sensor.softReset();
    uint8_t cfgA = Wire.getRegister(ADDR, LIS2MDL_CFG_REG_A);
    TEST_ASSERT_EQUAL_HEX8(0b1 << 5, cfgA & (0b1 << 5));
}

void test_reboot_writes_bit6_cfg_reg_a(void) {
    sensor.begin();
    sensor.reboot();
    uint8_t cfgA = Wire.getRegister(ADDR, LIS2MDL_CFG_REG_A);
    TEST_ASSERT_EQUAL_HEX8(0b1 << 6, cfgA & (0b1 << 6));
}

void test_get_mode_returns_correct_string(void) {
    sensor.begin();
    sensor.powerOff();
    const char* mode = sensor.getMode();
    TEST_ASSERT_EQUAL_STRING("idle", mode);
}

void test_is_idle_returns_true_when_md_bits_are_11(void) {
    sensor.begin();
    sensor.setMode("powerdown");
    TEST_ASSERT_TRUE(sensor.isIdle());
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_begin_detects_device);
    RUN_TEST(test_begin_rejects_wrong_who_am_i);
    RUN_TEST(test_device_id_returns_who_am_i);
    RUN_TEST(test_power_on_sets_continuous_mode);
    RUN_TEST(test_power_off_sets_idle_mode);
    RUN_TEST(test_set_continuous_writes_expected_cfg_reg_a);
    RUN_TEST(test_set_odr_writes_expected_bits);
    RUN_TEST(test_trigger_one_shot_sets_single_mode);
    RUN_TEST(test_data_ready_reflects_status_register);
    RUN_TEST(test_magnetic_field_returns_signed_values);
    RUN_TEST(test_magnetic_field_ut_applies_lsb_conversion);
    RUN_TEST(test_calibrated_field_applies_offset_and_scale);
    RUN_TEST(test_magnitude_ut_is_positive);
    RUN_TEST(test_read_all_returns_all_channels);
    RUN_TEST(test_read_one_shot_returns_magnetic_field);
    RUN_TEST(test_read_one_shot_returns_zeros_on_timeout);
    RUN_TEST(test_temperature_raw_is_signed);
    RUN_TEST(test_set_temp_offset_shifts_reading);
    RUN_TEST(test_calibrate_temperature_applies_two_point_correction);
    RUN_TEST(test_set_low_power_sets_bit4_cfg_reg_a);
    RUN_TEST(test_set_low_pass_sets_bit0_cfg_reg_b);
    RUN_TEST(test_set_offset_cancellation_sets_bits_cfg_reg_b);
    RUN_TEST(test_set_bdu_sets_bit4_cfg_reg_c);
    RUN_TEST(test_set_hw_offsets_writes_offset_registers);
    RUN_TEST(test_read_hw_offsets_returns_written_values);
    RUN_TEST(test_calibrate_reset_clears_offsets_and_scales);
    RUN_TEST(test_calibrate_apply_normalizes_values);
    RUN_TEST(test_heading_flat_only_returns_angle_in_0_360);
    RUN_TEST(test_heading_filter_smooths_angle);
    RUN_TEST(test_normalize_deg_wraps_negative_angles);
    RUN_TEST(test_direction_label_returns_correct_cardinal);
    RUN_TEST(test_soft_reset_writes_bit5_cfg_reg_a);
    RUN_TEST(test_reboot_writes_bit6_cfg_reg_a);
    RUN_TEST(test_get_mode_returns_correct_string);
    RUN_TEST(test_is_idle_returns_true_when_md_bits_are_11);
    return UNITY_END();
}