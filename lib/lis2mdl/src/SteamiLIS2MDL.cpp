// SPDX-License-Identifier: GPL-3.0-or-later

#include "SteamiLIS2MDL.h"

#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#ifndef radians
#define radians(deg) ((deg) * M_PI / 180.0f)
#endif
#ifndef degrees
#define degrees(rad) ((rad) * 180.0f / M_PI)
#endif

SteamiLIS2MDL::SteamiLIS2MDL(TwoWire& wire, uint8_t address, uint8_t odrHz, bool tempComp,
                             bool lowPower, bool drdyEnable)
    : _wire(&wire),
      _address(address),
      _odrHz(odrHz),
      _tempComp(tempComp),
      _lowPower(lowPower),
      _drdyEnable(drdyEnable) {}

bool SteamiLIS2MDL::begin() {
    if (deviceId() != LIS2MDL_WHO_AM_I_VAL) {
        return false;
    }

    writeReg(LIS2MDL_CFG_REG_A, 0x20);
    delay(10);

    uint8_t odrBits;
    switch (_odrHz) {
        case 20:
            odrBits = 0b01;
            break;
        case 50:
            odrBits = 0b10;
            break;
        case 100:
            odrBits = 0b11;
            break;
        default:
            odrBits = 0b00;
            break;
    }

    uint8_t comp = _tempComp ? 1 : 0;
    uint8_t lp = _lowPower ? 1 : 0;
    uint8_t cfgA = (comp << 7) | (lp << 4) | (odrBits << 2);
    writeReg(LIS2MDL_CFG_REG_A, cfgA);

    writeReg(LIS2MDL_CFG_REG_B, 0x00);

    uint8_t cfgC = 0x10 | (_drdyEnable ? 0x01 : 0x00);
    writeReg(LIS2MDL_CFG_REG_C, cfgC);

    return true;
}

//--------------------------------------------------------
// -------------------- SET functions --------------------
//--------------------------------------------------------

void SteamiLIS2MDL::setMode(const char* mode) {
    uint8_t md = 0b00;
    if (strcmp(mode, "single") == 0) {
        md = 0b01;
    } else if (strcmp(mode, "powerdown") == 0) {
        md = 0b11;
    }

    uint8_t reg = readReg(LIS2MDL_CFG_REG_A);
    reg = (reg & ~0b11) | md;
    writeReg(LIS2MDL_CFG_REG_A, reg);
}

void SteamiLIS2MDL::setOdr(int hz) {
    uint8_t odrBits;
    switch (hz) {
        case 10:
            odrBits = 0b00;
            break;
        case 20:
            odrBits = 0b01;
            break;
        case 50:
            odrBits = 0b10;
            break;
        case 100:
            odrBits = 0b11;
            break;
        default:
            odrBits = 0b00;
            break;
    }

    uint8_t reg = readReg(LIS2MDL_CFG_REG_A);
    reg = (reg & ~(0b11 << 2)) | (odrBits << 2);
    writeReg(LIS2MDL_CFG_REG_A, reg);
}

void SteamiLIS2MDL::setContinuous(uint8_t hz) {
    setOdr(hz);
    setMode("continuous");
}

void SteamiLIS2MDL::triggerOneShot() {
    setMode("single");
}

MagneticField SteamiLIS2MDL::readOneShot() {
    triggerOneShot();
    for (int i = 0; i < 50; i++) {
        if (dataReady())
            return magneticField();
        delay(2);
    }
    return {0, 0, 0};
}

void SteamiLIS2MDL::setLowPower(bool enabled) {
    uint8_t reg = readReg(LIS2MDL_CFG_REG_A);
    if (enabled) {
        reg |= 1 << 4;
    } else {
        reg &= ~(1 << 4);
    }
    writeReg(LIS2MDL_CFG_REG_A, reg);
}

void SteamiLIS2MDL::setLowPass(bool enabled) {
    uint8_t reg = readReg(LIS2MDL_CFG_REG_B);
    if (enabled) {
        reg |= 1 << 0;
    } else {
        reg &= ~(1 << 0);
    }
    writeReg(LIS2MDL_CFG_REG_B, reg);
}

