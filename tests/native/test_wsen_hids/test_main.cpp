// SPDX-License-Identifier: GPL-3.0-or-later

#include <math.h>
#include <unity.h>

#include "SteamiWsenHids.h"
#include "Wire.h"
#include "driver_checks.h"
#include "steami_fake_chips.h"

// The driver uses bit 7 of the sub-address as the ST auto-increment flag for
// multi-byte reads (see WSEN_HIDS_AUTO_INCREMENT). The Wire mock is a straight
// register map, so preloads for burst-read ranges have to sit at the
// post-OR address the driver will actually emit on the wire.
constexpr uint8_t ADDR = WSEN_HIDS_DEFAULT_ADDRESS;

constexpr uint8_t CAL_BASE = STEAMI_FAKE_HIDS_CAL_BASE;
constexpr uint8_t HUM_OUT_BASE = STEAMI_FAKE_HIDS_HUM_BASE;
constexpr uint8_t TEMP_OUT_BASE = STEAMI_FAKE_HIDS_TEMP_BASE;

static void preloadWhoAmI(bool valid = true) {
    steamiFakeWsenHidsWhoAmI(Wire, valid);
}

static void preloadFactoryCalibration() {
    steamiFakeWsenHidsCalibration(Wire);
}

static void preloadMeasurement(float tempC, float humidityPct) {
    steamiFakeWsenHidsMeasurement(Wire, tempC, humidityPct);
}

SteamiWsenHids sensor;

// Register model of the WSEN-HIDS as seen on the STeaMi V1a/V1b
// (steamicc/micropython-steami-lib#425): a CTRL2.ONE_SHOT request stays
// latched and converts nothing, and samples only come in continuous mode
// (PD = 1, ODR != 00). A conversion copies the staged sample into the
// output registers when STATUS is polled, so output registers that were
// not refreshed by a conversion hold stale data. With `dead` set, no
// conversion ever completes. Every write is logged, in order.
class LatchedHids : public I2CDevice {
   public:
    struct Write {
        uint8_t reg;
        uint8_t value;
    };

    std::map<uint8_t, uint8_t> regs;
    std::vector<Write> writes;
    bool dead = false;

    // Start from the WHO_AM_I and calibration preloaded in the Wire
    // register map, with stale (zero) output registers and STATUS.
    void loadFrom(const TwoWire& wire) {
        for (uint16_t reg = 0; reg < 0x80; ++reg) {
            regs[static_cast<uint8_t>(reg)] =
                wire.getRegister(ADDR, reg | WSEN_HIDS_AUTO_INCREMENT);
            uint8_t direct = wire.getRegister(ADDR, reg);
            if (direct != 0) {
                regs[static_cast<uint8_t>(reg)] = direct;
            }
        }
        for (uint8_t reg = WSEN_HIDS_REG_HUMIDITY_OUT_L; reg <= WSEN_HIDS_REG_TEMP_OUT_L + 1;
             ++reg) {
            regs[reg] = 0;
        }
        regs[WSEN_HIDS_REG_STATUS] = 0;
        regs[WSEN_HIDS_REG_CTRL1] = 0;
        regs[WSEN_HIDS_REG_CTRL2] = 0;
    }

    // Sample delivered by the next conversion.
    void stage(float tempC, float humidity) {
        _tempRaw = static_cast<int16_t>(tempC / 0.001f);
        _humRaw = static_cast<int16_t>(humidity * 300.0f);
    }

    bool continuous() const {
        uint8_t ctrl1 = regs.at(WSEN_HIDS_REG_CTRL1);
        return (ctrl1 & WSEN_HIDS_CTRL1_PD) && (ctrl1 & WSEN_HIDS_CTRL1_ODR_MASK) != 0;
    }

    bool wroteOneShot() const {
        for (const auto& w : writes) {
            if (w.reg == WSEN_HIDS_REG_CTRL2 && (w.value & WSEN_HIDS_CTRL2_ONE_SHOT)) {
                return true;
            }
        }
        return false;
    }

