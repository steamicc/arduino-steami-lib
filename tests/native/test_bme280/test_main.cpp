// SPDX-License-Identifier: GPL-3.0-or-later

#include <math.h>
#include <unity.h>

#include "BME280.h"
#include "Wire.h"
#include "driver_checks.h"

constexpr uint8_t ADDR = BME280_DEFAULT_ADDRESS;

// Reference vectors. The calibration set (and the T / P raw values) is the
// well-known example of the Bosch BME280 datasheet / reference driver. The
// expected values were computed with an arbitrary-precision Python port of
// the integer formulas used by the MicroPython driver
// (steamicc/micropython-steami-lib, lib/bme280), which this driver mirrors.
constexpr int32_t RAW_TEMP = 519888;   // t_fine = 128422 -> 25.08 degC
constexpr int32_t RAW_PRESS = 415148;  // -> 1006.5325 hPa (100653.25 Pa)
constexpr int32_t RAW_HUM = 30000;     // -> 55.3535 %RH (with H1..H6 below)

struct Calibration {
    uint16_t t1;
    int16_t t2, t3;
    uint16_t p1;
    int16_t p2, p3, p4, p5, p6, p7, p8, p9;
    uint8_t h1;
    int16_t h2;
    uint8_t h3;
    int16_t h4, h5;
    int8_t h6;
};

constexpr Calibration BOSCH_EXAMPLE = {27504, 26435,  -1000, 36477, -10685, 3024, 2855, 140, -7,
                                       15500, -14600, 6000,  75,    362,    0,    312,  50,  30};

static void setLe16(uint8_t reg, int32_t value) {
    Wire.setRegister(ADDR, reg, value & 0xFF);
    Wire.setRegister(ADDR, reg + 1, (value >> 8) & 0xFF);
}

static void preloadCalibration(const Calibration& c) {
    setLe16(0x88, c.t1);
    setLe16(0x8A, c.t2);
    setLe16(0x8C, c.t3);
    setLe16(0x8E, c.p1);
    setLe16(0x90, c.p2);
    setLe16(0x92, c.p3);
    setLe16(0x94, c.p4);
    setLe16(0x96, c.p5);
    setLe16(0x98, c.p6);
    setLe16(0x9A, c.p7);
    setLe16(0x9C, c.p8);
    setLe16(0x9E, c.p9);
    Wire.setRegister(ADDR, 0xA0, 0x00);  // reserved
    Wire.setRegister(ADDR, 0xA1, c.h1);
    setLe16(0xE1, c.h2);
    Wire.setRegister(ADDR, 0xE3, c.h3);
    // H4 = E4[7:0] << 4 | E5[3:0]; H5 = E6[7:0] << 4 | E5[7:4] (12-bit signed).
    Wire.setRegister(ADDR, 0xE4, (c.h4 >> 4) & 0xFF);
    Wire.setRegister(ADDR, 0xE5, ((c.h5 & 0x0F) << 4) | (c.h4 & 0x0F));
    Wire.setRegister(ADDR, 0xE6, (c.h5 >> 4) & 0xFF);
    Wire.setRegister(ADDR, 0xE7, static_cast<uint8_t>(c.h6));
}

static void preloadChipId(bool valid = true) {
    Wire.setRegister(ADDR, BME280_REG_CHIP_ID, valid ? BME280_CHIP_ID_VALUE : 0x42);
}

static void preloadRaw(int32_t press, int32_t temp, int32_t hum) {
    Wire.setRegister(ADDR, 0xF7, (press >> 12) & 0xFF);
    Wire.setRegister(ADDR, 0xF8, (press >> 4) & 0xFF);
    Wire.setRegister(ADDR, 0xF9, (press & 0x0F) << 4);
    Wire.setRegister(ADDR, 0xFA, (temp >> 12) & 0xFF);
    Wire.setRegister(ADDR, 0xFB, (temp >> 4) & 0xFF);
    Wire.setRegister(ADDR, 0xFC, (temp & 0x0F) << 4);
    Wire.setRegister(ADDR, 0xFD, (hum >> 8) & 0xFF);
    Wire.setRegister(ADDR, 0xFE, hum & 0xFF);
}

static void preloadMeasurement() {
    preloadRaw(RAW_PRESS, RAW_TEMP, RAW_HUM);
}

