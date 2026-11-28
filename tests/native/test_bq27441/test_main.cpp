// SPDX-License-Identifier: GPL-3.0-or-later

#include <math.h>
#include <unity.h>

#include "SteamiBQ27441.h"
#include "Wire.h"
#include "steami_fake_chips.h"

constexpr uint8_t ADDR = BQ27441_I2C_ADDRESS;

static void preloadWhoAmI(bool valid = true) {
    steamiFakeBq27441WhoAmI(Wire, valid);
}

static void preloadMeasurement(uint16_t voltage, int16_t current_avg, uint8_t soc) {
    steamiFakeBq27441Measurement(Wire, voltage, current_avg, soc);
}

SteamiBQ27441* sensor = nullptr;
void setUp(void) {
    Wire = TwoWire();
    millisClock() = 0xFFFFFFFF - 1;
    steamiFakeBq27441(Wire);  // control responses and the CFGUPMODE flag
    sensor = new SteamiBQ27441(Wire);
    preloadWhoAmI(true);
}

void tearDown(void) {
    delete sensor;
    sensor = nullptr;
}

void test_begin_returns_true_when_device_id_matches(void) {
    TEST_ASSERT_TRUE(sensor->begin());
}

void test_begin_returns_false_when_device_id_wrong(void) {
    Wire.setControlResponse(BQ27441_CONTROL_DEVICE_TYPE, 0x1234);
    TEST_ASSERT_FALSE(sensor->begin());
}

void test_voltage_returns_raw_register_value(void) {
    preloadMeasurement(4700, 0, 0);
    TEST_ASSERT_EQUAL(4700, sensor->voltageMv());
}

void test_current_average_returns_signed_value(void) {
    preloadMeasurement(0, -1000, 0);
    TEST_ASSERT_EQUAL(-1000, sensor->currentAverage());
}

void test_state_of_charge_returns_filtered_soc(void) {
    preloadMeasurement(0, 0, 80);
    TEST_ASSERT_EQUAL(80, sensor->stateOfCharge());
}

void test_state_of_health_returns_low_byte(void) {
    Wire.setRegister(ADDR, BQ27441_COMMAND_SOH, 0xAB);
    Wire.setRegister(ADDR, BQ27441_COMMAND_SOH + 1, 0xCD);
    TEST_ASSERT_EQUAL(0xAB, sensor->stateOfHealth());
}

void test_temperature_converts_decikelvin_to_celsius(void) {
    Wire.setRegister(ADDR, BQ27441_COMMAND_TEMP, 0x6C);
    Wire.setRegister(ADDR, BQ27441_COMMAND_TEMP + 1, 0x1C);
    float tempC = sensor->temperature();
    TEST_ASSERT_FLOAT_WITHIN(0.1, 454.45, tempC);
}

void test_temperature_internal_reads_correct_register(void) {
    Wire.setRegister(ADDR, BQ27441_COMMAND_INT_TEMP, 0x34);
    Wire.setRegister(ADDR, BQ27441_COMMAND_INT_TEMP + 1, 0x12);
    float tempC = sensor->temperature(SteamiBQ27441::TempMeasureType::INTERNAL_TEMP);
    TEST_ASSERT_FLOAT_WITHIN(0.1, 4660 * 0.1 - 273.15, tempC);
}

// Ported from the MicroPython sister project's bq27441.yaml scenarios:
// the average-power register lives in its own command and the readout
// is a plain signed 16-bit value (185 mW in the MP fixture).
void test_power_reads_avg_power_register(void) {
    Wire.setRegister(ADDR, BQ27441_COMMAND_AVG_POWER, 0xB9);
    Wire.setRegister(ADDR, BQ27441_COMMAND_AVG_POWER + 1, 0x00);
    TEST_ASSERT_EQUAL(185, sensor->power());
}

// MP's "Battery temperature in Kelvin" check. Raw 2981 in deci-K maps
// to 298.1 K.
void test_temperature_k_converts_decikelvin_to_kelvin(void) {
    Wire.setRegister(ADDR, BQ27441_COMMAND_TEMP, 0xA5);
    Wire.setRegister(ADDR, BQ27441_COMMAND_TEMP + 1, 0x0B);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 298.1f, sensor->temperatureK());
}

