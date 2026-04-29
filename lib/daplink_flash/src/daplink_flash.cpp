// SPDX-License-Identifier: GPL-3.0-or-later
#include "daplink_flash.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

DaplinkFlash::DaplinkFlash(daplink_bridge& bridge) : _bridge(&bridge) {}

// --------------------------------------------------
// Filename management
// --------------------------------------------------

void DaplinkFlash::setFilename(const char* name, const char* ext) {
    // Set 8.3 filename. name: max 8 chars, ext: max 3 chars.
    _bridge->waitBusy();
    char n[FILENAME_LEN];
    size_t nameLen = std::min(strlen(name), (size_t(FILENAME_LEN)));
    for (int i = 0; i < nameLen; i++) {
        n[i] = toupper((unsigned char)name[i]);
    }

    char e[EXT_LEN];
    size_t extLen = std::min(strlen(ext), (size_t(EXT_LEN)));
    for (int j = 0; j < extLen; j++) {
        e[j] = toupper((unsigned char)ext[j]);
    }

    uint8_t padded[FILENAME_LEN + EXT_LEN];
    memset(padded, ' ', sizeof(padded));
    memcpy(padded, n, nameLen);
    memcpy(padded + FILENAME_LEN, e, extLen);

    uint8_t buf[1 + FILENAME_LEN + EXT_LEN];
    buf[0] = CMD_SET_FILENAME;
    memcpy(buf + 1, padded, sizeof(padded));
    _bridge->writeTo(buf, sizeof(buf));
}

DaplinkFlash::FilenameResult DaplinkFlash::getFilename() {
    _bridge->waitBusy();
    uint8_t raw[FILENAME_LEN + EXT_LEN];
    _bridge->readBlock(CMD_GET_FILENAME, raw, FILENAME_LEN + EXT_LEN);
    FilenameResult result;

    memcpy(result.name, raw, FILENAME_LEN);
    result.name[FILENAME_LEN] = '\0';
    size_t nameLen = strnlen(result.name, FILENAME_LEN);
    while (nameLen > 0 && result.name[nameLen - 1] == ' ')
        nameLen--;
    result.name[nameLen] = '\0';

    memcpy(result.ext, raw + FILENAME_LEN, EXT_LEN);
    result.ext[EXT_LEN] = '\0';
    size_t extLen = strnlen(result.ext, EXT_LEN);
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
    _bridge->waitBusy();
    _bridge->writeCmd(CMD_CLEAR_FLASH);
}

size_t DaplinkFlash::write(const uint8_t* data, size_t length) {
    /*Append data to current file. data: bytes or str.

        Returns the number of bytes written.*/

    size_t offset = 0;
    uint8_t buf[MAX_WRITE_CHUNK + 2];
    buf[0] = CMD_WRITE_DATA;

    while (offset < length) {
        _bridge->waitBusy();
        uint8_t chunkLen = (uint8_t)std::min((size_t)MAX_WRITE_CHUNK, length - offset);
        buf[1] = chunkLen;
        memcpy(&buf[2], &data[offset], chunkLen);
        for (size_t i = 2 + chunkLen; i < sizeof(buf); i++) {
            buf[i] = 0;
        }
        _bridge->writeTo(buf, sizeof(buf));
        offset += chunkLen;
    }
    _bridge->waitBusy();
    if (_bridge->error()) {
        return 0;
    }
    return length;
}

size_t DaplinkFlash::write(const char* data) {
    return write(reinterpret_cast<const uint8_t*>(data), strlen(data));
}

size_t DaplinkFlash::writeLine(const char* text) {
    size_t len = strlen(text);
    uint8_t buf[len + 1];
    strncpy(reinterpret_cast<char*>(buf), text, len);
    buf[len] = '\n';
    return write(buf, len + 1);
}

// --------------------------------------------------
// Read operations
// --------------------------------------------------

void DaplinkFlash::readSector(uint16_t sector, uint8_t* buf) {
    _bridge->waitBusy();
    uint8_t payload[3] = {CMD_READ_SECTOR, (uint8_t)(sector >> 8), (uint8_t)(sector & 0xFF)};
    _bridge->writeTo(payload, 3);
    delay(200);
    _bridge->readFrom(buf, SECTOR_SIZE);
}

size_t DaplinkFlash::read(uint8_t* result, size_t maxLen, bool limitLen) {
    if (limitLen && maxLen == 0) {
        return 0;
    }
    size_t resultLen = 0;
    uint16_t sector = 0;

    while (sector < MAX_SECTORS) {
        uint8_t data[SECTOR_SIZE];
        readSector(sector, data);
        for (int i = 0; i < SECTOR_SIZE; i++) {
            if (limitLen) {
                result[resultLen++] = data[i];
                if (resultLen >= maxLen) {
                    return resultLen;
                }
            } else {
                if (data[i] == 0xFF) {
                    return resultLen;
                }
                result[resultLen++] = data[i];
            }
        }
        sector++;
    }
    return resultLen;
}