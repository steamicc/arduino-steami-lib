// SPDX-License-Identifier: GPL-3.0-or-later

#include "SteamiWsenHids.h"

#include <math.h>

SteamiWsenHids::SteamiWsenHids(TwoWire& wire, uint8_t address) : _wire(&wire), _address(address) {}

bool SteamiWsenHids::begin() {
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

uint8_t SteamiWsenHids::deviceId() {
    return readReg(WSEN_HIDS_REG_WHO_AM_I);
}

void SteamiWsenHids::reboot() {
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

void SteamiWsenHids::softReset() {
    reboot();
}

void SteamiWsenHids::powerOn() {
    uint8_t ctrl1 = readReg(WSEN_HIDS_REG_CTRL1);
    writeReg(WSEN_HIDS_REG_CTRL1, (ctrl1 & ~WSEN_HIDS_CTRL1_BDU) | WSEN_HIDS_CTRL1_PD | _bdu);
}

void SteamiWsenHids::powerOff() {
    uint8_t ctrl1 = readReg(WSEN_HIDS_REG_CTRL1);
    writeReg(WSEN_HIDS_REG_CTRL1, ctrl1 & ~WSEN_HIDS_CTRL1_PD);
}

bool SteamiWsenHids::dataReady() {
    return (status() & (WSEN_HIDS_STATUS_H_DA | WSEN_HIDS_STATUS_T_DA)) ==
           (WSEN_HIDS_STATUS_H_DA | WSEN_HIDS_STATUS_T_DA);
}

bool SteamiWsenHids::temperatureReady() {
    return (status() & WSEN_HIDS_STATUS_T_DA) != 0;
}

bool SteamiWsenHids::humidityReady() {
    return (status() & WSEN_HIDS_STATUS_H_DA) != 0;
}

float SteamiWsenHids::temperature() {
    return read().temperature;
}

float SteamiWsenHids::humidity() {
    return read().humidity;
}

SteamiWsenHids::ReadResult SteamiWsenHids::read() {
    if (!isContinuous()) {
        // Power-down, or PD = 1 with ODR = 00 (after powerOn() on a fresh
        // chip): no conversion is running, so the output registers are
        // stale. Bring the chip up in continuous mode at 12.5 Hz rather
        // than through CTRL2.ONE_SHOT, which latches at 1 and converts
        // nothing on the STeaMi V1a/V1b (steamicc/micropython-steami-lib#425).
        // 12.5 Hz keeps the first-sample latency under ~80 ms, and the
        // chip stays awake so the next reads are immediate.
        setContinuous(WSEN_HIDS_ODR_12_5_HZ);
        if (!waitForDataReady()) {
            // Device never reported fresh data — bus issue or missing
            // sensor. Surface the failure as NaN so silent stale
            // readings can't propagate; callers can check with isnan().
            return {NAN, NAN};
        }
    }
    return readOutputs();
}

SteamiWsenHids::ReadResult SteamiWsenHids::readOutputs() {
    uint8_t humBytes[2];
    uint8_t tempBytes[2];
    readRegs(WSEN_HIDS_REG_HUMIDITY_OUT_L, humBytes, 2);
    readRegs(WSEN_HIDS_REG_TEMP_OUT_L, tempBytes, 2);

    int16_t humRaw = static_cast<int16_t>(humBytes[0] | (humBytes[1] << 8));
    int16_t tempRaw = static_cast<int16_t>(tempBytes[0] | (tempBytes[1] << 8));

    return {computeTemperature(tempRaw), computeHumidity(humRaw)};
}

void SteamiWsenHids::setContinuous(uint8_t odr) {
    uint8_t ctrl1 = WSEN_HIDS_CTRL1_PD | _bdu | (odr & WSEN_HIDS_CTRL1_ODR_MASK);
    writeReg(WSEN_HIDS_REG_CTRL1, ctrl1);
}

void SteamiWsenHids::enableBdu(bool enabled) {
    _bdu = enabled ? WSEN_HIDS_CTRL1_BDU : 0;
    uint8_t ctrl1 = readReg(WSEN_HIDS_REG_CTRL1);
    writeReg(WSEN_HIDS_REG_CTRL1, (ctrl1 & ~WSEN_HIDS_CTRL1_BDU) | _bdu);
}

void SteamiWsenHids::setOneShotMode() {
    uint8_t ctrl1 = readReg(WSEN_HIDS_REG_CTRL1);
    ctrl1 &= static_cast<uint8_t>(~(WSEN_HIDS_CTRL1_ODR_MASK | WSEN_HIDS_CTRL1_BDU));
    writeReg(WSEN_HIDS_REG_CTRL1, ctrl1 | WSEN_HIDS_CTRL1_PD | _bdu | WSEN_HIDS_ODR_ONE_SHOT);
}

void SteamiWsenHids::enableHeater(bool enabled) {
    uint8_t ctrl2 = readReg(WSEN_HIDS_REG_CTRL2);
    if (enabled) {
        ctrl2 |= WSEN_HIDS_CTRL2_HEATER;
    } else {
        ctrl2 &= static_cast<uint8_t>(~WSEN_HIDS_CTRL2_HEATER);
    }
    writeReg(WSEN_HIDS_REG_CTRL2, ctrl2);
}

void SteamiWsenHids::triggerOneShot() {
    // Must be powered on at ODR=one-shot for the ONE_SHOT bit to matter.
    // Read-modify-write of CTRL2 keeps the heater as the caller set it.
    setOneShotMode();
    writeReg(WSEN_HIDS_REG_CTRL2, readReg(WSEN_HIDS_REG_CTRL2) | WSEN_HIDS_CTRL2_ONE_SHOT);
}

SteamiWsenHids::ReadResult SteamiWsenHids::readOneShot() {
    // One sample in continuous mode at 12.5 Hz, then CTRL1 back as it was
    // (power-down after begin()), also on timeout. CTRL2.ONE_SHOT is not
    // used: it converts nothing on the STeaMi V1a/V1b (#425). Same flow as
    // read_one_shot() in the MicroPython driver.
    uint8_t ctrl1 = readReg(WSEN_HIDS_REG_CTRL1);
    setContinuous(WSEN_HIDS_ODR_12_5_HZ);
    ReadResult result = {NAN, NAN};
    if (waitForDataReady()) {
        result = readOutputs();
    }
    writeReg(WSEN_HIDS_REG_CTRL1, ctrl1);
    return result;
}

void SteamiWsenHids::setAveraging(uint8_t humidityAvg, uint8_t temperatureAvg) {
    uint8_t value = static_cast<uint8_t>(humidityAvg & WSEN_HIDS_AV_CONF_AVGH_MASK) |
                    static_cast<uint8_t>((temperatureAvg << WSEN_HIDS_AV_CONF_AVGT_SHIFT) &
                                         WSEN_HIDS_AV_CONF_AVGT_MASK);
    writeReg(WSEN_HIDS_REG_AV_CONF, value);
}

void SteamiWsenHids::setTemperatureOffset(float offset) {
    _tempGain = 1.0f;
    _tempOffset = offset;
}

bool SteamiWsenHids::calibrateTemperature(float refLow, float measLow, float refHigh,
                                          float measHigh) {
    const float span = measHigh - measLow;
    if (span == 0.0f) {
        return false;
    }
    _tempGain = (refHigh - refLow) / span;
    _tempOffset = refLow - _tempGain * measLow;
    return true;
}

uint8_t SteamiWsenHids::status() {
    return readReg(WSEN_HIDS_REG_STATUS);
}

uint8_t SteamiWsenHids::readReg(uint8_t reg) {
    _wire->beginTransmission(_address);
    _wire->write(reg);
    _wire->endTransmission(false);
    _wire->requestFrom(_address, static_cast<uint8_t>(1));
    if (_wire->available()) {
        return static_cast<uint8_t>(_wire->read());
    }
    return 0;
}

void SteamiWsenHids::writeReg(uint8_t reg, uint8_t value) {
    _wire->beginTransmission(_address);
    _wire->write(reg);
    _wire->write(value);
    _wire->endTransmission();
}

void SteamiWsenHids::readRegs(uint8_t reg, uint8_t* buf, size_t len) {
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

void SteamiWsenHids::loadCalibration() {
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

bool SteamiWsenHids::isContinuous() {
    uint8_t ctrl1 = readReg(WSEN_HIDS_REG_CTRL1);
    return (ctrl1 & WSEN_HIDS_CTRL1_PD) != 0 && (ctrl1 & WSEN_HIDS_CTRL1_ODR_MASK) != 0;
}

bool SteamiWsenHids::waitForDataReady(uint32_t timeoutMs) {
    uint32_t start = millis();
    while (millis() - start < timeoutMs) {
        if (dataReady()) {
            return true;
        }
        delay(1);
    }
    return false;
}

float SteamiWsenHids::computeTemperature(int16_t raw) {
    float factory = _tempIntercept + _tempSlope * static_cast<float>(raw);
    return _tempGain * factory + _tempOffset;
}

float SteamiWsenHids::computeHumidity(int16_t raw) {
    float value = _humIntercept + _humSlope * static_cast<float>(raw);
    if (value < 0.0f)
        return 0.0f;
    if (value > 100.0f)
        return 100.0f;
    return value;
}
