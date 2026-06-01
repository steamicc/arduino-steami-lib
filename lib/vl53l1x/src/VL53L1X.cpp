// SPDX-License-Identifier: GPL-3.0-or-later

#include "VL53L1X.h"

#include <math.h>

const uint8_t VL53L1X::DEFAULT_CONFIG[] = {
    0x00,  // 0x2d : fast plus mode disabled
    0x00,  // 0x2e : bit 0 if I2C pulled up at 1.8V
    0x00,  // 0x2f : bit 0 if GPIO pulled up at 1.8V
    0x01,  // 0x30 : set bit 4 to 0 for active high interrupt
    0x02,  // 0x31 : bit 1 = interrupt depending on the polarity
    0x00,  // 0x32 : not user-modifiable
    0x02,  // 0x33 : not user-modifiable
    0x08,  // 0x34 : not user-modifiable
    0x00,  // 0x35 : not user-modifiable
    0x08,  // 0x36 : not user-modifiable
    0x10,  // 0x37 : not user-modifiable
    0x01,  // 0x38 : not user-modifiable
    0x01,  // 0x39 : not user-modifiable
    0x00,  // 0x3a : not user-modifiable
    0x00,  // 0x3b : not user-modifiable
    0x00,  // 0x3c : not user-modifiable
    0x00,  // 0x3d : not user-modifiable
    0xFF,  // 0x3e : not user-modifiable
    0x00,  // 0x3f : not user-modifiable
    0x0F,  // 0x40 : not user-modifiable
    0x00,  // 0x41 : not user-modifiable
    0x00,  // 0x42 : not user-modifiable
    0x00,  // 0x43 : not user-modifiable
    0x00,  // 0x44 : not user-modifiable
    0x00,  // 0x45 : not user-modifiable
    0x20,  // 0x46 : interrupt configuration 0x20 = new sample ready
    0x0B,  // 0x47 : not user-modifiable
    0x00,  // 0x48 : not user-modifiable
    0x00,  // 0x49 : not user-modifiable
    0x02,  // 0x4a : not user-modifiable
    0x0A,  // 0x4b : not user-modifiable
    0x21,  // 0x4c : not user-modifiable
    0x00,  // 0x4d : not user-modifiable
    0x00,  // 0x4e : not user-modifiable
    0x05,  // 0x4f : not user-modifiable
    0x00,  // 0x50 : not user-modifiable
    0x00,  // 0x51 : not user-modifiable
    0x00,  // 0x52 : not user-modifiable
    0x00,  // 0x53 : not user-modifiable
    0xC8,  // 0x54 : not user-modifiable
    0x00,  // 0x55 : not user-modifiable
    0x00,  // 0x56 : not user-modifiable
    0x38,  // 0x57 : not user-modifiable
    0xFF,  // 0x58 : not user-modifiable
    0x01,  // 0x59 : not user-modifiable
    0x00,  // 0x5a : not user-modifiable
    0x08,  // 0x5b : not user-modifiable
    0x00,  // 0x5c : not user-modifiable
    0x00,  // 0x5d : not user-modifiable
    0x01,  // 0x5e : not user-modifiable
    0xDB,  // 0x5f : not user-modifiable
    0x0F,  // 0x60 : not user-modifiable
    0x01,  // 0x61 : not user-modifiable
    0xF1,  // 0x62 : not user-modifiable
    0x0D,  // 0x63 : not user-modifiable
    0x01,  // 0x64 : Sigma threshold MSB
    0x68,  // 0x65 : Sigma threshold LSB
    0x00,  // 0x66 : Min count Rate MSB
    0x80,  // 0x67 : Min count Rate LSB
    0x08,  // 0x68 : not user-modifiable
    0xB8,  // 0x69 : not user-modifiable
    0x00,  // 0x6a : not user-modifiable
    0x00,  // 0x6b : not user-modifiable
    0x00,  // 0x6c : Intermeasurement period MSB
    0x00,  // 0x6d : Intermeasurement period
    0x0F,  // 0x6e : Intermeasurement period
    0x89,  // 0x6f : Intermeasurement period LSB
    0x00,  // 0x70 : not user-modifiable
    0x00,  // 0x71 : not user-modifiable
    0x00,  // 0x72 : distance threshold high MSB
    0x00,  // 0x73 : distance threshold high LSB
    0x00,  // 0x74 : distance threshold low MSB
    0x00,  // 0x75 : distance threshold low LSB
    0x00,  // 0x76 : not user-modifiable
    0x01,  // 0x77 : not user-modifiable
    0x0F,  // 0x78 : not user-modifiable
    0x0D,  // 0x79 : not user-modifiable
    0x0E,  // 0x7a : not user-modifiable
    0x0E,  // 0x7b : not user-modifiable
    0x00,  // 0x7c : not user-modifiable
    0x00,  // 0x7d : not user-modifiable
    0x02,  // 0x7e : not user-modifiable
    0xC7,  // 0x7f : ROI center
    0xFF,  // 0x80 : XY ROI (X=Width, Y=Height)
    0x9B,  // 0x81 : not user-modifiable
    0x00,  // 0x82 : not user-modifiable
    0x00,  // 0x83 : not user-modifiable
    0x00,  // 0x84 : not user-modifiable
    0x01,  // 0x85 : not user-modifiable
    0x01,  // 0x86 : clear interrupt
    0x40,  // 0x87 : start ranging
};

