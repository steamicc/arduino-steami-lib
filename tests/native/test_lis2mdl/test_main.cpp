// SPDX-License-Identifier: GPL-3.0-or-later

#include <math.h>
#include <unity.h>

#include <array>
#include <map>
#include <vector>

#include "SteamiLIS2MDL.h"
#include "Wire.h"
#include "driver_checks.h"

constexpr uint8_t ADDR = LIS2MDL_I2C_ADDR;

static void preloadWhoAmI(bool valid = true) {
    Wire.setRegister(ADDR, LIS2MDL_WHO_AM_I, valid ? LIS2MDL_WHO_AM_I_VAL : 0x00);
}

SteamiLIS2MDL sensor;

void setUp(void) {
    Wire = TwoWire();
    preloadWhoAmI(true);
    sensor = SteamiLIS2MDL(Wire, ADDR);
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

// read_calibration() of the MicroPython driver (#316): the values in the
// setCalibrateStep() order.
void test_read_calibration_returns_offsets_and_scales(void) {
    sensor.begin();
    sensor.setCalibrateStep(10.0f, -20.0f, 30.0f, 2.0f, 3.0f, 4.0f);
    MagCalibration cal = sensor.readCalibration();
    TEST_ASSERT_EQUAL_FLOAT(10.0f, cal.xOff);
    TEST_ASSERT_EQUAL_FLOAT(-20.0f, cal.yOff);
    TEST_ASSERT_EQUAL_FLOAT(30.0f, cal.zOff);
    TEST_ASSERT_EQUAL_FLOAT(2.0f, cal.xScale);
    TEST_ASSERT_EQUAL_FLOAT(3.0f, cal.yScale);
    TEST_ASSERT_EQUAL_FLOAT(4.0f, cal.zScale);

    sensor.calibrateReset();
    cal = sensor.readCalibration();
    TEST_ASSERT_EQUAL_FLOAT(0.0f, cal.xOff);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, cal.yOff);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, cal.zOff);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, cal.xScale);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, cal.yScale);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, cal.zScale);
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

static void setField(int16_t x, int16_t y, int16_t z) {
    const uint16_t ux = static_cast<uint16_t>(x);
    const uint16_t uy = static_cast<uint16_t>(y);
    const uint16_t uz = static_cast<uint16_t>(z);
    Wire.setRegister(ADDR, LIS2MDL_OUTX_L_REG, ux & 0xFF);
    Wire.setRegister(ADDR, LIS2MDL_OUTX_H_REG, (ux >> 8) & 0xFF);
    Wire.setRegister(ADDR, LIS2MDL_OUTY_L_REG, uy & 0xFF);
    Wire.setRegister(ADDR, LIS2MDL_OUTY_H_REG, (uy >> 8) & 0xFF);
    Wire.setRegister(ADDR, LIS2MDL_OUTZ_L_REG, uz & 0xFF);
    Wire.setRegister(ADDR, LIS2MDL_OUTZ_H_REG, (uz >> 8) & 0xFF);
}

void test_magnetic_field_reads_in_one_burst(void) {
    sensor.begin();
    setField(0x1234, -200, 300);
    const size_t before = Wire.transactions(ADDR);
    MagneticField field = {0, 0, 0};
    TEST_ASSERT_TRUE(sensor.magneticField(field));
    // CFG_REG_A for the idle check, then the six output bytes: one address write and one read
    // each. Six single-byte reads would take twelve transactions for the field alone.
    TEST_ASSERT_EQUAL_UINT32(4, Wire.transactions(ADDR) - before);
    TEST_ASSERT_EQUAL_INT16(0x1234, field.x);
    TEST_ASSERT_EQUAL_INT16(-200, field.y);
    TEST_ASSERT_EQUAL_INT16(300, field.z);
}