static uint8_t reg(uint8_t r) {
    return Wire.getRegister(ADDR, r);
}

// Number of recorded writes to register r.
static int writesTo(uint8_t r) {
    int n = 0;
    for (const auto& w : Wire.getWrites()) {
        if (w.reg == r)
            n++;
    }
    return n;
}

BME280 sensor;

void setUp(void) {
    Wire = TwoWire();
    millisClock() = 0;
    preloadChipId(true);
    preloadCalibration(BOSCH_EXAMPLE);
    sensor = BME280();
}

void tearDown(void) {}

// --- Lifecycle ---

void test_begin_detects_device(void) {
    check_begin(sensor);
}

void test_begin_rejects_wrong_chip_id(void) {
    preloadChipId(false);
    TEST_ASSERT_FALSE(sensor.begin());
}

void test_begin_fails_when_nvm_copy_never_ends(void) {
    Wire.setRegister(ADDR, BME280_REG_STATUS, BME280_STATUS_IM_UPDATE);
    TEST_ASSERT_FALSE(sensor.begin());
}

void test_device_id_returns_chip_id(void) {
    check_who_am_i(sensor, BME280_CHIP_ID_VALUE);
}

void test_default_address_is_0x76(void) {
    TEST_ASSERT_EQUAL_HEX8(0x76, BME280_DEFAULT_ADDRESS);
}

void test_begin_applies_default_configuration(void) {
    // Pretend the chip kept a previous sketch's state.
    Wire.setRegister(ADDR, BME280_REG_CONFIG, 0xFC);
    Wire.setRegister(ADDR, BME280_REG_CTRL_MEAS, 0xFF);
    Wire.setRegister(ADDR, BME280_REG_CTRL_HUM, 0x07);

    sensor.begin();

    TEST_ASSERT_EQUAL_HEX8(0x00, reg(BME280_REG_CONFIG));
    TEST_ASSERT_EQUAL_HEX8(BME280_OSRS_X1, reg(BME280_REG_CTRL_HUM));
    // osrs_t = x1, osrs_p = x1, sleep mode.
    TEST_ASSERT_EQUAL_HEX8(0x24, reg(BME280_REG_CTRL_MEAS));
}

void test_begin_writes_ctrl_hum_before_final_ctrl_meas(void) {
    sensor.begin();
    const auto& w = Wire.getWrites();
    int humIndex = -1;
    int lastMeasIndex = -1;
    for (size_t i = 0; i < w.size(); ++i) {
        if (w[i].reg == BME280_REG_CTRL_HUM)
            humIndex = static_cast<int>(i);
        if (w[i].reg == BME280_REG_CTRL_MEAS)
            lastMeasIndex = static_cast<int>(i);
    }
    TEST_ASSERT_TRUE(humIndex >= 0);
    TEST_ASSERT_TRUE(humIndex < lastMeasIndex);
}

void test_soft_reset_writes_reset_command_and_reconfigures(void) {
    sensor.begin();
    sensor.setOversampling(BME280_OSRS_X16, BME280_OSRS_X16, BME280_OSRS_X16);
    Wire.clearWrites();

    sensor.softReset();

    TEST_ASSERT_EQUAL_HEX8(BME280_SOFT_RESET_CMD, reg(BME280_REG_SOFT_RESET));
    TEST_ASSERT_EQUAL_HEX8(0x24, reg(BME280_REG_CTRL_MEAS));
    TEST_ASSERT_EQUAL_HEX8(BME280_OSRS_X1, reg(BME280_REG_CTRL_HUM));
}

void test_power_on_enters_normal_mode_and_keeps_oversampling(void) {
    sensor.begin();
    sensor.powerOn();
    TEST_ASSERT_EQUAL_HEX8(0x27, reg(BME280_REG_CTRL_MEAS));
}

void test_power_off_enters_sleep_mode(void) {
    sensor.begin();
    sensor.powerOn();
    sensor.powerOff();
    TEST_ASSERT_EQUAL_HEX8(0x24, reg(BME280_REG_CTRL_MEAS));
}

// --- Status ---