void SteamiLIS2MDL::setOffsetCancellation(bool enabled, bool oneShot) {
    uint8_t reg = readReg(LIS2MDL_CFG_REG_B);
    if (enabled) {
        reg |= 1 << 1;
    } else {
        reg &= ~(1 << 1);
    }
    if (oneShot) {
        reg |= 1 << 2;
    } else {
        reg &= ~(1 << 2);
    }
    writeReg(LIS2MDL_CFG_REG_B, reg);
}

void SteamiLIS2MDL::setBdu(bool enabled) {
    uint8_t reg = readReg(LIS2MDL_CFG_REG_C);
    if (enabled) {
        reg |= 1 << 4;
    } else {
        reg &= ~(1 << 4);
    }
    writeReg(LIS2MDL_CFG_REG_C, reg);
}

void SteamiLIS2MDL::setEndianness(bool bigEndian) {
    uint8_t reg = readReg(LIS2MDL_CFG_REG_C);
    if (bigEndian) {
        reg |= 1 << 3;
    } else {
        reg &= ~(1 << 3);
    }
    writeReg(LIS2MDL_CFG_REG_C, reg);
}

void SteamiLIS2MDL::useSpi4wire(bool enable) {
    uint8_t reg = readReg(LIS2MDL_CFG_REG_C);
    if (enable) {
        reg |= 1 << 2;
    } else {
        reg &= ~(1 << 2);
    }
    writeReg(LIS2MDL_CFG_REG_C, reg);
}

void SteamiLIS2MDL::setHeadingOffset(float deg) {
    _headingOffsetDeg = deg;
}

void SteamiLIS2MDL::setDeclination(float deg) {
    _declinationDeg = deg;
}

void SteamiLIS2MDL::writeReg(uint8_t reg, uint8_t data) {
    _writeBuffer[0] = data;
    _wire->beginTransmission(_address);
    _wire->write(reg);
    _wire->write(_writeBuffer[0]);
    _wire->endTransmission();
}

void SteamiLIS2MDL::write16(uint8_t regL, int16_t value) {
    _wire->beginTransmission(_address);
    _wire->write(regL);
    _wire->write(value & 0xFF);
    _wire->write((value >> 8) & 0xFF);
    _wire->endTransmission();
}

void SteamiLIS2MDL::setHwOffsets(int16_t x, int16_t y, int16_t z) {
    write16(LIS2MDL_OFFSET_X_REG_L, x);
    write16(LIS2MDL_OFFSET_Y_REG_L, y);
    write16(LIS2MDL_OFFSET_Z_REG_L, z);
}

//--------------------------------------------------------
// -------------------- READ functions --------------------
//--------------------------------------------------------

bool SteamiLIS2MDL::ensureData() {
    // Trigger a single conversion if the sensor is in idle mode.
    if (isIdle()) {
        setMode("single");
        for (int i = 0; i < 50; i++) {
            if (dataReady())
                return true;
            delay(2);
        }
        return false;
    }
    return true;
}

MagneticField SteamiLIS2MDL::magneticFieldRaw() {
    // Reads the raw magnetic field (LSB). Same as magnetic_field(), but more explicit.
    return magneticField();
}

uint8_t SteamiLIS2MDL::status() {
    return readReg(LIS2MDL_STATUS_REG);
}

bool SteamiLIS2MDL::dataReady() {
    // True if a new XYZ triplet is ready (Zyxda bit).
    return (status() & (1 << 3)) != 0;
}

uint8_t SteamiLIS2MDL::readIntSource() {
    // Reads INT_SOURCE_REG (0x64): source of the interrupt.
    return readReg(LIS2MDL_INT_SOURCE_REG);
}

uint8_t SteamiLIS2MDL::readReg(uint8_t reg) {
    _wire->beginTransmission(_address);
    _wire->write(reg);
    _wire->endTransmission(false);
    _wire->requestFrom(_address, (uint8_t)1);
    if (_wire->available())
        return _wire->read();
    return 0;
}

MagneticFieldUt SteamiLIS2MDL::magneticFieldUt() {
    // Reads the magnetic field in µT, uncalibrated (simple conversion from LSB). NAN on a bus
    // error.
    MagneticField raw;
    if (!magneticField(raw)) {
        return {NAN, NAN, NAN};
    }
    MagneticFieldUt f;
    f.x = raw.x * MAG_LSB_TO_UT;
    f.y = raw.y * MAG_LSB_TO_UT;
    f.z = raw.z * MAG_LSB_TO_UT;
    return f;
}