void test_magnetic_field_reports_absent_chip(void) {
    sensor.begin();
    setField(0x1234, 0x5678, 0x0102);
    Wire.setAbsent(ADDR);
    MagneticField field = {1, 2, 3};
    TEST_ASSERT_FALSE(sensor.magneticField(field));
    TEST_ASSERT_EQUAL_INT16(0, field.x);
    TEST_ASSERT_EQUAL_INT16(0, field.y);
    TEST_ASSERT_EQUAL_INT16(0, field.z);
    const MagneticField plain = sensor.magneticField();
    TEST_ASSERT_EQUAL_INT16(0, plain.x);
    TEST_ASSERT_EQUAL_INT16(0, plain.y);
    TEST_ASSERT_EQUAL_INT16(0, plain.z);
}

void test_magnetic_field_reports_failed_address_write(void) {
    sensor.begin();
    setField(0x1234, 0x5678, 0x0102);
    Wire.setEndTransmissionResult(4);
    MagneticField field = {1, 2, 3};
    TEST_ASSERT_FALSE(sensor.magneticField(field));
    TEST_ASSERT_EQUAL_INT16(0, field.x);
}

void test_magnetic_field_reports_data_ready_timeout(void) {
    sensor.begin();
    sensor.setMode("powerdown");
    Wire.setRegister(ADDR, LIS2MDL_STATUS_REG, 0b0000);
    setField(0x1234, 0x5678, 0x0102);  // stale values: never returned
    MagneticField field = {1, 2, 3};
    TEST_ASSERT_FALSE(sensor.magneticField(field));
    TEST_ASSERT_EQUAL_INT16(0, field.x);
    TEST_ASSERT_EQUAL_INT16(0, field.y);
    TEST_ASSERT_EQUAL_INT16(0, field.z);
}

void test_float_readings_are_nan_on_bus_error(void) {
    sensor.begin();
    setField(0x1234, 0x5678, 0x0102);
    Wire.setAbsent(ADDR);
    const MagneticFieldUt ut = sensor.magneticFieldUt();
    TEST_ASSERT_TRUE(isnan(ut.x) && isnan(ut.y) && isnan(ut.z));
    const CalibratedField cal = sensor.calibratedField();
    TEST_ASSERT_TRUE(isnan(cal.x) && isnan(cal.y) && isnan(cal.z));
    TEST_ASSERT_TRUE(isnan(sensor.magnitudeUt()));
}

void test_heading_flat_only_is_nan_on_bus_error(void) {
    sensor.begin();
    setField(0, 300, 0);
    Wire.setAbsent(ADDR);
    TEST_ASSERT_TRUE(isnan(sensor.headingFlatOnly()));
    TEST_ASSERT_EQUAL_STRING("", sensor.directionLabel());
    TEST_ASSERT_EQUAL_STRING("", sensor.directionLabel(NAN));
}

static void assertCalibration(float xo, float yo, float zo, float xs, float ys, float zs) {
    TEST_ASSERT_FLOAT_WITHIN(0.001f, xo, sensor.xOff);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, yo, sensor.yOff);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, zo, sensor.zOff);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, xs, sensor.xScale);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, ys, sensor.yScale);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, zs, sensor.zScale);
}

void test_calibrate_minmax_keeps_calibration_on_bus_error(void) {
    sensor.begin();
    sensor.setCalibrateStep(10.0f, 20.0f, 30.0f, 2.0f, 3.0f, 4.0f);
    Wire.setAbsent(ADDR);
    TEST_ASSERT_FALSE(sensor.calibrateMinmax3d(5, 0));
    TEST_ASSERT_FALSE(sensor.calibrateMinmax2d(5, 0));
    TEST_ASSERT_FALSE(sensor.calibrateStep());
    assertCalibration(10.0f, 20.0f, 30.0f, 2.0f, 3.0f, 4.0f);
}

// LIS2MDL register map that serves one field per burst read from OUTX_L, in
// order, and fails every burst once the list is used up: a bus error in the
// middle of a calibration.
class ScriptedMagnetometer : public I2CDevice {
   public:
    std::vector<std::array<int16_t, 3>> fields;