// MP's "Battery temperature in decikelvin" check. The Dk getter is the
// raw register value with no conversion applied.
void test_temperature_dk_returns_raw_register(void) {
    Wire.setRegister(ADDR, BQ27441_COMMAND_TEMP, 0xA5);
    Wire.setRegister(ADDR, BQ27441_COMMAND_TEMP + 1, 0x0B);
    TEST_ASSERT_EQUAL(2981, sensor->temperatureDk());
}

void test_capacity_remaining_reads_correct_register(void) {
    Wire.setRegister(ADDR, BQ27441_COMMAND_REM_CAPACITY, 0x78);
    Wire.setRegister(ADDR, BQ27441_COMMAND_REM_CAPACITY + 1, 0x56);
    TEST_ASSERT_EQUAL(0x5678, sensor->capacityRemaining());
}

void test_capacity_full_reads_correct_register(void) {
    Wire.setRegister(ADDR, BQ27441_COMMAND_FULL_CAPACITY, 0x34);
    Wire.setRegister(ADDR, BQ27441_COMMAND_FULL_CAPACITY + 1, 0x12);
    TEST_ASSERT_EQUAL(0x1234, sensor->capacityFull());
}

void test_data_ready_returns_false_when_initcomp_not_set(void) {
    Wire.setControlResponse(BQ27441_CONTROL_STATUS, 0x0000);
    TEST_ASSERT_FALSE(sensor->dataReady());
}

void test_data_ready_returns_true_when_initcomp_set(void) {
    TEST_ASSERT_TRUE(sensor->dataReady());
}

void test_sealed_returns_true_when_ss_bit_set(void) {
    Wire.setControlResponse(BQ27441_CONTROL_STATUS, 0x2000);
    TEST_ASSERT_TRUE(sensor->sealed());
}

void test_power_off_sends_shutdown_commands(void) {
    sensor->powerOff();
    uint8_t subcommand_low = Wire.getRegister(ADDR, BQ27441_COMMAND_CONTROL);
    uint8_t subcommand_high = Wire.getRegister(ADDR, BQ27441_COMMAND_CONTROL + 1);
    TEST_ASSERT_EQUAL(BQ27441_CONTROL_SHUTDOWN, (subcommand_high << 8) | subcommand_low);
}

void test_set_capacity_returns_success(void) {
    // setCapacity forwards writeExtendedData's bool through a uint16_t —
    // 1 on success, 0 on bridge error. setUp() already preloads
    // FLAGS.CFGUPMODE so enterConfig() advances past the polling loop.
    uint16_t capacity = 0x1234;
    uint16_t result = sensor->setCapacity(capacity);
    TEST_ASSERT_EQUAL(1, result);
}

void test_set_capacity_returns_zero_when_enter_config_fails(void) {
    // Clear FLAGS so the CFGUPMODE bit is unset — enterConfig() then
    // loops until timeout and writeExtendedData() must propagate the
    // failure as a zero return.
    Wire.setRegister(ADDR, BQ27441_COMMAND_FLAGS, 0x00);
    Wire.setRegister(ADDR, BQ27441_COMMAND_FLAGS + 1, 0x00);

    uint16_t result = sensor->setCapacity(0x1234);
    TEST_ASSERT_EQUAL(0, result);
}

void test_voltage_returns_max_value_when_registers_maxed(void) {
    Wire.setRegister(ADDR, BQ27441_COMMAND_VOLTAGE, 0xFF);
    Wire.setRegister(ADDR, BQ27441_COMMAND_VOLTAGE + 1, 0xFF);
    TEST_ASSERT_EQUAL(0xFFFF, sensor->voltageMv());
}

void test_device_id_returns_expected_value(void) {
    TEST_ASSERT_EQUAL(BQ27441_DEVICE_ID, sensor->deviceId());
}

void test_power_on_executes_successfully(void) {
    sensor->powerOn();
    TEST_ASSERT_TRUE(true);
}

void test_voltage_returns_zero_on_i2c_error(void) {
    // Inject a NACK on the address phase: every endTransmission() now
    // returns 2, which readReg() must propagate as a failure (and
    // readWord() / voltageMv() surface as 0).
    Wire.setEndTransmissionResult(2);
    Wire.setRegister(ADDR, BQ27441_COMMAND_VOLTAGE, 0xCD);
    Wire.setRegister(ADDR, BQ27441_COMMAND_VOLTAGE + 1, 0xAB);

    TEST_ASSERT_EQUAL(0, sensor->voltageMv());
}