void test_data_ready_follows_measuring_bit(void) {
    sensor.begin();
    Wire.setRegister(ADDR, BME280_REG_STATUS, BME280_STATUS_MEASURING);
    TEST_ASSERT_FALSE(sensor.dataReady());
    TEST_ASSERT_FALSE(sensor.temperatureReady());
    TEST_ASSERT_FALSE(sensor.pressureReady());
    TEST_ASSERT_FALSE(sensor.humidityReady());
    Wire.setRegister(ADDR, BME280_REG_STATUS, 0x00);
    TEST_ASSERT_TRUE(sensor.dataReady());
    TEST_ASSERT_TRUE(sensor.temperatureReady());
    TEST_ASSERT_TRUE(sensor.pressureReady());
    TEST_ASSERT_TRUE(sensor.humidityReady());
}

// --- Compensation ---

void test_temperature_matches_reference_vector(void) {
    sensor.begin();
    sensor.powerOn();
    preloadMeasurement();
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 25.08f, sensor.temperature());
}

void test_negative_temperature(void) {
    sensor.begin();
    sensor.powerOn();
    preloadRaw(RAW_PRESS, 400000, RAW_HUM);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -12.64f, sensor.temperature());
}

void test_pressure_matches_reference_vector(void) {
    sensor.begin();
    sensor.powerOn();
    preloadMeasurement();
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1006.5325f, sensor.pressureHpa());
}

void test_humidity_matches_reference_vector(void) {
    sensor.begin();
    sensor.powerOn();
    preloadMeasurement();
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 55.3535f, sensor.humidity());
}

void test_read_returns_all_channels(void) {
    sensor.begin();
    sensor.powerOn();
    preloadMeasurement();
    auto r = sensor.read();
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 25.08f, r.temperature);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 55.3535f, r.humidity);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1006.5325f, r.pressure);
}

void test_signed_humidity_calibration_is_decoded(void) {
    // H4, H5 and H6 negative: checks the 12-bit sign extension of H4 / H5
    // and the int8 H6. Expected value from the Python reference port.
    Calibration c = BOSCH_EXAMPLE;
    c.h3 = 10;
    c.h4 = -100;
    c.h5 = -60;
    c.h6 = -5;
    preloadCalibration(c);
    sensor.begin();
    sensor.powerOn();
    preloadRaw(RAW_PRESS, RAW_TEMP, 4000);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 57.7822f, sensor.humidity());
}

void test_humidity_is_clamped_to_0_100(void) {
    sensor.begin();
    sensor.powerOn();
    preloadRaw(RAW_PRESS, RAW_TEMP, 50000);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 100.0f, sensor.humidity());
    preloadRaw(RAW_PRESS, RAW_TEMP, 0);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, sensor.humidity());
}

void test_skipped_channels_report_nan(void) {
    sensor.begin();
    sensor.powerOn();
    // 0x80000 / 0x8000 is what the chip outputs for a disabled channel.
    preloadRaw(BME280_RAW_SKIPPED_20BIT, RAW_TEMP, BME280_RAW_SKIPPED_16BIT);
    auto r = sensor.read();
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 25.08f, r.temperature);
    TEST_ASSERT_TRUE(isnan(r.pressure));
    TEST_ASSERT_TRUE(isnan(r.humidity));

    preloadRaw(RAW_PRESS, BME280_RAW_SKIPPED_20BIT, RAW_HUM);
    r = sensor.read();
    TEST_ASSERT_TRUE(isnan(r.temperature));
    TEST_ASSERT_TRUE(isnan(r.pressure));
    TEST_ASSERT_TRUE(isnan(r.humidity));
}

// --- Auto-trigger, one-shot and continuous ---

void test_read_auto_triggers_forced_mode_when_sleeping(void) {
    sensor.begin();
    preloadMeasurement();
    Wire.clearWrites();

    auto r = sensor.read();

    TEST_ASSERT_EQUAL_INT(1, writesTo(BME280_REG_CTRL_MEAS));
    TEST_ASSERT_EQUAL_HEX8(0x25, reg(BME280_REG_CTRL_MEAS));  // x1 / x1, forced
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 25.08f, r.temperature);
}

void test_read_does_not_trigger_in_normal_mode(void) {
    sensor.begin();
    sensor.setContinuous(BME280_STANDBY_62_5_MS);
    preloadMeasurement();
    Wire.clearWrites();

    sensor.read();

    TEST_ASSERT_EQUAL_INT(0, Wire.getWrites().size());
}