CalibratedField SteamiLIS2MDL::calibratedField() {
    // Reads the calibrated field (offset/scale per axis), normalized (unitless, ~circle in XY).
    // NAN on a bus error.
    MagneticField raw;
    if (!magneticField(raw)) {
        return {NAN, NAN, NAN};
    }
    CalibratedField f;
    f.x = (raw.x - xOff) / (xScale != 0 ? xScale : 1.0f);
    f.y = (raw.y - yOff) / (yScale != 0 ? yScale : 1.0f);
    f.z = (raw.z - zOff) / (zScale != 0 ? zScale : 1.0f);
    return f;
}

float SteamiLIS2MDL::magnitudeUt() {
    // Total magnetic field strength (µT). NAN on a bus error.
    MagneticFieldUt f = magneticFieldUt();
    return sqrt(f.x * f.x + f.y * f.y + f.z * f.z);
}

int16_t SteamiLIS2MDL::toInt16(uint16_t v) {
    // Convert an unsigned 16-bit value to a signed 16-bit value.
    return (v & 0x8000) ? (int16_t)(v - 0x10000) : (int16_t)v;
}

MagneticField SteamiLIS2MDL::magneticField() {
    // Read the raw magnetic field data (X, Y, Z) from the sensor; {0, 0, 0} on failure.
    MagneticField f;
    magneticField(f);
    return f;
}

bool SteamiLIS2MDL::magneticField(MagneticField& value) {
    // X, Y and Z in one burst from OUTX_L (the address auto-increments), with BDU set by
    // begin(): one transaction on the shared bus, and a bus error or a data-ready timeout is
    // reported instead of being read as a plausible field.
    value = {0, 0, 0};
    uint8_t buf[6];
    if (!ensureData() || !readBytes(LIS2MDL_OUTX_L_REG, buf, sizeof(buf))) {
        return false;
    }
    value.x = toInt16(buf[0] | ((uint16_t)buf[1] << 8));
    value.y = toInt16(buf[2] | ((uint16_t)buf[3] << 8));
    value.z = toInt16(buf[4] | ((uint16_t)buf[5] << 8));
    return true;
}

bool SteamiLIS2MDL::readBytes(uint8_t reg, uint8_t* buffer, uint8_t length) {
    // Consecutive registers in one transaction; false when the chip does not answer.
    _wire->beginTransmission(_address);
    _wire->write(reg);
    if (_wire->endTransmission(false) != 0) {
        return false;
    }
    if (_wire->requestFrom(_address, length) != length) {
        while (_wire->available()) {
            _wire->read();
        }
        return false;
    }
    for (uint8_t i = 0; i < length; i++) {
        if (!_wire->available()) {
            return false;
        }
        buffer[i] = static_cast<uint8_t>(_wire->read());
    }
    return true;
}

int16_t SteamiLIS2MDL::readTemperatureRaw() {
    // Reads the raw temperature (LSB), 8 LSB/°C, absolute offset not guaranteed.
    ensureData();
    uint16_t lo = readReg(LIS2MDL_TEMP_OUT_L_REG);
    uint16_t hi = readReg(LIS2MDL_TEMP_OUT_H_REG);
    return toInt16((hi << 8) | lo);
}

float SteamiLIS2MDL::temperature() {
    /* Temperature in °C (8 LSB/°C + empirical offset).

        The LIS2MDL temperature sensor has no guaranteed absolute zero
        point (see datasheet Table 4).  The offset defaults to 25 °C
        based on empirical observation (confirmed by Zephyr RTOS
        PR #35912).  Use ``setTemperatureOffset()`` or ``calibrateTemperature()``
        to calibrate against a reference thermometer.
        */
    float factory =
        _tempBaseOffset + static_cast<float>(readTemperatureRaw()) / LIS2MDL_TEMP_SENSITIVITY;
    return _tempGain * factory + _tempOffset;
}

void SteamiLIS2MDL::setTemperatureOffset(float offset) {
    /*Set a temperature offset in °C (gain is reset to 1.0).

        Replaces any two-point correction set by calibrateTemperature().

        Args:
            offset: offset value in degrees Celsius.*/
    _tempGain = 1.0f;
    _tempOffset = offset;
}

