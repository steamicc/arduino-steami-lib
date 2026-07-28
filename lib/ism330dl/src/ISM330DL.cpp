// SPDX-License-Identifier: GPL-3.0-or-later

#include "ISM330DL.h"

#include <math.h>

ISM330DL::ISM330DL(TwoWire& wire, uint8_t address)
    : _wire(wire),
      _address(address),
      _accelScale(AccelScale::G_2),
      _accelOdr(AccelOdr::HZ_104),
      _gyroScale(GyroScale::DPS_250),
      _gyroOdr(GyroOdr::HZ_104),
      _accelOffset{0.0F, 0.0F, 0.0F},
      _tempGain(1.0F),
      _tempOffset(0.0F) {}

bool ISM330DL::begin() {
    if (!isConnected()) {
        return false;
    }
    if (!softReset()) {
        return false;
    }
    return configureAccel(AccelOdr::HZ_104, AccelScale::G_2) &&
           configureGyro(GyroOdr::HZ_104, GyroScale::DPS_250);
}

uint8_t ISM330DL::deviceId() {
    uint8_t id = 0;
    return readReg(ISM330DL_REG_WHO_AM_I, id) ? id : 0;
}

bool ISM330DL::isConnected() {
    uint8_t id = 0;
    return readReg(ISM330DL_REG_WHO_AM_I, id) && id == ISM330DL_WHO_AM_I_VALUE;
}

bool ISM330DL::softReset() {
    if (!writeReg(ISM330DL_REG_CTRL3_C, ISM330DL_CTRL3_C_SW_RESET)) {
        return false;
    }

    const uint32_t start = millis();
    uint8_t ctrl3 = 0;
    do {
        if (!readReg(ISM330DL_REG_CTRL3_C, ctrl3)) {
            return false;
        }
        if ((ctrl3 & ISM330DL_CTRL3_C_SW_RESET) == 0) {
            break;
        }
        delay(1);
    } while (millis() - start < 100U);

    if ((ctrl3 & ISM330DL_CTRL3_C_SW_RESET) != 0) {
        return false;
    }

    return writeReg(ISM330DL_REG_CTRL3_C, ISM330DL_CTRL3_C_BDU | ISM330DL_CTRL3_C_IF_INC);
}

bool ISM330DL::configureAccel(AccelOdr odr, AccelScale scale) {
    uint8_t scaleBits = 0;
    if (!accelOdrValid(odr) || !accelScaleBits(scale, scaleBits)) {
        return false;
    }

    const uint8_t value = (static_cast<uint8_t>(odr) << 4) | (scaleBits << 2);
    if (!writeReg(ISM330DL_REG_CTRL1_XL, value)) {
        return false;
    }

    _accelScale = scale;
    if (odr != AccelOdr::POWER_DOWN) {
        _accelOdr = odr;
    }
    return true;
}

bool ISM330DL::configureGyro(GyroOdr odr, GyroScale scale) {
    uint8_t scaleBits = 0;
    bool fs125 = false;
    if (!gyroOdrValid(odr) || !gyroScaleBits(scale, scaleBits, fs125)) {
        return false;
    }

    uint8_t value = static_cast<uint8_t>(odr) << 4;
    value |= fs125 ? 0x02 : static_cast<uint8_t>(scaleBits << 2);
    if (!writeReg(ISM330DL_REG_CTRL2_G, value)) {
        return false;
    }

    _gyroScale = scale;
    if (odr != GyroOdr::POWER_DOWN) {
        _gyroOdr = odr;
    }
    return true;
}

bool ISM330DL::accelerationRaw(RawVector& value) {
    return ensureData() && readVector(ISM330DL_REG_OUTX_L_XL, value);
}

bool ISM330DL::gyroscopeRaw(RawVector& value) {
    return ensureData() && readVector(ISM330DL_REG_OUTX_L_G, value);
}

bool ISM330DL::temperatureRaw(int16_t& value) {
    return ensureData() && readInt16(ISM330DL_REG_OUT_TEMP_L, value);
}

