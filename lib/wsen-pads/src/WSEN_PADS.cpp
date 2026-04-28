// SPDX-License-Identifier: GPL-3.0-or-later
#include "WSEN_PADS.h"

#include <math.h>
#include <stdint.h>

WSEN_PADS::WSEN_PADS(TwoWire& wire, uint8_t address)
    : _wire(&wire),
      _address(address),
      _tempUserSlope(1.0f),
      _tempUserOffset(0.0f),
      _tempOffset(0.0f) {}

// ---------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------

bool WSEN_PADS::begin() {
    delay(BOOT_DELAY_MS);
    if (!isPresent()) {
        return false;
    }
    if (!waitBoot()) {
        return false;
    }
    if (deviceId() != WSEN_PADS_DEVICE_ID) {
        return false;
    }
    configureDefault();
    return true;
}

uint8_t WSEN_PADS::deviceId() {
    return readReg(REG_DEVICE_ID);
}

void WSEN_PADS::softReset() {
    updateReg(REG_CTRL_2, CTRL2_SWRESET, CTRL2_SWRESET);
    delay(1);
    configureDefault();
}

void WSEN_PADS::reboot() {
    updateReg(REG_CTRL_2, CTRL2_BOOT, CTRL2_BOOT);
    waitBoot();
    configureDefault();
}

void WSEN_PADS::powerOff() {
    // ODR = 000 puts the part in power-down. ONE_SHOT is only effective
    // from this state.
    updateReg(REG_CTRL_1, CTRL1_ODR_MASK, ODR_POWER_DOWN << CTRL1_ODR_SHIFT);
}

void WSEN_PADS::powerOn() {
    setContinuous(ODR_1_HZ);
}

// ---------------------------------------------------------------------
// Reading
// ---------------------------------------------------------------------

float WSEN_PADS::pressureHpa() {
    int32_t raw = pressureRaw();
    if (raw == INT32_MIN) {
        return NAN;
    }
    return raw * PRESSURE_HPA_PER_DIGIT;
}

float WSEN_PADS::temperature() {
    int32_t raw = temperatureRaw();
    if (raw == INT32_MIN) {
        return NAN;
    }
    float factory = raw * TEMPERATURE_C_PER_DIGIT;
    return _tempUserSlope * factory + _tempUserOffset + _tempOffset;
}

WSEN_PADS::ReadResult WSEN_PADS::read() {
    if (!isPoweredOn()) {
        triggerOneShot();
        if (!waitForDataReady()) {
            return {NAN, NAN};
        }
    }
    return readSensorData();
}

// ---------------------------------------------------------------------
// Status
// ---------------------------------------------------------------------

bool WSEN_PADS::dataReady() {
    uint8_t s = status();
    return (s & (STATUS_P_DA | STATUS_T_DA)) == (STATUS_P_DA | STATUS_T_DA);
}

bool WSEN_PADS::pressureReady() {
    return (status() & STATUS_P_DA) != 0;
}

bool WSEN_PADS::temperatureReady() {
    return (status() & STATUS_T_DA) != 0;
}

// ---------------------------------------------------------------------
// Modes
// ---------------------------------------------------------------------

void WSEN_PADS::setContinuous(uint8_t odr) {
    // Build CTRL_1 with the requested ODR plus BDU. LPF and low-noise
    // bits live elsewhere (CTRL_2 / CTRL_1.LPFP) and aren't touched here
    // — callers layer them on with enableLowPass / enableLowNoise.
    uint8_t ctrl1 = CTRL1_BDU | ((odr << CTRL1_ODR_SHIFT) & CTRL1_ODR_MASK);
    writeReg(REG_CTRL_1, ctrl1);
}

void WSEN_PADS::triggerOneShot() {
    // The ONE_SHOT bit only matters when ODR = 000, so put the part in
    // power-down first. Non-blocking: callers that need to wait for the
    // result poll dataReady() (or use readOneShot()).
    powerOff();
    updateReg(REG_CTRL_2, CTRL2_ONE_SHOT, CTRL2_ONE_SHOT);
}

WSEN_PADS::ReadResult WSEN_PADS::readOneShot() {
    triggerOneShot();
    if (!waitForDataReady()) {
        return {NAN, NAN};
    }
    return readSensorData();
}

// ---------------------------------------------------------------------
// Calibration
// ---------------------------------------------------------------------

void WSEN_PADS::setTemperatureOffset(float offset) {
    _tempOffset = offset;
}

void WSEN_PADS::calibrateTemperature(float refLow, float measLow, float refHigh, float measHigh) {
    float span = measHigh - measLow;
    if (span == 0.0f) {
        // Identity — caller asked for a degenerate two-point calibration.
        _tempUserSlope = 1.0f;
        _tempUserOffset = 0.0f;
        return;
    }
    _tempUserSlope = (refHigh - refLow) / span;
    _tempUserOffset = refLow - _tempUserSlope * measLow;
}

// ---------------------------------------------------------------------
// Driver-specific tuning
// ---------------------------------------------------------------------

