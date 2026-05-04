// SPDX-License-Identifier: GPL-3.0-or-later
#include "daplink_flash.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>

DaplinkFlash::DaplinkFlash(DaplinkBridge& bridge) : _bridge(&bridge) {}

// --------------------------------------------------
// Filename management
// --------------------------------------------------

bool DaplinkFlash::begin() {
    return _bridge->begin();
}

void DaplinkFlash::setFilename(const char* name, const char* ext) {
    // Set 8.3 filename. name: max 8 chars, ext: max 3 chars.
    char n[DAPLINK_FLASH_FILENAME_LEN];
    size_t nameLen = std::min(strlen(name), (size_t(DAPLINK_FLASH_FILENAME_LEN)));
    for (int i = 0; i < nameLen; i++) {
        n[i] = toupper((unsigned char)name[i]);
    }

    char e[DAPLINK_FLASH_EXT_LEN];
    size_t extLen = std::min(strlen(ext), (size_t(DAPLINK_FLASH_EXT_LEN)));
    for (int j = 0; j < extLen; j++) {
        e[j] = toupper((unsigned char)ext[j]);
    }

    uint8_t padded[DAPLINK_FLASH_FILENAME_LEN + DAPLINK_FLASH_EXT_LEN];
    memset(padded, ' ', sizeof(padded));
    memcpy(padded, n, nameLen);
    memcpy(padded + DAPLINK_FLASH_FILENAME_LEN, e, extLen);

    _bridge->sendCommand(DAPLINK_FLASH_CMD_SET_FILENAME, padded, sizeof(padded));
}

DaplinkFlash::FilenameResult DaplinkFlash::getFilename() {
    uint8_t raw[DAPLINK_FLASH_FILENAME_LEN + DAPLINK_FLASH_EXT_LEN];
    _bridge->readResponse(DAPLINK_FLASH_CMD_GET_FILENAME, raw,
                          DAPLINK_FLASH_FILENAME_LEN + DAPLINK_FLASH_EXT_LEN);
    FilenameResult result;

    memcpy(result.name, raw, DAPLINK_FLASH_FILENAME_LEN);
    result.name[DAPLINK_FLASH_FILENAME_LEN] = '\0';
    size_t nameLen = strnlen(result.name, DAPLINK_FLASH_FILENAME_LEN);
    while (nameLen > 0 && result.name[nameLen - 1] == ' ')
        nameLen--;
    result.name[nameLen] = '\0';

    memcpy(result.ext, raw + DAPLINK_FLASH_FILENAME_LEN, DAPLINK_FLASH_EXT_LEN);
    result.ext[DAPLINK_FLASH_EXT_LEN] = '\0';
    size_t extLen = strnlen(result.ext, DAPLINK_FLASH_EXT_LEN);
    while (extLen > 0 && result.ext[extLen - 1] == ' ')
        extLen--;
    result.ext[extLen] = '\0';

    return result;
}

// --------------------------------------------------
// Flash operations
// --------------------------------------------------

void DaplinkFlash::clearFlash() {
    // Erase entire flash memory.
    _bridge->sendCommand(DAPLINK_FLASH_CMD_CLEAR_FLASH);
}

size_t DaplinkFlash::write(const uint8_t* data, size_t length) {
    // Append data to the current file. Returns the number of bytes
    if (data == nullptr && length == 0) {
        return 0;
    }

    uint8_t payload[1 + DAPLINK_FLASH_MAX_WRITE_CHUNK];
    size_t offset = 0;

    while (offset < length) {
        uint8_t chunkLen =
            static_cast<uint8_t>(std::min<size_t>(DAPLINK_FLASH_MAX_WRITE_CHUNK, length - offset));

        payload[0] = chunkLen;
        memcpy(&payload[1], &data[offset], chunkLen);

        if (!_bridge->sendCommand(DAPLINK_FLASH_CMD_WRITE_DATA, payload, 1 + chunkLen)) {
            return 0;
        }
        offset += chunkLen;
    }
    return length;
}

size_t DaplinkFlash::write(const char* data) {
    return write(reinterpret_cast<const uint8_t*>(data), strlen(data));
}

size_t DaplinkFlash::writeLine(const char* text) {
    if (text == nullptr) {
        return 0;
    }

    size_t len = strlen(text);
    std::vector<uint8_t> buf(len + 1);
    memcpy(buf.data(), text, len);
    buf[len] = '\n';

    size_t written = write(buf.data(), buf.size());
    if (written == 0) {
        return 0;
    }
    return written;
}

// --------------------------------------------------
// Read operations
// --------------------------------------------------

void DaplinkFlash::readSector(uint16_t sector, uint8_t* buf) {
    uint8_t payload[2];
    payload[0] = (sector >> 8) & 0xFF;
    payload[1] = sector & 0xFF;

    _bridge->sendCommand(DAPLINK_FLASH_CMD_READ_SECTOR, payload, sizeof(payload));
    _bridge->readResponse(DAPLINK_FLASH_CMD_READ_SECTOR, buf, DAPLINK_FLASH_SECTOR_SIZE);
}

size_t DaplinkFlash::readUntilSentinel(uint8_t* result, size_t maxLen) {
    if (result == nullptr || maxLen == 0) {
        return 0;
    }

    size_t resultLen = 0;

    for (uint16_t sector = 0; sector < DAPLINK_FLASH_MAX_SECTORS; sector++) {
        uint8_t data[DAPLINK_FLASH_SECTOR_SIZE];
        readSector(sector, data);
        for (int i = 0; i < DAPLINK_FLASH_SECTOR_SIZE; i++) {
            if (data[i] == 0xFF) {
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

size_t DaplinkFlash::readN(uint8_t* result, size_t n) {
    if (result == nullptr || n == 0) {
        return 0;
    }

    size_t resultLen = 0;

    for (uint16_t sector = 0; sector < DAPLINK_FLASH_MAX_SECTORS; sector++) {
        uint8_t data[DAPLINK_FLASH_SECTOR_SIZE];
        readSector(sector, data);

        for (size_t i = 0; i < DAPLINK_FLASH_SECTOR_SIZE; i++) {
            if (resultLen >= n) {
                return resultLen;
            }

            result[resultLen++] = data[i];
        }
    }

    return resultLen;
}
