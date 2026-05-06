// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <Arduino.h>
#include <Wire.h>

#include "bq27441_const.h"

class bq27441 {
   public:
    // Parameters for the current() function, to specify which current to read
    // current_measure types
    enum class CurrentMeasureType {
        AVG = 0,   // Average Current (DEFAULT)
        STBY = 1,  // Standby Current
        MAX = 2    // Max Current
    };

    // Parameters for the capacity() function, to specify which capacity to read
    enum class CapacityMeasureType {
        REMAIN = 0,      // Remaining Capacity (DEFAULT)
        FULL = 1,        // Full Capacity
        AVAIL = 2,       // Available Capacity
        AVAIL_FULL = 3,  // Full Available Capacity
        REMAIN_F = 4,    // Remaining Capacity Filtered
        REMAIN_UF = 5,   // Remaining Capacity Unfiltered
        FULL_F = 6,      // Full Capacity Filtered
        FULL_UF = 7,     // Full Capacity Unfiltered
        DESIGN = 8       // Design Capacity
    };

    // Parameters for the soc() function
    enum class SocMeasureType {
        FILTERED = 0,   // State of Charge Filtered (DEFAULT)
        UNFILTERED = 1  // State of Charge Unfiltered
    };

    // Parameters for the soh() function
    enum class SohMeasureType {
        PERCENT = 0,  // State of Health Percentage (DEFAULT)
        SOH_STAT = 1  // State of Health Status Bits
    };

    // Parameters for the temperature() function
    enum class TempMeasureType {
        BATTERY = 0,       // Battery Temperature (DEFAULT)
        INTERNAL_TEMP = 1  // Internal IC Temperature
    };

    // Parameters for the set_gpout_function() function
    enum class GpoutFunctionType {
        SOC_INT = 0,  // Set GPOUT to SOC_INT functionality
        BAT_LOW = 1   // Set GPOUT to BAT_LOW functionality
    };

    bq27441(TwoWire& wire, uint16_t capacity_mAh = LIPO_BATTERY_CAPACITY,
            uint8_t address = BQ27441_I2C_ADDRESS, int gpout_pin = -1);

    bool begin();
    void configure_gpout_input();
    void configure_gpout_output();
    void power_on();
    void power_off();
    void enable_shutdown_mode();
    void enter_shutdown_mode();
    void disable_shutdown_mode();
    bool is_valid_device();
    uint16_t set_capacity(uint16_t capacity);
    int16_t current_average();
    uint16_t capacity_full();
    uint16_t capacity_remaining();
    uint8_t state_of_charge();
    uint8_t state_of_health();
    uint16_t voltage_mv();
    int16_t current(CurrentMeasureType current_measure_type);
    int16_t capacity(CapacityMeasureType capacity_measure_type);
    int16_t power();
    uint16_t soc(SocMeasureType soc_measure_type = SocMeasureType::FILTERED);
    uint16_t soh(SohMeasureType soh_measure_type = SohMeasureType::PERCENT);
    uint16_t read_temperature_dk(TempMeasureType temp_measure_type = TempMeasureType::BATTERY);
    float temperature(TempMeasureType temp_measure_type = TempMeasureType::BATTERY);
    float temperature_k(TempMeasureType temp_measure_type = TempMeasureType::BATTERY);
    uint16_t temperature_dk(TempMeasureType temp_measure_type = TempMeasureType::BATTERY);
    uint16_t gpout_polarity();
    bool set_gpout_polarity(bool active_high);
    uint16_t gpout_function();
    bool set_gpout_function(bool gpout_function);
    uint16_t soc1_set_threshold();
    uint16_t set_soc1_thresholds(uint16_t set_soc, uint16_t clear_soc);
    uint16_t socf_set_threshold();
    uint16_t socf_clear_threshold();
    uint16_t set_socf_thresholds(uint16_t set_socf, uint16_t clear_socf);
    uint16_t soc_flag();
    uint16_t soci_delta();
    uint16_t set_soci_delta(uint16_t delta);
    uint16_t pulse_gpout();
    uint16_t device_id();
    uint8_t get_time_ms();
    bool enter_config(bool user_control);
    bool exit_config(bool resim = true);

   private:
    TwoWire& _wire;
    uint16_t _capacity_mAh;
    uint8_t _address;
    int _gpout_pin;
    bool _shutdown_en = false;
    bool _user_config_control = false;
    bool _seal_flag = false;
};