bool ISM330DL::accelerationG(Vector3& value) {
    RawVector raw{};
    if (!accelerationRaw(raw)) {
        return false;
    }
    const float sensitivity = accelSensitivityMg(_accelScale) / 1000.0F;
    value.x = raw.x * sensitivity - _accelOffset.x;
    value.y = raw.y * sensitivity - _accelOffset.y;
    value.z = raw.z * sensitivity - _accelOffset.z;
    return true;
}

bool ISM330DL::accelerationMs2(Vector3& value) {
    if (!accelerationG(value)) {
        return false;
    }
    value.x *= ISM330DL_STANDARD_GRAVITY;
    value.y *= ISM330DL_STANDARD_GRAVITY;
    value.z *= ISM330DL_STANDARD_GRAVITY;
    return true;
}

bool ISM330DL::gyroscopeDps(Vector3& value) {
    RawVector raw{};
    if (!gyroscopeRaw(raw)) {
        return false;
    }
    const float sensitivity = gyroSensitivityMdps(_gyroScale) / 1000.0F;
    value.x = raw.x * sensitivity;
    value.y = raw.y * sensitivity;
    value.z = raw.z * sensitivity;
    return true;
}

bool ISM330DL::gyroscopeRads(Vector3& value) {
    if (!gyroscopeDps(value)) {
        return false;
    }
    value.x *= ISM330DL_DEG_TO_RAD;
    value.y *= ISM330DL_DEG_TO_RAD;
    value.z *= ISM330DL_DEG_TO_RAD;
    return true;
}

bool ISM330DL::temperature(float& value) {
    int16_t raw = 0;
    if (!temperatureRaw(raw)) {
        return false;
    }
    const float factory = ISM330DL_TEMP_OFFSET + raw / ISM330DL_TEMP_SENSITIVITY;
    value = _tempGain * factory + _tempOffset;
    return true;
}

void ISM330DL::setAccelOffset(float x, float y, float z) {
    _accelOffset = {x, y, z};
}

ISM330DL::Vector3 ISM330DL::accelOffset() const {
    return _accelOffset;
}

void ISM330DL::setTemperatureOffset(float offsetC) {
    _tempGain = 1.0F;
    _tempOffset = offsetC;
}

bool ISM330DL::calibrateTemperature(float refLow, float measuredLow, float refHigh,
                                    float measuredHigh) {
    const float delta = measuredHigh - measuredLow;
    if (delta == 0.0F) {
        return false;
    }
    _tempGain = (refHigh - refLow) / delta;
    _tempOffset = refLow - _tempGain * measuredLow;
    return true;
}

bool ISM330DL::orientation(Orientation& value) {
    Vector3 accel{};
    if (!accelerationG(accel)) {
        return false;
    }
    constexpr float threshold = 0.75F;
    if (accel.z > threshold) {
        value = Orientation::SCREEN_DOWN;
    } else if (accel.z < -threshold) {
        value = Orientation::SCREEN_UP;
    } else if (accel.x > threshold) {
        value = Orientation::TOP_EDGE_DOWN;
    } else if (accel.x < -threshold) {
        value = Orientation::BOTTOM_EDGE_DOWN;
    } else if (accel.y > threshold) {
        value = Orientation::RIGHT_EDGE_DOWN;
    } else if (accel.y < -threshold) {
        value = Orientation::LEFT_EDGE_DOWN;
    } else {
        value = Orientation::MOVING;
    }
    return true;
}

bool ISM330DL::motion(Motion& value) {
    Vector3 gyro{};
    if (!gyroscopeDps(gyro)) {
        return false;
    }
    constexpr float threshold = 10.0F;
    if (fabsf(gyro.z) > fabsf(gyro.x) && fabsf(gyro.z) > fabsf(gyro.y)) {
        if (gyro.z > threshold) {
            value = {MotionType::TURNING_RIGHT, gyro.z};
            return true;
        }
        if (gyro.z < -threshold) {
            value = {MotionType::TURNING_LEFT, fabsf(gyro.z)};
            return true;
        }
    }
    if (fabsf(gyro.x) > fabsf(gyro.y)) {
        if (gyro.x > threshold) {
            value = {MotionType::TILTING_LEFT, gyro.x};
            return true;
        }
        if (gyro.x < -threshold) {
            value = {MotionType::TILTING_RIGHT, fabsf(gyro.x)};
            return true;
        }
    } else {
        if (gyro.y > threshold) {
            value = {MotionType::TILTING_DOWN, gyro.y};
            return true;
        }
        if (gyro.y < -threshold) {
            value = {MotionType::TILTING_UP, fabsf(gyro.y)};
            return true;
        }
    }
    value = {MotionType::STABLE, 0.0F};
    return true;
}