    bool wroteCtrl1(uint8_t value) const {
        for (const auto& w : writes) {
            if (w.reg == WSEN_HIDS_REG_CTRL1 && w.value == value) {
                return true;
            }
        }
        return false;
    }

    uint8_t onWrite(const std::vector<uint8_t>& frame) override {
        if (frame.empty()) {
            return 0;
        }
        _pointer = frame[0] & static_cast<uint8_t>(~WSEN_HIDS_AUTO_INCREMENT);
        for (size_t i = 1; i < frame.size(); ++i) {
            uint8_t reg = static_cast<uint8_t>(_pointer + i - 1);
            regs[reg] = frame[i];
            writes.push_back({reg, frame[i]});
        }
        return 0;
    }

    std::vector<uint8_t> onRead(size_t quantity) override {
        std::vector<uint8_t> out;
        for (size_t i = 0; i < quantity; ++i) {
            uint8_t reg = static_cast<uint8_t>(_pointer + i);
            if (reg == WSEN_HIDS_REG_STATUS) {
                convertIfRunning();
            }
            out.push_back(regs[reg]);
        }
        return out;
    }

   private:
    void convertIfRunning() {
        if (dead || !continuous()) {
            regs[WSEN_HIDS_REG_STATUS] = 0;
            return;
        }
        regs[WSEN_HIDS_REG_HUMIDITY_OUT_L] = _humRaw & 0xFF;
        regs[WSEN_HIDS_REG_HUMIDITY_OUT_L + 1] = (_humRaw >> 8) & 0xFF;
        regs[WSEN_HIDS_REG_TEMP_OUT_L] = _tempRaw & 0xFF;
        regs[WSEN_HIDS_REG_TEMP_OUT_L + 1] = (_tempRaw >> 8) & 0xFF;
        regs[WSEN_HIDS_REG_STATUS] = WSEN_HIDS_STATUS_H_DA | WSEN_HIDS_STATUS_T_DA;
    }

    uint8_t _pointer = 0;
    int16_t _tempRaw = 0;
    int16_t _humRaw = 0;
};

LatchedHids chip;

// begin() on the latched chip model, with a sample staged.
static void beginOnLatchedChip(float tempC = 23.0f, float humidityPct = 40.0f) {
    chip = LatchedHids();
    chip.loadFrom(Wire);
    chip.stage(tempC, humidityPct);
    Wire.attachDevice(ADDR, &chip);
    TEST_ASSERT_TRUE(sensor.begin());
    chip.writes.clear();
}

void setUp(void) {
    Wire = TwoWire();
    preloadWhoAmI(true);
    preloadFactoryCalibration();
    sensor = SteamiWsenHids();
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
    check_who_am_i(sensor, WSEN_HIDS_WHO_AM_I_VALUE);
}

void test_power_on_sets_ctrl1_pd(void) {
    sensor.begin();
    sensor.powerOn();
    uint8_t ctrl1 = Wire.getRegister(ADDR, WSEN_HIDS_REG_CTRL1);
    TEST_ASSERT_BITS_HIGH(WSEN_HIDS_CTRL1_PD, ctrl1);
    TEST_ASSERT_BITS_HIGH(WSEN_HIDS_CTRL1_BDU, ctrl1);
}

void test_power_off_clears_ctrl1_pd(void) {
    sensor.begin();
    sensor.powerOn();
    sensor.powerOff();
    uint8_t ctrl1 = Wire.getRegister(ADDR, WSEN_HIDS_REG_CTRL1);
    TEST_ASSERT_BITS_LOW(WSEN_HIDS_CTRL1_PD, ctrl1);
}

