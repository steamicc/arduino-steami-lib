// SPDX-License-Identifier: GPL-3.0-or-later

#include "BME280.h"

#include <math.h>

BME280::BME280(TwoWire& wire, uint8_t address) : _wire(&wire), _address(address) {}

bool BME280::begin() {
    _initialized = false;
    if (deviceId() != BME280_CHIP_ID_VALUE) {
        return false;
    }
    // The chip is not power-cycled by an MCU reset: wait for the NVM copy,
    // then force a known configuration so nothing is inherited from the
    // sketch that ran before.
    if (!waitBoot() || !loadCalibration()) {
        return false;
    }
    configureDefault();
    _initialized = true;
    return true;
}

uint8_t BME280::deviceId() {
    return readReg(BME280_REG_CHIP_ID);
}

void BME280::softReset() {
    writeReg(BME280_REG_SOFT_RESET, BME280_SOFT_RESET_CMD);
    delay(BME280_RESET_DELAY_MS);
    // A reset never makes a driver usable that begin() did not accept.
    bool ok = waitBoot() && loadCalibration();
    configureDefault();
    _initialized = _initialized && ok;
}

void BME280::powerOn() {
    uint8_t ctrl = readReg(BME280_REG_CTRL_MEAS);
    writeReg(BME280_REG_CTRL_MEAS, (ctrl & ~BME280_MODE_MASK) | BME280_MODE_NORMAL);
    _oneShotPending = false;
}

void BME280::powerOff() {
    uint8_t ctrl = readReg(BME280_REG_CTRL_MEAS);
    writeReg(BME280_REG_CTRL_MEAS, ctrl & ~BME280_MODE_MASK);
    _oneShotPending = false;
}

bool BME280::dataReady() {
    if (_oneShotPending) {
        return oneShotDone();
    }
    return conversionIdle();
}

bool BME280::temperatureReady() {
    return dataReady();
}

bool BME280::pressureReady() {
    return dataReady();
}

bool BME280::humidityReady() {
    return dataReady();
}

float BME280::temperature() {
    return read().temperature;
}

float BME280::humidity() {
    return read().humidity;
}

float BME280::pressureHpa() {
    return read().pressure;
}

BME280::ReadResult BME280::read() {
    if (!_initialized || !ensureData()) {
        return {NAN, NAN, NAN};
    }
    return readData();
}

void BME280::setContinuous(uint8_t standby) {
    setStandby(standby);
    powerOn();
}

void BME280::triggerOneShot() {
    _oneShotDurationMs = measurementTimeMs();
    uint8_t ctrl = readReg(BME280_REG_CTRL_MEAS);
    writeReg(BME280_REG_CTRL_MEAS, (ctrl & ~BME280_MODE_MASK) | BME280_MODE_FORCED);
    _oneShotStartMs = millis();
    _oneShotPending = true;
}

BME280::ReadResult BME280::readOneShot() {
    if (!_initialized) {
        return {NAN, NAN, NAN};
    }
    triggerOneShot();
    _oneShotPending = false;
    if (!waitMeasurement()) {
        return {NAN, NAN, NAN};
    }
    return readData();
}

void BME280::setOversampling(uint8_t temperature, uint8_t pressure, uint8_t humidity) {
    if (humidity != BME280_OSRS_KEEP) {
        writeReg(BME280_REG_CTRL_HUM, humidity & BME280_OSRS_MASK);
    }
    uint8_t ctrl = readReg(BME280_REG_CTRL_MEAS);
    if (temperature != BME280_OSRS_KEEP) {
        ctrl = (ctrl & ~(BME280_OSRS_MASK << BME280_OSRS_T_SHIFT)) |
               ((temperature & BME280_OSRS_MASK) << BME280_OSRS_T_SHIFT);
    }
    if (pressure != BME280_OSRS_KEEP) {
        ctrl = (ctrl & ~(BME280_OSRS_MASK << BME280_OSRS_P_SHIFT)) |
               ((pressure & BME280_OSRS_MASK) << BME280_OSRS_P_SHIFT);
    }
    // CTRL_HUM only takes effect once CTRL_MEAS is written (datasheet
    // section 5.4.3), so CTRL_MEAS is always rewritten.
    writeReg(BME280_REG_CTRL_MEAS, ctrl);
}

void BME280::setIirFilter(uint8_t coefficient) {
    writeConfig(BME280_FILTER_MASK, BME280_FILTER_SHIFT, coefficient);
}

void BME280::setStandby(uint8_t standby) {
    writeConfig(BME280_STANDBY_MASK, BME280_STANDBY_SHIFT, standby);
}

