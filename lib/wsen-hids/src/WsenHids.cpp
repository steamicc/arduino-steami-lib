// SPDX-License-Identifier: GPL-3.0-or-later

#include "WsenHids.h"

#include <cmath>

#include "WsenHids_const.h"

using namespace WsenHidsConst;

WsenHids::WsenHids(TwoWire& wire, uint8_t address)
    : _calibrationLoaded(false),
      _wire(wire),
      _address(address),
      _h0_rh(0.0f),
      _h1_rh(0.0f),
      _h0_t0_out(0),
      _h1_t0_out(0),
      _t0_degC(0.0f),
      _t1_degC(0.0f),
      _t0_out(0),
      _t1_out(0) {}

bool WsenHids::begin() {
    _wire.begin();

    uint8_t id = deviceId();
    if (id != WHO_AM_I_VALUE) {
        return false;
    }

    powerOn();
    setContinuous(1);

    return readCalibration();
}

uint8_t WsenHids::deviceId() {
    uint8_t id = 0;
    if (!readReg(WHO_AM_I_REG, &id, 1)) {
        return 0x00;
    }
    return id;
}

void WsenHids::powerOn() {
    updateReg(CTRL1_REG, CTRL1_PD_BIT, CTRL1_PD_BIT);
}

void WsenHids::powerOff() {
    updateReg(CTRL1_REG, CTRL1_PD_BIT, 0x00);
}

void WsenHids::reboot() {
    updateReg(CTRL2_REG, CTRL2_REBOOT_BIT, CTRL2_REBOOT_BIT);
}

uint8_t WsenHids::status() {
    uint8_t value = 0;
    if (!readReg(STATUS_REG, &value, 1)) {
        return 0;
    }
    return value;
}

bool WsenHids::temperatureReady() {
    return (status() & STATUS_T_DA) != 0;
}

bool WsenHids::humidityReady() {
    return (status() & STATUS_H_DA) != 0;
}

bool WsenHids::dataReady() {
    uint8_t s = status();
    return (s & (STATUS_T_DA | STATUS_H_DA)) == (STATUS_T_DA | STATUS_H_DA);
}

float WsenHids::temperature() {
    if (!_calibrationLoaded) {
        if (!readCalibration()) {
            return NAN;
        }
    }

    if (_t1_out == _t0_out) {
        return NAN;
    }

    int16_t raw = 0;
    if (!readS16(TEMP_OUT_L_REG, raw)) {
        return NAN;
    }

    float temp = _t0_degC + (static_cast<float>(raw - _t0_out) * (_t1_degC - _t0_degC)) /
                                static_cast<float>(_t1_out - _t0_out);

    return temp;
}

float WsenHids::humidity() {
    if (!_calibrationLoaded) {
        if (!readCalibration()) {
            return NAN;
        }
    }

    if (_h1_t0_out == _h0_t0_out) {
        return NAN;
    }

    int16_t raw = 0;
    if (!readS16(HUMIDITY_OUT_L_REG, raw)) {
        return NAN;
    }

    float hum =
        _h0_rh + ((float)(raw - _h0_t0_out) * (_h1_rh - _h0_rh)) / (float)(_h1_t0_out - _h0_t0_out);

    if (hum < 0.0f) {
        hum = 0.0f;
    }
    if (hum > 100.0f) {
        hum = 100.0f;
    }

    return hum;
}

void WsenHids::setContinuous(uint8_t odr) {
    uint8_t ctrl1 = 0;
    if (!readReg(CTRL1_REG, &ctrl1, 1)) {
        return;
    }

    ctrl1 &= (uint8_t)~CTRL1_ODR_MASK;

    switch (odr) {
        case 1:
            ctrl1 |= ODR_1HZ;
            break;
        case 7:
            ctrl1 |= ODR_7HZ;
            break;
        default:
            ctrl1 |= ODR_12_5HZ;
            break;
    }

    ctrl1 |= CTRL1_PD_BIT;
    ctrl1 |= CTRL1_BDU_BIT;

    writeReg(CTRL1_REG, ctrl1);
}

void WsenHids::triggerOneShot() {
    uint8_t ctrl1 = 0;
    uint8_t ctrl2 = 0;

    if (!readReg(CTRL1_REG, &ctrl1, 1)) {
        return;
    }

    ctrl1 |= CTRL1_PD_BIT;
    ctrl1 |= CTRL1_BDU_BIT;
    ctrl1 &= (uint8_t)~CTRL1_ODR_MASK;
    ctrl1 |= ODR_ONE_SHOT;

    if (!writeReg(CTRL1_REG, ctrl1)) {
        return;
    }

    updateReg(CTRL2_REG, CTRL2_ONE_SHOT_BIT, CTRL2_ONE_SHOT_BIT);
}