void test_set_continuous_writes_expected_ctrl1(void) {
    sensor.begin();
    sensor.setContinuous(WSEN_HIDS_ODR_7_HZ);
    uint8_t ctrl1 = Wire.getRegister(ADDR, WSEN_HIDS_REG_CTRL1);
    uint8_t expected = WSEN_HIDS_CTRL1_PD | WSEN_HIDS_CTRL1_BDU | WSEN_HIDS_ODR_7_HZ;
    TEST_ASSERT_EQUAL_HEX8(expected, ctrl1);
}

// enable_bdu() of the MicroPython driver (#316).
void test_enable_bdu_flips_only_the_bdu_bit(void) {
    sensor.begin();
    sensor.setContinuous(WSEN_HIDS_ODR_7_HZ);
    sensor.enableBdu(false);
    TEST_ASSERT_EQUAL_HEX8(WSEN_HIDS_CTRL1_PD | WSEN_HIDS_ODR_7_HZ,
                           Wire.getRegister(ADDR, WSEN_HIDS_REG_CTRL1));
    sensor.enableBdu();
    TEST_ASSERT_EQUAL_HEX8(WSEN_HIDS_CTRL1_PD | WSEN_HIDS_CTRL1_BDU | WSEN_HIDS_ODR_7_HZ,
                           Wire.getRegister(ADDR, WSEN_HIDS_REG_CTRL1));
}

void test_disabled_bdu_survives_mode_changes(void) {
    sensor.begin();
    sensor.enableBdu(false);
    sensor.setContinuous(WSEN_HIDS_ODR_1_HZ);
    TEST_ASSERT_BITS_LOW(WSEN_HIDS_CTRL1_BDU, Wire.getRegister(ADDR, WSEN_HIDS_REG_CTRL1));
    sensor.powerOff();
    sensor.powerOn();
    TEST_ASSERT_BITS_LOW(WSEN_HIDS_CTRL1_BDU, Wire.getRegister(ADDR, WSEN_HIDS_REG_CTRL1));
    sensor.triggerOneShot();
    TEST_ASSERT_BITS_LOW(WSEN_HIDS_CTRL1_BDU, Wire.getRegister(ADDR, WSEN_HIDS_REG_CTRL1));
}

void test_trigger_one_shot_sets_ctrl2_one_shot(void) {
    sensor.begin();
    sensor.triggerOneShot();
    uint8_t ctrl2 = Wire.getRegister(ADDR, WSEN_HIDS_REG_CTRL2);
    TEST_ASSERT_BITS_HIGH(WSEN_HIDS_CTRL2_ONE_SHOT, ctrl2);
}

void test_data_ready_reflects_status_register(void) {
    sensor.begin();
    sensor.setContinuous(WSEN_HIDS_ODR_1_HZ);
    Wire.setRegister(ADDR, WSEN_HIDS_REG_STATUS, 0);
    TEST_ASSERT_FALSE(sensor.dataReady());
    Wire.setRegister(ADDR, WSEN_HIDS_REG_STATUS, WSEN_HIDS_STATUS_T_DA);
    TEST_ASSERT_FALSE(sensor.dataReady());  // one channel only
    Wire.setRegister(ADDR, WSEN_HIDS_REG_STATUS, WSEN_HIDS_STATUS_H_DA | WSEN_HIDS_STATUS_T_DA);
    TEST_ASSERT_TRUE(sensor.dataReady());
    TEST_ASSERT_TRUE(sensor.temperatureReady());
    TEST_ASSERT_TRUE(sensor.humidityReady());
}

void test_temperature_is_plausible(void) {
    sensor.begin();
    sensor.setContinuous(WSEN_HIDS_ODR_1_HZ);
    preloadMeasurement(21.5f, 42.0f);

    check_read_plausible(sensor, &SteamiWsenHids::temperature, -40.0f, 120.0f);
}

void test_humidity_is_plausible(void) {
    sensor.begin();
    sensor.setContinuous(WSEN_HIDS_ODR_1_HZ);
    preloadMeasurement(21.5f, 42.0f);

    check_read_plausible(sensor, &SteamiWsenHids::humidity, 0.0f, 100.0f);
}

