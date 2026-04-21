// SPDX-License-Identifier: GPL-3.0-or-later

#include "HTS221.h"

#include <math.h>

HTS221::HTS221(TwoWire& wire, uint8_t address) : _wire(&wire), _address(address) {}

bool HTS221::begin() {
    if (deviceId() != HTS221_WHO_AM_I_VALUE) {
        return false;
    }
    loadCalibration();
    // Leave the part powered down after detection. Measurement methods
    // auto-trigger on demand; users that want streaming call setContinuous().
    powerOff();
    return true;
}

uint8_t HTS221::deviceId() {
    return readReg(HTS221_REG_WHO_AM_I);
}

void HTS221::reboot() {
    writeReg(HTS221_REG_CTRL2, HTS221_CTRL2_BOOT);
    // BOOT clears itself when the reload completes. Poll with a bounded
    // timeout so a stuck bus doesn't hang the caller.
    for (uint8_t i = 0; i < 20; ++i) {
        if ((readReg(HTS221_REG_CTRL2) & HTS221_CTRL2_BOOT) == 0) {
            return;
        }
        delay(5);
    }
}

void HTS221::softReset() {
    reboot();
}

void HTS221::powerOn() {
    uint8_t ctrl1 = readReg(HTS221_REG_CTRL1);
    writeReg(HTS221_REG_CTRL1, ctrl1 | HTS221_CTRL1_PD | HTS221_CTRL1_BDU);
}

void HTS221::powerOff() {
    uint8_t ctrl1 = readReg(HTS221_REG_CTRL1);
    writeReg(HTS221_REG_CTRL1, ctrl1 & ~HTS221_CTRL1_PD);
}

bool HTS221::dataReady() {
    return (status() & (HTS221_STATUS_H_DA | HTS221_STATUS_T_DA)) ==
           (HTS221_STATUS_H_DA | HTS221_STATUS_T_DA);
}

bool HTS221::temperatureReady() {
    return (status() & HTS221_STATUS_T_DA) != 0;
}

bool HTS221::humidityReady() {
    return (status() & HTS221_STATUS_H_DA) != 0;
}

float HTS221::temperature() {
    return read().temperature;
}

float HTS221::humidity() {
    return read().humidity;
}

HTS221::ReadResult HTS221::read() {
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
    readRegs(HTS221_REG_HUMIDITY_OUT_L, humBytes, 2);
    readRegs(HTS221_REG_TEMP_OUT_L, tempBytes, 2);

    int16_t humRaw = static_cast<int16_t>(humBytes[0] | (humBytes[1] << 8));
    int16_t tempRaw = static_cast<int16_t>(tempBytes[0] | (tempBytes[1] << 8));

    return {computeTemperature(tempRaw), computeHumidity(humRaw)};
}

void HTS221::setContinuous(uint8_t odr) {
    uint8_t ctrl1 = HTS221_CTRL1_PD | HTS221_CTRL1_BDU | (odr & HTS221_CTRL1_ODR_MASK);
    writeReg(HTS221_REG_CTRL1, ctrl1);
}

void HTS221::triggerOneShot() {
    // Must be powered on at ODR=one-shot for the ONE_SHOT bit to matter.
    writeReg(HTS221_REG_CTRL1, HTS221_CTRL1_PD | HTS221_CTRL1_BDU | HTS221_ODR_ONE_SHOT);
    writeReg(HTS221_REG_CTRL2, HTS221_CTRL2_ONE_SHOT);
}

HTS221::ReadResult HTS221::readOneShot() {
    triggerOneShot();
    if (!waitForDataReady()) {
        return {NAN, NAN};
    }
    return read();
}

void HTS221::setTemperatureOffset(float offset) {
    _tempOffset = offset;
}

void HTS221::calibrateTemperature(float refLow, float measLow, float refHigh, float measHigh) {
    float span = measHigh - measLow;
    if (span == 0.0f) {
        _tempUserSlope = 1.0f;
        _tempUserIntercept = 0.0f;
        return;
    }
    _tempUserSlope = (refHigh - refLow) / span;
    _tempUserIntercept = refLow - _tempUserSlope * measLow;
}

uint8_t HTS221::status() {
    return readReg(HTS221_REG_STATUS);
}

uint8_t HTS221::readReg(uint8_t reg) {
    _wire->beginTransmission(_address);
    _wire->write(reg);
    _wire->endTransmission(false);
    _wire->requestFrom(_address, static_cast<uint8_t>(1));
    if (_wire->available()) {
        return static_cast<uint8_t>(_wire->read());
    }
    return 0;
}

void HTS221::writeReg(uint8_t reg, uint8_t value) {
    _wire->beginTransmission(_address);
    _wire->write(reg);
    _wire->write(value);
    _wire->endTransmission();
}

void HTS221::readRegs(uint8_t reg, uint8_t* buf, size_t len) {
    // Zero-init up front so a short bus read leaves defined bytes in buf
    // rather than whatever was on the stack — callers assume every slot
    // was written.
    for (size_t i = 0; i < len; ++i) {
        buf[i] = 0;
    }

    _wire->beginTransmission(_address);
    _wire->write(reg | HTS221_AUTO_INCREMENT);
    _wire->endTransmission(false);
    _wire->requestFrom(_address, static_cast<uint8_t>(len));
    for (size_t i = 0; i < len && _wire->available(); ++i) {
        buf[i] = static_cast<uint8_t>(_wire->read());
    }
}

void HTS221::loadCalibration() {
    uint8_t block[16];
    readRegs(HTS221_REG_H0_RH_X2, block, 16);

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

bool HTS221::isPoweredOn() {
    return (readReg(HTS221_REG_CTRL1) & HTS221_CTRL1_PD) != 0;
}

bool HTS221::waitForDataReady(uint32_t timeoutMs) {
    uint32_t start = millis();
    while (millis() - start < timeoutMs) {
        if (dataReady()) {
            return true;
        }
        delay(1);
    }
    return false;
}

float HTS221::computeTemperature(int16_t raw) {
    float factory = _tempIntercept + _tempSlope * static_cast<float>(raw);
    return _tempUserSlope * factory + _tempUserIntercept + _tempOffset;
}

float HTS221::computeHumidity(int16_t raw) {
    float value = _humIntercept + _humSlope * static_cast<float>(raw);
    if (value < 0.0f)
        return 0.0f;
    if (value > 100.0f)
        return 100.0f;
    return value;
}