uint32_t BME280::measurementTimeMs() {
    uint8_t ctrl = readReg(BME280_REG_CTRL_MEAS);
    uint8_t osrsT = (ctrl >> BME280_OSRS_T_SHIFT) & BME280_OSRS_MASK;
    uint8_t osrsP = (ctrl >> BME280_OSRS_P_SHIFT) & BME280_OSRS_MASK;
    uint8_t osrsH = readReg(BME280_REG_CTRL_HUM) & BME280_OSRS_MASK;

    // Datasheet section 9.1, in microseconds. Codes above x16 behave as x16.
    auto samples = [](uint8_t osrs) -> uint32_t {
        return 1UL << ((osrs > BME280_OSRS_X16 ? BME280_OSRS_X16 : osrs) - 1);
    };
    uint32_t us = 1250;
    if (osrsT != BME280_OSRS_SKIP) {
        us += 2300 * samples(osrsT);
    }
    if (osrsP != BME280_OSRS_SKIP) {
        us += 2300 * samples(osrsP) + 575;
    }
    if (osrsH != BME280_OSRS_SKIP) {
        us += 2300 * samples(osrsH) + 575;
    }
    return (us + 999) / 1000;
}

float BME280::altitude(float seaLevelHpa) {
    return altitudeFromPressure(pressureHpa(), seaLevelHpa);
}

float BME280::dewPoint() {
    ReadResult r = read();
    return dewPointFrom(r.temperature, r.humidity);
}

float BME280::altitudeFromPressure(float pressureHpa, float seaLevelHpa) {
    if (isnan(pressureHpa) || seaLevelHpa <= 0.0f) {
        return NAN;
    }
    return 44330.0f * (1.0f - powf(pressureHpa / seaLevelHpa, 0.1903f));
}

float BME280::dewPointFrom(float temperatureC, float humidityPct) {
    constexpr float A = 17.625f;
    constexpr float B = 243.04f;
    constexpr float MIN_HUMIDITY = 0.01f;

    // logf() needs a strictly positive argument — clamp dry readings.
    float rh = humidityPct < MIN_HUMIDITY ? MIN_HUMIDITY : humidityPct;
    float gamma = logf(rh / 100.0f) + (A * temperatureC) / (B + temperatureC);
    return (B * gamma) / (A - gamma);
}

void BME280::setTemperatureOffset(float offset) {
    _tempOffset = offset;
}

void BME280::calibrateTemperature(float refLow, float measLow, float refHigh, float measHigh) {
    float span = measHigh - measLow;
    if (span == 0.0f) {
        _tempUserSlope = 1.0f;
        _tempUserIntercept = 0.0f;
        return;
    }
    _tempUserSlope = (refHigh - refLow) / span;
    _tempUserIntercept = refLow - _tempUserSlope * measLow;
}

uint8_t BME280::readReg(uint8_t reg) {
    uint8_t value = 0;
    readRegs(reg, &value, 1);
    return value;
}

void BME280::writeReg(uint8_t reg, uint8_t value) {
    _wire->beginTransmission(_address);
    _wire->write(reg);
    _wire->write(value);
    _wire->endTransmission();
}

bool BME280::readRegs(uint8_t reg, uint8_t* buf, size_t len) {
    // Zero-init up front so a short bus read leaves defined bytes in buf.
    for (size_t i = 0; i < len; ++i) {
        buf[i] = 0;
    }

    _wire->beginTransmission(_address);
    _wire->write(reg);
    if (_wire->endTransmission(false) != 0) {
        return false;
    }
    if (_wire->requestFrom(_address, static_cast<uint8_t>(len)) != len) {
        return false;
    }
    size_t received = 0;
    while (received < len && _wire->available()) {
        buf[received++] = static_cast<uint8_t>(_wire->read());
    }
    return received == len;
}

bool BME280::readStatus(uint8_t& value) {
    return readRegs(BME280_REG_STATUS, &value, 1);
}

bool BME280::waitBoot() {
    for (uint8_t waited = 0; waited <= BME280_BOOT_TIMEOUT_MS; waited += 5) {
        uint8_t st;
        if (readStatus(st) && (st & BME280_STATUS_IM_UPDATE) == 0) {
            return true;
        }
        delay(5);
    }
    return false;
}