void test_current_average_returns_negative_value_near_zero(void) {
    Wire.setRegister(ADDR, BQ27441_COMMAND_AVG_CURRENT, 0xFF);
    Wire.setRegister(ADDR, BQ27441_COMMAND_AVG_CURRENT + 1, 0xFF);
    TEST_ASSERT_EQUAL(-1, sensor->currentAverage());
}

void test_soc_returns_zero_when_empty(void) {
    Wire.setRegister(ADDR, BQ27441_COMMAND_SOC, 0x00);
    Wire.setRegister(ADDR, BQ27441_COMMAND_SOC + 1, 0x00);
    TEST_ASSERT_EQUAL(0, sensor->stateOfCharge());
}

void test_soc_returns_100_when_full(void) {
    Wire.setRegister(ADDR, BQ27441_COMMAND_SOC, 100);
    Wire.setRegister(ADDR, BQ27441_COMMAND_SOC + 1, 0x00);
    TEST_ASSERT_EQUAL(100, sensor->stateOfCharge());
}

// Regression: 856628f — begin() must pulse GPOUT (LOW then HIGH)
// BEFORE probing deviceId() when a gpout_pin is wired. The
// previous order (probe first) returned false whenever the gauge
// was already in shutdown, because shutdown silences the I2C
// interface. Verify the wake pulse landed on the configured pin.
void test_begin_pulses_gpout_low_high_when_pin_wired(void) {
    constexpr int kGpoutPin = 7;
    gpioPinState().clear();
    gpioPinMode().clear();
    delete sensor;
    sensor = new SteamiBQ27441(Wire, LIPO_BATTERY_CAPACITY, ADDR, kGpoutPin);

    TEST_ASSERT_TRUE(sensor->begin());

    // After begin(): disableShutdownMode() set OUTPUT then drove
    // LOW -> HIGH, and the post-probe configureGpoutInput() switched
    // the pin to INPUT_PULLUP. The pinMode map records the LAST mode,
    // so the final state must be INPUT_PULLUP; the line must be HIGH.
    TEST_ASSERT_EQUAL(INPUT_PULLUP, gpioPinMode()[kGpoutPin]);
    TEST_ASSERT_EQUAL(HIGH, gpioPinState()[kGpoutPin]);
}

// Regression: 856628f — begin() with no wake pin (-1) must NOT
// touch the pin map at all (no spurious pinMode / digitalWrite
// on whatever pin -1 maps to). Keeps the no-wake path strictly
// no-op so the wake reorder doesn't sneak in any side effects.
void test_begin_does_not_touch_pins_when_no_gpout(void) {
    gpioPinState().clear();
    gpioPinMode().clear();
    TEST_ASSERT_TRUE(sensor->begin());
    TEST_ASSERT_TRUE(gpioPinMode().empty());
    TEST_ASSERT_TRUE(gpioPinState().empty());
}

// Block-boundary guard of writeExtendedData() (#185). Every public setter
// stays inside one block, so the rule is locked through the pure helper
// the private method calls.
void test_extended_write_rejects_crossing_block_boundary(void) {
    TEST_ASSERT_FALSE(bq27441_detail::isExtendedDataWriteInRange(31, 2));
    // Same rule on a later page: offset 63 is the last byte of page 1.
    TEST_ASSERT_FALSE(bq27441_detail::isExtendedDataWriteInRange(63, 2));
    TEST_ASSERT_FALSE(bq27441_detail::isExtendedDataWriteInRange(42, 23));
}

void test_extended_write_rejects_length_over_one_block(void) {
    TEST_ASSERT_FALSE(bq27441_detail::isExtendedDataWriteInRange(0, 33));
    TEST_ASSERT_FALSE(bq27441_detail::isExtendedDataWriteInRange(32, 33));
    // No wrap-around on the uint16_t sum.
    TEST_ASSERT_FALSE(bq27441_detail::isExtendedDataWriteInRange(31, 0xFFFF));
}

void test_extended_write_accepts_writes_inside_one_block(void) {
    TEST_ASSERT_TRUE(bq27441_detail::isExtendedDataWriteInRange(31, 1));
    TEST_ASSERT_TRUE(bq27441_detail::isExtendedDataWriteInRange(0, 32));
    TEST_ASSERT_TRUE(bq27441_detail::isExtendedDataWriteInRange(32, 32));
    TEST_ASSERT_TRUE(bq27441_detail::isExtendedDataWriteInRange(42, 22));
}

