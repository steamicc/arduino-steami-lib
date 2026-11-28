// SPDX-License-Identifier: GPL-3.0-or-later

#include <math.h>
#include <unity.h>

#include "SteamiBME280.h"
#include "Wire.h"
#include "driver_checks.h"
#include "steami_fake_chips.h"

constexpr uint8_t ADDR = BME280_DEFAULT_ADDRESS;

// Reference vectors. The calibration set (and the T / P raw values) is the
// well-known example of the Bosch BME280 datasheet / reference driver. The
// expected values were computed with an arbitrary-precision Python port of
// the integer formulas used by the MicroPython driver
// (steamicc/micropython-steami-lib, lib/bme280), which this driver mirrors.
constexpr int32_t RAW_TEMP = 519888;   // t_fine = 128422 -> 25.08 degC
constexpr int32_t RAW_PRESS = 415148;  // -> 1006.5325 hPa (100653.25 Pa)
constexpr int32_t RAW_HUM = 30000;     // -> 55.3535 %RH (with H1..H6 below)

using Calibration = SteamiFakeBme280Calibration;
constexpr Calibration BOSCH_EXAMPLE = STEAMI_FAKE_BME280_BOSCH;

static void preloadCalibration(const Calibration& c) {
    steamiFakeBme280Calibration(Wire, c);
}

static void preloadChipId(bool valid = true) {
    steamiFakeBme280ChipId(Wire, valid);
}

