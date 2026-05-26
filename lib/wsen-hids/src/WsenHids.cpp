// SPDX-License-Identifier: GPL-3.0-or-later

#include "WsenHids.h"

#include <math.h>

WsenHids::WsenHids(TwoWire& wire, uint8_t address) : _wire(&wire), _address(address) {}

bool WsenHids::begin() {
    if (deviceId() != WSEN_HIDS_WHO_AM_I_VALUE) {
        return false;
    }
    // The chip lives on its own VDD rail on the STeaMi — an MCU reset
    // does NOT power-cycle it. Force AV_CONF and CTRL2 to defaults so
    // we don't inherit max-averaging or a latched ONE_SHOT bit from
    // whatever sketch ran previously.
    writeReg(WSEN_HIDS_REG_AV_CONF, WSEN_HIDS_AV_CONF_DEFAULT);
    writeReg(WSEN_HIDS_REG_CTRL2, 0x00);

    loadCalibration();
    // Leave the part powered down after detection. Measurement methods
    // auto-trigger on demand; users that want streaming call setContinuous().
    powerOff();
    return true;
}

uint8_t WsenHids::deviceId() {
    return readReg(WSEN_HIDS_REG_WHO_AM_I);
}

void WsenHids::reboot() {
    writeReg(WSEN_HIDS_REG_CTRL2, WSEN_HIDS_CTRL2_BOOT);
    // BOOT clears itself when the reload completes. Poll with a bounded
    // timeout so a stuck bus doesn't hang the caller.
    for (uint8_t i = 0; i < 20; ++i) {
        if ((readReg(WSEN_HIDS_REG_CTRL2) & WSEN_HIDS_CTRL2_BOOT) == 0) {
            return;
        }
        delay(5);
    }
}

void WsenHids::softReset() {
    reboot();
}

void WsenHids::powerOn() {
    uint8_t ctrl1 = readReg(WSEN_HIDS_REG_CTRL1);
    writeReg(WSEN_HIDS_REG_CTRL1, ctrl1 | WSEN_HIDS_CTRL1_PD | WSEN_HIDS_CTRL1_BDU);
}

void WsenHids::powerOff() {
    uint8_t ctrl1 = readReg(WSEN_HIDS_REG_CTRL1);
    writeReg(WSEN_HIDS_REG_CTRL1, ctrl1 & ~WSEN_HIDS_CTRL1_PD);
}

bool WsenHids::dataReady() {
    return (status() & (WSEN_HIDS_STATUS_H_DA | WSEN_HIDS_STATUS_T_DA)) ==
           (WSEN_HIDS_STATUS_H_DA | WSEN_HIDS_STATUS_T_DA);
}

bool WsenHids::temperatureReady() {
    return (status() & WSEN_HIDS_STATUS_T_DA) != 0;
}

bool WsenHids::humidityReady() {
    return (status() & WSEN_HIDS_STATUS_H_DA) != 0;
}

float WsenHids::temperature() {
    return read().temperature;
}

float WsenHids::humidity() {
    return read().humidity;
}

WsenHids::ReadResult WsenHids::read() {
    if (!isPoweredOn()) {
        triggerOneShot();
        if (!waitForDataReady()) {
            // Device never reported fresh data — bus issue, missing sensor,
            // or the caller disabled ODR and didn't trigger a conversion.
            // Surface the failure as NaN so silent stale readings can't
            // propagate; callers can check with isnan().
            return {NAN, NAN};
        }
    }

    uint8_t humBytes[2];
    uint8_t tempBytes[2];
    readRegs(WSEN_HIDS_REG_HUMIDITY_OUT_L, humBytes, 2);
    readRegs(WSEN_HIDS_REG_TEMP_OUT_L, tempBytes, 2);

    int16_t humRaw = static_cast<int16_t>(humBytes[0] | (humBytes[1] << 8));
    int16_t tempRaw = static_cast<int16_t>(tempBytes[0] | (tempBytes[1] << 8));

    return {computeTemperature(tempRaw), computeHumidity(humRaw)};
}

void WsenHids::setContinuous(uint8_t odr) {
    uint8_t ctrl1 = WSEN_HIDS_CTRL1_PD | WSEN_HIDS_CTRL1_BDU | (odr & WSEN_HIDS_CTRL1_ODR_MASK);
    writeReg(WSEN_HIDS_REG_CTRL1, ctrl1);
}

void WsenHids::triggerOneShot() {
    // Must be powered on at ODR=one-shot for the ONE_SHOT bit to matter.
    writeReg(WSEN_HIDS_REG_CTRL1,
             WSEN_HIDS_CTRL1_PD | WSEN_HIDS_CTRL1_BDU | WSEN_HIDS_ODR_ONE_SHOT);
    writeReg(WSEN_HIDS_REG_CTRL2, WSEN_HIDS_CTRL2_ONE_SHOT);
}

WsenHids::ReadResult WsenHids::readOneShot() {
    triggerOneShot();
    if (!waitForDataReady()) {
        return {NAN, NAN};
    }
    return read();
}

