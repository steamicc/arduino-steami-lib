// SPDX-License-Identifier: GPL-3.0-or-later

#include "APDS9960.h"

#include <stdlib.h>

APDS9960::APDS9960(TwoWire& wire, uint8_t address) : _wire(&wire), _address(address) {}

bool APDS9960::begin() {
    uint8_t id = deviceId();
    if (!validDeviceId(id)) {
        return false;
    }

    setMode(Mode::ALL, false);

    writeReg(APDS9960_REG_ATIME, APDS9960_DEFAULT_ATIME);
    writeReg(APDS9960_REG_WTIME, APDS9960_DEFAULT_WTIME);
    writeReg(APDS9960_REG_PPULSE, APDS9960_DEFAULT_PROX_PPULSE);
    writeReg(APDS9960_REG_POFFSET_UR, APDS9960_DEFAULT_POFFSET_UR);
    writeReg(APDS9960_REG_POFFSET_DL, APDS9960_DEFAULT_POFFSET_DL);
    writeReg(APDS9960_REG_CONFIG1, APDS9960_DEFAULT_CONFIG1);

    setLedDrive(LedDrive::MA_100);
    setProximityGain(ProximityGain::X4);
    setAmbientLightGain(AmbientLightGain::X4);
    setProximityInterruptLowThreshold(APDS9960_DEFAULT_PILT);
    setProximityInterruptHighThreshold(APDS9960_DEFAULT_PIHT);
    setLightInterruptLowThreshold(APDS9960_DEFAULT_AILT);
    setLightInterruptHighThreshold(APDS9960_DEFAULT_AIHT);

    writeReg(APDS9960_REG_PERS, APDS9960_DEFAULT_PERS);
    writeReg(APDS9960_REG_CONFIG2, APDS9960_DEFAULT_CONFIG2);
    writeReg(APDS9960_REG_CONFIG3, APDS9960_DEFAULT_CONFIG3);

    setGestureEnterThreshold(APDS9960_DEFAULT_GPENTH);
    setGestureExitThreshold(APDS9960_DEFAULT_GEXTH);
    writeReg(APDS9960_REG_GCONF1, APDS9960_DEFAULT_GCONF1);
    setGestureGain(GestureGain::X4);
    setGestureLedDrive(LedDrive::MA_100);
    setGestureWaitTime(GestureWaitTime::MS_2_8);
    writeReg(APDS9960_REG_GOFFSET_U, APDS9960_DEFAULT_GOFFSET);
    writeReg(APDS9960_REG_GOFFSET_D, APDS9960_DEFAULT_GOFFSET);
    writeReg(APDS9960_REG_GOFFSET_L, APDS9960_DEFAULT_GOFFSET);
    writeReg(APDS9960_REG_GOFFSET_R, APDS9960_DEFAULT_GOFFSET);
    writeReg(APDS9960_REG_GPULSE, APDS9960_DEFAULT_GPULSE);
    writeReg(APDS9960_REG_GCONF3, APDS9960_DEFAULT_GCONF3);
    setGestureInterrupt(false);

    resetGestureParameters();
    return true;
}

uint8_t APDS9960::deviceId() {
    uint8_t value = 0;
    readReg(APDS9960_REG_ID, value);
    return value;
}

uint8_t APDS9960::status() {
    uint8_t value = 0;
    readReg(APDS9960_REG_STATUS, value);
    return value;
}

uint8_t APDS9960::mode() {
    uint8_t value = 0;
    readReg(APDS9960_REG_ENABLE, value);
    return value;
}

bool APDS9960::setMode(Mode selectedMode, bool enable) {
    uint8_t rawMode = static_cast<uint8_t>(selectedMode);
    if (rawMode > static_cast<uint8_t>(Mode::ALL)) {
        return false;
    }

    uint8_t value = mode();
    if (selectedMode == Mode::ALL) {
        value = enable ? 0x7F : 0x00;
    } else if (enable) {
        value |= static_cast<uint8_t>(1U << rawMode);
    } else {
        value &= static_cast<uint8_t>(~(1U << rawMode));
    }

    return writeReg(APDS9960_REG_ENABLE, value);
}