static void preloadRaw(int32_t press, int32_t temp, int32_t hum) {
    steamiFakeBme280Raw(Wire, press, temp, hum);
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

SteamiBME280 sensor;

void setUp(void) {
    Wire = TwoWire();
    millisClock() = 0;
    preloadChipId(true);
    preloadCalibration(BOSCH_EXAMPLE);
    sensor = SteamiBME280();
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

// The calibration and raw sample above are the worked example of the Bosch
// BMP280 datasheet (BST-BMP280-DS001, section 3.12, "Calculation of pressure
// and temperature for BMP280"); the BME280 uses the same temperature and
// pressure formulas (BME280 datasheet, section 4.2.3). The example gives
// T = 2508 (25.08 degC), an int32 pressure of 100653 Pa and an int64 pressure
// of 25767236 / 256 Pa (100653.27 Pa), with the note that the integer result
// "may deviate slightly" from it. The exact int64 result of the formulas is
// 25767233 / 256 Pa (100653.254 Pa), 3 LSB below.
constexpr float DATASHEET_TEMP_C = 25.08f;
constexpr float DATASHEET_PRESS_PA = 25767236.0f / 256.0f;
constexpr int32_t DATASHEET_PRESS_INT32_PA = 100653;

// The driver converts the Q24.8 pressure to hPa with this exact expression, so
// a pressure known as a Q24.8 integer can be compared without any tolerance.
static float hpaFromQ24_8(uint32_t q) {
    return static_cast<float>(q) / 25600.0f;
}

void test_compensation_matches_bosch_datasheet_example(void) {
    sensor.begin();
    sensor.powerOn();
    preloadMeasurement();
    auto r = sensor.read();
    TEST_ASSERT_EQUAL_FLOAT(DATASHEET_TEMP_C, r.temperature);
    float pa = r.pressure * 100.0f;
    TEST_ASSERT_EQUAL_INT32(DATASHEET_PRESS_INT32_PA, static_cast<int32_t>(pa));
    // Within 8 LSB of the Q24.8 example (1 LSB = 1/256 Pa).
    TEST_ASSERT_FLOAT_WITHIN(8.0f / 256.0f, DATASHEET_PRESS_PA, pa);
}

// The tests below pin the integer results bit for bit (expected values from
// an arbitrary-precision Python port of the datasheet formulas) on the inputs
// that feed a negative value to the datasheet's left shifts: var1 < 0 when
// t_fine < 128000 (shift by 17), and negative P4 (shift by 35) and P7 (shift
// by 4). P2 < 0 with var1 > 0 (shift by 12) is the datasheet example above,
// and H4 < 0 (shift by 20) is test_signed_humidity_calibration_is_decoded.

void test_pressure_exact_below_t_fine_128000(void) {
    sensor.begin();
    sensor.powerOn();
    preloadRaw(RAW_PRESS, 400000, RAW_HUM);  // t_fine = -64736 (-12.64 degC)
    TEST_ASSERT_FLOAT_WITHIN(0.0f, hpaFromQ24_8(24298573), sensor.pressureHpa());
}

void test_pressure_exact_with_negative_p4_and_p7(void) {
    Calibration c = BOSCH_EXAMPLE;
    c.p4 = -2855;
    c.p7 = -15500;
    preloadCalibration(c);
    sensor.begin();
    sensor.powerOn();
    preloadRaw(RAW_PRESS, 400000, RAW_HUM);
    TEST_ASSERT_FLOAT_WITHIN(0.0f, hpaFromQ24_8(27602040), sensor.pressureHpa());
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

static void assertAllNan(const SteamiBME280::ReadResult& r) {
    TEST_ASSERT_TRUE(isnan(r.temperature));
    TEST_ASSERT_TRUE(isnan(r.humidity));
    TEST_ASSERT_TRUE(isnan(r.pressure));
}

void test_begin_returns_false_on_i2c_nack(void) {
    Wire.setEndTransmissionResult(2);
    TEST_ASSERT_FALSE(sensor.begin());
}

void test_read_returns_nan_when_bus_fails_after_begin(void) {
    TEST_ASSERT_TRUE(sensor.begin());
    preloadMeasurement();
    // Module unplugged or bus fault: every transfer is NACKed.
    Wire.setEndTransmissionResult(2);
    assertAllNan(sensor.read());
    assertAllNan(sensor.readOneShot());
    TEST_ASSERT_TRUE(isnan(sensor.temperature()));
}

void test_read_returns_nan_without_begin(void) {
    preloadMeasurement();
    assertAllNan(sensor.read());
    assertAllNan(sensor.readOneShot());
}

void test_read_returns_nan_after_failed_begin(void) {
    preloadChipId(false);  // e.g. a V1a/V1b board: no BME280 at 0x76
    TEST_ASSERT_FALSE(sensor.begin());
    preloadMeasurement();
    assertAllNan(sensor.read());
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

void test_data_ready_waits_for_pending_one_shot(void) {
    sensor.begin();
    uint32_t nominal = sensor.measurementTimeMs();
    sensor.triggerOneShot();
    // MEASURING may lag the trigger: STATUS still reads idle.
    Wire.setRegister(ADDR, BME280_REG_STATUS, 0x00);

    // Forced mode is still set: the conversion has not finished.
    TEST_ASSERT_FALSE(sensor.dataReady());

    // Back to sleep, but before the nominal conversion time: still not ready.
    Wire.setRegister(ADDR, BME280_REG_CTRL_MEAS, 0x24);
    TEST_ASSERT_FALSE(sensor.dataReady());

    // Nominal time elapsed and chip back to sleep: the new sample is there.
    delay(nominal);
    TEST_ASSERT_TRUE(sensor.dataReady());
}

void test_read_after_trigger_one_shot_waits_for_conversion(void) {
    sensor.begin();
    preloadMeasurement();
    uint32_t nominal = sensor.measurementTimeMs();
    sensor.triggerOneShot();
    // The chip is back to sleep once the conversion is over.
    Wire.setRegister(ADDR, BME280_REG_CTRL_MEAS, 0x24);
    uint32_t start = millis();

    auto r = sensor.read();

    TEST_ASSERT_TRUE(millis() - start >= nominal);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 25.08f, r.temperature);
}

void test_read_after_trigger_one_shot_times_out_while_forced(void) {
    sensor.begin();
    preloadMeasurement();
    sensor.triggerOneShot();
    // STATUS idle (flag lag) but CTRL_MEAS never returns to sleep.
    Wire.setRegister(ADDR, BME280_REG_STATUS, 0x00);

    auto r = sensor.read();

    TEST_ASSERT_TRUE(isnan(r.temperature));
    TEST_ASSERT_TRUE(isnan(r.humidity));
    TEST_ASSERT_TRUE(isnan(r.pressure));
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
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, SteamiBME280::altitudeFromPressure(1013.25f));
}

void test_altitude_from_pressure(void) {
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 988.67f, SteamiBME280::altitudeFromPressure(900.0f));
}

void test_altitude_uses_given_sea_level_pressure(void) {
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, SteamiBME280::altitudeFromPressure(1020.0f, 1020.0f));
}

void test_altitude_reads_pressure(void) {
    sensor.begin();
    sensor.powerOn();
    preloadMeasurement();
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 56.08f, sensor.altitude());
}