    ScriptedMagnetometer() { regs_[LIS2MDL_WHO_AM_I] = LIS2MDL_WHO_AM_I_VAL; }

    uint8_t onWrite(const std::vector<uint8_t>& frame) override {
        if (frame.empty()) {
            return 0;
        }
        reg_ = frame[0];
        for (size_t i = 1; i < frame.size(); i++) {
            regs_[static_cast<uint8_t>(reg_ + i - 1)] = frame[i];
        }
        return 0;
    }

    std::vector<uint8_t> onRead(size_t quantity) override {
        std::vector<uint8_t> out;
        if (reg_ == LIS2MDL_OUTX_L_REG && quantity == 6) {
            if (next_ >= fields.size()) {
                return out;  // the chip stops answering
            }
            for (int16_t v : fields[next_++]) {
                const uint16_t u = static_cast<uint16_t>(v);
                out.push_back(u & 0xFF);
                out.push_back(u >> 8);
            }
            return out;
        }
        for (size_t i = 0; i < quantity; i++) {
            out.push_back(regs_[static_cast<uint8_t>(reg_ + i)]);
        }
        return out;
    }

   private:
    std::map<uint8_t, uint8_t> regs_;
    uint8_t reg_ = 0;
    size_t next_ = 0;
};

void test_calibrate_minmax_3d_uses_every_sample(void) {
    ScriptedMagnetometer chip;
    chip.fields = {{100, -50, 10}, {300, 150, -30}, {200, 50, 90}};
    Wire.attachDevice(ADDR, &chip);
    TEST_ASSERT_TRUE(sensor.begin());
    TEST_ASSERT_TRUE(sensor.calibrateMinmax3d(3, 0));
    assertCalibration(200.0f, 50.0f, 30.0f, 100.0f, 100.0f, 60.0f);
}

void test_calibrate_minmax_2d_uses_every_sample_and_keeps_z(void) {
    ScriptedMagnetometer chip;
    chip.fields = {{100, -50, 10}, {300, 250, 20}};
    Wire.attachDevice(ADDR, &chip);
    TEST_ASSERT_TRUE(sensor.begin());
    sensor.setCalibrateStep(0.0f, 0.0f, 30.0f, 1.0f, 1.0f, 4.0f);
    TEST_ASSERT_TRUE(sensor.calibrateMinmax2d(2, 0));
    // X range 200, Y range 300: both scales take the mean half-range, 125.
    assertCalibration(200.0f, 100.0f, 30.0f, 125.0f, 125.0f, 4.0f);
}

void test_calibrate_minmax_3d_keeps_calibration_when_a_sample_fails(void) {
    // One good sample, then the bus fails: the old code built a degenerate
    // calibration (offsets = that sample, scales = 1) from it.
    ScriptedMagnetometer chip;
    chip.fields = {{400, -300, 200}};
    Wire.attachDevice(ADDR, &chip);
    TEST_ASSERT_TRUE(sensor.begin());
    sensor.setCalibrateStep(10.0f, 20.0f, 30.0f, 2.0f, 3.0f, 4.0f);
    TEST_ASSERT_FALSE(sensor.calibrateMinmax3d(5, 0));
    assertCalibration(10.0f, 20.0f, 30.0f, 2.0f, 3.0f, 4.0f);
}

void test_calibrate_minmax_2d_keeps_calibration_when_a_sample_fails(void) {
    ScriptedMagnetometer chip;
    chip.fields = {{400, -300, 200}, {100, 100, 0}};
    Wire.attachDevice(ADDR, &chip);
    TEST_ASSERT_TRUE(sensor.begin());
    sensor.setCalibrateStep(10.0f, 20.0f, 30.0f, 2.0f, 3.0f, 4.0f);
    TEST_ASSERT_FALSE(sensor.calibrateMinmax2d(5, 0));
    assertCalibration(10.0f, 20.0f, 30.0f, 2.0f, 3.0f, 4.0f);
}

