// SPDX-License-Identifier: GPL-3.0-or-later

#include "bq27441.h"

#include <math.h>

bq27441::bq27441(TwoWire& wire, uint16_t capacity_mAh, uint8_t address, int gpout_pin)
    : _wire(wire), _capacity_mAh(capacity_mAh), _address(address), _gpout_pin(gpout_pin) {}

bool bq27441::begin() {
    if (!deviceId()) {
        return false;
    }

    configure_gpout_input();
    power_on();
    return true;
}

void bq27441::configure_gpout_input() {
    if (_gpout_pin != -1) {
        pinMode(_gpout_pin, INPUT_PULLUP);
    }
}

void bq27441::configure_gpout_output() {
    if (_gpout_pin != -1) {
        pinMode(_gpout_pin, OUTPUT);
    }
}

void bq27441::power_on() {
    // Wake up fuel gauge ic if in shutdown mode
    disable_shutdown_mode();
    delay(10);
    set_capacity(_capacity_mAh);
}

void bq27441::power_off() {
    // Put fuel gauge ic in shutdown mode by sending shutdown i2c cmd
    enter_shutdown_mode();
}

void bq27441::enable_shutdown_mode() {
    execute_control_word(BQ27441_CONTROL_SHUTDOWN_ENABLE);
    _shutdown_en = true;
}

void bq27441::enter_shutdown_mode() {
    configure_gpout_input();
    enable_shutdown_mode();
    execute_control_word(BQ27441_CONTROL_SHUTDOWN);
}

void bq27441::disable_shutdown_mode() {
    if (_gpout_pin != -1) {
        configure_gpout_output();
        digitalWrite(_gpout_pin, LOW);
        delay(10);
        digitalWrite(_gpout_pin, HIGH);
        delay(10);
        _shutdown_en = false;
    }
}

bool bq27441::is_valid_device() {
    // Checks if device id returned matches bq27441
    return deviceId() == BQ27441_DEVICE_ID;
}

uint16_t bq27441::set_capacity(uint16_t capacity) {
    // Write to STATE subclass(82) of BQ27441 extended memory.
    // Offset 0x0A(10)Design capacity is a 2 - byte piece of data - MSB first
    uint8_t cap_msb = capacity >> 8;
    uint8_t cap_lsb = capacity & 0x00FF;
    uint8_t capacity_data[2] = {cap_msb, cap_lsb};

    return write_extended_data(BQ27441_ID_STATE, 10, capacity_data, 2);
}

int16_t bq27441::current_average() {
    // Return average current
    int16_t result = current(CurrentMeasureType::AVG);
    return result;
}

uint16_t bq27441::capacity_full() {
    // Return full capacity (mAh)
    return capacity(CapacityMeasureType::FULL);
}

uint16_t bq27441::capacity_remaining() {
    return capacity(CapacityMeasureType::REMAIN);
}

uint8_t bq27441::state_of_charge() {
    // Return remaining charge %
    return soc(SocMeasureType::FILTERED);
}

uint8_t bq27441::state_of_health() {
    // Return state of health %
    return soh(SohMeasureType::PERCENT);
}

uint16_t bq27441::voltage_mv() {
    // Return current voltage
    return read_word(BQ27441_COMMAND_VOLTAGE);
}

int16_t bq27441::current(CurrentMeasureType current_measure_type) {
    int16_t result = 0;
    if (current_measure_type == CurrentMeasureType::AVG) {
        result = read_word(BQ27441_COMMAND_AVG_CURRENT);
    } else if (current_measure_type == CurrentMeasureType::STBY) {
        result = read_word(BQ27441_COMMAND_STDBY_CURRENT);
    } else if (current_measure_type == CurrentMeasureType::MAX) {
        result = read_word(BQ27441_COMMAND_MAX_CURRENT);
    }

    return result;
}

