// SPDX-License-Identifier: GPL-3.0-or-later
#include "SteamiDaplinkFlash.h"

#include <ctype.h>
#include <string.h>

#include <algorithm>

namespace {

// DAPLink filename_valid(), restricted to ASCII: no lower case, no control
// character and none of "*+,./:;<=>?[\]|.
bool filenameCharValid(uint8_t c) {
    if (c < 0x20 || c >= 0x7F || (c >= 'a' && c <= 'z')) {
        return false;
    }
    return strchr("\"*+,./:;<=>?[\\]|", c) == nullptr;
}

// Copy `src` upper-cased into a `width`-byte field padded with spaces.
bool packField(const char* src, uint8_t* dst, size_t width) {
    const size_t len = strlen(src);
    if (len > width) {
        return false;
    }
    for (size_t i = 0; i < width; ++i) {
        const uint8_t c =
            i < len ? static_cast<uint8_t>(toupper(static_cast<unsigned char>(src[i]))) : ' ';
        if (!filenameCharValid(c)) {
            return false;
        }
        dst[i] = c;
    }
    return true;
}

// Copy a space-padded field into a NUL-terminated string without the padding.
void unpackField(const uint8_t* src, char* dst, size_t width) {
    size_t len = width;
    while (len > 0 && src[len - 1] == ' ') {
        len--;
    }
    memcpy(dst, src, len);
    dst[len] = '\0';
}

}  // namespace

SteamiDaplinkFlash::SteamiDaplinkFlash(SteamiDaplinkBridge& bridge) : _bridge(&bridge) {}

bool SteamiDaplinkFlash::begin() {
    return _bridge->begin();
}

// --------------------------------------------------
// Filename management
// --------------------------------------------------

bool SteamiDaplinkFlash::setFilename(const char* name, const char* ext) {
    if (name == nullptr || ext == nullptr || name[0] == '\0') {
        return false;
    }
    uint8_t packed[DAPLINK_FLASH_FILENAME_LEN + DAPLINK_FLASH_EXT_LEN];
    if (!packField(name, packed, DAPLINK_FLASH_FILENAME_LEN) ||
        !packField(ext, packed + DAPLINK_FLASH_FILENAME_LEN, DAPLINK_FLASH_EXT_LEN)) {
        return false;
    }

    if (!_bridge->waitReady(DAPLINK_FLASH_TIMEOUT_MS) ||
        !_bridge->sendCommand(DAPLINK_FLASH_CMD_SET_FILENAME, packed, sizeof(packed)) ||
        !_bridge->waitReady(DAPLINK_FLASH_TIMEOUT_MS)) {
        return false;
    }
    uint8_t err = 0;
    if (!_bridge->readError(err) || err != 0) {
        return false;
    }

    // A flash failure is reported through an error bit that the STATUS
    // polls above already cleared: read the name back instead.
    Filename expected;
    Filename stored;
    unpackField(packed, expected.name, DAPLINK_FLASH_FILENAME_LEN);
    unpackField(packed + DAPLINK_FLASH_FILENAME_LEN, expected.ext, DAPLINK_FLASH_EXT_LEN);
    return getFilename(stored) && strcmp(stored.name, expected.name) == 0 &&
           strcmp(stored.ext, expected.ext) == 0;
}

bool SteamiDaplinkFlash::getFilename(Filename& filename) {
    filename.name[0] = '\0';
    filename.ext[0] = '\0';

    if (!_bridge->waitReady(DAPLINK_FLASH_TIMEOUT_MS) ||
        !_bridge->sendCommand(DAPLINK_FLASH_CMD_GET_FILENAME)) {
        return false;
    }
    delay(DAPLINK_FLASH_RESPONSE_DELAY_MS);

    uint8_t raw[DAPLINK_FLASH_FILENAME_LEN + DAPLINK_FLASH_EXT_LEN];
    if (_bridge->readResponse(raw, sizeof(raw)) != sizeof(raw)) {
        return false;
    }
    // An erased filename sector reads 0xFF: no name was ever set.
    if (raw[0] == 0xFF) {
        return false;
    }
    unpackField(raw, filename.name, DAPLINK_FLASH_FILENAME_LEN);
    unpackField(raw + DAPLINK_FLASH_FILENAME_LEN, filename.ext, DAPLINK_FLASH_EXT_LEN);
    return true;
}

// --------------------------------------------------
// Flash operations
// --------------------------------------------------