bool BME280::loadCalibration() {
    uint8_t tp[BME280_CALIB_TP_SIZE];
    uint8_t hum[BME280_CALIB_H_SIZE];
    if (!readRegs(BME280_REG_CALIB_TEMP_PRESS, tp, sizeof(tp)) ||
        !readRegs(BME280_REG_CALIB_HUM, hum, sizeof(hum))) {
        return false;
    }
    auto u16 = [&tp](size_t i) -> uint16_t {
        return static_cast<uint16_t>(tp[i] | (tp[i + 1] << 8));
    };
    _digT1 = u16(0);
    _digT2 = static_cast<int16_t>(u16(2));
    _digT3 = static_cast<int16_t>(u16(4));
    _digP1 = u16(6);
    _digP2 = static_cast<int16_t>(u16(8));
    _digP3 = static_cast<int16_t>(u16(10));
    _digP4 = static_cast<int16_t>(u16(12));
    _digP5 = static_cast<int16_t>(u16(14));
    _digP6 = static_cast<int16_t>(u16(16));
    _digP7 = static_cast<int16_t>(u16(18));
    _digP8 = static_cast<int16_t>(u16(20));
    _digP9 = static_cast<int16_t>(u16(22));
    // tp[24] is reserved.
    _digH1 = tp[25];

    _digH2 = static_cast<int16_t>(hum[0] | (hum[1] << 8));
    _digH3 = hum[2];
    // H4 and H5 are 12-bit signed values that share register 0xE5.
    int16_t h4 = static_cast<int16_t>((hum[3] << 4) | (hum[4] & 0x0F));
    int16_t h5 = static_cast<int16_t>((hum[4] >> 4) | (hum[5] << 4));
    _digH4 = h4 > 2047 ? h4 - 4096 : h4;
    _digH5 = h5 > 2047 ? h5 - 4096 : h5;
    _digH6 = static_cast<int8_t>(hum[6]);

    _tFine = 0;
    return true;
}

void BME280::configureDefault() {
    // Sleep first: CONFIG writes can be ignored while the chip is converting.
    writeReg(BME280_REG_CTRL_MEAS, BME280_MODE_SLEEP);
    writeReg(BME280_REG_CONFIG, 0x00);  // standby 0.5 ms, IIR filter off
    // CTRL_HUM is only latched by the CTRL_MEAS write that follows.
    writeReg(BME280_REG_CTRL_HUM, BME280_OSRS_X1);
    writeReg(BME280_REG_CTRL_MEAS, (BME280_OSRS_X1 << BME280_OSRS_T_SHIFT) |
                                       (BME280_OSRS_X1 << BME280_OSRS_P_SHIFT) | BME280_MODE_SLEEP);
    _oneShotPending = false;
}

void BME280::writeConfig(uint8_t mask, uint8_t shift, uint8_t value) {
    // The chip may ignore CONFIG writes in normal mode: drop to sleep for
    // the write and restore the previous CTRL_MEAS afterwards.
    uint8_t ctrl = readReg(BME280_REG_CTRL_MEAS);
    bool normal = (ctrl & BME280_MODE_MASK) == BME280_MODE_NORMAL;
    if (normal) {
        writeReg(BME280_REG_CTRL_MEAS, ctrl & ~BME280_MODE_MASK);
    }
    uint8_t config = readReg(BME280_REG_CONFIG);
    config = (config & ~(mask << shift)) | ((value & mask) << shift);
    writeReg(BME280_REG_CONFIG, config);
    if (normal) {
        writeReg(BME280_REG_CTRL_MEAS, ctrl);
    }
}

bool BME280::isSleeping() {
    return (readReg(BME280_REG_CTRL_MEAS) & BME280_MODE_MASK) == BME280_MODE_SLEEP;
}

bool BME280::conversionIdle() {
    // A failed STATUS read is not "idle" (0x00 would read as idle): the
    // caller's poll then times out and the read returns NAN.
    uint8_t st;
    return readStatus(st) && (st & BME280_STATUS_MEASURING) == 0;
}

bool BME280::oneShotDone() {
    // MEASURING may lag the trigger, so an idle STATUS alone does not mean
    // the forced conversion is over. Also require the nominal duration to
    // have elapsed and the mode bits to be back to sleep: the chip clears
    // them by itself at the end of a forced conversion.
    if (millis() - _oneShotStartMs < _oneShotDurationMs) {
        return false;
    }
    return conversionIdle() && isSleeping();
}

bool BME280::waitMeasurement(bool settle) {
    // Right after a trigger, let the conversion run for its nominal
    // duration before polling (the MEASURING flag may lag the trigger).
    // The poll has a 2x margin so slow oversampling settings never time
    // out spuriously.
    uint32_t nominal = measurementTimeMs();
    if (settle) {
        delay(nominal);
    }
    uint32_t timeout = nominal * 2 + 10;
    uint32_t start = millis();
    while (millis() - start < timeout) {
        if (dataReady()) {
            return true;
        }
        delay(1);
    }
    return false;
}