void test_read_returns_calibrated_values(void) {
    sensor.begin();
    sensor.setContinuous(WSEN_HIDS_ODR_1_HZ);
    preloadMeasurement(21.5f, 42.0f);

    auto r = sensor.read();
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 21.5f, r.temperature);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 42.0f, r.humidity);
}

void test_humidity_is_clamped_to_0_100(void) {
    sensor.begin();
    sensor.setContinuous(WSEN_HIDS_ODR_1_HZ);
    Wire.setRegister(ADDR, TEMP_OUT_BASE + 0, 0);
    Wire.setRegister(ADDR, TEMP_OUT_BASE + 1, 0);
    Wire.setRegister(ADDR, WSEN_HIDS_REG_STATUS, WSEN_HIDS_STATUS_H_DA | WSEN_HIDS_STATUS_T_DA);

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

    // The read() flow should have written CTRL1 with PD=1, BDU=1, and
    // ODR=12.5 Hz (continuous mode auto-bring-up). It must NOT have
    // touched CTRL2.ONE_SHOT — the one-shot path is unreliable on this
    // silicon and the driver deliberately avoids it here.
    bool sawCtrl1Continuous = false;
    bool sawCtrl2OneShot = false;
    for (const auto& w : Wire.getWrites()) {
        if (w.reg == WSEN_HIDS_REG_CTRL1) {
            uint8_t expected = WSEN_HIDS_CTRL1_PD | WSEN_HIDS_CTRL1_BDU | WSEN_HIDS_ODR_12_5_HZ;
            if (w.value == expected) {
                sawCtrl1Continuous = true;
            }
        }
        if (w.reg == WSEN_HIDS_REG_CTRL2 && (w.value & WSEN_HIDS_CTRL2_ONE_SHOT)) {
            sawCtrl2OneShot = true;
        }
    }
    TEST_ASSERT_TRUE_MESSAGE(sawCtrl1Continuous,
                             "auto-trigger missed CTRL1 PD|BDU|ODR=12.5Hz write");
    TEST_ASSERT_FALSE_MESSAGE(sawCtrl2OneShot,
                              "auto-trigger must not poke the unreliable ONE_SHOT path");
}

void test_set_temperature_offset_shifts_reading(void) {
    sensor.begin();
    sensor.setContinuous(WSEN_HIDS_ODR_1_HZ);
    preloadMeasurement(20.0f, 50.0f);
    sensor.setTemperatureOffset(-1.5f);

    TEST_ASSERT_FLOAT_WITHIN(0.01f, 18.5f, sensor.temperature());
}

void test_calibrate_temperature_applies_two_point_correction(void) {
    sensor.begin();
    sensor.setContinuous(WSEN_HIDS_ODR_1_HZ);
    // Factory curve maps raw -> 20 °C; user says "when sensor reads 20,
    // the truth is 22; when sensor reads 0, the truth is 1". Linear fit:
    //   corrected = 1 + (22 - 1) / (20 - 0) * factory = 1 + 1.05 * factory
    sensor.calibrateTemperature(1.0f, 0.0f, 22.0f, 20.0f);
    preloadMeasurement(20.0f, 50.0f);

    TEST_ASSERT_FLOAT_WITHIN(0.01f, 22.0f, sensor.temperature());
}

void test_calibrate_temperature_returns_true(void) {
    sensor.begin();
    TEST_ASSERT_TRUE(sensor.calibrateTemperature(1.0f, 0.0f, 22.0f, 20.0f));
}