void test_calibrate_minmax_with_zero_samples_keeps_calibration(void) {
    sensor.begin();
    setField(400, -300, 200);
    sensor.setCalibrateStep(10.0f, 20.0f, 30.0f, 2.0f, 3.0f, 4.0f);
    TEST_ASSERT_FALSE(sensor.calibrateMinmax3d(0, 0));
    TEST_ASSERT_FALSE(sensor.calibrateMinmax2d(0, 0));
    assertCalibration(10.0f, 20.0f, 30.0f, 2.0f, 3.0f, 4.0f);
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

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    sensor.setTempOffset(10.0f);
#pragma GCC diagnostic pop
    float tempC = sensor.temperature();
    TEST_ASSERT_FLOAT_WITHIN(0.01f, baseTemp + 10.0f, tempC);
}

void test_set_temperature_offset_shifts_reading(void) {
    sensor.begin();
    Wire.setRegister(ADDR, LIS2MDL_TEMP_OUT_L_REG, 0x00);
    Wire.setRegister(ADDR, LIS2MDL_TEMP_OUT_H_REG, 0x00);
    Wire.setRegister(ADDR, LIS2MDL_STATUS_REG, 0b1000);
    float baseTemp = sensor.temperature();

    sensor.setTemperatureOffset(-3.5f);
    float tempC = sensor.temperature();
    TEST_ASSERT_FLOAT_WITHIN(0.01f, baseTemp - 3.5f, tempC);
}

void test_set_temperature_offset_resets_two_point_gain(void) {
    sensor.begin();
    Wire.setRegister(ADDR, LIS2MDL_TEMP_OUT_L_REG, 0x00);
    Wire.setRegister(ADDR, LIS2MDL_TEMP_OUT_H_REG, 0x00);
    Wire.setRegister(ADDR, LIS2MDL_STATUS_REG, 0b1000);
    sensor.setTemperatureOffset(0.0f);
    float baseTemp = sensor.temperature();

    sensor.calibrateTemperature(21.0f, 20.0f, 27.0f, 25.0f);
    sensor.setTemperatureOffset(2.0f);
    float tempC = sensor.temperature();
    TEST_ASSERT_FLOAT_WITHIN(0.01f, baseTemp + 2.0f, tempC);
}

void test_set_temp_offset_is_alias_of_set_temperature_offset(void) {
    sensor.begin();
    Wire.setRegister(ADDR, LIS2MDL_TEMP_OUT_L_REG, 0x40);
    Wire.setRegister(ADDR, LIS2MDL_TEMP_OUT_H_REG, 0x00);
    Wire.setRegister(ADDR, LIS2MDL_STATUS_REG, 0b1000);

    sensor.setTemperatureOffset(4.25f);
    float viaCanonical = sensor.temperature();
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    sensor.setTempOffset(4.25f);
#pragma GCC diagnostic pop
    float viaAlias = sensor.temperature();
    TEST_ASSERT_FLOAT_WITHIN(0.001f, viaCanonical, viaAlias);
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
    float heading = sensor.headingFromVectors(-1.0f, 0.0f, 0.0f, false);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 270.0f, heading);
    sensor.setHeadingOffset(0.0f);
}

void test_tiny_negative_angle_never_gives_360(void) {
    sensor.begin();
    sensor.setHeadingOffset(0.0f);
    sensor.setDeclination(0.0f);
    sensor.setHeadingFilter(0.0f);
    // atan2(-1e-9, 1) is a tiny negative angle: + 360 rounds to 360.0f in float.
    const float heading = sensor.headingFromVectors(1e-9f, 1.0f, 0.0f, false);
    TEST_ASSERT_TRUE(heading >= 0.0f && heading < 360.0f);
}