void test_extended_write_accepts_every_setter_range(void) {
    // (offset, length) pairs used by setCapacity, setSoc1Thresholds,
    // setSocfThresholds, setSociDelta and writeOpConfig.
    TEST_ASSERT_TRUE(bq27441_detail::isExtendedDataWriteInRange(10, 2));
    TEST_ASSERT_TRUE(bq27441_detail::isExtendedDataWriteInRange(0, 2));
    TEST_ASSERT_TRUE(bq27441_detail::isExtendedDataWriteInRange(2, 2));
    TEST_ASSERT_TRUE(bq27441_detail::isExtendedDataWriteInRange(26, 1));
}

// --- Methods shared with the MicroPython driver (#316) ---------------------

// True when the control subcommand was written to Control() (LSB then MSB).
static bool controlSent(uint16_t subcommand) {
    const auto& writes = Wire.getWrites();
    for (size_t i = 0; i + 1 < writes.size(); ++i) {
        if (writes[i].address == ADDR && writes[i].reg == BQ27441_COMMAND_CONTROL &&
            writes[i].value == (subcommand & 0xFF) && writes[i + 1].reg == 1 &&
            writes[i + 1].value == (subcommand >> 8)) {
            return true;
        }
    }
    return false;
}

static void preloadWord(uint8_t reg, uint16_t value) {
    Wire.setRegister(ADDR, reg, value & 0xFF);
    Wire.setRegister(ADDR, reg + 1, (value >> 8) & 0xFF);
}

void test_current_reads_the_requested_register(void) {
    preloadWord(BQ27441_COMMAND_AVG_CURRENT, static_cast<uint16_t>(-120));
    preloadWord(BQ27441_COMMAND_STDBY_CURRENT, static_cast<uint16_t>(-10));
    preloadWord(BQ27441_COMMAND_MAX_CURRENT, static_cast<uint16_t>(-900));
    TEST_ASSERT_EQUAL(-120, sensor->current());
    TEST_ASSERT_EQUAL(-10, sensor->current(SteamiBQ27441::CurrentMeasureType::STBY));
    TEST_ASSERT_EQUAL(-900, sensor->current(SteamiBQ27441::CurrentMeasureType::MAX));
}

void test_capacity_reads_the_requested_register(void) {
    preloadWord(BQ27441_COMMAND_REM_CAPACITY, 812);
    preloadWord(BQ27441_COMMAND_FULL_CAP_FIL, 1500);
    TEST_ASSERT_EQUAL(812, sensor->capacity());
    TEST_ASSERT_EQUAL(1500, sensor->capacity(SteamiBQ27441::CapacityMeasureType::FULL_F));
}

void test_soc_unfiltered_reads_its_own_register(void) {
    preloadWord(BQ27441_COMMAND_SOC, 70);
    preloadWord(BQ27441_COMMAND_SOC_UNFL, 73);
    TEST_ASSERT_EQUAL(70, sensor->soc());
    TEST_ASSERT_EQUAL(73, sensor->soc(SteamiBQ27441::SocMeasureType::UNFILTERED));
}

void test_soh_status_returns_the_high_byte(void) {
    preloadWord(BQ27441_COMMAND_SOH, 0x0362);
    TEST_ASSERT_EQUAL(0x62, sensor->soh());
    TEST_ASSERT_EQUAL(0x03, sensor->soh(SteamiBQ27441::SohMeasureType::SOH_STAT));
}

void test_flags_and_soc_alert_bits(void) {
    preloadWord(BQ27441_COMMAND_FLAGS, BQ27441_FLAG_CFGUPMODE | BQ27441_FLAG_SOC1);
    TEST_ASSERT_EQUAL_HEX16(BQ27441_FLAG_CFGUPMODE | BQ27441_FLAG_SOC1, sensor->flags());
    TEST_ASSERT_TRUE(sensor->socFlag());
    TEST_ASSERT_FALSE(sensor->socfFlag());

    preloadWord(BQ27441_COMMAND_FLAGS, BQ27441_FLAG_CFGUPMODE | BQ27441_FLAG_SOCF);
    TEST_ASSERT_FALSE(sensor->socFlag());
    TEST_ASSERT_TRUE(sensor->socfFlag());
}

