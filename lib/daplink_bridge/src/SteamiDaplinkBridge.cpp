// SPDX-License-Identifier: GPL-3.0-or-later
#include "SteamiDaplinkBridge.h"

#include <string.h>

#include <algorithm>

SteamiDaplinkBridge::SteamiDaplinkBridge(TwoWire& wire, uint8_t address)
    : _wire(&wire), _address(address) {}

// ---------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------

bool SteamiDaplinkBridge::begin() {
    if (!isPresent()) {
        return false;
    }
    return deviceId() == DAPLINK_BRIDGE_WHO_AM_I;
}

uint8_t SteamiDaplinkBridge::deviceId() {
    uint8_t id = 0;
    return readReg(DAPLINK_BRIDGE_CMD_WHO_AM_I, id) ? id : 0;
}

// ---------------------------------------------------------------------
// Status
// ---------------------------------------------------------------------

bool SteamiDaplinkBridge::busy() {
    // A bridge that does not answer is not idle: treating a failed read as
    // "not busy" would let the next command race the current one.
    uint8_t status = 0;
    return !readReg(DAPLINK_BRIDGE_REG_STATUS, status) ||
           (status & DAPLINK_BRIDGE_STATUS_BUSY) != 0;
}

// ---------------------------------------------------------------------
// Config zone
// ---------------------------------------------------------------------

bool SteamiDaplinkBridge::hasConfigZone() {
    if (_configZone == ConfigZoneSupport::Unknown) {
        _configZone = probeConfigZone();
    }
    return _configZone == ConfigZoneSupport::Present;
}

SteamiDaplinkBridge::ConfigZoneSupport SteamiDaplinkBridge::probeConfigZone() {
    // Ask for flash sector 0, a harmless read that every firmware accepts
    // with its 2-byte sector number. Firmwares without the config zone
    // only handle a 1-byte sector number and flag the request as failed;
    // newer ones read the sector without error. Reading the error
    // register clears it, so start from a clean state.
    //
    // Anything but a clean "no error" must keep the config commands away:
    // the firmwares without the zone lock up on them.
    uint8_t stale = 0;
    if (!waitNotBusy(DAPLINK_BRIDGE_PROBE_TIMEOUT_MS) ||
        !readReg(DAPLINK_BRIDGE_REG_ERROR, stale)) {
        return ConfigZoneSupport::Unknown;
    }

    const uint8_t frame[3] = {DAPLINK_BRIDGE_CMD_READ_SECTOR, 0x00, 0x00};
    if (!writeFrame(frame, sizeof(frame))) {
        return ConfigZoneSupport::Unknown;
    }
    switch (commandResult(DAPLINK_BRIDGE_PROBE_SETTLE_MS, DAPLINK_BRIDGE_PROBE_TIMEOUT_MS)) {
        case CommandResult::Ok:
            return ConfigZoneSupport::Present;
        case CommandResult::Failed:
            return ConfigZoneSupport::Absent;
        default:
            return ConfigZoneSupport::Unknown;
    }
}

bool SteamiDaplinkBridge::clearConfig() {
    if (!hasConfigZone()) {
        return false;
    }
    if (!waitNotBusy(DAPLINK_BRIDGE_WRITE_TIMEOUT_MS)) {
        return false;
    }

    const uint8_t cmd = DAPLINK_BRIDGE_CMD_CLEAR_CONFIG;
    if (!writeFrame(&cmd, 1)) {
        return false;
    }
    return commandResult(DAPLINK_BRIDGE_CLEAR_CONFIG_DELAY_MS, DAPLINK_BRIDGE_CLEAR_TIMEOUT_MS) ==
           CommandResult::Ok;
}

