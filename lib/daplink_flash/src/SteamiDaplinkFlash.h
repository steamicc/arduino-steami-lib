// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <Arduino.h>

#include "DaplinkBridge.h"
#include "SteamiDaplinkFlash_const.h"

// User file stored by the DAPLink chip in the 8 MB external flash, and
// shown on the STeaMi USB drive under its 8.3 name.
class SteamiDaplinkFlash {
   public:
    struct Filename {
        char name[DAPLINK_FLASH_FILENAME_LEN + 1];
        char ext[DAPLINK_FLASH_EXT_LEN + 1];
    };

    explicit SteamiDaplinkFlash(DaplinkBridge& bridge);

    bool begin();

    // Filename management
    bool setFilename(const char* name, const char* ext);
    bool getFilename(Filename& filename);

    // Flash operations
    bool clearFlash(uint32_t timeoutMs = DAPLINK_FLASH_CLEAR_TIMEOUT_MS);
    size_t write(const uint8_t* data, size_t length);
    size_t write(const char* text);
    size_t writeLine(const char* text);

    // Read operations
    bool readSector(uint16_t sector, uint8_t* buf);
    size_t read(uint8_t* buf, size_t maxLen);

   private:
    DaplinkBridge* _bridge;
};