void test_soc_thresholds_read_the_discharge_block(void) {
    Wire.setRegister(ADDR, BQ27441_EXTENDED_BLOCKDATA + 0, 10);
    Wire.setRegister(ADDR, BQ27441_EXTENDED_BLOCKDATA + 1, 15);
    Wire.setRegister(ADDR, BQ27441_EXTENDED_BLOCKDATA + 2, 2);
    Wire.setRegister(ADDR, BQ27441_EXTENDED_BLOCKDATA + 3, 5);
    TEST_ASSERT_EQUAL(10, sensor->soc1SetThreshold());
    TEST_ASSERT_EQUAL(15, sensor->soc1ClearThreshold());
    TEST_ASSERT_EQUAL(2, sensor->socfSetThreshold());
    TEST_ASSERT_EQUAL(5, sensor->socfClearThreshold());
    TEST_ASSERT_EQUAL(BQ27441_ID_DISCHARGE, Wire.getRegister(ADDR, BQ27441_EXTENDED_DATACLASS));
}

void test_set_soc1_thresholds_writes_clamped_percentages(void) {
    // 300 must clamp to 100, not wrap to 44 through a uint8_t cast.
    TEST_ASSERT_TRUE(sensor->setSoc1Thresholds(300, 20));
    TEST_ASSERT_EQUAL(BQ27441_ID_DISCHARGE, Wire.getRegister(ADDR, BQ27441_EXTENDED_DATACLASS));
    TEST_ASSERT_EQUAL(100, Wire.getRegister(ADDR, BQ27441_EXTENDED_BLOCKDATA + 0));
    TEST_ASSERT_EQUAL(20, Wire.getRegister(ADDR, BQ27441_EXTENDED_BLOCKDATA + 1));
}

void test_set_socf_thresholds_writes_offsets_2_and_3(void) {
    TEST_ASSERT_TRUE(sensor->setSocfThresholds(3, 6));
    TEST_ASSERT_EQUAL(3, Wire.getRegister(ADDR, BQ27441_EXTENDED_BLOCKDATA + 2));
    TEST_ASSERT_EQUAL(6, Wire.getRegister(ADDR, BQ27441_EXTENDED_BLOCKDATA + 3));
}

void test_set_soc_thresholds_return_false_when_config_fails(void) {
    preloadWord(BQ27441_COMMAND_FLAGS, 0x0000);  // CFGUPMODE never rises
    TEST_ASSERT_FALSE(sensor->setSoc1Thresholds(10, 15));
    TEST_ASSERT_FALSE(sensor->setSocfThresholds(2, 5));
}

void test_set_gpout_function_sets_batlowen_in_op_config(void) {
    preloadWord(BQ27441_EXTENDED_OPCONFIG, 0x0000);
    TEST_ASSERT_TRUE(sensor->setGpoutFunction(true));
    TEST_ASSERT_EQUAL(BQ27441_ID_REGISTERS, Wire.getRegister(ADDR, BQ27441_EXTENDED_DATACLASS));
    // OpConfig is stored MSB first in the data block.
    TEST_ASSERT_EQUAL_HEX8(0x00, Wire.getRegister(ADDR, BQ27441_EXTENDED_BLOCKDATA + 0));
    TEST_ASSERT_EQUAL_HEX8(BQ27441_OPCONFIG_BATLOWEN,
                           Wire.getRegister(ADDR, BQ27441_EXTENDED_BLOCKDATA + 1));
}

void test_set_gpout_function_skips_the_write_when_unchanged(void) {
    preloadWord(BQ27441_EXTENDED_OPCONFIG, BQ27441_OPCONFIG_BATLOWEN);
    Wire.clearWrites();
    TEST_ASSERT_TRUE(sensor->setGpoutFunction(true));
    TEST_ASSERT_FALSE(controlSent(BQ27441_CONTROL_SET_CFGUPDATE));
}

void test_seal_and_unseal_send_their_subcommands(void) {
    Wire.clearWrites();
    TEST_ASSERT_TRUE(sensor->seal());
    TEST_ASSERT_TRUE(controlSent(BQ27441_CONTROL_SEALED));
    Wire.clearWrites();
    TEST_ASSERT_TRUE(sensor->unseal());
    TEST_ASSERT_TRUE(controlSent(BQ27441_UNSEAL_KEY));
}