void APDS9960::powerOn() {
    setMode(Mode::POWER, true);
}

void APDS9960::powerOff() {
    setMode(Mode::POWER, false);
}

bool APDS9960::dataReady() {
    uint8_t value = status();
    return (value & (APDS9960_STATUS_AVALID | APDS9960_STATUS_PVALID)) ==
           (APDS9960_STATUS_AVALID | APDS9960_STATUS_PVALID);
}

bool APDS9960::lightReady() {
    return (status() & APDS9960_STATUS_AVALID) != 0;
}

bool APDS9960::proximityReady() {
    return (status() & APDS9960_STATUS_PVALID) != 0;
}

void APDS9960::enableLightSensor(bool interrupts) {
    setAmbientLightInterrupt(interrupts);
    powerOn();
    setMode(Mode::AMBIENT_LIGHT, true);
}

void APDS9960::disableLightSensor() {
    setAmbientLightInterrupt(false);
    setMode(Mode::AMBIENT_LIGHT, false);
}

void APDS9960::enableProximitySensor(bool interrupts) {
    setProximityInterrupt(interrupts);
    powerOn();
    setMode(Mode::PROXIMITY, true);
}

void APDS9960::disableProximitySensor() {
    setProximityInterrupt(false);
    setMode(Mode::PROXIMITY, false);
}

void APDS9960::enableGestureSensor(bool interrupts) {
    resetGestureParameters();
    writeReg(APDS9960_REG_WTIME, 0xFF);
    writeReg(APDS9960_REG_PPULSE, APDS9960_DEFAULT_GESTURE_PPULSE);
    setLedBoost(LedBoost::PERCENT_300);
    setGestureInterrupt(interrupts);
    setGestureMode(true);
    powerOn();
    setMode(Mode::WAIT, true);
    setMode(Mode::PROXIMITY, true);
    setMode(Mode::GESTURE, true);
}

void APDS9960::disableGestureSensor() {
    resetGestureParameters();
    setGestureInterrupt(false);
    setGestureMode(false);
    setMode(Mode::GESTURE, false);
}

bool APDS9960::ambientLight(uint16_t& value) {
    return readColorChannel(APDS9960_REG_CDATAL, value);
}

bool APDS9960::redLight(uint16_t& value) {
    return readColorChannel(APDS9960_REG_RDATAL, value);
}

bool APDS9960::greenLight(uint16_t& value) {
    return readColorChannel(APDS9960_REG_GDATAL, value);
}

bool APDS9960::blueLight(uint16_t& value) {
    return readColorChannel(APDS9960_REG_BDATAL, value);
}

bool APDS9960::proximity(uint8_t& value) {
    if (!ensureProximityEnabled()) {
        value = 0;
        return false;
    }
    return readReg(APDS9960_REG_PDATA, value);
}

bool APDS9960::gestureAvailable() {
    uint8_t value = 0;
    return readReg(APDS9960_REG_GSTATUS, value) && (value & APDS9960_GSTATUS_GVALID) != 0;
}

APDS9960::Gesture APDS9960::readGesture(uint32_t timeoutMs) {
    if ((mode() & (APDS9960_ENABLE_PON | APDS9960_ENABLE_GEN)) !=
            (APDS9960_ENABLE_PON | APDS9960_ENABLE_GEN) ||
        !gestureAvailable()) {
        return Gesture::NONE;
    }

    uint32_t started = millis();
    do {
        uint8_t fifoLevel = 0;
        if (!readReg(APDS9960_REG_GFLVL, fifoLevel)) {
            break;
        }

        if (fifoLevel > 0) {
            uint8_t datasets = fifoLevel;
            if (datasets > APDS9960_GESTURE_DATASETS_MAX) {
                datasets = APDS9960_GESTURE_DATASETS_MAX;
            }

            uint8_t fifo[APDS9960_GESTURE_DATASETS_MAX * 4];
            size_t bytesRead =
                readRegs(APDS9960_REG_GFIFO_U, fifo, static_cast<size_t>(datasets) * 4U);
            size_t completeSets = bytesRead / 4U;

            for (size_t i = 0;
                 i < completeSets && _gestureData.index < APDS9960_GESTURE_DATASETS_MAX; ++i) {
                size_t offset = i * 4U;
                uint8_t index = _gestureData.index++;
                _gestureData.up[index] = fifo[offset];
                _gestureData.down[index] = fifo[offset + 1];
                _gestureData.left[index] = fifo[offset + 2];
                _gestureData.right[index] = fifo[offset + 3];
                ++_gestureData.total;
            }

            processGestureData();
            decodeGesture();
            _gestureData.index = 0;
            _gestureData.total = 0;
        }

        delay(APDS9960_FIFO_PAUSE_MS);
    } while (gestureAvailable() && millis() - started < timeoutMs);

    decodeGesture();
    Gesture result = _gestureMotion;
    resetGestureParameters();
    return result;
}