void SteamiLIS2MDL::setTempOffset(float offset) {
    // Alias of setTemperatureOffset(), the name shared by every
    // temperature-capable driver (see CONTRIBUTING.md).
    setTemperatureOffset(offset);
}

bool SteamiLIS2MDL::calibrateTemperature(float refLow, float measuredLow, float refHigh,
                                         float measuredHigh) {
    /*Two-point calibration from reference measurements.

        Computes gain and offset so that the sensor reading is adjusted
        to match reference values at two temperature points.

        Args:
            ref_low: reference temperature at the low point (°C).
            measured_low: sensor reading at the low point (°C).
            ref_high: reference temperature at the high point (°C).
            measured_high: sensor reading at the high point (°C).*/
    float delta = measuredHigh - measuredLow;
    if (delta == 0.0f)
        return false;
    _tempGain = (refHigh - refLow) / delta;
    _tempOffset = refLow - _tempGain * measuredLow;
    return true;
}

uint8_t SteamiLIS2MDL::deviceId() {
    // Reads WHO_AM_I (should be 0x40).
    return readReg(LIS2MDL_WHO_AM_I);
}

int16_t SteamiLIS2MDL::read16(uint8_t reg) {
    // Reads a signed 16-bit value from a _L (LSB) register.
    uint16_t lo = readReg(reg);
    uint16_t hi = readReg(reg + 1);
    return toInt16((hi << 8) | lo);
}

HwOffsets SteamiLIS2MDL::readHwOffsets() {
    // Reads the hardware offsets (OFFSET_* registers).
    HwOffsets o;
    o.x = read16(LIS2MDL_OFFSET_X_REG_L);
    o.y = read16(LIS2MDL_OFFSET_Y_REG_L);
    o.z = read16(LIS2MDL_OFFSET_Z_REG_L);
    return o;
}

void SteamiLIS2MDL::readRegisters(uint8_t startAddr, uint8_t length, uint8_t* buffer) {
    // Dump of consecutive registers (useful for debugging).
    for (uint8_t i = 0; i < length; i++) {
        buffer[i] = 0;
    }
    _wire->beginTransmission(_address);
    _wire->write(startAddr);
    _wire->endTransmission(false);
    _wire->requestFrom(_address, length);
    for (uint8_t i = 0; i < length && _wire->available(); i++) {
        buffer[i] = _wire->read();
    }
}

ReadAll SteamiLIS2MDL::readAll() {
    // Grouped reading useful for debug & logs.
    ReadAll r;
    r.raw = magneticFieldRaw();
    r.ut = magneticFieldUt();
    r.cal = calibratedField();
    r.tempC = temperature();
    r.status = status();
    return r;
}

bool SteamiLIS2MDL::isIdle() {
    return (readReg(LIS2MDL_CFG_REG_A) & 0b11) == 0b11;
}

//--------------------------------------------------------
// -------------------- CALIBRATIONS ---------------------
//--------------------------------------------------------

void SteamiLIS2MDL::setCalibrateStep(float xoff, float yoff, float zoff, float xscale, float yscale,
                                     float zscale) {
    // Set the calibration offsets and scales manually.
    xOff = xoff;
    yOff = yoff;
    zOff = zoff;
    xScale = xscale;
    yScale = yscale;
    zScale = zscale;
}

bool SteamiLIS2MDL::calibrateMinmax2d(uint16_t samples, uint16_t delayMs) {
    /*MIN/MAX calibration while flat (XY only).
        Slowly rotate the board FLAT during acquisition.
        Updates x_off, y_off, x_scale, y_scale (leaves Z unchanged).
        All or nothing: false, with the calibration unchanged, when samples is 0 or a read
        fails (MicroPython raises OSError out of the loop at the same point).*/

    float xmin, xmax, ymin, ymax, zmin, zmax;
    if (!sampleMinmax(samples, delayMs, xmin, xmax, ymin, ymax, zmin, zmax)) {
        return false;
    }

    xOff = (xmin + xmax) / 2.0f;
    yOff = (ymin + ymax) / 2.0f;
    xScale = (xmax - xmin) / 2.0f != 0.0f ? (xmax - xmin) / 2.0f : 1.0f;
    yScale = (ymax - ymin) / 2.0f != 0.0f ? (ymax - ymin) / 2.0f : 1.0f;

    float avg = (xScale + yScale) / 2.0f;
    xScale = (xScale != 0.0f) ? avg : 1.0f;
    yScale = (yScale != 0.0f) ? avg : 1.0f;
    return true;
}