void test_heading_convention_matches_board(void) {
    sensor.begin();
    sensor.setHeadingOffset(0.0f);
    sensor.setDeclination(0.0f);
    sensor.setHeadingFilter(0.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, sensor.headingFromVectors(0.0f, 1.0f, 0.0f, false));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 90.0f, sensor.headingFromVectors(-1.0f, 0.0f, 0.0f, false));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 180.0f, sensor.headingFromVectors(0.0f, -1.0f, 0.0f, false));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 270.0f, sensor.headingFromVectors(1.0f, 0.0f, 0.0f, false));
}

// --- Tilt-compensated heading ---
//
// Board frame: R right of the screen, T top of the screen (the USB connector side),
// U out of the screen. LIS2MDL: x = R, y = T, z = LIS2MDL_MAG_Z_SIGN * U. ISM330DL at rest:
// x = -T, y = -R, z = -U (screen up and flat: about 0, 0, -1 g).

struct TiltReading {
    float x, y, z;     // LIS2MDL
    float ax, ay, az;  // ISM330DL, in g
};

static void rotatePair(double* a, double* b, double angleRad) {
    const double c = cos(angleRad);
    const double s = sin(angleRad);
    for (int i = 0; i < 3; i++) {
        const double ai = a[i];
        a[i] = c * ai + s * b[i];
        b[i] = c * b[i] - s * ai;
    }
}

static double dot3(const double* a, const double* b) {
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

// What the two chips read when the top of the screen points `headingDeg` clockwise from
// magnetic north, under a field dipping `dipDeg` below the horizon, with the top of the screen
// raised by `pitchDeg` and then the right edge raised by `rollDeg` (about the tilted top axis,
// so the top of the screen keeps pointing at `headingDeg`). `zSign` is the LIS2MDL Z sign used
// to build the reading. World frame: east, north, up.
static TiltReading tiltReading(double headingDeg, double dipDeg, double pitchDeg, double rollDeg,
                               float zSign = LIS2MDL_MAG_Z_SIGN) {
    const double k = M_PI / 180.0;
    const double h = headingDeg * k;
    double t[3] = {sin(h), cos(h), 0.0};
    double r[3] = {cos(h), -sin(h), 0.0};  // T x U
    double u[3] = {0.0, 0.0, 1.0};
    rotatePair(t, u, pitchDeg * k);
    rotatePair(r, u, rollDeg * k);
    const double field[3] = {0.0, cos(dipDeg * k), -sin(dipDeg * k)};
    const double up[3] = {0.0, 0.0, 1.0};
    TiltReading out;
    out.x = static_cast<float>(dot3(field, r));
    out.y = static_cast<float>(dot3(field, t));
    out.z = static_cast<float>(zSign * dot3(field, u));
    out.ax = static_cast<float>(-dot3(up, t));
    out.ay = static_cast<float>(-dot3(up, r));
    out.az = static_cast<float>(-dot3(up, u));
    return out;
}

static float angleError(float a, float b) {
    return fabsf(fmodf(a - b + 540.0f, 360.0f) - 180.0f);
}

static float tiltHeading(const TiltReading& m) {
    return sensor.headingFromVectorsTilt(m.x, m.y, m.z, m.ax, m.ay, m.az, false);
}

void test_tilt_heading_equals_flat_heading_when_flat(void) {
    // Screen up and flat, the tilt path must give the flat heading atan2(-x, y) bit for bit,
    // whatever the gravity norm.
    sensor.begin();
    const float azs[] = {-1.0f, -0.97f, -1.04f};
    for (int deg = 0; deg < 360; deg += 5) {
        const float h = deg * static_cast<float>(M_PI) / 180.0f;
        const float x = -300.0f * sinf(h);
        const float y = 300.0f * cosf(h);
        const float z = -450.0f;
        for (float az : azs) {
            const float flat = sensor.headingFromVectors(x, y, z, false);
            const float tilt = sensor.headingFromVectorsTilt(x, y, z, 0.0f, 0.0f, az, false);
            TEST_ASSERT_TRUE_MESSAGE(flat == tilt, "tilt path differs from the flat heading");
        }
    }
    sensor.setCalibrateStep(10.0f, -20.0f, 30.0f, 1.5f, 0.8f, 1.2f);
    const float flat = sensor.headingFromVectors(300.0f, -150.0f, 450.0f);
    TEST_ASSERT_TRUE(flat == sensor.headingFromVectorsTilt(300.0f, -150.0f, 450.0f, 0, 0, -1));
}

void test_tilt_heading_recovers_heading_when_tilted(void) {
    sensor.begin();
    float worst = 0.0f;
    for (int heading = 0; heading < 360; heading += 15) {
        for (int pitch = -45; pitch <= 45; pitch += 15) {
            for (int roll = -45; roll <= 45; roll += 15) {
                const float got = tiltHeading(tiltReading(heading, 60.0, pitch, roll));
                TEST_ASSERT_TRUE(got >= 0.0f && got < 360.0f);
                const float err = angleError(got, static_cast<float>(heading));
                worst = err > worst ? err : worst;
            }
        }
    }
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, worst);
}