bool SteamiDaplinkBridge::writeConfig(const uint8_t* data, size_t length, uint16_t offset) {
    if (length == 0) {
        return true;
    }
    if (offset >= DAPLINK_BRIDGE_CONFIG_SIZE || length > DAPLINK_BRIDGE_CONFIG_SIZE - offset) {
        return false;
    }
    if (!hasConfigZone()) {
        return false;
    }
    if (!waitNotBusy(DAPLINK_BRIDGE_WRITE_TIMEOUT_MS)) {
        return false;
    }

    // Frame layout: [CMD | offsetHi | offsetLo | chunkLen | payload...]
    constexpr size_t kHeaderLen = 4;
    uint8_t buf[kHeaderLen + DAPLINK_BRIDGE_MAX_WRITE_CHUNK];

    size_t pos = 0;

    while (pos < length) {
        const size_t remaining = length - pos;

        const uint8_t chunkLen =
            static_cast<uint8_t>(std::min<size_t>(DAPLINK_BRIDGE_MAX_WRITE_CHUNK, remaining));

        const uint16_t curOffset = static_cast<uint16_t>(offset + pos);

        // The firmware waits for a full 32-byte frame before running the
        // command: pad the unused payload bytes with zeros.
        memset(buf, 0, sizeof(buf));

        buf[0] = DAPLINK_BRIDGE_CMD_WRITE_CONFIG;
        buf[1] = static_cast<uint8_t>((curOffset >> 8) & 0xFF);
        buf[2] = static_cast<uint8_t>(curOffset & 0xFF);
        buf[3] = chunkLen;

        memcpy(&buf[kHeaderLen], &data[pos], chunkLen);

        // Check every chunk: a failure is only visible until the next
        // command reaches the bridge.
        if (!writeFrame(buf, sizeof(buf)) ||
            commandResult(DAPLINK_BRIDGE_WRITE_CONFIG_DELAY_MS, DAPLINK_BRIDGE_WRITE_TIMEOUT_MS) !=
                CommandResult::Ok) {
            return false;
        }

        pos += chunkLen;
    }
    return true;
}

bool SteamiDaplinkBridge::writeConfig(const char* str, uint16_t offset) {
    if (str == nullptr) {
        return false;
    }
    return writeConfig(reinterpret_cast<const uint8_t*>(str), strlen(str), offset);
}

size_t SteamiDaplinkBridge::readConfig(uint8_t* result, size_t maxLen) {
    if (result == nullptr || maxLen == 0 || !hasConfigZone()) {
        return 0;
    }

    // Each READ_CONFIG loads 256 bytes from `offset` into the bridge TX
    // buffer, and each I2C read returns that buffer from its first byte.
    // Reads are capped at 255 bytes, so walk the zone with one READ_CONFIG
    // per read. The offset may not pass CONFIG_SIZE - 256: near the end,
    // ask for the last block and skip the bytes already returned. That
    // reaches every byte but the very last one of the zone.
    constexpr uint16_t kLastOffset = DAPLINK_BRIDGE_CONFIG_SIZE - DAPLINK_BRIDGE_READ_CONFIG_LEN;
    const size_t limit = std::min<size_t>(maxLen, DAPLINK_BRIDGE_CONFIG_SIZE);
    uint8_t chunk[DAPLINK_BRIDGE_MAX_READ_LEN];

    size_t produced = 0;
    while (produced < limit) {
        const uint16_t offset = static_cast<uint16_t>(std::min<size_t>(produced, kLastOffset));
        const size_t skip = produced - offset;
        if (skip >= DAPLINK_BRIDGE_MAX_READ_LEN) {
            break;
        }
        const size_t want = std::min<size_t>(limit - produced, DAPLINK_BRIDGE_MAX_READ_LEN - skip);
        const uint8_t length = static_cast<uint8_t>(skip + want);

        if (!waitNotBusy(DAPLINK_BRIDGE_READ_TIMEOUT_MS)) {
            break;
        }

        const uint8_t frame[3] = {DAPLINK_BRIDGE_CMD_READ_CONFIG,
                                  static_cast<uint8_t>((offset >> 8) & 0xFF),
                                  static_cast<uint8_t>(offset & 0xFF)};
        _wire->beginTransmission(_address);
        _wire->write(frame, sizeof(frame));
        if (_wire->endTransmission() != 0) {
            break;
        }
        delay(DAPLINK_BRIDGE_READ_CONFIG_DELAY_MS);

        if (_wire->requestFrom(_address, length) != length) {
            break;
        }
        for (uint8_t i = 0; i < length; ++i) {
            chunk[i] = static_cast<uint8_t>(_wire->read());
        }

        for (size_t i = skip; i < length; ++i) {
            // 0xFF marks the first unused byte of the zone.
            if (chunk[i] == 0xFF) {
                return produced;
            }
            result[produced++] = chunk[i];
        }
    }
    return produced;
}

// ---------------------------------------------------------------------
// Raw protocol
// ---------------------------------------------------------------------

bool SteamiDaplinkBridge::waitReady(uint32_t timeoutMs) {
    return waitNotBusy(timeoutMs);
}

