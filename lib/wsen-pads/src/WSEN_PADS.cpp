// SPDX-License-Identifier: GPL-3.0-or-later
#include "WSEN_PADS.h"

#include <math.h>
#include <stdint.h>

WSEN_PADS::WSEN_PADS(TwoWire& wire, uint8_t address, float temp_gain, float temp_offset)
    : _wire(&wire), _address(address), _temp_gain(temp_gain), _temp_offset(temp_offset) {}

bool WSEN_PADS::begin() {
    delay(BOOT_DELAY_MS);

    if (!isPresent()) {
        return false;
    }

    if (!waitBoot()) {
        return false;
    }

    if (!checkDevice()) {
        return false;
    }
    configureDefault();

    powerOff();
    return true;
}

// ---------------------------------------------------------------------
// Low-level I2C helpers
// ---------------------------------------------------------------------

bool WSEN_PADS::isPresent() {
    // Return True if the device address is visible on the I2C bus.
    _wire->beginTransmission(_address);
    return (_wire->endTransmission() == 0);
}

uint8_t WSEN_PADS::readReg(uint8_t reg) {
    // Read and return one unsigned byte from a register.
    _wire->beginTransmission(_address);
    _wire->write(reg);
    _wire->endTransmission(false);
    _wire->requestFrom(_address, static_cast<uint8_t>(1));
    if (_wire->available()) {
        return static_cast<uint8_t>(_wire->read());
    }
    return 0;
}

void WSEN_PADS::readBlock(uint8_t reg, uint8_t* buf, size_t len) {
    // Read and return multiple bytes starting at a register.
    for (size_t i = 0; i < len; ++i) {
        buf[i] = 0;
    }
    _wire->beginTransmission(_address);
    _wire->write(reg);
    _wire->endTransmission(false);
    _wire->requestFrom(_address, static_cast<uint8_t>(len));
    for (size_t i = 0; i < len && _wire->available(); ++i) {
        buf[i] = static_cast<uint8_t>(_wire->read());
    }
}

void WSEN_PADS::writeReg(uint8_t reg, uint8_t value) {
    // Write one unsigned byte to a register.
    _wire->beginTransmission(_address);
    _wire->write(reg);
    _wire->write(value);
    _wire->endTransmission();
}

void WSEN_PADS::updateReg(uint8_t reg, uint8_t mask, uint8_t value) {
    /* Update selected bits in a register.
       Only the bits set in 'mask' are modified. Other bits are preserved. */
    uint8_t current = readReg(reg);
    current = (current & ~mask) | (value & mask);
    writeReg(reg, current);
}

// ---------------------------------------------------------------------
// Internal conversion helpers
// ---------------------------------------------------------------------

int32_t WSEN_PADS::toSigned24(int32_t value) {
    // Convert a 24-bit integer to a signed Python integer.
    if (value & 0x800000) {
        value -= 0x1000000;
    }
    return value;
}

int32_t WSEN_PADS::toSigned16(uint16_t value) {
    // Convert a 16-bit integer to a signed Python integer.
    if (value & 0x8000) {
        value -= 0x10000;
    }
    return value;
}

// ---------------------------------------------------------------------
// Internal device helpers
// ---------------------------------------------------------------------

bool WSEN_PADS::waitBoot(uint32_t timeoutMs) {
    /*Wait until the BOOT_ON flag is cleared.
    The sensor sets BOOT_ON while internal trimming parameters are loaded.*/
    uint32_t start = millis();
    while (readReg(REG_INT_SOURCE) & INT_SOURCE_BOOT_ON) {
        if ((millis() - start) > timeoutMs) {
            return false;
        }
        delay(1);
    }
    return true;
}

bool WSEN_PADS::checkDevice() {
    return deviceId() == WSEN_PADS_DEVICE_ID;
}

void WSEN_PADS::configureDefault() {
    /*
     Apply a safe default configuration.

        Default choices:
        - power-down mode
        - block data update enabled
        - register auto-increment enabled
        - low-noise disabled
        - low-pass filter disabled
    */
    powerOff();
    updateReg(REG_CTRL_2, CTRL2_IF_ADD_INC, CTRL2_IF_ADD_INC);
    updateReg(REG_CTRL_1, CTRL1_BDU, CTRL1_BDU);
    updateReg(REG_CTRL_2, CTRL2_LOW_NOISE_EN, 0);
}

// ---------------------------------------------------------------------
// Identification and status
// ---------------------------------------------------------------------

uint8_t WSEN_PADS::deviceId() {
    // Return the value of the DEVICE_ID register.
    return readReg(REG_DEVICE_ID);
}

uint8_t WSEN_PADS::status() {
    // Return the raw STATUS register value.
    return readReg(REG_STATUS);
}

bool WSEN_PADS::pressureReady() {
    // Return True when new pressure data is available.
    return bool(status() & STATUS_P_DA);
}

bool WSEN_PADS::temperatureReady() {
    // Return True when new temperature data is available.
    return bool(status() & STATUS_T_DA);
}

