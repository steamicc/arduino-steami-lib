// SPDX-License-Identifier: GPL-3.0-or-later

#include <math.h>
#include <unity.h>

#include "HTS221.h"
#include "Wire.h"
#include "driver_checks.h"

// The driver uses bit 7 of the sub-address as the ST auto-increment flag for
// multi-byte reads (see HTS221_AUTO_INCREMENT). The Wire mock is a straight
// register map, so preloads for burst-read ranges have to sit at the
// post-OR address the driver will actually emit on the wire.
constexpr uint8_t ADDR = HTS221_DEFAULT_ADDRESS;
constexpr uint8_t CAL_BASE = HTS221_REG_H0_RH_X2 | HTS221_AUTO_INCREMENT;
constexpr uint8_t HUM_OUT_BASE = HTS221_REG_HUMIDITY_OUT_L | HTS221_AUTO_INCREMENT;
constexpr uint8_t TEMP_OUT_BASE = HTS221_REG_TEMP_OUT_L | HTS221_AUTO_INCREMENT;

static void preloadWhoAmI(bool valid = true) {
    Wire.setRegister(ADDR, HTS221_REG_WHO_AM_I, valid ? HTS221_WHO_AM_I_VALUE : 0x42);
}

// Factory calibration chosen so the math is trivial to verify:
//   temperature = 0.001 * raw       (T0=0 °C at raw 0, T1=25 °C at raw 25000)
//   humidity    = (100/30000) * raw (H0=0 %RH at raw 0, H1=100 %RH at raw 30000)
static void preloadFactoryCalibration() {
    // H0_rH_x2 = 0 → H0 = 0 %RH, H1_rH_x2 = 200 → H1 = 100 %RH.
    Wire.setRegister(ADDR, CAL_BASE + 0x00, 0);
    Wire.setRegister(ADDR, CAL_BASE + 0x01, 200);
    // T0_degC_x8 = 0 → T0 = 0 °C, T1_degC_x8 = 200 → T1 = 25 °C.
    Wire.setRegister(ADDR, CAL_BASE + 0x02, 0);
    Wire.setRegister(ADDR, CAL_BASE + 0x03, 200);
    // Reserved byte (offset 4) and T1_T0_MSB (offset 5, high bits all zero).
    Wire.setRegister(ADDR, CAL_BASE + 0x04, 0);
    Wire.setRegister(ADDR, CAL_BASE + 0x05, 0);
    // H0_T0_OUT = 0 (bytes 6-7).
    Wire.setRegister(ADDR, CAL_BASE + 0x06, 0x00);
    Wire.setRegister(ADDR, CAL_BASE + 0x07, 0x00);
    // Reserved (bytes 8-9).
    Wire.setRegister(ADDR, CAL_BASE + 0x08, 0);
    Wire.setRegister(ADDR, CAL_BASE + 0x09, 0);
    // H1_T0_OUT = 30000 = 0x7530 (bytes 10-11, little endian).
    Wire.setRegister(ADDR, CAL_BASE + 0x0A, 0x30);
    Wire.setRegister(ADDR, CAL_BASE + 0x0B, 0x75);
    // T0_OUT = 0 (bytes 12-13).
    Wire.setRegister(ADDR, CAL_BASE + 0x0C, 0x00);
    Wire.setRegister(ADDR, CAL_BASE + 0x0D, 0x00);
    // T1_OUT = 25000 = 0x61A8 (bytes 14-15, little endian).
    Wire.setRegister(ADDR, CAL_BASE + 0x0E, 0xA8);
    Wire.setRegister(ADDR, CAL_BASE + 0x0F, 0x61);
}

// Preload humidity and temperature OUT registers with values that, through
// the calibration above, map to the requested clean values in %RH and °C.
static void preloadMeasurement(float tempC, float humidityPct) {
    auto rawFromTemp = [](float t) -> int16_t {
        return static_cast<int16_t>(t / 0.001f);  // 1 °C = 1000 LSB
    };
    auto rawFromHum = [](float h) -> int16_t {
        return static_cast<int16_t>(h * 300.0f);  // 100 %RH = 30000 LSB
    };

    int16_t tempRaw = rawFromTemp(tempC);
    int16_t humRaw = rawFromHum(humidityPct);

    Wire.setRegister(ADDR, TEMP_OUT_BASE + 0, tempRaw & 0xFF);
    Wire.setRegister(ADDR, TEMP_OUT_BASE + 1, (tempRaw >> 8) & 0xFF);
    Wire.setRegister(ADDR, HUM_OUT_BASE + 0, humRaw & 0xFF);
    Wire.setRegister(ADDR, HUM_OUT_BASE + 1, (humRaw >> 8) & 0xFF);
    Wire.setRegister(ADDR, HTS221_REG_STATUS, HTS221_STATUS_H_DA | HTS221_STATUS_T_DA);
}