void test_read_returns_nan_on_timeout(void) {
    sensor.begin();
    preloadMeasurement();
    Wire.setRegister(ADDR, BME280_REG_STATUS, BME280_STATUS_MEASURING);

    auto r = sensor.read();

    TEST_ASSERT_TRUE(isnan(r.temperature));
    TEST_ASSERT_TRUE(isnan(r.humidity));
    TEST_ASSERT_TRUE(isnan(r.pressure));
}

void test_trigger_one_shot_sets_forced_mode(void) {
    sensor.begin();
    sensor.triggerOneShot();
    TEST_ASSERT_EQUAL_HEX8(0x25, reg(BME280_REG_CTRL_MEAS));
}

void test_read_after_trigger_one_shot_does_not_retrigger(void) {
    sensor.begin();
    preloadMeasurement();
    sensor.triggerOneShot();
    // The chip is back to sleep once the conversion is over.
    Wire.setRegister(ADDR, BME280_REG_CTRL_MEAS, 0x24);
    Wire.clearWrites();

    auto r = sensor.read();

    TEST_ASSERT_EQUAL_INT(0, Wire.getWrites().size());
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 25.08f, r.temperature);
}

void test_read_one_shot_triggers_waits_and_returns(void) {
    sensor.begin();
    preloadMeasurement();
    Wire.clearWrites();

    auto r = sensor.readOneShot();

    TEST_ASSERT_EQUAL_INT(1, writesTo(BME280_REG_CTRL_MEAS));
    TEST_ASSERT_EQUAL_HEX8(0x25, reg(BME280_REG_CTRL_MEAS));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 25.08f, r.temperature);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 55.3535f, r.humidity);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1006.5325f, r.pressure);
}

void test_read_one_shot_returns_nan_on_timeout(void) {
    sensor.begin();
    preloadMeasurement();
    Wire.setRegister(ADDR, BME280_REG_STATUS, BME280_STATUS_MEASURING);
    auto r = sensor.readOneShot();
    TEST_ASSERT_TRUE(isnan(r.temperature));
}

void test_set_continuous_writes_standby_then_normal_mode(void) {
    sensor.begin();
    sensor.setContinuous(BME280_STANDBY_1000_MS);
    TEST_ASSERT_EQUAL_HEX8(BME280_STANDBY_1000_MS << 5, reg(BME280_REG_CONFIG));
    TEST_ASSERT_EQUAL_HEX8(0x27, reg(BME280_REG_CTRL_MEAS));
}

// --- Configuration ---

void test_set_oversampling_writes_all_channels(void) {
    sensor.begin();
    sensor.setOversampling(BME280_OSRS_X2, BME280_OSRS_X16, BME280_OSRS_X4);
    TEST_ASSERT_EQUAL_HEX8(BME280_OSRS_X4, reg(BME280_REG_CTRL_HUM));
    // osrs_t = 2 (010), osrs_p = 5 (101), sleep: 010 101 00 = 0x54.
    TEST_ASSERT_EQUAL_HEX8(0x54, reg(BME280_REG_CTRL_MEAS));
}

void test_set_oversampling_keep_leaves_channel_unchanged(void) {
    sensor.begin();
    sensor.setOversampling(BME280_OSRS_KEEP, BME280_OSRS_X8, BME280_OSRS_KEEP);
    TEST_ASSERT_EQUAL_HEX8(BME280_OSRS_X1, reg(BME280_REG_CTRL_HUM));
    // osrs_t stays x1, osrs_p = 4 (100): 001 100 00 = 0x30.
    TEST_ASSERT_EQUAL_HEX8(0x30, reg(BME280_REG_CTRL_MEAS));
}

void test_set_oversampling_rewrites_ctrl_meas_to_latch_humidity(void) {
    sensor.begin();
    Wire.clearWrites();
    sensor.setOversampling(BME280_OSRS_KEEP, BME280_OSRS_KEEP, BME280_OSRS_X2);
    const auto& w = Wire.getWrites();
    TEST_ASSERT_EQUAL_size_t(2, w.size());
    TEST_ASSERT_EQUAL_HEX8(BME280_REG_CTRL_HUM, w[0].reg);
    TEST_ASSERT_EQUAL_HEX8(BME280_REG_CTRL_MEAS, w[1].reg);
}