void test_reset_and_soft_reset_send_their_subcommands(void) {
    Wire.clearWrites();
    TEST_ASSERT_TRUE(sensor->reset());
    TEST_ASSERT_TRUE(controlSent(BQ27441_CONTROL_RESET));
    Wire.clearWrites();
    TEST_ASSERT_TRUE(sensor->softReset());
    TEST_ASSERT_TRUE(controlSent(BQ27441_CONTROL_SOFT_RESET));
}

void test_control_commands_return_false_on_i2c_error(void) {
    Wire.setEndTransmissionResult(2);
    TEST_ASSERT_FALSE(sensor->seal());
    TEST_ASSERT_FALSE(sensor->unseal());
    TEST_ASSERT_FALSE(sensor->reset());
    TEST_ASSERT_FALSE(sensor->softReset());
}

void test_user_config_session_batches_writes_then_hands_back_control(void) {
    // Inside a user session, the setters do not enter or leave the
    // configuration mode themselves.
    TEST_ASSERT_TRUE(sensor->enterConfig(true));
    Wire.clearWrites();
    TEST_ASSERT_TRUE(sensor->setSoc1Thresholds(10, 15));
    TEST_ASSERT_FALSE(controlSent(BQ27441_CONTROL_SET_CFGUPDATE));
    TEST_ASSERT_TRUE(sensor->exitConfig(false));
    TEST_ASSERT_TRUE(controlSent(BQ27441_CONTROL_EXIT_CFGUPDATE));

    // After exitConfig(), a setter opens its own session again.
    Wire.clearWrites();
    TEST_ASSERT_TRUE(sensor->setSocfThresholds(2, 5));
    TEST_ASSERT_TRUE(controlSent(BQ27441_CONTROL_SET_CFGUPDATE));
}

void test_failed_enter_config_does_not_open_a_user_session(void) {
    // CFGUPMODE never rises: enterConfig(true) fails and must not leave
    // a user session behind, otherwise the next setter would write the
    // data block without entering the configuration mode.
    preloadWord(BQ27441_COMMAND_FLAGS, 0x0000);
    TEST_ASSERT_FALSE(sensor->enterConfig(true));

    preloadWord(BQ27441_COMMAND_FLAGS, BQ27441_FLAG_CFGUPMODE);
    Wire.clearWrites();
    TEST_ASSERT_TRUE(sensor->setSoc1Thresholds(10, 15));
    TEST_ASSERT_TRUE(controlSent(BQ27441_CONTROL_SET_CFGUPDATE));
    TEST_ASSERT_TRUE(controlSent(BQ27441_CONTROL_SOFT_RESET));
}

void test_exit_config_ends_the_user_session_even_after_resim(void) {
    TEST_ASSERT_TRUE(sensor->enterConfig(true));
    sensor->exitConfig();  // SOFT_RESET path

    Wire.clearWrites();
    TEST_ASSERT_TRUE(sensor->setSociDelta(5));
    TEST_ASSERT_TRUE(controlSent(BQ27441_CONTROL_SET_CFGUPDATE));
}

void test_percentage_setters_clamp_negative_values_to_zero(void) {
    // Same as constrain(x, 0, 100) in MicroPython: -5 % is 0 %, not a
    // uint16_t wrap to 65531 then clamped to 100 %.
    TEST_ASSERT_TRUE(sensor->setSoc1Thresholds(-1, -20));
    TEST_ASSERT_EQUAL(0, Wire.getRegister(ADDR, BQ27441_EXTENDED_BLOCKDATA + 0));
    TEST_ASSERT_EQUAL(0, Wire.getRegister(ADDR, BQ27441_EXTENDED_BLOCKDATA + 1));

    TEST_ASSERT_TRUE(sensor->setSocfThresholds(-3, 150));
    TEST_ASSERT_EQUAL(0, Wire.getRegister(ADDR, BQ27441_EXTENDED_BLOCKDATA + 2));
    TEST_ASSERT_EQUAL(100, Wire.getRegister(ADDR, BQ27441_EXTENDED_BLOCKDATA + 3));

    TEST_ASSERT_TRUE(sensor->setSociDelta(-5));
    TEST_ASSERT_EQUAL(0, Wire.getRegister(ADDR, BQ27441_EXTENDED_BLOCKDATA + 26));
}

