// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <Arduino.h>
#include <Wire.h>

#include "BQ27441_const.h"

namespace bq27441_detail {
template <typename T>
inline T clamp(T x, T a, T b) {
    if (x < a) {
        return a;
    }
    if (x > b) {
        return b;
    }
    return x;
}
}  // namespace bq27441_detail

class BQ27441 {
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

    BQ27441(TwoWire& wire = Wire, uint16_t capacity_mAh = LIPO_BATTERY_CAPACITY,
            uint8_t address = BQ27441_I2C_ADDRESS, int gpout_pin = -1);

    bool begin();
    void powerOn();
    void powerOff();

    int16_t currentAverage();
    uint8_t stateOfCharge();
    uint8_t stateOfHealth();
    uint16_t voltageMv();
    float temperature(TempMeasureType temp_measure_type = TempMeasureType::BATTERY);
    uint16_t capacityFull();
    uint16_t capacityRemaining();
    int16_t power();
    float temperatureK(TempMeasureType temp_measure_type = TempMeasureType::BATTERY);
    uint16_t temperatureDk(TempMeasureType temp_measure_type = TempMeasureType::BATTERY);

    uint16_t setCapacity(uint16_t capacity);
    bool setGpoutPolarity(bool active_high);

    uint16_t gpoutPolarity();
    uint16_t gpoutFunction();

    uint16_t deviceId();
    bool dataReady();

    bool sealed();

   private:
    TwoWire& _wire;
    uint16_t _capacity_mAh;
    uint8_t _address;
    int _gpout_pin;
    bool _shutdown_en = false;
    bool _user_config_control = false;
    bool _seal_flag = false;

    void configureGpoutInput();
    void configureGpoutOutput();
    void enableShutdownMode();
    void enterShutdownMode();
    void disableShutdownMode();
    bool isValidDevice();
    int16_t current(CurrentMeasureType current_measure_type);
    int16_t capacity(CapacityMeasureType capacity_measure_type);
    uint16_t soc(SocMeasureType soc_measure_type = SocMeasureType::FILTERED);
    uint16_t soh(SohMeasureType soh_measure_type = SohMeasureType::PERCENT);
    uint16_t readTemperatureDk(TempMeasureType temp_measure_type = TempMeasureType::BATTERY);
    bool setGpoutFunction(bool gpout_function);
    uint16_t soc1SetThreshold();
    uint16_t setSoc1Thresholds(uint16_t set_soc, uint16_t clear_soc);
    uint16_t socfSetThreshold();
    uint16_t socfClearThreshold();
    uint16_t setSocfThresholds(uint16_t set_socf, uint16_t clear_socf);
    uint16_t socFlag();
    uint16_t sociDelta();
    uint16_t setSociDelta(uint16_t delta);
    uint16_t pulseGpout();
    uint32_t TimeMs();
    bool enterConfig(bool user_control);
    bool exitConfig(bool resim = true);
    uint16_t flags();
    uint16_t controlStatus();
    bool seal();
    bool unseal();
    uint16_t opConfig();
    uint16_t writeOpConfig(uint16_t value);
    uint16_t reset();
    uint16_t softReset();
    int16_t readWord(uint8_t sub_address);
    uint16_t readControlWord(uint16_t function);
    uint16_t executeControlWord(uint16_t function);
    uint16_t blockDataControl();
    uint16_t blockDataClass(uint16_t id);
    bool blockDataOffset(uint16_t offset);
    bool blockDataChecksum(uint8_t& out);
    uint16_t readBlockData(uint16_t offset);
    uint16_t writeBlockData(uint16_t offset, uint16_t data);
    uint16_t writeBlockChecksum(uint8_t csum);
    bool computeBlockChecksum(uint8_t& out);
    uint16_t readExtendedData(uint16_t class_id, uint16_t offset);
    uint16_t writeExtendedData(uint16_t class_id, uint16_t offset, const uint8_t* data,
                               uint16_t length);
    bool readReg(uint8_t sub_address, uint8_t* buf, uint16_t count);
    bool writeReg(uint8_t sub_address, const uint8_t* buf, uint16_t count);
    uint16_t status();
};