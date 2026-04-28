// SPDX-License-Identifier: GPL-3.0-or-later
#include "daplink_bridge.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

#include <algorithm>
#include <cstdio>

daplink_bridge::daplink_bridge(TwoWire& wire, uint8_t address)
    : _wire(&wire), _address(address), _buffer1(0) {}

// --------------------------------------------------
// Low level I2C
// --------------------------------------------------

uint8_t daplink_bridge::readReg(uint8_t reg) {
    // Read n bytes from register.
    _wire->beginTransmission(_address);
    _wire->write(reg);
    _wire->endTransmission(false);
    _wire->requestFrom(_address, (uint8_t)1);
    _buffer1 = _wire->read();
    return _buffer1;
}

void daplink_bridge::readBlock(uint8_t reg, uint8_t* buf, uint16_t n) {
    _wire->beginTransmission(_address);
    _wire->write(reg);
    _wire->endTransmission(false);
    _wire->requestFrom(_address, (uint16_t)n);
    _wire->requestFrom(_address, static_cast<uint8_t>(n < 256 ? n : 255));

    for (size_t i = 0; i < n && _wire->available(); ++i) {
        buf[i] = static_cast<uint8_t>(_wire->read());
    }
}

void daplink_bridge::writeReg(uint8_t reg, uint8_t data) {
    // Read and return multiple bytes starting at a register.
    _wire->beginTransmission(_address);
    _wire->write(reg);
    _wire->write(data);
    _wire->endTransmission();
}

void daplink_bridge::writeCmd(uint8_t cmd) {
    // Write a single command byte (no payload).
    _buffer1 = cmd;
    writeTo(&_buffer1, 1);
}

void daplink_bridge::writeTo(uint8_t* data, size_t len) {
    // Write raw data frame to the bridge.
    _wire->beginTransmission(_address);
    for (int i = 0; i < len; i++) {
        _wire->write(data[i]);
    }
    _wire->endTransmission();
}

void daplink_bridge::readFrom(uint8_t* n, size_t len) {
    // Read n raw bytes from the bridge.
    _wire->requestFrom(_address, uint8_t(len));
    for (int i = 0; i < len; i++) {
        n[i] = static_cast<uint8_t>(_wire->read());
    }
}

// --------------------------------------------------
// Device identification
// --------------------------------------------------

uint8_t daplink_bridge::deviceId() {
    // Read WHO_AM_I register. Expected: 0x4C.
    return readReg(CMD_WHO_AM_I);
}

// --------------------------------------------------
// Status and error registers
// --------------------------------------------------

uint8_t daplink_bridge::status() {
    // Read raw status register.
    return readReg(REG_STATUS);
}

uint8_t daplink_bridge::error() {
    // Read raw error register.
    return readReg(REG_ERROR);
}

bool daplink_bridge::busy() {
    // Return True if bridge is busy.
    return bool(status() & STATUS_BUSY);
}

bool daplink_bridge::waitBusy(uint32_t timeout_ms) {
    // Poll busy bit until clear. Raises OSError on timeout.
    uint32_t elapsed = 0;
    while (busy()) {
        delay(5);
        elapsed += 5;
        if (elapsed >= timeout_ms) {
            return false;
        }
    }
    return true;
}

// --------------------------------------------------
// Config zone (1 KB persistent storage in F103 internal flash)
// --------------------------------------------------

bool daplink_bridge::clearConfig() {
    // Erase the 1 KB config zone.
    waitBusy();
    writeCmd(CMD_CLEAR_CONFIG);
    delay(100);
    waitBusy();
    uint8_t err = error();
    if (err) {
        return error() == 0;
    }
    return true;
}

bool daplink_bridge::writeConfig(const uint8_t* data, size_t length, uint16_t offset) {
    /*Write data to the config zone at the given offset.

        Args:
            data: bytes or str to store.
            offset: byte offset within the config zone (0-1023).*/

    if (offset < 0 or offset >= CONFIG_SIZE) {
        return false;
    }
    if ((offset + length) > CONFIG_SIZE) {
        return false;
    }

    uint8_t buf[MAX_WRITE_CHUNK + 4];
    uint8_t maxPayload = sizeof(buf) - 4;
    buf[0] = CMD_WRITE_CONFIG;
    size_t pos = 0;

    while (pos < length) {
        waitBusy();
        uint8_t chunkLen = (uint8_t)std::min((size_t)maxPayload, length - pos);
        uint16_t curOffset = offset + pos;
        buf[1] = (curOffset >> 8) & 0xff;
        buf[2] = curOffset & 0xff;
        buf[3] = chunkLen;
        memcpy(&buf[4], &data[pos], chunkLen);
        for (size_t i = 4 + chunkLen; i < sizeof(buf); i++) {
            buf[i] = 0;
        }
        writeTo(buf, sizeof(buf));
        delay(50);
        pos += chunkLen;
    }
    waitBusy();
    uint8_t err = error();
    if (err) {
        return error() == 0;
    }
    return true;
}

bool daplink_bridge::writeConfig(const char* data, uint16_t offset) {
    return writeConfig(reinterpret_cast<const uint8_t*>(data), strlen(data), offset);
}

size_t daplink_bridge::readConfig(uint8_t* result, size_t maxLen) {
    /*Read config zone content.

        Returns:
            bytes: config data up to first 0xFF, or b'' if empty.*/

    size_t resultLen = 0;
    for (uint16_t pageOffset = 0; pageOffset < CONFIG_SIZE; pageOffset += SECTOR_SIZE) {
        waitBusy();
        delay(100);
        uint8_t data[SECTOR_SIZE];
        memset(data, 0xFF, SECTOR_SIZE);
        readBlock(CMD_READ_CONFIG, data, (uint16_t)255);
        printf("data[0]=0x%02X data[1]=0x%02X data[2]=0x%02X\n", data[0], data[1], data[2]);
        for (int i = 0; i < SECTOR_SIZE; i++) {
            if (data[i] == 0xff) {
                return resultLen;
            }
            if (resultLen >= maxLen) {
                return resultLen;
            }
            result[resultLen++] = data[i];
        }
    }
    return resultLen;
}