void test_tilt_heading_removes_roll_error_pointing_north(void) {
    sensor.begin();
    const TiltReading m = tiltReading(0.0, 60.0, 0.0, 20.0);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, angleError(tiltHeading(m), 0.0f));
    // The flat heading is about 30.6 degrees off with 20 degrees of roll.
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 30.6f, sensor.headingFromVectors(m.x, m.y, m.z, false));
}

void test_tilt_heading_removes_pitch_error_pointing_east(void) {
    sensor.begin();
    const TiltReading m = tiltReading(90.0, 60.0, 20.0, 0.0);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 90.0f, tiltHeading(m));
    TEST_ASSERT_TRUE(angleError(sensor.headingFromVectors(m.x, m.y, m.z, false), 90.0f) > 20.0f);
}

void test_tilt_heading_uses_the_z_sign(void) {
    // A reading built with the other Z sign gives a wrong heading: the constant is used.
    sensor.begin();
    const TiltReading m = tiltReading(0.0, 60.0, 0.0, 20.0, -LIS2MDL_MAG_Z_SIGN);
    TEST_ASSERT_TRUE(angleError(tiltHeading(m), 0.0f) > 40.0f);
}

void test_tilt_heading_screen_down(void) {
    sensor.begin();
    for (int heading = 0; heading < 360; heading += 30) {
        const float got = tiltHeading(tiltReading(heading, 60.0, 0.0, 180.0));
        TEST_ASSERT_TRUE(got >= 0.0f && got < 360.0f);
        TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, angleError(got, static_cast<float>(heading)));
    }
}

void test_tilt_heading_is_nan_without_gravity(void) {
    sensor.begin();
    sensor.setHeadingFilter(0.5f);
    TEST_ASSERT_TRUE(isnan(sensor.headingFromVectorsTilt(0, 300, 0, 0, 0, 0, false)));
    TEST_ASSERT_TRUE(isnan(sensor.headingFromVectorsTilt(0, 300, 0, NAN, NAN, NAN, false)));
    // The heading filter is left as is.
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f,
                             sensor.headingFromVectorsTilt(0, 300, 0, 0, 0, -1, false));
}

static Vec3f flatAccel() {
    return {0.0f, 0.0f, -1.0f};
}

void test_tilt_heading_reads_the_sensor(void) {
    sensor.begin();
    Wire.setRegister(ADDR, LIS2MDL_STATUS_REG, 0b1000);
    setField(300, -150, 450);
    const float flat = sensor.headingFlatOnly();
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 243.43f, flat);
    TEST_ASSERT_TRUE(flat == sensor.headingWithTiltCompensation(Vec3f{0.0f, 0.0f, -1.0f}));
    TEST_ASSERT_TRUE(flat == sensor.headingWithTiltCompensation(flatAccel));
}

void test_tilt_heading_is_nan_on_bus_error(void) {
    sensor.begin();
    setField(0, 300, 0);
    Wire.setAbsent(ADDR);
    TEST_ASSERT_TRUE(isnan(sensor.headingWithTiltCompensation(Vec3f{0.0f, 0.0f, -1.0f})));
    TEST_ASSERT_TRUE(isnan(sensor.headingWithTiltCompensation(flatAccel)));
}