void test_calibrate_temperature_with_equal_points_keeps_calibration(void) {
    // Same contract as ISM330DL, LIS2MDL and MicroPython (ValueError there).
    // Start from a gain != 1 so that a reset of the gain alone would show.
    sensor.begin();
    sensor.setContinuous(WSEN_HIDS_ODR_1_HZ);
    sensor.calibrateTemperature(1.0f, 0.0f, 22.0f, 20.0f);  // gain 1.05, offset 1.0
    TEST_ASSERT_FALSE(sensor.calibrateTemperature(0.0f, 5.0f, 100.0f, 5.0f));
    preloadMeasurement(20.0f, 50.0f);

    // 1.05 * 20 + 1.0 = 22.0 (gain reset to 1: 21.0, identity: 20.0)
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 22.0f, sensor.temperature());
}

void test_set_temperature_offset_replaces_two_point_calibration(void) {
    // The offset resets the gain to 1, as in MicroPython set_temp_offset().
    sensor.begin();
    sensor.setContinuous(WSEN_HIDS_ODR_1_HZ);
    sensor.calibrateTemperature(1.0f, 0.0f, 22.0f, 20.0f);
    sensor.setTemperatureOffset(-1.5f);
    preloadMeasurement(20.0f, 50.0f);

    TEST_ASSERT_FLOAT_WITHIN(0.01f, 18.5f, sensor.temperature());
}

void test_calibrate_temperature_replaces_offset(void) {
    sensor.begin();
    sensor.setContinuous(WSEN_HIDS_ODR_1_HZ);
    sensor.setTemperatureOffset(-1.5f);
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
    Wire.setRegister(ADDR, WSEN_HIDS_REG_CTRL2, 0);

    sensor.reboot();

    bool sawBootSet = false;
    for (const auto& w : Wire.getWrites()) {
        if (w.reg == WSEN_HIDS_REG_CTRL2 && (w.value & WSEN_HIDS_CTRL2_BOOT)) {
            sawBootSet = true;
            break;
        }
    }
    TEST_ASSERT_TRUE(sawBootSet);
}

void test_soft_reset_routes_to_reboot(void) {
    sensor.begin();
    Wire.clearWrites();
    Wire.setRegister(ADDR, WSEN_HIDS_REG_CTRL2, 0);

    sensor.softReset();

    bool sawBootSet = false;
    for (const auto& w : Wire.getWrites()) {
        if (w.reg == WSEN_HIDS_REG_CTRL2 && (w.value & WSEN_HIDS_CTRL2_BOOT)) {
            sawBootSet = true;
            break;
        }
    }
    TEST_ASSERT_TRUE_MESSAGE(sawBootSet, "softReset() should toggle CTRL2.BOOT like reboot()");
}

void test_set_averaging_writes_av_conf(void) {
    sensor.begin();
    // Humidity averaging code 5 (=> AVGH_128), temperature averaging code 3
    // (=> AVGT_16). Driver packs them as [AVGT(5..3) | AVGH(2..0)].
    sensor.setAveraging(0x05, 0x03);
    uint8_t avconf = Wire.getRegister(ADDR, WSEN_HIDS_REG_AV_CONF);
    uint8_t expected = static_cast<uint8_t>((0x03 << WSEN_HIDS_AV_CONF_AVGT_SHIFT) |
                                            (0x05 & WSEN_HIDS_AV_CONF_AVGH_MASK));
    TEST_ASSERT_EQUAL_HEX8(expected, avconf);
}

// The chip lives on its own VDD rail on the STeaMi — an MCU reset does not
// power-cycle it, so previous code can leave AV_CONF pegged at max
// averaging (0x3F → 256+512 samples, which pushes any conversion past a
// reasonable timeout). begin() must override AV_CONF back to the datasheet
// default regardless of what the chip carries from a previous run.
void test_begin_resets_av_conf_to_default(void) {
    // Simulate the chip coming up with AV_CONF stuck at 0x3F from a
    // previous sketch.
    Wire.setRegister(ADDR, WSEN_HIDS_REG_AV_CONF, 0x3F);

    sensor.begin();

    TEST_ASSERT_EQUAL_HEX8(WSEN_HIDS_AV_CONF_DEFAULT,
                           Wire.getRegister(ADDR, WSEN_HIDS_REG_AV_CONF));
}