HTS221 sensor;

void setUp(void) {
    Wire = TwoWire();
    preloadWhoAmI(true);
    preloadFactoryCalibration();
    sensor = HTS221();
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
    check_who_am_i(sensor, HTS221_WHO_AM_I_VALUE);
}

void test_power_on_sets_ctrl1_pd(void) {
    sensor.begin();
    sensor.powerOn();
    uint8_t ctrl1 = Wire.getRegister(ADDR, HTS221_REG_CTRL1);
    TEST_ASSERT_BITS_HIGH(HTS221_CTRL1_PD, ctrl1);
    TEST_ASSERT_BITS_HIGH(HTS221_CTRL1_BDU, ctrl1);
}

void test_power_off_clears_ctrl1_pd(void) {
    sensor.begin();
    sensor.powerOn();
    sensor.powerOff();
    uint8_t ctrl1 = Wire.getRegister(ADDR, HTS221_REG_CTRL1);
    TEST_ASSERT_BITS_LOW(HTS221_CTRL1_PD, ctrl1);
}

void test_set_continuous_writes_expected_ctrl1(void) {
    sensor.begin();
    sensor.setContinuous(HTS221_ODR_7_HZ);
    uint8_t ctrl1 = Wire.getRegister(ADDR, HTS221_REG_CTRL1);
    uint8_t expected = HTS221_CTRL1_PD | HTS221_CTRL1_BDU | HTS221_ODR_7_HZ;
    TEST_ASSERT_EQUAL_HEX8(expected, ctrl1);
}

void test_trigger_one_shot_sets_ctrl2_one_shot(void) {
    sensor.begin();
    sensor.triggerOneShot();
    uint8_t ctrl2 = Wire.getRegister(ADDR, HTS221_REG_CTRL2);
    TEST_ASSERT_BITS_HIGH(HTS221_CTRL2_ONE_SHOT, ctrl2);
}

void test_data_ready_reflects_status_register(void) {
    sensor.begin();
    sensor.setContinuous(HTS221_ODR_1_HZ);
    Wire.setRegister(ADDR, HTS221_REG_STATUS, 0);
    TEST_ASSERT_FALSE(sensor.dataReady());
    Wire.setRegister(ADDR, HTS221_REG_STATUS, HTS221_STATUS_T_DA);
    TEST_ASSERT_FALSE(sensor.dataReady());  // one channel only
    Wire.setRegister(ADDR, HTS221_REG_STATUS, HTS221_STATUS_H_DA | HTS221_STATUS_T_DA);
    TEST_ASSERT_TRUE(sensor.dataReady());
    TEST_ASSERT_TRUE(sensor.temperatureReady());
    TEST_ASSERT_TRUE(sensor.humidityReady());
}

void test_temperature_is_plausible(void) {
    sensor.begin();
    sensor.setContinuous(HTS221_ODR_1_HZ);
    preloadMeasurement(21.5f, 42.0f);

    check_read_plausible(sensor, &HTS221::temperature, -40.0f, 120.0f);
}

void test_humidity_is_plausible(void) {
    sensor.begin();
    sensor.setContinuous(HTS221_ODR_1_HZ);
    preloadMeasurement(21.5f, 42.0f);

    check_read_plausible(sensor, &HTS221::humidity, 0.0f, 100.0f);
}

void test_read_returns_calibrated_values(void) {
    sensor.begin();
    sensor.setContinuous(HTS221_ODR_1_HZ);
    preloadMeasurement(21.5f, 42.0f);

    auto r = sensor.read();
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 21.5f, r.temperature);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 42.0f, r.humidity);
}

void test_humidity_is_clamped_to_0_100(void) {
    sensor.begin();
    sensor.setContinuous(HTS221_ODR_1_HZ);
    Wire.setRegister(ADDR, TEMP_OUT_BASE + 0, 0);
    Wire.setRegister(ADDR, TEMP_OUT_BASE + 1, 0);
    Wire.setRegister(ADDR, HTS221_REG_STATUS, HTS221_STATUS_H_DA | HTS221_STATUS_T_DA);

    // Positive raw above the 30000 span → calibrated value > 100 %RH,
    // driver clamps to 100.
    Wire.setRegister(ADDR, HUM_OUT_BASE + 0, 0x00);
    Wire.setRegister(ADDR, HUM_OUT_BASE + 1, 0x7D);  // raw = 0x7D00 = 32000
    TEST_ASSERT_EQUAL_FLOAT(100.0f, sensor.humidity());

    // Negative raw (int16 interpretation) → calibrated value < 0, clamps to 0.
    Wire.setRegister(ADDR, HUM_OUT_BASE + 0, 0x00);
    Wire.setRegister(ADDR, HUM_OUT_BASE + 1, 0x80);  // raw = 0x8000 = -32768
    TEST_ASSERT_EQUAL_FLOAT(0.0f, sensor.humidity());
}