bool WSEN_PADS::dataReady() {
    // Return True when both pressure and temperature data are available.
    // This is mainly useful in continuous mode.
    uint8_t statusVal = status();
    return bool((statusVal & STATUS_P_DA) && (statusVal & STATUS_T_DA));
}

// --------------------------------------------------------------------
// Power and reset control
// ---------------------------------------------------------------------

void WSEN_PADS::powerOff() {
    // Put the device in power-down mode by setting ODR = 000.
    updateReg(REG_CTRL_1, CTRL1_ODR_MASK, ODR_POWER_DOWN << CTRL1_ODR_SHIFT);
}

void WSEN_PADS::powerOn(uint8_t odr) {
    // Resume continuous measurement at the given ODR.
    setContinuous(odr);
}

void WSEN_PADS::softReset() {
    // Trigger a software reset.
    // This restores user registers to their default values.
    updateReg(REG_CTRL_2, CTRL2_SWRESET, CTRL2_SWRESET);
    delay(1);

    // Re-apply the minimal driver configuration after reset.
    configureDefault();
}

void WSEN_PADS::reboot() {
    // Trigger a reboot of the internal memory content.
    // This reloads trimming parameters from internal non-volatile memory.
    updateReg(REG_CTRL_2, CTRL2_BOOT, CTRL2_BOOT);
    waitBoot();

    // Re-apply the minimal driver configuration after reboot.
    configureDefault();
}

// ---------------------------------------------------------------------
//  Raw data reading
//  ---------------------------------------------------------------------

bool WSEN_PADS::isPowerDown() {
    // Return True if the sensor is in power-down mode (ODR = 000).
    return (readReg(REG_CTRL_1) & CTRL1_ODR_MASK) == 0;
}

bool WSEN_PADS::ensureData() {
    // Trigger a one-shot conversion if the sensor is in power-down mode.
    if (isPowerDown()) {
        triggerOneShot();
        uint8_t readyMask = STATUS_P_DA | STATUS_T_DA;
        for (int i = 0; i < 50; i++) {
            if ((readReg(REG_STATUS) & readyMask) == readyMask) {
                return true;
            }
            delay(2);
        }
        return false;
    }
    return true;
}

int32_t WSEN_PADS::pressureRaw() {
    /*Read and return raw pressure as a signed 24-bit integer.
    If the sensor is in power-down mode, a one-shot conversion is
    triggered automatically before reading.*/
    if (!ensureData()) {
        return false;
    }
    uint8_t data[3];
    readBlock(REG_DATA_P_XL, data, 3);
    uint32_t raw = (data[2] << 16) | (data[1] << 8) | data[0];
    return toSigned24(raw);
}

int32_t WSEN_PADS::temperatureRaw() {
    /*Read and return raw temperature as a signed 16-bit integer.
    If the sensor is in power-down mode, a one-shot conversion is
    triggered automatically before reading.*/
    if (!ensureData()) {
        return false;
    }
    uint8_t data[2];
    readBlock(REG_DATA_T_L, data, 2);
    uint16_t raw = (data[1] << 8) | data[0];
    return toSigned16(raw);
}

// ---------------------------------------------------------------------
// Converted data reading
// ---------------------------------------------------------------------

float WSEN_PADS::pressureHpa() {
    // Read and return pressure in hPa.
    return pressureRaw() * PRESSURE_HPA_PER_DIGIT;
}

float WSEN_PADS::pressurePa() {
    // Read and return pressure in Pa.
    return pressureRaw() * PRESSURE_PA_PER_DIGIT;
}

float WSEN_PADS::pressureKpa() {
    // Read and return pressure in kPa.
    return pressureRaw() * PRESSURE_KPA_PER_DIGIT;
}

float WSEN_PADS::temperature() {
    // Read and return temperature in degrees Celsius.
    int32_t raw = temperatureRaw();
    if (raw == INT32_MIN) {
        return NAN;
    }
    float factory = raw * TEMPERATURE_C_PER_DIGIT;
    return _temp_gain * factory + _temp_offset;
}

WSEN_PADS::ReadResult WSEN_PADS::read(bool lowNoise) {
    if (isPowerDown()) {
        triggerOneShot(lowNoise);
    }
    uint8_t pData[3];
    readBlock(REG_DATA_P_XL, pData, 3);
    int32_t pRaw = toSigned24((pData[2] << 16) | (pData[1] << 8) | pData[0]);
    uint8_t tData[2];
    readBlock(REG_DATA_T_L, tData, 2);
    int32_t tRaw = toSigned16((tData[1] << 8) | tData[0]);
    float tC = _temp_gain * (tRaw * TEMPERATURE_C_PER_DIGIT) + _temp_offset;
    return {pRaw * PRESSURE_HPA_PER_DIGIT, tC};
}

// ---------------------------------------------------------------------
// One-shot mode
// ---------------------------------------------------------------------