std::tuple<float, float> WsenHids::readOneShot() {
    triggerOneShot();

    unsigned long start = millis();
    while (!dataReady()) {
        if (millis() - start > 100) {
            return {NAN, NAN};
        }
        delay(1);
    }

    float hum = humidity();
    float temp = temperature();

    return {hum, temp};
}

void WsenHids::setAveraging(uint8_t humidityAvg, uint8_t temperatureAvg) {
    uint8_t value = 0;

    value |= (uint8_t)(humidityAvg & AV_CONF_AVGH_MASK);
    value |= (uint8_t)((temperatureAvg & 0x07) << 3);

    writeReg(AV_CONF_REG, value);
}

bool WsenHids::readReg(uint8_t reg, uint8_t* data, size_t len) {
    if (len == 0) {
        return true;
    }

    uint8_t regAddress = reg;
    if (len > 1) {
        regAddress |= AUTO_INCREMENT;
    }

    _wire.beginTransmission(_address);
    _wire.write(regAddress);
    if (_wire.endTransmission(false) != 0) {
        return false;
    }

    size_t readCount = _wire.requestFrom(_address, (uint8_t)len);
    if (readCount != len) {
        return false;
    }

    for (size_t i = 0; i < len; ++i) {
        if (!_wire.available()) {
            return false;
        }
        data[i] = (uint8_t)_wire.read();
    }

    return true;
}

bool WsenHids::writeReg(uint8_t reg, uint8_t data) {
    return writeReg(reg, &data, 1);
}

bool WsenHids::updateReg(uint8_t reg, uint8_t mask, uint8_t value) {
    uint8_t current = 0;
    if (!readReg(reg, &current, 1)) {
        return false;
    }

    current &= (uint8_t)~mask;
    current |= (uint8_t)(value & mask);

    return writeReg(reg, current);
}

bool WsenHids::writeReg(uint8_t reg, const uint8_t* data, size_t len) {
    if (len == 0) {
        return true;
    }

    uint8_t regAddress = reg;
    if (len > 1) {
        regAddress |= AUTO_INCREMENT;
    }

    _wire.beginTransmission(_address);
    _wire.write(regAddress);

    for (size_t i = 0; i < len; ++i) {
        _wire.write(data[i]);
    }

    return _wire.endTransmission(true) == 0;
}

bool WsenHids::readU16(uint8_t lowReg, uint16_t& value) {
    uint8_t buffer[2] = {0, 0};
    if (!readReg(lowReg, buffer, 2)) {
        value = 0;
        return false;
    }

    value = (uint16_t)buffer[0] | ((uint16_t)buffer[1] << 8);
    return true;
}

bool WsenHids::readS16(uint8_t lowReg, int16_t& value) {
    uint16_t raw = 0;
    if (!readU16(lowReg, raw)) {
        value = 0;
        return false;
    }

    value = (int16_t)raw;
    return true;
}

bool WsenHids::readCalibration() {
    uint8_t h0_x2 = 0;
    uint8_t h1_x2 = 0;
    uint8_t t0_x8_l = 0;
    uint8_t t1_x8_l = 0;
    uint8_t t1_t0_msb = 0;

    _calibrationLoaded = false;

    if (!readReg(H0_RH_X2_REG, &h0_x2, 1)) {
        return false;
    }
    if (!readReg(H1_RH_X2_REG, &h1_x2, 1)) {
        return false;
    }
    if (!readReg(T0_DEGC_X8_REG, &t0_x8_l, 1)) {
        return false;
    }
    if (!readReg(T1_DEGC_X8_REG, &t1_x8_l, 1)) {
        return false;
    }
    if (!readReg(T1_T0_MSB_REG, &t1_t0_msb, 1)) {
        return false;
    }

    _h0_rh = ((float)h0_x2) / 2.0f;
    _h1_rh = ((float)h1_x2) / 2.0f;

    uint16_t t0_x8 = (uint16_t)t0_x8_l | (((uint16_t)(t1_t0_msb & 0x03)) << 8);
    uint16_t t1_x8 = (uint16_t)t1_x8_l | (((uint16_t)((t1_t0_msb >> 2) & 0x03)) << 8);

    _t0_degC = ((float)t0_x8) / 8.0f;
    _t1_degC = ((float)t1_x8) / 8.0f;

    if (!readS16(H0_T0_OUT_L_REG, _h0_t0_out)) {
        return false;
    }
    if (!readS16(H1_T0_OUT_L_REG, _h1_t0_out)) {
        return false;
    }
    if (!readS16(T0_OUT_L_REG, _t0_out)) {
        return false;
    }
    if (!readS16(T1_OUT_L_REG, _t1_out)) {
        return false;
    }

    _calibrationLoaded = true;
    return true;
}
