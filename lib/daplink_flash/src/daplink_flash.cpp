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

void DaplinkFlash::setFilename(const char* name, const char* ext) {
    // Set 8.3 filename. name: max 8 chars, ext: max 3 chars.
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

    _bridge->sendCommand(CMD_SET_FILENAME, padded, sizeof(padded));
}

DaplinkFlash::FilenameResult DaplinkFlash::getFilename() {
    uint8_t raw[FILENAME_LEN + EXT_LEN];
    _bridge->readResponse(CMD_GET_FILENAME, raw, FILENAME_LEN + EXT_LEN);
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
    _bridge->sendCommand(CMD_CLEAR_FLASH);
}

size_t DaplinkFlash::write(const uint8_t* data, size_t length) {
    // Append data to the current file. Returns the number of bytes
    // written, or 0 if any chunk fails (busy timeout or device error).

    uint8_t payload[1 + MAX_WRITE_CHUNK];
    size_t offset = 0;

    while (offset < length) {
        uint8_t chunkLen = (uint8_t)std::min((size_t)MAX_WRITE_CHUNK, length - offset);
        payload[0] = chunkLen;
        memcpy(&payload[1], &data[offset], chunkLen);

        if (!_bridge->sendCommand(CMD_WRITE_DATA, payload, 1 + chunkLen)) {
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
    (void)sector;  // FIXME: the bridge protocol probably needs the
                   // sector index in the request payload, but the
                   // original implementation never wired it. Keeping
                   // the same behaviour here so this PR stays focused
                   // on the API migration.
    _bridge->readResponse(CMD_READ_SECTOR, buf, SECTOR_SIZE);
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