int16_t bq27441::capacity(CapacityMeasureType capacity_measure_type) {
    int16_t result = 0;
    if (capacity_measure_type == CapacityMeasureType::REMAIN) {
        return read_word(BQ27441_COMMAND_REM_CAPACITY);
    } else if (capacity_measure_type == CapacityMeasureType::FULL) {
        return read_word(BQ27441_COMMAND_FULL_CAPACITY);
    } else if (capacity_measure_type == CapacityMeasureType::AVAIL) {
        result = read_word(BQ27441_COMMAND_NOM_CAPACITY);
    } else if (capacity_measure_type == CapacityMeasureType::AVAIL_FULL) {
        result = read_word(BQ27441_COMMAND_AVAIL_CAPACITY);
    } else if (capacity_measure_type == CapacityMeasureType::REMAIN_F) {
        result = read_word(BQ27441_COMMAND_REM_CAP_FIL);
    } else if (capacity_measure_type == CapacityMeasureType::REMAIN_UF) {
        result = read_word(BQ27441_COMMAND_REM_CAP_UNFL);
    } else if (capacity_measure_type == CapacityMeasureType::FULL_F) {
        result = read_word(BQ27441_COMMAND_FULL_CAP_FIL);
    } else if (capacity_measure_type == CapacityMeasureType::FULL_UF) {
        result = read_word(BQ27441_COMMAND_FULL_CAP_UNFL);
    } else if (capacity_measure_type == CapacityMeasureType::DESIGN) {
        result = read_word(BQ27441_EXTENDED_CAPACITY);
    }

    return result;
}

int16_t bq27441::power() {
    return read_word(BQ27441_COMMAND_AVG_POWER);
}

uint16_t bq27441::soc(SocMeasureType soc_measure_type) {
    uint16_t soc_ret = 0;
    if (soc_measure_type == SocMeasureType::FILTERED) {
        soc_ret = read_word(BQ27441_COMMAND_SOC);
    } else if (soc_measure_type == SocMeasureType::UNFILTERED) {
        soc_ret = read_word(BQ27441_COMMAND_SOC_UNFL);
    }

    return soc_ret;
}

uint16_t bq27441::soh(SohMeasureType soh_measure_type) {
    uint16_t soh_raw = read_word(BQ27441_COMMAND_SOH);
    uint16_t soh_status = soh_raw >> 8;
    uint16_t soh_percent = soh_raw & 0x00FF;

    if (soh_measure_type == SohMeasureType::PERCENT) {
        return soh_percent;
    } else {
        return soh_status;
    }
}

uint16_t bq27441::read_temperature_dk(TempMeasureType temp_measure_type) {
    if (temp_measure_type == TempMeasureType::BATTERY) {
        return read_word(BQ27441_COMMAND_TEMP);
    } else if (temp_measure_type == TempMeasureType::INTERNAL_TEMP) {
        return read_word(BQ27441_COMMAND_INT_TEMP);
    } else {
        return 0;
    }
}

float bq27441::temperature(TempMeasureType temp_measure_type) {
    return read_temperature_dk(temp_measure_type) / 10.0 - 273.15;
}

float bq27441::temperature_k(TempMeasureType temp_measure_type) {
    return read_temperature_dk(temp_measure_type) / 10.0;
}

uint16_t bq27441::temperature_dk(TempMeasureType temp_measure_type) {
    return read_temperature_dk(temp_measure_type);
}

uint16_t bq27441::gpout_polarity() {
    uint16_t op_config_register = op_config();
    return op_config_register & BQ27441_OPCONFIG_GPIOPOL;
}

bool bq27441::set_gpout_polarity(bool active_high) {
    uint16_t old_op_config = op_config();

    if ((active_high and (old_op_config & BQ27441_OPCONFIG_GPIOPOL)) ||
        (not active_high and not(old_op_config & BQ27441_OPCONFIG_GPIOPOL)))
        return true;
    uint16_t new_op_config = old_op_config;
    if (active_high) {
        new_op_config |= BQ27441_OPCONFIG_GPIOPOL;
    } else {
        new_op_config &= ~(BQ27441_OPCONFIG_GPIOPOL);
    }

    return write_op_config(new_op_config);
}