// Same chip-state-survives-reset concern for CTRL2: a previous one-shot
// attempt that the caller gave up on can leave CTRL2.ONE_SHOT latched at
// 1, and the next trigger then sees no 0→1 edge. begin() must clear it.
void test_begin_clears_ctrl2(void) {
    Wire.setRegister(ADDR, WSEN_HIDS_REG_CTRL2, WSEN_HIDS_CTRL2_ONE_SHOT);

    sensor.begin();

    TEST_ASSERT_EQUAL_HEX8(0x00, Wire.getRegister(ADDR, WSEN_HIDS_REG_CTRL2));
}

// Following the stm32duino / Würth reference-driver pattern, the
// auto-bring-up inside read() routes through continuous mode. After
// read() returns, the chip must remain in continuous mode at the chosen
// ODR so the next read short-circuits to a plain OUT-register read
// instead of paying the bring-up cost again.
void test_read_auto_bringup_leaves_chip_in_continuous(void) {
    sensor.begin();
    preloadMeasurement(20.0f, 50.0f);

    sensor.read();

    uint8_t ctrl1 = Wire.getRegister(ADDR, WSEN_HIDS_REG_CTRL1);
    uint8_t expected = WSEN_HIDS_CTRL1_PD | WSEN_HIDS_CTRL1_BDU | WSEN_HIDS_ODR_12_5_HZ;
    TEST_ASSERT_EQUAL_HEX8(expected, ctrl1);
}

// readOneShot() takes its sample in continuous mode, as MicroPython
// read_one_shot() since steamicc/micropython-steami-lib#486, so it works
// on a chip whose ONE_SHOT bit latches (#425).
void test_read_one_shot_on_latched_chip_returns_sample(void) {
    beginOnLatchedChip(23.0f, 40.0f);

    auto r = sensor.readOneShot();

    TEST_ASSERT_FLOAT_WITHIN(0.01f, 23.0f, r.temperature);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 40.0f, r.humidity);
    TEST_ASSERT_TRUE_MESSAGE(
        chip.wroteCtrl1(WSEN_HIDS_CTRL1_PD | WSEN_HIDS_CTRL1_BDU | WSEN_HIDS_ODR_12_5_HZ),
        "readOneShot() must bring the chip up at 12.5 Hz");
    TEST_ASSERT_FALSE_MESSAGE(chip.wroteOneShot(), "readOneShot() must not use CTRL2.ONE_SHOT");
}

// A one-shot leaves the chip in the mode it found it in: power-down
// after begin().
void test_read_one_shot_goes_back_to_power_down(void) {
    beginOnLatchedChip();

    sensor.readOneShot();

    TEST_ASSERT_EQUAL_HEX8(0x00, chip.regs[WSEN_HIDS_REG_CTRL1] & WSEN_HIDS_CTRL1_PD);
}

void test_read_one_shot_restores_continuous_mode(void) {
    beginOnLatchedChip();
    sensor.setContinuous(WSEN_HIDS_ODR_1_HZ);

    sensor.readOneShot();

    TEST_ASSERT_EQUAL_HEX8(WSEN_HIDS_CTRL1_PD | WSEN_HIDS_CTRL1_BDU | WSEN_HIDS_ODR_1_HZ,
                           chip.regs[WSEN_HIDS_REG_CTRL1]);
}

void test_read_one_shot_restores_one_shot_mode_and_bdu_off(void) {
    beginOnLatchedChip();
    sensor.enableBdu(false);
    sensor.setOneShotMode();

    sensor.readOneShot();

    TEST_ASSERT_EQUAL_HEX8(WSEN_HIDS_CTRL1_PD | WSEN_HIDS_ODR_ONE_SHOT,
                           chip.regs[WSEN_HIDS_REG_CTRL1]);
    TEST_ASSERT_TRUE_MESSAGE(chip.wroteCtrl1(WSEN_HIDS_CTRL1_PD | WSEN_HIDS_ODR_12_5_HZ),
                             "the bring-up must keep BDU off");
}