void APDS9960::resetGestureParameters() {
    _gestureData = GestureData{};
    _gestureUdDelta = 0;
    _gestureLrDelta = 0;
    _gestureUdCount = 0;
    _gestureLrCount = 0;
    _gestureNearCount = 0;
    _gestureFarCount = 0;
    _gestureMotion = Gesture::NONE;
    _gestureState = Gesture::NONE;
}

APDS9960::AmbientLightGain APDS9960::ambientLightGain() {
    uint8_t value = 0;
    readReg(APDS9960_REG_CONTROL, value);
    return static_cast<AmbientLightGain>(value & APDS9960_CONTROL_AGAIN_MASK);
}

void APDS9960::setAmbientLightGain(AmbientLightGain gain) {
    updateRegister(APDS9960_REG_CONTROL, APDS9960_CONTROL_AGAIN_MASK, static_cast<uint8_t>(gain));
}

APDS9960::ProximityGain APDS9960::proximityGain() {
    uint8_t value = 0;
    readReg(APDS9960_REG_CONTROL, value);
    return static_cast<ProximityGain>((value & APDS9960_CONTROL_PGAIN_MASK) >> 2);
}

void APDS9960::setProximityGain(ProximityGain gain) {
    updateRegister(APDS9960_REG_CONTROL, APDS9960_CONTROL_PGAIN_MASK,
                   static_cast<uint8_t>(static_cast<uint8_t>(gain) << 2));
}

APDS9960::LedDrive APDS9960::ledDrive() {
    uint8_t value = 0;
    readReg(APDS9960_REG_CONTROL, value);
    return static_cast<LedDrive>((value & APDS9960_CONTROL_LDRIVE_MASK) >> 6);
}

void APDS9960::setLedDrive(LedDrive drive) {
    updateRegister(APDS9960_REG_CONTROL, APDS9960_CONTROL_LDRIVE_MASK,
                   static_cast<uint8_t>(static_cast<uint8_t>(drive) << 6));
}

APDS9960::LedBoost APDS9960::ledBoost() {
    uint8_t value = 0;
    readReg(APDS9960_REG_CONFIG2, value);
    return static_cast<LedBoost>((value & APDS9960_CONFIG2_LED_BOOST_MASK) >> 4);
}

void APDS9960::setLedBoost(LedBoost boost) {
    updateRegister(APDS9960_REG_CONFIG2, APDS9960_CONFIG2_LED_BOOST_MASK,
                   static_cast<uint8_t>(static_cast<uint8_t>(boost) << 4));
}

bool APDS9960::proximityGainCompensationEnabled() {
    uint8_t value = 0;
    readReg(APDS9960_REG_CONFIG3, value);
    return (value & APDS9960_CONFIG3_PCMP) != 0;
}

void APDS9960::setProximityGainCompensation(bool enable) {
    updateRegister(APDS9960_REG_CONFIG3, APDS9960_CONFIG3_PCMP, enable ? APDS9960_CONFIG3_PCMP : 0);
}

uint8_t APDS9960::proximityPhotodiodeMask() {
    uint8_t value = 0;
    readReg(APDS9960_REG_CONFIG3, value);
    return value & APDS9960_CONFIG3_PMASK_MASK;
}