// Rows of the V1c measurement of 2026-11-28 (steamicc/micropython-steami-lib#344): the calibrated
// field (calibratedField() after a 15 s calibrateMinmax3d()), the ISM330DL acceleration in g, the
// flat heading atan2(-x, y) and the tilt heading the board printed with LIS2MDL_MAG_Z_SIGN = -1.
// Indoors, the reference direction reads about 187 flat.
struct V1cRow {
    float x, y, z;
    float ax, ay, az;
    float flat;
    float heading;
};

static const V1cRow kV1cFlatRows[] = {
    {+0.039f, -0.590f, +0.784f, -0.05f, -0.00f, -1.00f, 183.8f, 184.1f},  // t = 166.1 s
    {+0.021f, -0.609f, +0.767f, -0.05f, -0.00f, -1.00f, 181.9f, 182.1f},  // t = 170.4 s
};

static const V1cRow kV1cTiltedRows[] = {
    {+0.589f, -0.631f, +0.457f, -0.09f, +0.68f, -0.72f, 223.0f, 191.5f},  // roll right, 232.5 s
    {-0.289f, -0.565f, +0.730f, -0.02f, -0.56f, -0.83f, 152.9f, 197.1f},  // roll left, 272.9 s
    {+0.460f, +0.457f, +0.696f, +0.27f, -0.09f, -0.96f, 314.9f, 296.9f},  // pitch, east, 366.0 s
};

static void beginWithIdentityCalibration() {
    sensor.begin();
    sensor.setCalibrateStep(0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f);
    sensor.setHeadingOffset(0.0f);
    sensor.setDeclination(0.0f);
    sensor.setHeadingFilter(0.0f);
}

static void checkV1cRow(const V1cRow& row) {
    const float got = sensor.headingFromVectorsTilt(row.x, row.y, row.z, row.ax, row.ay, row.az);
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.5f, 0.0f, angleError(got, row.heading),
                                     "tilt heading differs from the V1c log");
    // The same row read from the chip registers (scaled to LSB, the heading does not change).
    Wire.setRegister(ADDR, LIS2MDL_STATUS_REG, 0b1000);
    setField(static_cast<int16_t>(lroundf(row.x * 1000.0f)),
             static_cast<int16_t>(lroundf(row.y * 1000.0f)),
             static_cast<int16_t>(lroundf(row.z * 1000.0f)));
    const float read = sensor.headingWithTiltCompensation(Vec3f{row.ax, row.ay, row.az});
    TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.5f, 0.0f, angleError(read, row.heading),
                                     "tilt heading read from the chip differs from the V1c log");
}

void test_tilt_heading_matches_v1c_log_when_flat(void) {
    beginWithIdentityCalibration();
    for (const V1cRow& row : kV1cFlatRows) {
        checkV1cRow(row);
        const float flat = sensor.headingFromVectors(row.x, row.y, row.z);
        TEST_ASSERT_FLOAT_WITHIN(0.5f, 0.0f, angleError(flat, row.flat));
        const float tilt =
            sensor.headingFromVectorsTilt(row.x, row.y, row.z, row.ax, row.ay, row.az);
        TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.5f, 0.0f, angleError(tilt, flat),
                                         "flat V1c row: tilt heading differs from the flat one");
    }
}

void test_tilt_heading_matches_v1c_log_when_tilted(void) {
    beginWithIdentityCalibration();
    for (const V1cRow& row : kV1cTiltedRows) {
        checkV1cRow(row);
        // Tilted, the flat heading is far off: these rows really test the compensation.
        const float flat = sensor.headingFromVectors(row.x, row.y, row.z);
        TEST_ASSERT_TRUE(angleError(flat, row.heading) > 15.0f);
    }
}