bool SteamiDaplinkBridge::sendCommand(uint8_t cmd, const uint8_t* args, size_t argLen) {
    if (argLen > DAPLINK_BRIDGE_MAX_ARGS || (argLen > 0 && args == nullptr)) {
        return false;
    }
    uint8_t frame[1 + DAPLINK_BRIDGE_MAX_ARGS];
    frame[0] = cmd;
    if (argLen > 0) {
        memcpy(&frame[1], args, argLen);
    }
    return writeFrame(frame, 1 + argLen);
}

bool SteamiDaplinkBridge::commandSucceeded(uint32_t settleMs, uint32_t timeoutMs) {
    return commandResult(settleMs, timeoutMs) == CommandResult::Ok;
}

bool SteamiDaplinkBridge::readError(uint8_t& error) {
    return readReg(DAPLINK_BRIDGE_REG_ERROR, error);
}

size_t SteamiDaplinkBridge::readResponse(uint8_t* buf, size_t length) {
    if (buf == nullptr || length == 0) {
        return 0;
    }
#if defined(STM32_CORE_VERSION_MAJOR) && (STM32_CORE_VERSION_MAJOR < 3)
    // STM32duino 2.x reads at most 255 bytes at once.
    if (length > DAPLINK_BRIDGE_MAX_READ_LEN) {
        return 0;
    }
    const size_t got = _wire->requestFrom(_address, static_cast<uint8_t>(length));
#else
    const size_t got = _wire->requestFrom(_address, length);
#endif
    size_t n = 0;
    while (n < got && _wire->available()) {
        buf[n++] = static_cast<uint8_t>(_wire->read());
    }
    return n;
}

// ---------------------------------------------------------------------
// I2C helpers
// ---------------------------------------------------------------------

bool SteamiDaplinkBridge::isPresent() {
    _wire->beginTransmission(_address);
    return _wire->endTransmission() == 0;
}

bool SteamiDaplinkBridge::readReg(uint8_t reg, uint8_t& value) {
    // Retry while the bridge refuses the request: the F103 answers reads
    // from its main loop and NACKs for a while after sending a block.
    // Once the bridge took the request, reading the error register has
    // cleared it: a retry would only read the cleared value.
    const bool retryRead = reg != DAPLINK_BRIDGE_REG_ERROR;
    const uint32_t start = millis();
    while (true) {
        _wire->beginTransmission(_address);
        _wire->write(reg);
        if (_wire->endTransmission(false) == 0) {
            if (_wire->requestFrom(_address, static_cast<uint8_t>(1)) == 1 && _wire->available()) {
                value = static_cast<uint8_t>(_wire->read());
                return true;
            }
            if (!retryRead) {
                return false;
            }
        }
        if ((millis() - start) > DAPLINK_BRIDGE_REG_TIMEOUT_MS) {
            return false;
        }
        delay(2);
    }
}

bool SteamiDaplinkBridge::writeFrame(const uint8_t* data, size_t len) {
    _wire->beginTransmission(_address);
    _wire->write(data, len);
    return _wire->endTransmission() == 0;
}

// ---------------------------------------------------------------------
// Status / error
// ---------------------------------------------------------------------

SteamiDaplinkBridge::CommandResult SteamiDaplinkBridge::commandResult(uint32_t settleMs,
                                                                      uint32_t timeoutMs) {
    // The firmware reports some failures only through the "last command
    // failed" bit of the error register, and clears that bit when it
    // receives the next command, a STATUS poll included. So send nothing
    // until the command has run: wait, read the error register first, and
    // only then check that the bridge is idle. Every command reports busy
    // until it has run, so an idle bridge proves the error register was
    // read after the command finished.
    delay(settleMs);
    uint8_t err = 0;
    uint8_t status = 0;
    if (!readReg(DAPLINK_BRIDGE_REG_ERROR, err) || !readReg(DAPLINK_BRIDGE_REG_STATUS, status)) {
        return CommandResult::Unknown;
    }
    if ((status & DAPLINK_BRIDGE_STATUS_BUSY) != 0) {
        // Still running: its outcome can no longer be read.
        waitNotBusy(timeoutMs);
        return CommandResult::Unknown;
    }
    return err == 0 ? CommandResult::Ok : CommandResult::Failed;
}

bool SteamiDaplinkBridge::waitNotBusy(uint32_t timeoutMs) {
    const uint32_t start = millis();
    while (busy()) {
        if ((millis() - start) > timeoutMs) {
            return false;
        }
        delay(2);
    }
    return true;
}