void APDS9960::setProximityPhotodiodeMask(uint8_t mask) {
    updateRegister(APDS9960_REG_CONFIG3, APDS9960_CONFIG3_PMASK_MASK,
                   mask & APDS9960_CONFIG3_PMASK_MASK);
}

uint8_t APDS9960::gestureEnterThreshold() {
    uint8_t value = 0;
    readReg(APDS9960_REG_GPENTH, value);
    return value;
}

void APDS9960::setGestureEnterThreshold(uint8_t threshold) {
    writeReg(APDS9960_REG_GPENTH, threshold);
}

uint8_t APDS9960::gestureExitThreshold() {
    uint8_t value = 0;
    readReg(APDS9960_REG_GEXTH, value);
    return value;
}

void APDS9960::setGestureExitThreshold(uint8_t threshold) {
    writeReg(APDS9960_REG_GEXTH, threshold);
}

APDS9960::GestureGain APDS9960::gestureGain() {
    uint8_t value = 0;
    readReg(APDS9960_REG_GCONF2, value);
    return static_cast<GestureGain>((value & APDS9960_GCONF2_GGAIN_MASK) >> 5);
}

void APDS9960::setGestureGain(GestureGain gain) {
    updateRegister(APDS9960_REG_GCONF2, APDS9960_GCONF2_GGAIN_MASK,
                   static_cast<uint8_t>(static_cast<uint8_t>(gain) << 5));
}

APDS9960::LedDrive APDS9960::gestureLedDrive() {
    uint8_t value = 0;
    readReg(APDS9960_REG_GCONF2, value);
    return static_cast<LedDrive>((value & APDS9960_GCONF2_GLDRIVE_MASK) >> 3);
}

void APDS9960::setGestureLedDrive(LedDrive drive) {
    updateRegister(APDS9960_REG_GCONF2, APDS9960_GCONF2_GLDRIVE_MASK,
                   static_cast<uint8_t>(static_cast<uint8_t>(drive) << 3));
}

APDS9960::GestureWaitTime APDS9960::gestureWaitTime() {
    uint8_t value = 0;
    readReg(APDS9960_REG_GCONF2, value);
    return static_cast<GestureWaitTime>(value & APDS9960_GCONF2_GWTIME_MASK);
}

void APDS9960::setGestureWaitTime(GestureWaitTime waitTime) {
    updateRegister(APDS9960_REG_GCONF2, APDS9960_GCONF2_GWTIME_MASK,
                   static_cast<uint8_t>(waitTime));
}

uint16_t APDS9960::lightInterruptLowThreshold() {
    uint8_t bytes[2] = {};
    readRegs(APDS9960_REG_AILTL, bytes, 2);
    return static_cast<uint16_t>(bytes[0] | (static_cast<uint16_t>(bytes[1]) << 8));
}

void APDS9960::setLightInterruptLowThreshold(uint16_t threshold) {
    writeReg(APDS9960_REG_AILTL, static_cast<uint8_t>(threshold));
    writeReg(APDS9960_REG_AILTH, static_cast<uint8_t>(threshold >> 8));
}

uint16_t APDS9960::lightInterruptHighThreshold() {
    uint8_t bytes[2] = {};
    readRegs(APDS9960_REG_AIHTL, bytes, 2);
    return static_cast<uint16_t>(bytes[0] | (static_cast<uint16_t>(bytes[1]) << 8));
}

void APDS9960::setLightInterruptHighThreshold(uint16_t threshold) {
    writeReg(APDS9960_REG_AIHTL, static_cast<uint8_t>(threshold));
    writeReg(APDS9960_REG_AIHTH, static_cast<uint8_t>(threshold >> 8));
}

uint8_t APDS9960::proximityInterruptLowThreshold() {
    uint8_t value = 0;
    readReg(APDS9960_REG_PILT, value);
    return value;
}

void APDS9960::setProximityInterruptLowThreshold(uint8_t threshold) {
    writeReg(APDS9960_REG_PILT, threshold);
}

