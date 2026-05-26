// SPDX-License-Identifier: GPL-3.0-or-later
#include "DaplinkBridge.h"

#include <string.h>

#include <algorithm>

DaplinkBridge::DaplinkBridge(TwoWire& wire, uint8_t address) : _wire(&wire), _address(address) {}

// ---------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------

bool DaplinkBridge::begin() {
    if (!isPresent()) {
        return false;
    }
    return deviceId() == DAPLINK_BRIDGE_WHO_AM_I;
}

uint8_t DaplinkBridge::deviceId() {
    return readReg(DAPLINK_BRIDGE_CMD_WHO_AM_I);
}

// ---------------------------------------------------------------------
// Status
// ---------------------------------------------------------------------

bool DaplinkBridge::busy() {
    return (status() & DAPLINK_BRIDGE_STATUS_BUSY) != 0;
}

// ---------------------------------------------------------------------
// Config zone
// ---------------------------------------------------------------------

bool DaplinkBridge::clearConfig() {
    if (!waitNotBusy(DAPLINK_BRIDGE_WRITE_TIMEOUT_MS)) {
        return false;
    }
    uint8_t cmd = DAPLINK_BRIDGE_CMD_CLEAR_CONFIG;
    writeFrame(&cmd, 1);
    if (!waitNotBusy(DAPLINK_BRIDGE_CLEAR_TIMEOUT_MS)) {
        return false;
    }
    return error() == 0;
}

bool DaplinkBridge::writeConfig(const uint8_t* data, size_t length, uint16_t offset) {
    if (length == 0) {
        return true;
    }
    if (offset >= DAPLINK_BRIDGE_CONFIG_SIZE || length > DAPLINK_BRIDGE_CONFIG_SIZE - offset) {
        return false;
    }

    // Frame layout: [CMD | offsetHi | offsetLo | chunkLen | payload...]
    constexpr size_t kHeaderLen = 4;
    uint8_t buf[kHeaderLen + DAPLINK_BRIDGE_MAX_WRITE_CHUNK];

    size_t pos = 0;
    while (pos < length) {
        if (!waitNotBusy(DAPLINK_BRIDGE_WRITE_TIMEOUT_MS)) {
            return false;
        }
        const size_t remaining = length - pos;
        const uint8_t chunkLen =
            static_cast<uint8_t>(std::min<size_t>(DAPLINK_BRIDGE_MAX_WRITE_CHUNK, remaining));
        const uint16_t curOffset = static_cast<uint16_t>(offset + pos);

        buf[0] = DAPLINK_BRIDGE_CMD_WRITE_CONFIG;
        buf[1] = static_cast<uint8_t>((curOffset >> 8) & 0xFF);
        buf[2] = static_cast<uint8_t>(curOffset & 0xFF);
        buf[3] = chunkLen;
        memcpy(&buf[kHeaderLen], &data[pos], chunkLen);

        writeFrame(buf, kHeaderLen + chunkLen);
        pos += chunkLen;
    }

    if (!waitNotBusy(DAPLINK_BRIDGE_WRITE_TIMEOUT_MS)) {
        return false;
    }
    return error() == 0;
}

bool DaplinkBridge::writeConfig(const char* str, uint16_t offset) {
    if (str == nullptr) {
        return false;
    }
    return writeConfig(reinterpret_cast<const uint8_t*>(str), strlen(str), offset);
}

size_t DaplinkBridge::readConfig(uint8_t* result, size_t maxLen) {
    if (result == nullptr || maxLen == 0) {
        return 0;
    }

    size_t produced = 0;
    while (produced < maxLen) {
        if (!waitNotBusy(DAPLINK_BRIDGE_READ_TIMEOUT_MS)) {
            return produced;
        }

        uint8_t chunk[DAPLINK_BRIDGE_MAX_READ_CHUNK];
        const uint8_t want = static_cast<uint8_t>(
            std::min<size_t>(DAPLINK_BRIDGE_MAX_READ_CHUNK, maxLen - produced));
        readBlock(DAPLINK_BRIDGE_CMD_READ_CONFIG, chunk, want);

        for (uint8_t i = 0; i < want; ++i) {
            // 0xFF marks the first unused byte in the config zone — treat
            // it as end-of-string and stop returning data to the caller.
            if (chunk[i] == 0xFF) {
                return produced;
            }
            result[produced++] = chunk[i];
            if (produced == maxLen) {
                return produced;
            }
        }
    }
    return produced;
}

// ---------------------------------------------------------------------
// I2C helpers
// ---------------------------------------------------------------------

bool DaplinkBridge::isPresent() {
    _wire->beginTransmission(_address);
    return _wire->endTransmission() == 0;
}

uint8_t DaplinkBridge::readReg(uint8_t reg) {
    _wire->beginTransmission(_address);
    _wire->write(reg);
    _wire->endTransmission(false);
    _wire->requestFrom(_address, static_cast<uint8_t>(1));
    if (_wire->available()) {
        return static_cast<uint8_t>(_wire->read());
    }
    return 0;
}

void DaplinkBridge::readBlock(uint8_t reg, uint8_t* buf, uint8_t len) {
    for (uint8_t i = 0; i < len; ++i) {
        buf[i] = 0;
    }
    _wire->beginTransmission(_address);
    _wire->write(reg);
    _wire->endTransmission(false);
    _wire->requestFrom(_address, len);
    for (uint8_t i = 0; i < len && _wire->available(); ++i) {
        buf[i] = static_cast<uint8_t>(_wire->read());
    }
}

void DaplinkBridge::writeFrame(const uint8_t* data, size_t len) {
    _wire->beginTransmission(_address);
    for (size_t i = 0; i < len; ++i) {
        _wire->write(data[i]);
    }
    _wire->endTransmission();
}

// ---------------------------------------------------------------------
// Status / error
// ---------------------------------------------------------------------

uint8_t DaplinkBridge::status() {
    return readReg(DAPLINK_BRIDGE_REG_STATUS);
}

uint8_t DaplinkBridge::error() {
    return readReg(DAPLINK_BRIDGE_REG_ERROR);
}

bool DaplinkBridge::waitNotBusy(uint32_t timeoutMs) {
    const uint32_t start = millis();
    while (busy()) {
        if ((millis() - start) > timeoutMs) {
            return false;
        }
        delay(2);
    }
    return true;
}