void test_read_one_shot_takes_a_new_sample_each_call(void) {
    beginOnLatchedChip(23.0f, 40.0f);
    sensor.readOneShot();
    chip.stage(26.0f, 55.0f);

    auto r = sensor.readOneShot();

    TEST_ASSERT_FLOAT_WITHIN(0.01f, 26.0f, r.temperature);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 55.0f, r.humidity);
}

// No sample within 200 ms (the MicroPython DEFAULT_ONE_SHOT_TIMEOUT_MS):
// NaN, and the chip still goes back to power-down.
void test_read_one_shot_timeout_returns_nan_and_powers_down(void) {
    beginOnLatchedChip();
    chip.dead = true;
    uint32_t start = millis();

    auto r = sensor.readOneShot();

    uint32_t elapsed = millis() - start;
    TEST_ASSERT_TRUE(isnan(r.temperature));
    TEST_ASSERT_TRUE(isnan(r.humidity));
    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(200, elapsed);
    TEST_ASSERT_LESS_THAN_UINT32(300, elapsed);
    TEST_ASSERT_EQUAL_HEX8(0x00, chip.regs[WSEN_HIDS_REG_CTRL1] & WSEN_HIDS_CTRL1_PD);
}

// CTRL2 is left alone, so the heater stays as the caller set it.
void test_read_one_shot_keeps_heater(void) {
    beginOnLatchedChip();
    sensor.enableHeater();

    sensor.readOneShot();

    TEST_ASSERT_EQUAL_HEX8(WSEN_HIDS_CTRL2_HEATER, chip.regs[WSEN_HIDS_REG_CTRL2]);
}

// PD = 1 with ODR = 00 runs no conversion: read() must take a new sample
// instead of returning the stale output registers, as MicroPython read().
void test_read_takes_new_sample_in_one_shot_mode(void) {
    beginOnLatchedChip(23.0f, 40.0f);
    sensor.setOneShotMode();

    auto r = sensor.read();

    TEST_ASSERT_FLOAT_WITHIN(0.01f, 23.0f, r.temperature);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 40.0f, r.humidity);
}

void test_read_takes_new_sample_after_power_on(void) {
    beginOnLatchedChip(23.0f, 40.0f);
    sensor.powerOn();

    auto r = sensor.read();

    TEST_ASSERT_FLOAT_WITHIN(0.01f, 23.0f, r.temperature);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 40.0f, r.humidity);
}

// enable_heater() of the MicroPython driver: CTRL2 bit 1, other bits kept.
void test_enable_heater_drives_ctrl2_bit_1(void) {
    sensor.begin();
    sensor.enableHeater(true);
    TEST_ASSERT_EQUAL_HEX8(0x02, Wire.getRegister(ADDR, WSEN_HIDS_REG_CTRL2));
    sensor.enableHeater(false);
    TEST_ASSERT_EQUAL_HEX8(0x00, Wire.getRegister(ADDR, WSEN_HIDS_REG_CTRL2));
}

void test_enable_heater_defaults_to_on_and_keeps_other_bits(void) {
    sensor.begin();
    Wire.setRegister(ADDR, WSEN_HIDS_REG_CTRL2, WSEN_HIDS_CTRL2_ONE_SHOT);
    sensor.enableHeater();
    TEST_ASSERT_EQUAL_HEX8(WSEN_HIDS_CTRL2_HEATER | WSEN_HIDS_CTRL2_ONE_SHOT,
                           Wire.getRegister(ADDR, WSEN_HIDS_REG_CTRL2));
}