uint8_t APDS9960::proximityInterruptHighThreshold() {
    uint8_t value = 0;
    readReg(APDS9960_REG_PIHT, value);
    return value;
}

void APDS9960::setProximityInterruptHighThreshold(uint8_t threshold) {
    writeReg(APDS9960_REG_PIHT, threshold);
}

bool APDS9960::ambientLightInterruptEnabled() {
    return (mode() & APDS9960_ENABLE_AIEN) != 0;
}

void APDS9960::setAmbientLightInterrupt(bool enable) {
    setMode(Mode::AMBIENT_LIGHT_INTERRUPT, enable);
}

bool APDS9960::proximityInterruptEnabled() {
    return (mode() & APDS9960_ENABLE_PIEN) != 0;
}

void APDS9960::setProximityInterrupt(bool enable) {
    setMode(Mode::PROXIMITY_INTERRUPT, enable);
}

bool APDS9960::gestureInterruptEnabled() {
    uint8_t value = 0;
    readReg(APDS9960_REG_GCONF4, value);
    return (value & APDS9960_GCONF4_GIEN) != 0;
}

void APDS9960::setGestureInterrupt(bool enable) {
    updateRegister(APDS9960_REG_GCONF4, APDS9960_GCONF4_GIEN, enable ? APDS9960_GCONF4_GIEN : 0);
}

void APDS9960::clearAmbientLightInterrupt() {
    uint8_t ignored = 0;
    readReg(APDS9960_REG_AICLEAR, ignored);
}

void APDS9960::clearProximityInterrupt() {
    uint8_t ignored = 0;
    readReg(APDS9960_REG_PICLEAR, ignored);
}

bool APDS9960::gestureModeEnabled() {
    uint8_t value = 0;
    readReg(APDS9960_REG_GCONF4, value);
    return (value & APDS9960_GCONF4_GMODE) != 0;
}

void APDS9960::setGestureMode(bool enable) {
    updateRegister(APDS9960_REG_GCONF4, APDS9960_GCONF4_GMODE, enable ? APDS9960_GCONF4_GMODE : 0);
}

bool APDS9960::validDeviceId(uint8_t id) const {
    return id == APDS9960_DEVICE_ID_1 || id == APDS9960_DEVICE_ID_2 || id == APDS9960_DEVICE_ID_3;
}

bool APDS9960::waitForLight(uint32_t timeoutMs) {
    uint32_t started = millis();
    while (millis() - started < timeoutMs) {
        if (lightReady()) {
            return true;
        }
        delay(10);
    }
    return false;
}

bool APDS9960::waitForProximity(uint32_t timeoutMs) {
    uint32_t started = millis();
    while (millis() - started < timeoutMs) {
        if (proximityReady()) {
            return true;
        }
        delay(10);
    }
    return false;
}

bool APDS9960::ensureLightEnabled() {
    uint8_t enabled = mode();

    if ((enabled & APDS9960_ENABLE_PON) == 0) {
        powerOn();
    }

    if ((enabled & APDS9960_ENABLE_AEN) == 0) {
        setMode(Mode::AMBIENT_LIGHT, true);
    }

    return lightReady() || waitForLight();
}

bool APDS9960::ensureProximityEnabled() {
    uint8_t enabled = mode();

    if ((enabled & APDS9960_ENABLE_PON) == 0) {
        powerOn();
    }

    if ((enabled & APDS9960_ENABLE_PEN) == 0) {
        setMode(Mode::PROXIMITY, true);
    }

    return proximityReady() || waitForProximity();
}

bool APDS9960::readColorChannel(uint8_t lowRegister, uint16_t& value) {
    value = 0;
    if (!ensureLightEnabled()) {
        return false;
    }

    uint8_t bytes[2] = {};
    if (readRegs(lowRegister, bytes, 2) != 2) {
        return false;
    }

    value = static_cast<uint16_t>(bytes[0] | (static_cast<uint16_t>(bytes[1]) << 8));
    return true;
}