void WSEN_PADS::triggerOneShot(bool lowNoise) {
    /*Trigger a single conversion.
    The device must be in power-down mode before setting ONE_SHOT.
    The function blocks until the typical conversion time has elapsed.
    Parameters:
    low_noise: False for low-power mode, True for low-noise mode*/
    powerOff();

    // LOW_NOISE_EN must only be changed while in power-down mode.
    if (lowNoise) {
        updateReg(REG_CTRL_2, CTRL2_LOW_NOISE_EN, CTRL2_LOW_NOISE_EN);
    } else {
        updateReg(REG_CTRL_2, CTRL2_LOW_NOISE_EN, 0);
    }

    // Start one-shot conversion.
    updateReg(REG_CTRL_2, CTRL2_ONE_SHOT, CTRL2_ONE_SHOT);

    // Wait for typical conversion completion time.
    if (lowNoise) {
        delay(ONE_SHOT_LOW_NOISE_DELAY_MS);
    } else {
        delay(ONE_SHOT_LOW_POWER_DELAY_MS);
    }
}

WSEN_PADS::ReadResult WSEN_PADS::readOneShot(bool lowNoise) {
    /*Trigger one conversion and return converted pressure and temperature.
    Returns:
    tuple: (pressure_hpa, temperature_c)*/
    return read(lowNoise);
}

// ---------------------------------------------------------------------
// Continuous mode
//---------------------------------------------------------------------

bool WSEN_PADS::setContinuous(uint8_t odr, bool lowNoise, bool lowPass, bool lowPassStrong) {
    /* Configure continuous measurement mode.
        Parameters:
            odr: one of the ODR_* constants
            low_noise: enable low-noise mode
            low_pass: enable LPF2 on pressure output
            low_pass_strong: when LPF2 is enabled:
                             False -> bandwidth ODR/9
                             True  -> bandwidth ODR/20*/

    if (odr != ODR_1_HZ && odr != ODR_10_HZ && odr != ODR_25_HZ && odr != ODR_50_HZ &&
        odr != ODR_75_HZ && odr != ODR_100_HZ && odr != ODR_200_HZ) {
        return false;
    }

    // Low-noise mode is not allowed at 100 Hz and 200 Hz.
    if (lowNoise && (odr == ODR_100_HZ || odr == ODR_200_HZ)) {
        return false;
    }

    // Enter power-down before changing LOW_NOISE_EN as required by the sensor.
    powerOff();

    // Configure low-noise mode and auto-increment.
    uint8_t ctrl2Value = CTRL2_IF_ADD_INC;
    if (lowNoise) {
        ctrl2Value |= CTRL2_LOW_NOISE_EN;
    }
    updateReg(REG_CTRL_2, CTRL2_IF_ADD_INC | CTRL2_LOW_NOISE_EN, ctrl2Value);

    // Build CTRL_1 configuration.
    uint8_t ctrl1Value = CTRL1_BDU | (odr << CTRL1_ODR_SHIFT);
    if (lowPass) {
        ctrl1Value |= CTRL1_EN_LPFP;
        if (lowPassStrong) {
            ctrl1Value |= CTRL1_LPFP_CFG;
        }
    }
    writeReg(REG_CTRL_1, ctrl1Value);
    return true;
}

// ---------------------------------------------------------------------
// Optional helper methods
// ---------------------------------------------------------------------

void WSEN_PADS::enableLowPass(bool strong) {
    /*Enable the optional LPF2 pressure filter.
    This helper preserves the current ODR and only updates filter bits.*/

    uint8_t current = readReg(REG_CTRL_1);
    current |= CTRL1_EN_LPFP;
    if (strong) {
        current |= CTRL1_LPFP_CFG;
    } else {
        current &= ~CTRL1_LPFP_CFG;
    }
    writeReg(REG_CTRL_1, current);
}

void WSEN_PADS::disableLowPass() {
    // Disable the optional LPF2 pressure filter.
    uint8_t current = readReg(REG_CTRL_1);
    current &= ~(CTRL1_EN_LPFP | CTRL1_LPFP_CFG);
    writeReg(REG_CTRL_1, current);
}

// ---------------------------------------------------------------------
// Temperature calibration
// ---------------------------------------------------------------------

void WSEN_PADS::setTempOffset(float offsetC) {
    /*Set a temperature offset in °C (gain remains 1.0).
    Args:
        offset_c: offset value in degrees Celsius.*/
    _temp_gain = 1.0;
    _temp_offset = offsetC;
}

bool WSEN_PADS::calibrateTemperature(float refLow, float measuredLow, float refHigh,
                                     float measuredHigh) {
    /*
    Two-point calibration from reference measurements.

        Computes gain and offset so that the sensor reading is adjusted
        to match reference values at two temperature points.

        Args:
            ref_low: reference temperature at the low point (°C).
            measured_low: sensor reading at the low point (°C).
            ref_high: reference temperature at the high point (°C).
            measured_high: sensor reading at the high point (°C).*/
    float delta = measuredHigh - measuredLow;
    if (delta == 0.0f) {
        return false;
    }
    _temp_gain = (refHigh - refLow) / delta;
    _temp_offset = refLow - _temp_gain * measuredLow;
    return true;
}