bool SteamiLIS2MDL::calibrateMinmax3d(uint16_t samples, uint16_t delayMs) {
    /*MIN/MAX calibration on 3 axes (rotate the board in ALL directions).
        Updates offsets + scales for X, Y, Z.
        All or nothing: false, with the calibration unchanged, when samples is 0 or a read
        fails (MicroPython raises OSError out of the loop at the same point).*/

    float xmin, xmax, ymin, ymax, zmin, zmax;
    if (!sampleMinmax(samples, delayMs, xmin, xmax, ymin, ymax, zmin, zmax)) {
        return false;
    }

    xOff = (xmin + xmax) / 2.0f;
    yOff = (ymin + ymax) / 2.0f;
    zOff = (zmin + zmax) / 2.0f;
    xScale = (xmax - xmin) / 2.0f != 0.0f ? (xmax - xmin) / 2.0f : 1.0f;
    yScale = (ymax - ymin) / 2.0f != 0.0f ? (ymax - ymin) / 2.0f : 1.0f;
    zScale = (zmax - zmin) / 2.0f != 0.0f ? (zmax - zmin) / 2.0f : 1.0f;
    return true;
}

bool SteamiLIS2MDL::sampleMinmax(uint16_t samples, uint16_t delayMs, float& xmin, float& xmax,
                                 float& ymin, float& ymax, float& zmin, float& zmax) {
    // Min/max of `samples` raw readings. Stops at the first failed read: a calibration
    // computed from the samples that happened to succeed could rest on a single one.
    if (samples == 0) {
        return false;
    }
    xmin = ymin = zmin = 1e9f;
    xmax = ymax = zmax = -1e9f;
    for (uint16_t i = 0; i < samples; i++) {
        MagneticField f;
        if (!magneticField(f)) {
            return false;
        }
        xmin = f.x < xmin ? f.x : xmin;
        xmax = f.x > xmax ? f.x : xmax;
        ymin = f.y < ymin ? f.y : ymin;
        ymax = f.y > ymax ? f.y : ymax;
        zmin = f.z < zmin ? f.z : zmin;
        zmax = f.z > zmax ? f.z : zmax;
        delay(delayMs);
    }
    return true;
}

CalibratedField SteamiLIS2MDL::calibrateApply(float x, float y, float z) {
    /*Applies the current calibration (offset + scale per axis).
        Returns normalized ~unitless values.*/
    CalibratedField f;
    f.x = (x - xOff) / (xScale != 0.0f ? xScale : 1.0f);
    f.y = (y - yOff) / (yScale != 0.0f ? yScale : 1.0f);
    f.z = (z - zOff) / (zScale != 0.0f ? zScale : 1.0f);
    return f;
}