bool APDS9960::processGestureData() {
    if (_gestureData.total <= 4 || _gestureData.total > APDS9960_GESTURE_DATASETS_MAX) {
        return false;
    }

    int16_t uFirst = 0;
    int16_t dFirst = 0;
    int16_t lFirst = 0;
    int16_t rFirst = 0;
    int16_t uLast = 0;
    int16_t dLast = 0;
    int16_t lLast = 0;
    int16_t rLast = 0;

    for (uint8_t i = 0; i < _gestureData.total; ++i) {
        if (_gestureData.up[i] > APDS9960_GESTURE_THRESHOLD_OUT &&
            _gestureData.down[i] > APDS9960_GESTURE_THRESHOLD_OUT &&
            _gestureData.left[i] > APDS9960_GESTURE_THRESHOLD_OUT &&
            _gestureData.right[i] > APDS9960_GESTURE_THRESHOLD_OUT) {
            uFirst = _gestureData.up[i];
            dFirst = _gestureData.down[i];
            lFirst = _gestureData.left[i];
            rFirst = _gestureData.right[i];
            break;
        }
    }

    if (uFirst == 0 || dFirst == 0 || lFirst == 0 || rFirst == 0) {
        return false;
    }

    for (int16_t i = _gestureData.total - 1; i >= 0; --i) {
        if (_gestureData.up[i] > APDS9960_GESTURE_THRESHOLD_OUT &&
            _gestureData.down[i] > APDS9960_GESTURE_THRESHOLD_OUT &&
            _gestureData.left[i] > APDS9960_GESTURE_THRESHOLD_OUT &&
            _gestureData.right[i] > APDS9960_GESTURE_THRESHOLD_OUT) {
            uLast = _gestureData.up[i];
            dLast = _gestureData.down[i];
            lLast = _gestureData.left[i];
            rLast = _gestureData.right[i];
            break;
        }
    }

    if (uLast == 0 || dLast == 0 || lLast == 0 || rLast == 0) {
        return false;
    }

    const int16_t udDenFirst = static_cast<int16_t>(uFirst + dFirst);
    const int16_t lrDenFirst = static_cast<int16_t>(lFirst + rFirst);
    const int16_t udDenLast = static_cast<int16_t>(uLast + dLast);
    const int16_t lrDenLast = static_cast<int16_t>(lLast + rLast);

    if (udDenFirst == 0 || lrDenFirst == 0 || udDenLast == 0 || lrDenLast == 0) {
        return false;
    }

    int16_t udRatioFirst = static_cast<int16_t>(((uFirst - dFirst) * 100L) / udDenFirst);
    int16_t lrRatioFirst = static_cast<int16_t>(((lFirst - rFirst) * 100L) / lrDenFirst);
    int16_t udRatioLast = static_cast<int16_t>(((uLast - dLast) * 100L) / udDenLast);
    int16_t lrRatioLast = static_cast<int16_t>(((lLast - rLast) * 100L) / lrDenLast);

    int16_t udDelta = udRatioLast - udRatioFirst;
    int16_t lrDelta = lrRatioLast - lrRatioFirst;
    _gestureUdDelta += udDelta;
    _gestureLrDelta += lrDelta;

    _gestureUdCount = _gestureUdDelta >= APDS9960_GESTURE_SENSITIVITY_1
                          ? 1
                          : (_gestureUdDelta <= -APDS9960_GESTURE_SENSITIVITY_1 ? -1 : 0);
    _gestureLrCount = _gestureLrDelta >= APDS9960_GESTURE_SENSITIVITY_1
                          ? 1
                          : (_gestureLrDelta <= -APDS9960_GESTURE_SENSITIVITY_1 ? -1 : 0);

    if (_gestureUdCount == 0 && _gestureLrCount == 0 &&
        abs(udDelta) < APDS9960_GESTURE_SENSITIVITY_2 &&
        abs(lrDelta) < APDS9960_GESTURE_SENSITIVITY_2) {
        if (udDelta == 0 && lrDelta == 0) {
            ++_gestureNearCount;
        } else {
            ++_gestureFarCount;
        }

        if (_gestureNearCount >= 10 && _gestureFarCount >= 2) {
            _gestureState = (udDelta == 0 && lrDelta == 0) ? Gesture::NEAR : Gesture::FAR;
            return true;
        }
    } else if ((_gestureUdCount != 0 || _gestureLrCount != 0) &&
               abs(udDelta) < APDS9960_GESTURE_SENSITIVITY_2 &&
               abs(lrDelta) < APDS9960_GESTURE_SENSITIVITY_2 && udDelta == 0 && lrDelta == 0) {
        ++_gestureNearCount;
        if (_gestureNearCount >= 10) {
            _gestureUdCount = 0;
            _gestureLrCount = 0;
            _gestureUdDelta = 0;
            _gestureLrDelta = 0;
        }
    }

    return false;
}