void WsenHids::setAveraging(uint8_t humidityAvg, uint8_t temperatureAvg) {
    uint8_t value = static_cast<uint8_t>(humidityAvg & WSEN_HIDS_AV_CONF_AVGH_MASK) |
                    static_cast<uint8_t>((temperatureAvg << WSEN_HIDS_AV_CONF_AVGT_SHIFT) &
                                         WSEN_HIDS_AV_CONF_AVGT_MASK);
    writeReg(WSEN_HIDS_REG_AV_CONF, value);
}

void WsenHids::setTemperatureOffset(float offset) {
    _tempOffset = offset;
}

void WsenHids::calibrateTemperature(float refLow, float measLow, float refHigh, float measHigh) {
    float span = measHigh - measLow;
    if (span == 0.0f) {
        _tempUserSlope = 1.0f;
        _tempUserIntercept = 0.0f;
        return;
    }
    _tempUserSlope = (refHigh - refLow) / span;
    _tempUserIntercept = refLow - _tempUserSlope * measLow;
}

uint8_t WsenHids::status() {
    return readReg(WSEN_HIDS_REG_STATUS);
}

uint8_t WsenHids::readReg(uint8_t reg) {
    _wire->beginTransmission(_address);
    _wire->write(reg);
    _wire->endTransmission(false);
    _wire->requestFrom(_address, static_cast<uint8_t>(1));
    if (_wire->available()) {
        return static_cast<uint8_t>(_wire->read());
    }
    return 0;
}

void WsenHids::writeReg(uint8_t reg, uint8_t value) {
    _wire->beginTransmission(_address);
    _wire->write(reg);
    _wire->write(value);
    _wire->endTransmission();
}

void WsenHids::readRegs(uint8_t reg, uint8_t* buf, size_t len) {
    // Zero-init up front so a short bus read leaves defined bytes in buf
    // rather than whatever was on the stack — callers assume every slot
    // was written.
    for (size_t i = 0; i < len; ++i) {
        buf[i] = 0;
    }

    _wire->beginTransmission(_address);
    _wire->write(reg | WSEN_HIDS_AUTO_INCREMENT);
    _wire->endTransmission(false);
    _wire->requestFrom(_address, static_cast<uint8_t>(len));
    for (size_t i = 0; i < len && _wire->available(); ++i) {
        buf[i] = static_cast<uint8_t>(_wire->read());
    }
}

void WsenHids::loadCalibration() {
    uint8_t block[16];
    readRegs(WSEN_HIDS_REG_H0_RH_X2, block, 16);

    // H0_rH and H1_rH are stored as %RH * 2.
    float h0Rh = block[0] * 0.5f;
    float h1Rh = block[1] * 0.5f;

    // T0_degC / T1_degC are 10-bit unsigned values * 8. The high 2 bits of
    // each live in the shared T1_T0_MSB register at block offset 5.
    uint8_t msb = block[5];
    uint16_t t0Raw = ((msb & 0x03) << 8) | block[2];
    uint16_t t1Raw = ((msb & 0x0C) << 6) | block[3];
    float t0DegC = t0Raw / 8.0f;
    float t1DegC = t1Raw / 8.0f;

    // 16-bit signed ADC reference points. T1_T0_MSB sits at offset 5,
    // so the OUT registers follow with one byte of gap: H0_T0_OUT at
    // offsets 6-7, H1_T0_OUT at 10-11, T0_OUT at 12-13, T1_OUT at 14-15.
    int16_t h0Out = static_cast<int16_t>(block[6] | (block[7] << 8));
    int16_t h1Out = static_cast<int16_t>(block[10] | (block[11] << 8));
    int16_t t0Out = static_cast<int16_t>(block[12] | (block[13] << 8));
    int16_t t1Out = static_cast<int16_t>(block[14] | (block[15] << 8));

    if (t1Out != t0Out) {
        _tempSlope = (t1DegC - t0DegC) / static_cast<float>(t1Out - t0Out);
        _tempIntercept = t0DegC - _tempSlope * static_cast<float>(t0Out);
    }
    if (h1Out != h0Out) {
        _humSlope = (h1Rh - h0Rh) / static_cast<float>(h1Out - h0Out);
        _humIntercept = h0Rh - _humSlope * static_cast<float>(h0Out);
    }
}

bool WsenHids::isPoweredOn() {
    return (readReg(WSEN_HIDS_REG_CTRL1) & WSEN_HIDS_CTRL1_PD) != 0;
}

bool WsenHids::waitForDataReady(uint32_t timeoutMs) {
    uint32_t start = millis();
    while (millis() - start < timeoutMs) {
        if (dataReady()) {
            return true;
        }
        delay(1);
    }
    return false;
}

float WsenHids::computeTemperature(int16_t raw) {
    float factory = _tempIntercept + _tempSlope * static_cast<float>(raw);
    return _tempUserSlope * factory + _tempUserIntercept + _tempOffset;
}

float WsenHids::computeHumidity(int16_t raw) {
    float value = _humIntercept + _humSlope * static_cast<float>(raw);
    if (value < 0.0f)
        return 0.0f;
    if (value > 100.0f)
        return 100.0f;
    return value;
}