void WSEN_PADS::enableLowPass(bool strong) {
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
    uint8_t current = readReg(REG_CTRL_1);
    current &= ~(CTRL1_EN_LPFP | CTRL1_LPFP_CFG);
    writeReg(REG_CTRL_1, current);
}

void WSEN_PADS::enableLowNoise() {
    // Datasheet: LOW_NOISE_EN must only be modified while in power-down.
    // Caller is expected to be in power-down (e.g. right after begin())
    // or to call powerOff() first.
    updateReg(REG_CTRL_2, CTRL2_LOW_NOISE_EN, CTRL2_LOW_NOISE_EN);
}

void WSEN_PADS::disableLowNoise() {
    updateReg(REG_CTRL_2, CTRL2_LOW_NOISE_EN, 0);
}

// ---------------------------------------------------------------------
// I2C helpers
// ---------------------------------------------------------------------

bool WSEN_PADS::isPresent() {
    _wire->beginTransmission(_address);
    return _wire->endTransmission() == 0;
}

uint8_t WSEN_PADS::readReg(uint8_t reg) {
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
    _wire->beginTransmission(_address);
    _wire->write(reg);
    _wire->write(value);
    _wire->endTransmission();
}

void WSEN_PADS::updateReg(uint8_t reg, uint8_t mask, uint8_t value) {
    uint8_t current = readReg(reg);
    current = (current & ~mask) | (value & mask);
    writeReg(reg, current);
}

// ---------------------------------------------------------------------
// Boot helpers
// ---------------------------------------------------------------------

bool WSEN_PADS::waitBoot(uint32_t timeoutMs) {
    uint32_t start = millis();
    while (readReg(REG_INT_SOURCE) & INT_SOURCE_BOOT_ON) {
        if ((millis() - start) > timeoutMs) {
            return false;
        }
        delay(1);
    }
    return true;
}

void WSEN_PADS::configureDefault() {
    // Auto-increment for multi-byte reads, low-noise off, BDU enabled,
    // ODR = power-down. LPF is left untouched.
    updateReg(REG_CTRL_2, CTRL2_IF_ADD_INC | CTRL2_LOW_NOISE_EN, CTRL2_IF_ADD_INC);
    writeReg(REG_CTRL_1, CTRL1_BDU);
}

// ---------------------------------------------------------------------
// Data path
// ---------------------------------------------------------------------

bool WSEN_PADS::isPoweredOn() {
    return (readReg(REG_CTRL_1) & CTRL1_ODR_MASK) != 0;
}

bool WSEN_PADS::waitForDataReady(uint32_t timeoutMs) {
    uint32_t start = millis();
    while ((millis() - start) < timeoutMs) {
        if (dataReady()) {
            return true;
        }
        delay(1);
    }
    return false;
}

uint8_t WSEN_PADS::status() {
    return readReg(REG_STATUS);
}

int32_t WSEN_PADS::pressureRaw() {
    if (!isPoweredOn()) {
        triggerOneShot();
        if (!waitForDataReady()) {
            return INT32_MIN;
        }
    }
    uint8_t data[3];
    readBlock(REG_DATA_P_XL, data, 3);
    uint32_t raw = (static_cast<uint32_t>(data[2]) << 16) | (static_cast<uint32_t>(data[1]) << 8) |
                   static_cast<uint32_t>(data[0]);
    return toSigned24(raw);
}

int32_t WSEN_PADS::temperatureRaw() {
    if (!isPoweredOn()) {
        triggerOneShot();
        if (!waitForDataReady()) {
            return INT32_MIN;
        }
    }
    uint8_t data[2];
    readBlock(REG_DATA_T_L, data, 2);
    uint16_t raw = static_cast<uint16_t>((data[1] << 8) | data[0]);
    return toSigned16(raw);
}

WSEN_PADS::ReadResult WSEN_PADS::readSensorData() {
    uint8_t pData[3];
    readBlock(REG_DATA_P_XL, pData, 3);
    uint32_t pBits = (static_cast<uint32_t>(pData[2]) << 16) |
                     (static_cast<uint32_t>(pData[1]) << 8) | static_cast<uint32_t>(pData[0]);
    int32_t pRaw = toSigned24(pBits);

    uint8_t tData[2];
    readBlock(REG_DATA_T_L, tData, 2);
    uint16_t tBits = static_cast<uint16_t>((tData[1] << 8) | tData[0]);
    int32_t tRaw = toSigned16(tBits);

    float tFactory = tRaw * TEMPERATURE_C_PER_DIGIT;
    float tC = _tempUserSlope * tFactory + _tempUserOffset + _tempOffset;
    return {pRaw * PRESSURE_HPA_PER_DIGIT, tC};
}

// ---------------------------------------------------------------------
// Conversion utilities
// ---------------------------------------------------------------------

int32_t WSEN_PADS::toSigned24(uint32_t value) {
    if (value & 0x800000u) {
        return static_cast<int32_t>(value) - 0x1000000;
    }
    return static_cast<int32_t>(value);
}

int32_t WSEN_PADS::toSigned16(uint16_t value) {
    if (value & 0x8000u) {
        return static_cast<int32_t>(value) - 0x10000;
    }
    return static_cast<int32_t>(value);
}