void test_read_auto_triggers_when_powered_down(void) {
    sensor.begin();  // begin() leaves the device in power-down
    preloadMeasurement(10.0f, 30.0f);
    Wire.clearWrites();

    auto r = sensor.read();

    TEST_ASSERT_FLOAT_WITHIN(0.01f, 10.0f, r.temperature);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 30.0f, r.humidity);

    // The read() flow should have emitted a CTRL1 power-up and a CTRL2
    // ONE_SHOT write before reading the OUT registers.
    bool sawCtrl1PowerUp = false;
    bool sawCtrl2OneShot = false;
    for (const auto& w : Wire.getWrites()) {
        if (w.reg == HTS221_REG_CTRL1 && (w.value & HTS221_CTRL1_PD)) {
            sawCtrl1PowerUp = true;
        }
        if (w.reg == HTS221_REG_CTRL2 && (w.value & HTS221_CTRL2_ONE_SHOT)) {
            sawCtrl2OneShot = true;
        }
    }
    TEST_ASSERT_TRUE_MESSAGE(sawCtrl1PowerUp, "auto-trigger missed CTRL1 PD write");
    TEST_ASSERT_TRUE_MESSAGE(sawCtrl2OneShot, "auto-trigger missed CTRL2 ONE_SHOT write");
}

void test_set_temperature_offset_shifts_reading(void) {
    sensor.begin();
    sensor.setContinuous(HTS221_ODR_1_HZ);
    preloadMeasurement(20.0f, 50.0f);
    sensor.setTemperatureOffset(-1.5f);

    TEST_ASSERT_FLOAT_WITHIN(0.01f, 18.5f, sensor.temperature());
}

void test_calibrate_temperature_applies_two_point_correction(void) {
    sensor.begin();
    sensor.setContinuous(HTS221_ODR_1_HZ);
    // Factory curve maps raw -> 20 °C; user says "when sensor reads 20,
    // the truth is 22; when sensor reads 0, the truth is 1". Linear fit:
    //   corrected = 1 + (22 - 1) / (20 - 0) * factory = 1 + 1.05 * factory
    sensor.calibrateTemperature(1.0f, 0.0f, 22.0f, 20.0f);
    preloadMeasurement(20.0f, 50.0f);

    TEST_ASSERT_FLOAT_WITHIN(0.01f, 22.0f, sensor.temperature());
}

void test_read_returns_nan_on_timeout(void) {
    // begin() succeeds (WHO_AM_I preloaded in setUp) but leaves the device
    // powered down, and STATUS is not preloaded with any DA bit. The
    // auto-trigger path in read() will therefore hit the waitForDataReady
    // timeout and must surface it as NaN rather than decoding whatever
    // happened to be left on the stack.
    sensor.begin();

    auto r = sensor.read();

    TEST_ASSERT_TRUE_MESSAGE(isnan(r.temperature), "temperature should be NaN on timeout");
    TEST_ASSERT_TRUE_MESSAGE(isnan(r.humidity), "humidity should be NaN on timeout");
}

void test_reboot_writes_ctrl2_boot(void) {
    sensor.begin();
    Wire.clearWrites();
    // Make BOOT bit clear itself on the very first poll so the driver
    // doesn't spin against the mock.
    Wire.setRegister(ADDR, HTS221_REG_CTRL2, 0);

    sensor.reboot();

    bool sawBootSet = false;
    for (const auto& w : Wire.getWrites()) {
        if (w.reg == HTS221_REG_CTRL2 && (w.value & HTS221_CTRL2_BOOT)) {
            sawBootSet = true;
            break;
        }
    }
    TEST_ASSERT_TRUE(sawBootSet);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_begin_detects_device);
    RUN_TEST(test_begin_rejects_wrong_who_am_i);
    RUN_TEST(test_device_id_returns_who_am_i);
    RUN_TEST(test_power_on_sets_ctrl1_pd);
    RUN_TEST(test_power_off_clears_ctrl1_pd);
    RUN_TEST(test_set_continuous_writes_expected_ctrl1);
    RUN_TEST(test_trigger_one_shot_sets_ctrl2_one_shot);
    RUN_TEST(test_data_ready_reflects_status_register);
    RUN_TEST(test_temperature_is_plausible);
    RUN_TEST(test_humidity_is_plausible);
    RUN_TEST(test_read_returns_calibrated_values);
    RUN_TEST(test_humidity_is_clamped_to_0_100);
    RUN_TEST(test_read_auto_triggers_when_powered_down);
    RUN_TEST(test_set_temperature_offset_shifts_reading);
    RUN_TEST(test_calibrate_temperature_applies_two_point_correction);
    RUN_TEST(test_read_returns_nan_on_timeout);
    RUN_TEST(test_reboot_writes_ctrl2_boot);
    return UNITY_END();
}