const char* ISM330DL::orientationToString(Orientation value) {
    switch (value) {
        case Orientation::SCREEN_DOWN:
            return "SCREEN_DOWN";
        case Orientation::SCREEN_UP:
            return "SCREEN_UP";
        case Orientation::TOP_EDGE_DOWN:
            return "TOP_EDGE_DOWN";
        case Orientation::BOTTOM_EDGE_DOWN:
            return "BOTTOM_EDGE_DOWN";
        case Orientation::RIGHT_EDGE_DOWN:
            return "RIGHT_EDGE_DOWN";
        case Orientation::LEFT_EDGE_DOWN:
            return "LEFT_EDGE_DOWN";
        default:
            return "MOVING";
    }
}

const char* ISM330DL::motionToString(MotionType value) {
    switch (value) {
        case MotionType::TURNING_RIGHT:
            return "TURNING_RIGHT";
        case MotionType::TURNING_LEFT:
            return "TURNING_LEFT";
        case MotionType::TILTING_LEFT:
            return "TILTING_LEFT";
        case MotionType::TILTING_RIGHT:
            return "TILTING_RIGHT";
        case MotionType::TILTING_DOWN:
            return "TILTING_DOWN";
        case MotionType::TILTING_UP:
            return "TILTING_UP";
        default:
            return "STABLE";
    }
}

uint8_t ISM330DL::status() {
    uint8_t value = 0;
    return readReg(ISM330DL_REG_STATUS, value) ? value : 0;
}

bool ISM330DL::accelReady() {
    return (status() & ISM330DL_STATUS_XLDA) != 0;
}

bool ISM330DL::gyroReady() {
    return (status() & ISM330DL_STATUS_GDA) != 0;
}

bool ISM330DL::temperatureReady() {
    return (status() & ISM330DL_STATUS_TDA) != 0;
}

bool ISM330DL::dataReady() {
    return (status() & ISM330DL_STATUS_ALL_READY) == ISM330DL_STATUS_ALL_READY;
}

bool ISM330DL::powerOff() {
    return writeReg(ISM330DL_REG_CTRL1_XL, 0) && writeReg(ISM330DL_REG_CTRL2_G, 0);
}

bool ISM330DL::powerOn() {
    return configureAccel(_accelOdr, _accelScale) && configureGyro(_gyroOdr, _gyroScale);
}

bool ISM330DL::readReg(uint8_t reg, uint8_t& value) {
    return readBytes(reg, &value, 1);
}

bool ISM330DL::writeReg(uint8_t reg, uint8_t value) {
    _wire.beginTransmission(_address);
    _wire.write(reg);
    _wire.write(value);
    return _wire.endTransmission() == 0;
}

bool ISM330DL::readBytes(uint8_t reg, uint8_t* buffer, size_t length) {
    if (buffer == nullptr || length == 0 || length > 255) {
        return false;
    }
    _wire.beginTransmission(_address);
    _wire.write(reg);
    if (_wire.endTransmission(false) != 0) {
        return false;
    }
    const size_t received = _wire.requestFrom(_address, static_cast<uint8_t>(length));
    if (received != length) {
        while (_wire.available()) {
            _wire.read();
        }
        return false;
    }
    for (size_t i = 0; i < length; ++i) {
        if (!_wire.available()) {
            return false;
        }
        buffer[i] = static_cast<uint8_t>(_wire.read());
    }
    return true;
}

bool ISM330DL::readInt16(uint8_t reg, int16_t& value) {
    uint8_t data[2]{};
    if (!readBytes(reg, data, sizeof(data))) {
        return false;
    }
    value = static_cast<int16_t>(static_cast<uint16_t>(data[0]) |
                                 (static_cast<uint16_t>(data[1]) << 8));
    return true;
}