bool BME280::ensureData() {
    if (_oneShotPending) {
        // Sleep through what is left of the nominal conversion time, then
        // poll dataReady(), which still applies the one-shot conditions.
        uint32_t elapsed = millis() - _oneShotStartMs;
        if (elapsed < _oneShotDurationMs) {
            delay(_oneShotDurationMs - elapsed);
        }
        bool ok = waitMeasurement(false);
        _oneShotPending = false;
        return ok;
    }
    if (isSleeping()) {
        triggerOneShot();
        _oneShotPending = false;
        return waitMeasurement();
    }
    return true;
}

BME280::ReadResult BME280::readData() {
    uint8_t d[BME280_DATA_BLOCK_SIZE];
    if (!readRegs(BME280_REG_DATA_START, d, sizeof(d))) {
        return {NAN, NAN, NAN};
    }

    int32_t rawPress =
        (static_cast<int32_t>(d[0]) << 12) | (static_cast<int32_t>(d[1]) << 4) | (d[2] >> 4);
    int32_t rawTemp =
        (static_cast<int32_t>(d[3]) << 12) | (static_cast<int32_t>(d[4]) << 4) | (d[5] >> 4);
    int32_t rawHum = (static_cast<int32_t>(d[6]) << 8) | d[7];

    ReadResult result = {NAN, NAN, NAN};
    // Without a temperature there is no t_fine: nothing can be compensated.
    if (rawTemp == BME280_RAW_SKIPPED_20BIT) {
        return result;
    }

    float tempC = static_cast<float>(compensateTemperature(rawTemp)) / 100.0f;
    result.temperature = _tempUserSlope * tempC + _tempUserIntercept + _tempOffset;
    if (rawPress != BME280_RAW_SKIPPED_20BIT) {
        result.pressure = static_cast<float>(compensatePressure(rawPress)) / 25600.0f;
    }
    if (rawHum != BME280_RAW_SKIPPED_16BIT) {
        result.humidity = static_cast<float>(compensateHumidity(rawHum)) / 1024.0f;
    }
    return result;
}

// Compensation formulas: Bosch BME280 datasheet, section 4.2.3 (integer
// versions, 64-bit for the pressure), as in the MicroPython driver.

// Returns the temperature in hundredths of a degree Celsius and updates _tFine.
int32_t BME280::compensateTemperature(int32_t rawTemp) {
    int32_t t1 = static_cast<int32_t>(_digT1);
    int32_t var1 = (((rawTemp >> 3) - (t1 << 1)) * static_cast<int32_t>(_digT2)) >> 11;
    int32_t diff = (rawTemp >> 4) - t1;
    int32_t var2 = (((diff * diff) >> 12) * static_cast<int32_t>(_digT3)) >> 14;
    _tFine = var1 + var2;
    return (_tFine * 5 + 128) >> 8;
}

// Returns the pressure in Pa as an unsigned Q24.8 fixed-point value.
uint32_t BME280::compensatePressure(int32_t rawPress) {
    int64_t var1 = static_cast<int64_t>(_tFine) - 128000;
    int64_t var2 = var1 * var1 * _digP6;
    var2 = var2 + ((var1 * _digP5) << 17);
    var2 = var2 + (static_cast<int64_t>(_digP4) << 35);
    var1 = ((var1 * var1 * _digP3) >> 8) + ((var1 * _digP2) << 12);
    var1 = (((static_cast<int64_t>(1) << 47) + var1) * _digP1) >> 33;
    if (var1 == 0) {
        return 0;  // avoid a division by zero with blank calibration data
    }
    int64_t p = 1048576 - rawPress;
    p = (((p << 31) - var2) * 3125) / var1;
    var1 = (static_cast<int64_t>(_digP9) * (p >> 13) * (p >> 13)) >> 25;
    var2 = (static_cast<int64_t>(_digP8) * p) >> 19;
    p = ((p + var1 + var2) >> 8) + (static_cast<int64_t>(_digP7) << 4);
    return static_cast<uint32_t>(p);
}

// Returns the relative humidity in %RH as an unsigned Q22.10 fixed-point value.
uint32_t BME280::compensateHumidity(int32_t rawHum) {
    int32_t v = _tFine - 76800;
    v = (((((rawHum << 14) - (static_cast<int32_t>(_digH4) << 20) -
            (static_cast<int32_t>(_digH5) * v)) +
           16384) >>
          15) *
         (((((((v * static_cast<int32_t>(_digH6)) >> 10) *
              (((v * static_cast<int32_t>(_digH3)) >> 11) + 32768)) >>
             10) +
            2097152) *
               static_cast<int32_t>(_digH2) +
           8192) >>
          14));
    v = v - (((((v >> 15) * (v >> 15)) >> 7) * static_cast<int32_t>(_digH1)) >> 4);
    v = v < 0 ? 0 : v;
    v = v > 419430400 ? 419430400 : v;
    return static_cast<uint32_t>(v >> 12);
}