void test_set_iir_filter_preserves_standby_bits(void) {
    sensor.begin();
    sensor.setStandby(BME280_STANDBY_250_MS);
    sensor.setIirFilter(BME280_FILTER_16);
    // t_sb = 011, filter = 100: 011 100 00 = 0x70.
    TEST_ASSERT_EQUAL_HEX8(0x70, reg(BME280_REG_CONFIG));
}

void test_config_written_in_sleep_mode_then_normal_mode_restored(void) {
    sensor.begin();
    sensor.powerOn();
    Wire.clearWrites();

    sensor.setIirFilter(BME280_FILTER_4);

    const auto& w = Wire.getWrites();
    TEST_ASSERT_EQUAL_size_t(3, w.size());
    TEST_ASSERT_EQUAL_HEX8(BME280_REG_CTRL_MEAS, w[0].reg);
    TEST_ASSERT_EQUAL_HEX8(0x24, w[0].value);  // sleep
    TEST_ASSERT_EQUAL_HEX8(BME280_REG_CONFIG, w[1].reg);
    TEST_ASSERT_EQUAL_HEX8(BME280_FILTER_4 << 2, w[1].value);
    TEST_ASSERT_EQUAL_HEX8(BME280_REG_CTRL_MEAS, w[2].reg);
    TEST_ASSERT_EQUAL_HEX8(0x27, w[2].value);  // back to normal
}

void test_measurement_time_default_is_10_ms(void) {
    sensor.begin();
    // 1.25 + 3 * 2.3 + 2 * 0.575 + ... = 9.3 ms -> 10 ms.
    TEST_ASSERT_EQUAL_UINT32(10, sensor.measurementTimeMs());
}

void test_measurement_time_at_max_oversampling(void) {
    sensor.begin();
    sensor.setOversampling(BME280_OSRS_X16, BME280_OSRS_X16, BME280_OSRS_X16);
    TEST_ASSERT_EQUAL_UINT32(113, sensor.measurementTimeMs());
}

void test_measurement_time_with_skipped_channels(void) {
    sensor.begin();
    sensor.setOversampling(BME280_OSRS_X1, BME280_OSRS_SKIP, BME280_OSRS_SKIP);
    // 1.25 + 2.3 = 3.55 ms -> 4 ms.
    TEST_ASSERT_EQUAL_UINT32(4, sensor.measurementTimeMs());
}

// --- Derived quantities ---

void test_altitude_at_sea_level_is_zero(void) {
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, BME280::altitudeFromPressure(1013.25f));
}

void test_altitude_from_pressure(void) {
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 988.67f, BME280::altitudeFromPressure(900.0f));
}

void test_altitude_uses_given_sea_level_pressure(void) {
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, BME280::altitudeFromPressure(1020.0f, 1020.0f));
}

void test_altitude_reads_pressure(void) {
    sensor.begin();
    sensor.powerOn();
    preloadMeasurement();
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 56.08f, sensor.altitude());
}

void test_altitude_is_nan_without_data(void) {
    TEST_ASSERT_TRUE(isnan(BME280::altitudeFromPressure(NAN)));
}

void test_dew_point_reference_value(void) {
    // Magnus (17.625 / 243.04): 25 degC at 50 %RH -> 13.86 degC.
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 13.86f, BME280::dewPointFrom(25.0f, 50.0f));
}

void test_dew_point_at_saturation_equals_temperature(void) {
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 20.0f, BME280::dewPointFrom(20.0f, 100.0f));
}

void test_dew_point_survives_zero_humidity(void) {
    TEST_ASSERT_FALSE(isnan(BME280::dewPointFrom(20.0f, 0.0f)));
}

void test_dew_point_reads_sensor(void) {
    sensor.begin();
    sensor.powerOn();
    preloadMeasurement();
    float expected = BME280::dewPointFrom(25.08f, 55.3535f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, expected, sensor.dewPoint());
}

// --- Calibration hooks ---

void test_set_temperature_offset_shifts_reading(void) {
    sensor.begin();
    sensor.powerOn();
    preloadMeasurement();
    sensor.setTemperatureOffset(1.5f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 26.58f, sensor.temperature());
    // Pressure and humidity are untouched.
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1006.5325f, sensor.pressureHpa());
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 55.3535f, sensor.humidity());
}