void test_direction_sectors_are_centred(void) {
    sensor.begin();
    TEST_ASSERT_EQUAL_STRING("N", sensor.directionLabel(350.0f));
    TEST_ASSERT_EQUAL_STRING("N", sensor.directionLabel(10.0f));
    TEST_ASSERT_EQUAL_STRING("NE", sensor.directionLabel(30.0f));
    TEST_ASSERT_EQUAL_STRING("E", sensor.directionLabel(90.0f));
    TEST_ASSERT_EQUAL_STRING("S", sensor.directionLabel(200.0f));
    TEST_ASSERT_EQUAL_STRING("NW", sensor.directionLabel(337.4f));
    TEST_ASSERT_EQUAL_STRING("N", sensor.directionLabel(337.6f));
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
    RUN_TEST(test_read_calibration_returns_offsets_and_scales);
    RUN_TEST(test_magnitude_ut_is_positive);
    RUN_TEST(test_read_all_returns_all_channels);
    RUN_TEST(test_read_one_shot_returns_magnetic_field);
    RUN_TEST(test_read_one_shot_returns_zeros_on_timeout);
    RUN_TEST(test_ensure_data_returns_false_on_timeout);
    RUN_TEST(test_magnetic_field_reads_in_one_burst);
    RUN_TEST(test_magnetic_field_reports_absent_chip);
    RUN_TEST(test_magnetic_field_reports_failed_address_write);
    RUN_TEST(test_magnetic_field_reports_data_ready_timeout);
    RUN_TEST(test_float_readings_are_nan_on_bus_error);
    RUN_TEST(test_heading_flat_only_is_nan_on_bus_error);
    RUN_TEST(test_calibrate_minmax_keeps_calibration_on_bus_error);
    RUN_TEST(test_calibrate_minmax_3d_uses_every_sample);
    RUN_TEST(test_calibrate_minmax_2d_uses_every_sample_and_keeps_z);
    RUN_TEST(test_calibrate_minmax_3d_keeps_calibration_when_a_sample_fails);
    RUN_TEST(test_calibrate_minmax_2d_keeps_calibration_when_a_sample_fails);
    RUN_TEST(test_calibrate_minmax_with_zero_samples_keeps_calibration);
    RUN_TEST(test_temperature_raw_is_signed);
    RUN_TEST(test_set_temp_offset_shifts_reading);
    RUN_TEST(test_set_temperature_offset_shifts_reading);
    RUN_TEST(test_set_temperature_offset_resets_two_point_gain);
    RUN_TEST(test_set_temp_offset_is_alias_of_set_temperature_offset);
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
    RUN_TEST(test_tiny_negative_angle_never_gives_360);
    RUN_TEST(test_direction_label_returns_correct_cardinal);
    RUN_TEST(test_heading_convention_matches_board);
    RUN_TEST(test_tilt_heading_equals_flat_heading_when_flat);
    RUN_TEST(test_tilt_heading_recovers_heading_when_tilted);
    RUN_TEST(test_tilt_heading_removes_roll_error_pointing_north);
    RUN_TEST(test_tilt_heading_removes_pitch_error_pointing_east);
    RUN_TEST(test_tilt_heading_uses_the_z_sign);
    RUN_TEST(test_tilt_heading_screen_down);
    RUN_TEST(test_tilt_heading_is_nan_without_gravity);
    RUN_TEST(test_tilt_heading_reads_the_sensor);
    RUN_TEST(test_tilt_heading_is_nan_on_bus_error);
    RUN_TEST(test_tilt_heading_matches_v1c_log_when_flat);
    RUN_TEST(test_tilt_heading_matches_v1c_log_when_tilted);
    RUN_TEST(test_direction_sectors_are_centred);
    RUN_TEST(test_soft_reset_writes_bit5_cfg_reg_a);
    RUN_TEST(test_reboot_writes_bit6_cfg_reg_a);
    RUN_TEST(test_get_mode_returns_correct_string);
    RUN_TEST(test_is_idle_returns_true_when_md_bits_are_11);
    return UNITY_END();
}