bool APDS9960::decodeGesture() {
    if (_gestureState == Gesture::NEAR || _gestureState == Gesture::FAR) {
        _gestureMotion = _gestureState;
        return true;
    }

    if (_gestureUdCount == -1 && _gestureLrCount == 0) {
        _gestureMotion = Gesture::UP;
    } else if (_gestureUdCount == 1 && _gestureLrCount == 0) {
        _gestureMotion = Gesture::DOWN;
    } else if (_gestureUdCount == 0 && _gestureLrCount == 1) {
        _gestureMotion = Gesture::RIGHT;
    } else if (_gestureUdCount == 0 && _gestureLrCount == -1) {
        _gestureMotion = Gesture::LEFT;
    } else if (_gestureUdCount == -1 && _gestureLrCount == 1) {
        _gestureMotion = abs(_gestureUdDelta) > abs(_gestureLrDelta) ? Gesture::UP : Gesture::RIGHT;
    } else if (_gestureUdCount == 1 && _gestureLrCount == -1) {
        _gestureMotion =
            abs(_gestureUdDelta) > abs(_gestureLrDelta) ? Gesture::DOWN : Gesture::LEFT;
    } else if (_gestureUdCount == -1 && _gestureLrCount == -1) {
        _gestureMotion = abs(_gestureUdDelta) > abs(_gestureLrDelta) ? Gesture::UP : Gesture::LEFT;
    } else if (_gestureUdCount == 1 && _gestureLrCount == 1) {
        _gestureMotion =
            abs(_gestureUdDelta) > abs(_gestureLrDelta) ? Gesture::DOWN : Gesture::RIGHT;
    } else {
        return false;
    }

    return true;
}

bool APDS9960::readReg(uint8_t reg, uint8_t& value) {
    value = 0;
    _wire->beginTransmission(_address);
    _wire->write(reg);
    if (_wire->endTransmission(false) != 0) {
        return false;
    }

    if (_wire->requestFrom(_address, static_cast<uint8_t>(1)) != 1 || !_wire->available()) {
        return false;
    }

    value = static_cast<uint8_t>(_wire->read());
    return true;
}

bool APDS9960::writeReg(uint8_t reg, uint8_t value) {
    _wire->beginTransmission(_address);
    _wire->write(reg);
    _wire->write(value);
    return _wire->endTransmission() == 0;
}

size_t APDS9960::readRegs(uint8_t reg, uint8_t* buffer, size_t length) {
    for (size_t i = 0; i < length; ++i) {
        buffer[i] = 0;
    }

    _wire->beginTransmission(_address);
    _wire->write(reg);
    if (_wire->endTransmission(false) != 0) {
        return 0;
    }

    size_t requested = length > 255 ? 255 : length;
    _wire->requestFrom(_address, static_cast<uint8_t>(requested));

    size_t count = 0;
    while (count < requested && _wire->available()) {
        buffer[count++] = static_cast<uint8_t>(_wire->read());
    }
    return count;
}

void APDS9960::updateRegister(uint8_t reg, uint8_t mask, uint8_t value) {
    uint8_t current = 0;
    if (!readReg(reg, current)) {
        return;
    }
    current = static_cast<uint8_t>((current & ~mask) | (value & mask));
    writeReg(reg, current);
}