void test_calibrate_temperature_applies_two_point_correction(void) {
    sensor.begin();
    sensor.powerOn();
    // The sensor reads 0 -> 1 and 100 -> 99 against the references.
    sensor.calibrateTemperature(0.0f, 1.0f, 100.0f, 99.0f);
    preloadMeasurement();
    // slope = 100 / 98, intercept = -slope; 25.08 -> (25.08 - 1) * 100 / 98.
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 24.5714f, sensor.temperature());
}

void test_calibrate_temperature_with_equal_points_is_identity(void) {
    sensor.begin();
    sensor.powerOn();
    sensor.calibrateTemperature(0.0f, 5.0f, 100.0f, 5.0f);
    preloadMeasurement();
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 25.08f, sensor.temperature());
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_begin_detects_device);
    RUN_TEST(test_begin_rejects_wrong_chip_id);
    RUN_TEST(test_begin_fails_when_nvm_copy_never_ends);
    RUN_TEST(test_device_id_returns_chip_id);
    RUN_TEST(test_default_address_is_0x76);
    RUN_TEST(test_begin_applies_default_configuration);
    RUN_TEST(test_begin_writes_ctrl_hum_before_final_ctrl_meas);
    RUN_TEST(test_soft_reset_writes_reset_command_and_reconfigures);
    RUN_TEST(test_power_on_enters_normal_mode_and_keeps_oversampling);
    RUN_TEST(test_power_off_enters_sleep_mode);
    RUN_TEST(test_data_ready_follows_measuring_bit);
    RUN_TEST(test_temperature_matches_reference_vector);
    RUN_TEST(test_negative_temperature);
    RUN_TEST(test_pressure_matches_reference_vector);
    RUN_TEST(test_humidity_matches_reference_vector);
    RUN_TEST(test_read_returns_all_channels);
    RUN_TEST(test_signed_humidity_calibration_is_decoded);
    RUN_TEST(test_humidity_is_clamped_to_0_100);
    RUN_TEST(test_skipped_channels_report_nan);
    RUN_TEST(test_read_auto_triggers_forced_mode_when_sleeping);
    RUN_TEST(test_read_does_not_trigger_in_normal_mode);
    RUN_TEST(test_read_returns_nan_on_timeout);
    RUN_TEST(test_trigger_one_shot_sets_forced_mode);
    RUN_TEST(test_read_after_trigger_one_shot_does_not_retrigger);
    RUN_TEST(test_read_one_shot_triggers_waits_and_returns);
    RUN_TEST(test_read_one_shot_returns_nan_on_timeout);
    RUN_TEST(test_set_continuous_writes_standby_then_normal_mode);
    RUN_TEST(test_set_oversampling_writes_all_channels);
    RUN_TEST(test_set_oversampling_keep_leaves_channel_unchanged);
    RUN_TEST(test_set_oversampling_rewrites_ctrl_meas_to_latch_humidity);
    RUN_TEST(test_set_iir_filter_preserves_standby_bits);
    RUN_TEST(test_config_written_in_sleep_mode_then_normal_mode_restored);
    RUN_TEST(test_measurement_time_default_is_10_ms);
    RUN_TEST(test_measurement_time_at_max_oversampling);
    RUN_TEST(test_measurement_time_with_skipped_channels);
    RUN_TEST(test_altitude_at_sea_level_is_zero);
    RUN_TEST(test_altitude_from_pressure);
    RUN_TEST(test_altitude_uses_given_sea_level_pressure);
    RUN_TEST(test_altitude_reads_pressure);
    RUN_TEST(test_altitude_is_nan_without_data);
    RUN_TEST(test_dew_point_reference_value);
    RUN_TEST(test_dew_point_at_saturation_equals_temperature);
    RUN_TEST(test_dew_point_survives_zero_humidity);
    RUN_TEST(test_dew_point_reads_sensor);
    RUN_TEST(test_set_temperature_offset_shifts_reading);
    RUN_TEST(test_calibrate_temperature_applies_two_point_correction);
    RUN_TEST(test_calibrate_temperature_with_equal_points_is_identity);
    return UNITY_END();
}