void test_altitude_is_nan_without_data(void) {
    TEST_ASSERT_TRUE(isnan(SteamiBME280::altitudeFromPressure(NAN)));
}

void test_dew_point_reference_value(void) {
    // Magnus (17.625 / 243.04): 25 degC at 50 %RH -> 13.86 degC.
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 13.86f, SteamiBME280::dewPointFrom(25.0f, 50.0f));
}

void test_dew_point_at_saturation_equals_temperature(void) {
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 20.0f, SteamiBME280::dewPointFrom(20.0f, 100.0f));
}

void test_dew_point_survives_zero_humidity(void) {
    TEST_ASSERT_FALSE(isnan(SteamiBME280::dewPointFrom(20.0f, 0.0f)));
}

void test_dew_point_reads_sensor(void) {
    sensor.begin();
    sensor.powerOn();
    preloadMeasurement();
    float expected = SteamiBME280::dewPointFrom(25.08f, 55.3535f);
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

void test_calibrate_temperature_returns_true(void) {
    sensor.begin();
    TEST_ASSERT_TRUE(sensor.calibrateTemperature(0.0f, 1.0f, 100.0f, 99.0f));
}

void test_calibrate_temperature_with_equal_points_keeps_calibration(void) {
    // Same contract as ISM330DL, LIS2MDL and MicroPython (ValueError there):
    // a degenerate input is refused and the previous calibration stays.
    // Start from a gain != 1 so that a reset of the gain alone would show.
    sensor.begin();
    sensor.powerOn();
    sensor.calibrateTemperature(0.0f, 1.0f, 100.0f, 99.0f);  // gain 1.0204, offset -1.0204
    TEST_ASSERT_FALSE(sensor.calibrateTemperature(0.0f, 5.0f, 100.0f, 5.0f));
    preloadMeasurement();
    // 1.0204 * 25.08 - 1.0204 = 24.5714 (gain reset to 1: 24.06, identity: 25.08)
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 24.5714f, sensor.temperature());
}

void test_set_temperature_offset_replaces_two_point_calibration(void) {
    // The offset resets the gain to 1, as in MicroPython set_temp_offset().
    sensor.begin();
    sensor.powerOn();
    sensor.calibrateTemperature(0.0f, 1.0f, 100.0f, 99.0f);
    sensor.setTemperatureOffset(1.5f);
    preloadMeasurement();
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 26.58f, sensor.temperature());
}

void test_calibrate_temperature_replaces_offset(void) {
    sensor.begin();
    sensor.powerOn();
    sensor.setTemperatureOffset(1.5f);
    sensor.calibrateTemperature(0.0f, 1.0f, 100.0f, 99.0f);
    preloadMeasurement();
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 24.5714f, sensor.temperature());
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
    RUN_TEST(test_compensation_matches_bosch_datasheet_example);
    RUN_TEST(test_pressure_exact_below_t_fine_128000);
    RUN_TEST(test_pressure_exact_with_negative_p4_and_p7);
    RUN_TEST(test_read_returns_all_channels);
    RUN_TEST(test_signed_humidity_calibration_is_decoded);
    RUN_TEST(test_humidity_is_clamped_to_0_100);
    RUN_TEST(test_skipped_channels_report_nan);
    RUN_TEST(test_read_auto_triggers_forced_mode_when_sleeping);
    RUN_TEST(test_read_does_not_trigger_in_normal_mode);
    RUN_TEST(test_read_returns_nan_on_timeout);
    RUN_TEST(test_begin_returns_false_on_i2c_nack);
    RUN_TEST(test_read_returns_nan_when_bus_fails_after_begin);
    RUN_TEST(test_read_returns_nan_without_begin);
    RUN_TEST(test_read_returns_nan_after_failed_begin);
    RUN_TEST(test_trigger_one_shot_sets_forced_mode);
    RUN_TEST(test_read_after_trigger_one_shot_does_not_retrigger);
    RUN_TEST(test_data_ready_waits_for_pending_one_shot);
    RUN_TEST(test_read_after_trigger_one_shot_waits_for_conversion);
    RUN_TEST(test_read_after_trigger_one_shot_times_out_while_forced);
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
    RUN_TEST(test_calibrate_temperature_returns_true);
    RUN_TEST(test_calibrate_temperature_with_equal_points_keeps_calibration);
    RUN_TEST(test_set_temperature_offset_replaces_two_point_calibration);
    RUN_TEST(test_calibrate_temperature_replaces_offset);
    return UNITY_END();
}