CalibrationQuality SteamiLIS2MDL::calibrateQuality(uint16_t samplesCheck, uint16_t delayMs) {
    /* Evaluates the quality of the current calibration over a short sample.
        Returns a dict with useful metrics: center (mean), anisotropy, XY radius dispersion.
        (Move the board a bit while flat during the measurement.)*/

    static constexpr uint16_t MAX_SAMPLES = 200;
    if (samplesCheck == 0) {
        return {0, 0, 0, 0, 0, 0, 0, 0, 0};
    }
    if (samplesCheck > MAX_SAMPLES) {
        samplesCheck = MAX_SAMPLES;
    }
    float xs[MAX_SAMPLES], ys[MAX_SAMPLES], zs[MAX_SAMPLES];
    for (uint16_t i = 0; i < samplesCheck; i++) {
        CalibratedField cal = calibratedField();
        xs[i] = cal.x;
        ys[i] = cal.y;
        zs[i] = cal.z;
        delay(delayMs);
    }

    float mx = 0.0f, my = 0.0f, mz = 0.0f;
    for (uint16_t i = 0; i < samplesCheck; i++) {
        mx += xs[i];
        my += ys[i];
        mz += zs[i];
    }
    mx /= samplesCheck;
    my /= samplesCheck;
    mz /= samplesCheck;

    float rMean = 0.0f;
    for (uint16_t i = 0; i < samplesCheck; i++) {
        rMean += sqrt(xs[i] * xs[i] + ys[i] * ys[i]);
    }
    rMean /= samplesCheck;

    float rVar = 0.0f;
    for (uint16_t i = 0; i < samplesCheck; i++) {
        float r = sqrt(xs[i] * xs[i] + ys[i] * ys[i]);
        rVar += (r - rMean) * (r - rMean);
    }
    rVar /= samplesCheck;

    float sx = 0.0f, sy = 0.0f, sz = 0.0f;
    for (uint16_t i = 0; i < samplesCheck; i++) {
        sx += (xs[i] - mx) * (xs[i] - mx);
        sy += (ys[i] - my) * (ys[i] - my);
        sz += (zs[i] - mz) * (zs[i] - mz);
    }
    sx = sqrt(sx / samplesCheck);
    sy = sqrt(sy / samplesCheck);
    sz = sqrt(sz / samplesCheck);

    float minSxy = (sx < sy ? sx : sy);
    float maxSxy = (sx > sy ? sx : sy);

    CalibrationQuality q;
    q.meanX = mx;
    q.meanY = my;
    q.meanZ = mz;
    q.stdX = sx;
    q.stdY = sy;
    q.stdZ = sz;
    q.rMeanXY = rMean;
    q.rStdXY = sqrt(rVar);
    q.anisotropyXY = maxSxy / (minSxy + 1e-9f);
    return q;
}

void SteamiLIS2MDL::calibrateReset() {
    // Resets to a 'neutral' calibration (useful before re-calibrating).
    xOff = 0.0f;
    yOff = 0.0f;
    zOff = 0.0f;
    xScale = 1.0f;
    yScale = 1.0f;
    zScale = 1.0f;
}

MagCalibration SteamiLIS2MDL::readCalibration() const {
    return {xOff, yOff, zOff, xScale, yScale, zScale};
}

bool SteamiLIS2MDL::calibrateStep() {
    return calibrateMinmax3d();
}

//--------------------------------------------------------
// -------------------- Heading functions ----------------
//--------------------------------------------------------

void SteamiLIS2MDL::setHeadingFilter(float alpha) {
    /*alpha=0 -> no filtering. 0.1..0.3 = light/medium smoothing.
        Filter by averaging cos/sin to avoid artifacts at 0/360°.*/
    _hfAlpha = (alpha < 0.0f) ? 0.0f : (alpha > 1.0f) ? 1.0f : alpha;
    _hfCos = 0.0f;
    _hfSin = 0.0f;
}

float SteamiLIS2MDL::normalizeDeg(float a) {
    a = fmod(a, 360.0f);
    if (a < 0.0f) {
        a += 360.0f;
    }
    // A tiny negative angle plus 360 rounds to 360 in float: that is north, 0.
    return a >= 360.0f ? 0.0f : a;
}

float SteamiLIS2MDL::applyHeadingOffsets(float angleDeg) {
    angleDeg = angleDeg + _headingOffsetDeg + _declinationDeg;
    return normalizeDeg(angleDeg);
}

float SteamiLIS2MDL::filterHeading(float angleDeg) {
    // Filters the angle via vector averaging; returns filtered angle (or raw if alpha=0).
    if (_hfAlpha <= 0.0f) {
        return angleDeg;
    }
    float c = cos(radians(angleDeg));
    float s = sin(radians(angleDeg));

    if (_hfCos == 0.0f && _hfSin == 0.0f) {
        _hfCos = c;
        _hfSin = s;
    } else {
        float a = _hfAlpha;
        _hfCos = (1.0f - a) * _hfCos + a * c;
        _hfSin = (1.0f - a) * _hfSin + a * s;

        float norm = sqrt(_hfCos * _hfCos + _hfSin * _hfSin);
        if (norm > 1e-6f) {
            _hfCos /= norm;
            _hfSin /= norm;
        }
    }
    return normalizeDeg(degrees(atan2(_hfSin, _hfCos)));
}