bool SteamiDaplinkFlash::clearFlash(uint32_t timeoutMs) {
    if (!_bridge->waitReady(DAPLINK_FLASH_TIMEOUT_MS) ||
        !_bridge->sendCommand(DAPLINK_FLASH_CMD_CLEAR_FLASH)) {
        return false;
    }
    // The F103 does nothing else until the erase is over: it answers the
    // STATUS polls once it is done.
    if (!_bridge->waitReady(timeoutMs)) {
        return false;
    }
    // CLEAR_FLASH reports no error: check that the file is now empty.
    uint8_t sector[DAPLINK_FLASH_SECTOR_SIZE];
    return readSector(0, sector) && sector[0] == 0xFF;
}

size_t SteamiDaplinkFlash::write(const uint8_t* data, size_t length) {
    if (data == nullptr) {
        return 0;
    }

    uint8_t args[DAPLINK_FLASH_WRITE_ARGS];
    size_t written = 0;

    while (written < length) {
        size_t chunk = std::min<size_t>(DAPLINK_FLASH_MAX_WRITE_CHUNK, length - written);
        // 0xFF marks the end of the file and cannot be stored: stop there.
        const void* end = memchr(data + written, 0xFF, chunk);
        if (end != nullptr) {
            chunk = static_cast<const uint8_t*>(end) - (data + written);
        }
        if (chunk == 0) {
            break;
        }

        // The firmware runs WRITE_DATA once it has all 31 argument bytes.
        memset(args, 0, sizeof(args));
        args[0] = static_cast<uint8_t>(chunk);
        memcpy(&args[1], data + written, chunk);

        if (!_bridge->waitReady(DAPLINK_FLASH_TIMEOUT_MS) ||
            !_bridge->sendCommand(DAPLINK_FLASH_CMD_WRITE_DATA, args, sizeof(args)) ||
            !_bridge->waitReady(DAPLINK_FLASH_TIMEOUT_MS)) {
            break;
        }
        uint8_t err = 0;
        if (!_bridge->readError(err) || err != 0) {
            break;
        }
        written += chunk;
    }
    return written;
}

size_t SteamiDaplinkFlash::write(const char* text) {
    if (text == nullptr) {
        return 0;
    }
    return write(reinterpret_cast<const uint8_t*>(text), strlen(text));
}

size_t SteamiDaplinkFlash::writeLine(const char* text) {
    if (text == nullptr) {
        return 0;
    }
    const size_t len = strlen(text);
    const size_t written = write(text);
    if (written != len) {
        return written;
    }
    return written + write("\n");
}

// --------------------------------------------------
// Read operations
// --------------------------------------------------

bool SteamiDaplinkFlash::readSector(uint16_t sector, uint8_t* buf) {
    if (buf == nullptr || sector >= DAPLINK_FLASH_MAX_SECTORS) {
        return false;
    }
    const uint8_t args[2] = {static_cast<uint8_t>(sector >> 8),
                             static_cast<uint8_t>(sector & 0xFF)};
    if (!_bridge->waitReady(DAPLINK_FLASH_TIMEOUT_MS) ||
        !_bridge->sendCommand(DAPLINK_FLASH_CMD_READ_SECTOR, args, sizeof(args))) {
        return false;
    }
    delay(DAPLINK_FLASH_RESPONSE_DELAY_MS);

    if (_bridge->readResponse(buf, DAPLINK_FLASH_SECTOR_SIZE) != DAPLINK_FLASH_SECTOR_SIZE) {
        return false;
    }
    // A failed READ_SECTOR leaves no answer to read but sets an error bit.
    uint8_t err = 0;
    return _bridge->readError(err) && err == 0;
}

size_t SteamiDaplinkFlash::read(uint8_t* buf, size_t maxLen) {
    if (buf == nullptr) {
        return 0;
    }
    uint8_t sector[DAPLINK_FLASH_SECTOR_SIZE];
    size_t produced = 0;

    for (uint16_t s = 0; s < DAPLINK_FLASH_MAX_SECTORS && produced < maxLen; ++s) {
        if (!readSector(s, sector)) {
            break;
        }
        for (size_t i = 0; i < DAPLINK_FLASH_SECTOR_SIZE; ++i) {
            // The file ends at the first 0xFF.
            if (sector[i] == 0xFF || produced >= maxLen) {
                return produced;
            }
            buf[produced++] = sector[i];
        }
    }
    return produced;
}