VL53L1X::VL53L1X(TwoWire& wire, uint8_t address) : _wire(wire), _address(address) {}

bool VL53L1X::begin() {
    reset();
    delay(100);

    if (deviceId() != VL53L1X_DEVICE_ID) {
        return false;
    }

    writeRegBytes(REG_DEFAULT_CONFIG_START, DEFAULT_CONFIG, sizeof(DEFAULT_CONFIG));
    writeReg16(REG_RANGE_CONFIG_VCSEL_PERIOD_A, readReg16(REG_RESULT_OSC_CALIBRATE_VAL) * 4);
    delay(200);
    return true;
}

void VL53L1X::writeReg(uint16_t reg, uint8_t value) {
    _wire.beginTransmission(_address);
    _wire.write((reg >> 8) & 0xFF);
    _wire.write(reg & 0xFF);
    _wire.write(static_cast<uint8_t>(value));
    _wire.endTransmission();
}

void VL53L1X::writeReg16(uint16_t reg, uint16_t value) {
    _wire.beginTransmission(_address);
    _wire.write((reg >> 8) & 0xFF);
    _wire.write(reg & 0xFF);
    _wire.write(static_cast<uint8_t>((value >> 8) & 0xFF));
    _wire.write(static_cast<uint8_t>(value & 0xFF));
    _wire.endTransmission();
}

uint8_t VL53L1X::readReg(uint16_t reg) {
    _wire.beginTransmission(_address);
    _wire.write((reg >> 8) & 0xFF);
    _wire.write(reg & 0xFF);
    _wire.endTransmission(false);
    _wire.requestFrom(_address, static_cast<uint8_t>(1));
    if (_wire.available()) {
        return static_cast<uint8_t>(_wire.read());
    }
    return 0;
}

uint16_t VL53L1X::readReg16(uint16_t reg) {
    _wire.beginTransmission(_address);
    _wire.write((reg >> 8) & 0xFF);
    _wire.write(reg & 0xFF);
    _wire.endTransmission(false);
    _wire.requestFrom(_address, static_cast<uint8_t>(2));
    if (_wire.available() >= 2) {
        uint8_t msb = static_cast<uint8_t>(_wire.read());
        uint8_t lsb = static_cast<uint8_t>(_wire.read());
        return (msb << 8) | lsb;
    }
    return 0;
}

void VL53L1X::writeRegBytes(uint16_t reg, const uint8_t* data, size_t len) {
    _wire.beginTransmission(_address);
    _wire.write((reg >> 8) & 0xFF);
    _wire.write(reg & 0xFF);
    for (size_t i = 0; i < len; i++) {
        _wire.write(data[i]);
    }
    _wire.endTransmission();
}

uint16_t VL53L1X::deviceId() {
    return readReg16(REG_MODEL_ID);
}

void VL53L1X::reset() {
    writeReg(REG_SOFT_RESET, SOFT_RESET_ASSERT);
    delay(100);
    writeReg(REG_SOFT_RESET, SOFT_RESET_RELEASE);
}

void VL53L1X::powerOff() {
    writeReg(REG_SOFT_RESET, SOFT_RESET_ASSERT);
}

void VL53L1X::powerOn() {
    writeReg(REG_SOFT_RESET, SOFT_RESET_RELEASE);
    delay(100);
}

void VL53L1X::startRanging() {
    writeReg(REG_SYSTEM_START, RANGING_START);
}

void VL53L1X::stopRanging() {
    writeReg(REG_SYSTEM_START, RANGING_STOP);
}

bool VL53L1X::dataReady() {
    bool polarity = (readReg(REG_GPIO_HV_MUX_CTRL) & GPIO_HV_MUX_CTRL_POLARITY) != 0;
    uint8_t readyVal = polarity ? 0 : 1;
    return (readReg(REG_GPIO_TIO_HV_STATUS) & GPIO_TIO_HV_STATUS_DATA_READY) == readyVal;
}

void VL53L1X::clearInterrupt() {
    writeReg(REG_SYSTEM_INTERRUPT_CLEAR, INTERRUPT_CLEAR);
}

bool VL53L1X::ensureData() {
    if (!dataReady()) {
        startRanging();
        for (uint16_t i = 0; i < 100; i++) {
            if (dataReady()) {
                return true;
            }
            delay(10);
        }
        return false;
    }
    return true;
}

uint16_t VL53L1X::distanceMm() {
    ensureData();
    uint8_t data[RESULT_BLOCK_SIZE] = {};
    _wire.beginTransmission(_address);
    _wire.write((REG_RESULT_RANGE_STATUS >> 8) & 0xFF);
    _wire.write(REG_RESULT_RANGE_STATUS & 0xFF);
    _wire.endTransmission(false);
    _wire.requestFrom(_address, static_cast<uint8_t>(RESULT_BLOCK_SIZE));
    for (uint8_t i = 0; i < RESULT_BLOCK_SIZE && _wire.available(); i++) {
        data[i] = static_cast<uint8_t>(_wire.read());
    }
    uint16_t distance = static_cast<uint16_t>(data[RESULT_DISTANCE_MSB_OFFSET] << 8) +
                        data[RESULT_DISTANCE_LSB_OFFSET];
    clearInterrupt();
    return distance;
}

uint16_t VL53L1X::read() {
    return distanceMm();
}