void test_soci_delta_and_pulse_gpout(void) {
    TEST_ASSERT_TRUE(sensor->setSociDelta(250));
    TEST_ASSERT_EQUAL(BQ27441_ID_STATE, Wire.getRegister(ADDR, BQ27441_EXTENDED_DATACLASS));
    TEST_ASSERT_EQUAL(100, Wire.getRegister(ADDR, BQ27441_EXTENDED_BLOCKDATA + 26));
    TEST_ASSERT_EQUAL(100, sensor->sociDelta());
    Wire.clearWrites();
    TEST_ASSERT_TRUE(sensor->pulseGpout());
    TEST_ASSERT_TRUE(controlSent(BQ27441_CONTROL_PULSE_SOC_INT));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_begin_returns_true_when_device_id_matches);
    RUN_TEST(test_begin_returns_false_when_device_id_wrong);
    RUN_TEST(test_voltage_returns_raw_register_value);
    RUN_TEST(test_current_average_returns_signed_value);
    RUN_TEST(test_state_of_charge_returns_filtered_soc);
    RUN_TEST(test_state_of_health_returns_low_byte);
    RUN_TEST(test_temperature_converts_decikelvin_to_celsius);
    RUN_TEST(test_temperature_internal_reads_correct_register);
    RUN_TEST(test_power_reads_avg_power_register);
    RUN_TEST(test_temperature_k_converts_decikelvin_to_kelvin);
    RUN_TEST(test_temperature_dk_returns_raw_register);
    RUN_TEST(test_capacity_remaining_reads_correct_register);
    RUN_TEST(test_capacity_full_reads_correct_register);
    RUN_TEST(test_data_ready_returns_false_when_initcomp_not_set);
    RUN_TEST(test_data_ready_returns_true_when_initcomp_set);
    RUN_TEST(test_sealed_returns_true_when_ss_bit_set);
    RUN_TEST(test_power_off_sends_shutdown_commands);
    RUN_TEST(test_set_capacity_returns_success);
    RUN_TEST(test_set_capacity_returns_zero_when_enter_config_fails);
    RUN_TEST(test_voltage_returns_max_value_when_registers_maxed);
    RUN_TEST(test_device_id_returns_expected_value);
    RUN_TEST(test_power_on_executes_successfully);
    RUN_TEST(test_voltage_returns_zero_on_i2c_error);
    RUN_TEST(test_current_average_returns_negative_value_near_zero);
    RUN_TEST(test_soc_returns_zero_when_empty);
    RUN_TEST(test_soc_returns_100_when_full);
    RUN_TEST(test_begin_pulses_gpout_low_high_when_pin_wired);
    RUN_TEST(test_begin_does_not_touch_pins_when_no_gpout);
    RUN_TEST(test_extended_write_rejects_crossing_block_boundary);
    RUN_TEST(test_extended_write_rejects_length_over_one_block);
    RUN_TEST(test_extended_write_accepts_writes_inside_one_block);
    RUN_TEST(test_extended_write_accepts_every_setter_range);
    RUN_TEST(test_current_reads_the_requested_register);
    RUN_TEST(test_capacity_reads_the_requested_register);
    RUN_TEST(test_soc_unfiltered_reads_its_own_register);
    RUN_TEST(test_soh_status_returns_the_high_byte);
    RUN_TEST(test_flags_and_soc_alert_bits);
    RUN_TEST(test_soc_thresholds_read_the_discharge_block);
    RUN_TEST(test_set_soc1_thresholds_writes_clamped_percentages);
    RUN_TEST(test_set_socf_thresholds_writes_offsets_2_and_3);
    RUN_TEST(test_set_soc_thresholds_return_false_when_config_fails);
    RUN_TEST(test_set_gpout_function_sets_batlowen_in_op_config);
    RUN_TEST(test_set_gpout_function_skips_the_write_when_unchanged);
    RUN_TEST(test_seal_and_unseal_send_their_subcommands);
    RUN_TEST(test_reset_and_soft_reset_send_their_subcommands);
    RUN_TEST(test_control_commands_return_false_on_i2c_error);
    RUN_TEST(test_user_config_session_batches_writes_then_hands_back_control);
    RUN_TEST(test_soci_delta_and_pulse_gpout);
    RUN_TEST(test_failed_enter_config_does_not_open_a_user_session);
    RUN_TEST(test_exit_config_ends_the_user_session_even_after_resim);
    RUN_TEST(test_percentage_setters_clamp_negative_values_to_zero);
    return UNITY_END();
}