uint16_t bq27441::gpout_function() {
    uint16_t op_config_register = op_config();
    return op_config_register & BQ27441_OPCONFIG_BATLOWEN;
}

bool bq27441::set_gpout_function(bool gpout_function) {
    uint16_t old_op_config = op_config();
    if ((gpout_function and (old_op_config & BQ27441_OPCONFIG_BATLOWEN)) ||
        (not gpout_function and not(old_op_config & BQ27441_OPCONFIG_BATLOWEN))) {
        return true;
    }
    uint16_t new_op_config = old_op_config;
    if (gpout_function) {
        new_op_config |= BQ27441_OPCONFIG_BATLOWEN;
    } else {
        new_op_config &= ~(BQ27441_OPCONFIG_BATLOWEN);
    }
    return write_op_config(new_op_config);
}

uint16_t bq27441::soc1_set_threshold() {
    return read_extended_data(BQ27441_ID_DISCHARGE, 0);
}

uint16_t bq27441::set_soc1_thresholds(uint16_t set_soc, uint16_t clear_soc) {
    uint8_t thresholds[2] = {0, 0};
    thresholds[0] = constrain(set_soc, 0, 100);
    thresholds[1] = constrain(clear_soc, 0, 100);
    return write_extended_data(BQ27441_ID_DISCHARGE, 0, thresholds, 2);
}

uint16_t bq27441::socf_set_threshold() {
    return read_extended_data(BQ27441_ID_DISCHARGE, 2);
}

uint16_t bq27441::socf_clear_threshold() {
    return read_extended_data(BQ27441_ID_DISCHARGE, 3);
}

uint16_t bq27441::set_socf_thresholds(uint16_t set_socf, uint16_t clear_socf) {
    uint8_t thresholds[2] = {0, 0};
    thresholds[0] = constrain(set_socf, 0, 100);
    thresholds[1] = constrain(clear_socf, 0, 100);
    return write_extended_data(BQ27441_ID_DISCHARGE, 2, thresholds, 2);
}

uint16_t bq27441::soc_flag() {
    uint8_t flag_state = flags();
    return flag_state & BQ27441_FLAG_SOCF;
}

uint16_t soci_delta() {
    return read_extended_data(BQ27441_ID_STATE, 26);
}

uint16_t bq27441::set_soci_delta(uint16_t delta) {
    uint16_t soci = constrain(delta, 0, 100);
    return write_extended_data(BQ27441_ID_STATE, 26, soci, 1);
}

uint16_t bq27441::pulse_gpout() {
    return execute_control_word(BQ27441_CONTROL_PULSE_SOC_INT);
}

uint16_t bq27441::device_id() {
    return read_control_word(BQ27441_CONTROL_DEVICE_TYPE);
}

uint8_t bq27441::get_time_ms() {
    return ticks_ms();
}

bool bq27441::enter_config(bool user_control) {
    if (user_control) {
        _user_config_control = true;
    }

    if (sealed()) {
        _seal_flag = true;
        unseal();
    }

    if (execute_control_word(BQ27441_CONTROL_SET_CFGUPDATE)) {
        uint8_t start_ms = get_time_ms();
        bool timeout = false;
        while (not(flags() & BQ27441_FLAG_CFGUPMODE)) {
            delay(1);
            uint8_t elapsed_ms = get_time_ms() - start_ms;
            if (elapsed_ms > BQ27441_I2C_TIMEOUT) {
                timeout = true;
                break;
            }
        }
        if (not timeout) {
            return true;
        }
    }
    return false;
}

bool bq27441::exit_config(bool resim) {
    if (resim) {
        if (soft_reset()) {
            uint16_t start_ms = get_time_ms();
            uint8_t timeout = false;

            while (not(flags() & BQ27441_FLAG_CFGUPMODE)) {
                delay(1);
                uint16_t elapsed_ms = get_time_ms() - start_ms;
                if (elapsed_ms > BQ27441_I2C_TIMEOUT) {
                    timeout = true;
                    break;
                }
            }

            if (not timeout) {
                if (_seal_flag) {
                    seal();
                }
                return true;
            }
        }
    }
}