void test_trigger_one_shot_keeps_heater(void) {
    sensor.begin();
    sensor.enableHeater();
    sensor.triggerOneShot();
    TEST_ASSERT_EQUAL_HEX8(WSEN_HIDS_CTRL2_HEATER | WSEN_HIDS_CTRL2_ONE_SHOT,
                           Wire.getRegister(ADDR, WSEN_HIDS_REG_CTRL2));
}

// set_one_shot_mode() of the MicroPython driver: PD = 1, ODR = 00.
void test_set_one_shot_mode_writes_pd_and_odr_00(void) {
    sensor.begin();
    sensor.setContinuous(WSEN_HIDS_ODR_7_HZ);
    sensor.setOneShotMode();
    TEST_ASSERT_EQUAL_HEX8(WSEN_HIDS_CTRL1_PD | WSEN_HIDS_CTRL1_BDU | WSEN_HIDS_ODR_ONE_SHOT,
                           Wire.getRegister(ADDR, WSEN_HIDS_REG_CTRL1));
    sensor.enableBdu(false);
    sensor.setOneShotMode();
    TEST_ASSERT_EQUAL_HEX8(WSEN_HIDS_CTRL1_PD | WSEN_HIDS_ODR_ONE_SHOT,
                           Wire.getRegister(ADDR, WSEN_HIDS_REG_CTRL1));
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
    RUN_TEST(test_enable_bdu_flips_only_the_bdu_bit);
    RUN_TEST(test_disabled_bdu_survives_mode_changes);
    RUN_TEST(test_data_ready_reflects_status_register);
    RUN_TEST(test_temperature_is_plausible);
    RUN_TEST(test_humidity_is_plausible);
    RUN_TEST(test_read_returns_calibrated_values);
    RUN_TEST(test_humidity_is_clamped_to_0_100);
    RUN_TEST(test_read_auto_triggers_when_powered_down);
    RUN_TEST(test_set_temperature_offset_shifts_reading);
    RUN_TEST(test_calibrate_temperature_applies_two_point_correction);
    RUN_TEST(test_calibrate_temperature_returns_true);
    RUN_TEST(test_calibrate_temperature_with_equal_points_keeps_calibration);
    RUN_TEST(test_set_temperature_offset_replaces_two_point_calibration);
    RUN_TEST(test_calibrate_temperature_replaces_offset);
    RUN_TEST(test_read_returns_nan_on_timeout);
    RUN_TEST(test_reboot_writes_ctrl2_boot);
    RUN_TEST(test_soft_reset_routes_to_reboot);
    RUN_TEST(test_set_averaging_writes_av_conf);
    RUN_TEST(test_begin_resets_av_conf_to_default);
    RUN_TEST(test_begin_clears_ctrl2);
    RUN_TEST(test_read_auto_bringup_leaves_chip_in_continuous);
    RUN_TEST(test_read_one_shot_on_latched_chip_returns_sample);
    RUN_TEST(test_read_one_shot_goes_back_to_power_down);
    RUN_TEST(test_read_one_shot_restores_continuous_mode);
    RUN_TEST(test_read_one_shot_restores_one_shot_mode_and_bdu_off);
    RUN_TEST(test_read_one_shot_takes_a_new_sample_each_call);
    RUN_TEST(test_read_one_shot_timeout_returns_nan_and_powers_down);
    RUN_TEST(test_read_one_shot_keeps_heater);
    RUN_TEST(test_read_takes_new_sample_in_one_shot_mode);
    RUN_TEST(test_read_takes_new_sample_after_power_on);
    RUN_TEST(test_enable_heater_drives_ctrl2_bit_1);
    RUN_TEST(test_enable_heater_defaults_to_on_and_keeps_other_bits);
    RUN_TEST(test_trigger_one_shot_keeps_heater);
    RUN_TEST(test_set_one_shot_mode_writes_pd_and_odr_00);
    return UNITY_END();
}
