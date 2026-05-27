// SPDX-License-Identifier: GPL-3.0-or-later

#include <math.h>
#include <unity.h>

#include "BQ27441.h"
#include "Wire.h"

constexpr uint8_t ADDR = BQ27441_I2C_ADDRESS;

static void preloadWhoAmI(bool valid = true) {
    uint16_t value = valid ? BQ27441_DEVICE_ID : 0x1234;
    Wire.setRegister(ADDR, 0x00, value & 0xFF);
    Wire.setRegister(ADDR, 0x01, (value >> 8) & 0xFF);
}

static void preloadMeasurement(uint16_t voltage, int16_t current_avg, uint8_t soc) {
    Wire.setRegister(ADDR, BQ27441_COMMAND_VOLTAGE, voltage & 0xFF);
    Wire.setRegister(ADDR, BQ27441_COMMAND_VOLTAGE + 1, (voltage >> 8) & 0xFF);
    Wire.setRegister(ADDR, BQ27441_COMMAND_AVG_CURRENT, current_avg & 0xFF);
    Wire.setRegister(ADDR, BQ27441_COMMAND_AVG_CURRENT + 1, (current_avg >> 8) & 0xFF);
    Wire.setRegister(ADDR, BQ27441_COMMAND_SOC, soc & 0xFF);
    Wire.setRegister(ADDR, BQ27441_COMMAND_SOC + 1, 0x00);
}

BQ27441* sensor = nullptr;
void setUp(void) {
    Wire = TwoWire();
    millisClock() = 0xFFFFFFFF - 1;
    Wire.setControlResponse(BQ27441_CONTROL_DEVICE_TYPE, BQ27441_DEVICE_ID);
    Wire.setControlResponse(BQ27441_CONTROL_STATUS, 0x0080);
    // Preload the FLAGS register with CFGUPMODE set so begin() →
    // powerOn() → setCapacity() → writeExtendedData() → enterConfig()
    // observes the chip already in config-update mode. Without it the
    // enterConfig loop spins until timeout and begin() now returns
    // false (the fix that brought this dependency in).
    Wire.setRegister(ADDR, BQ27441_COMMAND_FLAGS, BQ27441_FLAG_CFGUPMODE & 0xFF);
    Wire.setRegister(ADDR, BQ27441_COMMAND_FLAGS + 1, (BQ27441_FLAG_CFGUPMODE >> 8) & 0xFF);
    sensor = new BQ27441(Wire);
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
    float tempC = sensor->temperature(BQ27441::TempMeasureType::INTERNAL_TEMP);
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
    sensor = new BQ27441(Wire, LIPO_BATTERY_CAPACITY, ADDR, kGpoutPin);

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
    return UNITY_END();
}