bool ISM330DL::readVector(uint8_t reg, RawVector& value) {
    uint8_t data[6]{};
    if (!readBytes(reg, data, sizeof(data))) {
        return false;
    }
    value.x = static_cast<int16_t>(static_cast<uint16_t>(data[0]) |
                                   (static_cast<uint16_t>(data[1]) << 8));
    value.y = static_cast<int16_t>(static_cast<uint16_t>(data[2]) |
                                   (static_cast<uint16_t>(data[3]) << 8));
    value.z = static_cast<int16_t>(static_cast<uint16_t>(data[4]) |
                                   (static_cast<uint16_t>(data[5]) << 8));
    return true;
}

bool ISM330DL::isPowerDown(bool& value) {
    uint8_t ctrl1 = 0;
    uint8_t ctrl2 = 0;
    if (!readReg(ISM330DL_REG_CTRL1_XL, ctrl1) || !readReg(ISM330DL_REG_CTRL2_G, ctrl2)) {
        return false;
    }
    value = (ctrl1 & 0xF0) == 0 && (ctrl2 & 0xF0) == 0;
    return true;
}

bool ISM330DL::ensureData() {
    bool poweredDown = false;
    if (!isPowerDown(poweredDown)) {
        return false;
    }
    if (!poweredDown) {
        return true;
    }
    if (!powerOn()) {
        return false;
    }
    const uint32_t start = millis();
    do {
        uint8_t currentStatus = 0;
        if (!readReg(ISM330DL_REG_STATUS, currentStatus)) {
            return false;
        }
        if ((currentStatus & ISM330DL_STATUS_ALL_READY) == ISM330DL_STATUS_ALL_READY) {
            return true;
        }
        delay(10);
    } while (millis() - start < 500U);
    return false;
}

bool ISM330DL::accelOdrValid(AccelOdr odr) {
    return static_cast<uint8_t>(odr) <= static_cast<uint8_t>(AccelOdr::HZ_1660);
}

bool ISM330DL::gyroOdrValid(GyroOdr odr) {
    return static_cast<uint8_t>(odr) <= static_cast<uint8_t>(GyroOdr::HZ_1660);
}

bool ISM330DL::accelScaleBits(AccelScale scale, uint8_t& bits) {
    switch (scale) {
        case AccelScale::G_2:
            bits = 0x00;
            return true;
        case AccelScale::G_16:
            bits = 0x01;
            return true;
        case AccelScale::G_4:
            bits = 0x02;
            return true;
        case AccelScale::G_8:
            bits = 0x03;
            return true;
        default:
            return false;
    }
}

bool ISM330DL::gyroScaleBits(GyroScale scale, uint8_t& bits, bool& fs125) {
    fs125 = false;
    switch (scale) {
        case GyroScale::DPS_125:
            fs125 = true;
            bits = 0;
            return true;
        case GyroScale::DPS_250:
            bits = 0x00;
            return true;
        case GyroScale::DPS_500:
            bits = 0x01;
            return true;
        case GyroScale::DPS_1000:
            bits = 0x02;
            return true;
        case GyroScale::DPS_2000:
            bits = 0x03;
            return true;
        default:
            return false;
    }
}

float ISM330DL::accelSensitivityMg(AccelScale scale) {
    switch (scale) {
        case AccelScale::G_2:
            return 0.061F;
        case AccelScale::G_4:
            return 0.122F;
        case AccelScale::G_8:
            return 0.244F;
        case AccelScale::G_16:
            return 0.488F;
        default:
            return 0.0F;
    }
}

float ISM330DL::gyroSensitivityMdps(GyroScale scale) {
    switch (scale) {
        case GyroScale::DPS_125:
            return 4.375F;
        case GyroScale::DPS_250:
            return 8.75F;
        case GyroScale::DPS_500:
            return 17.50F;
        case GyroScale::DPS_1000:
            return 35.0F;
        case GyroScale::DPS_2000:
            return 70.0F;
        default:
            return 0.0F;
    }
}