float SteamiLIS2MDL::headingFromVectors(float x, float y, float z, bool calibrated) {
    /*Computes the angle (0..360°) from a triplet.
        - calibrated=True: applies offset/scale per axis (recommended)
        - flat only (uses XY)*/
    if (calibrated) {
        x = (x - xOff) / (xScale != 0.0f ? xScale : 1.0f);
        y = (y - yOff) / (yScale != 0.0f ? yScale : 1.0f);
    }
    // Clockwise from north, 0 when the top of the screen points north (measured on a V1c).
    float angle = degrees(atan2(-x, y));
    angle = applyHeadingOffsets(angle);
    return filterHeading(angle);
}

float SteamiLIS2MDL::headingFlatOnly() {
    /*Reads the sensor and returns the angle (0..360°) assuming the board is FLAT.
        Uses XY (no tilt compensation). NAN on a bus error; the heading filter is left as is.*/
    MagneticField f;
    if (!magneticField(f)) {
        return NAN;
    }
    return headingFromVectors(f.x, f.y, f.z, true);
}

float SteamiLIS2MDL::headingWithTiltCompensation(Vec3f (*readAccel)()) {
    /*Tilt-compensated compass (if an accelerometer is available).
        readAccel() must return (ax, ay, az) ~g.*/
    MagneticField raw = magneticField();
    float x = (raw.x - xOff) / (xScale != 0.0f ? xScale : 1.0f);
    float y = (raw.y - yOff) / (yScale != 0.0f ? yScale : 1.0f);
    float z = (raw.z - zOff) / (zScale != 0.0f ? zScale : 1.0f);

    Vec3f accel = readAccel();
    float roll = atan2(accel.y, accel.z);
    float pitch = atan2(-accel.x, sqrt(accel.y * accel.y + accel.z * accel.z));

    float xh = x * cos(pitch) + z * sin(pitch);
    float yh = x * sin(roll) * sin(pitch) + y * cos(roll) - z * sin(roll) * cos(pitch);
    float angle = degrees(atan2(yh, xh));
    angle = applyHeadingOffsets(angle);
    return filterHeading(angle);
}

const char* SteamiLIS2MDL::directionLabel(float angle) {
    // Returns N/NE/E/... ; if angle=-1, reads headingFlatOnly(). "" for NAN (failed read).
    if (angle < 0.0f) {
        angle = headingFlatOnly();
    }
    if (isnan(angle)) {
        return "";
    }
    const char* dirs[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
    return dirs[(int)((angle + 22.5f) / 45.0f) % 8];
}

//--------------------------------------------------------
// -------------------- Power/reset functions ------------
//--------------------------------------------------------

const char* SteamiLIS2MDL::getMode() {
    uint8_t r = readReg(LIS2MDL_CFG_REG_A);
    uint8_t md = r & 0b11;
    if (md == 0b00)
        return "continuous";
    if (md == 0b01)
        return "single";
    return "idle";
}

void SteamiLIS2MDL::powerOff() {
    // Switches to IDLE mode (low power).
    uint8_t r = readReg(LIS2MDL_CFG_REG_A);
    r = (r & ~0b11) | 0b11;
    writeReg(LIS2MDL_CFG_REG_A, r);
}

void SteamiLIS2MDL::powerOn(const char* mode) {
    // Power on the sensor: 'continuous' (default) or 'single'.
    uint8_t md;
    if (mode != nullptr && strcmp(mode, "single") == 0) {
        md = 0b01;
    } else {
        md = 0b00;
    }
    uint8_t r = readReg(LIS2MDL_CFG_REG_A);
    r = (r & ~0b11) | md;
    writeReg(LIS2MDL_CFG_REG_A, r);
}

void SteamiLIS2MDL::softReset(uint16_t waitMs) {
    /*SOFT_RST (bit5) in CFG_REG_A.
        The bit auto-clears; after reset, the sensor returns to default values (idle mode expected).
        */
    uint8_t r = readReg(LIS2MDL_CFG_REG_A);
    r |= 1 << 5;
    writeReg(LIS2MDL_CFG_REG_A, r);
    delay(waitMs);
}

void SteamiLIS2MDL::reboot(uint16_t waitMs) {
    /*REBOOT (bit6) in CFG_REG_A: reload internal registers.
        The bit auto-clears.
        */
    uint8_t r = readReg(LIS2MDL_CFG_REG_A);
    r |= 1 << 6;
    writeReg(LIS2MDL_CFG_REG_A, r);
    delay(waitMs);
}