// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <Arduino.h>
#include <Wire.h>

#include "DaplinkBridge.h"
#include "daplink_flash_const.h"

class DaplinkFlash {
   public:
    bool begin();
    DaplinkFlash(DaplinkBridge& bridge);

    struct FilenameResult {
        char name[DAPLINK_FLASH_FILENAME_LEN + 1];
        char ext[DAPLINK_FLASH_EXT_LEN + 1];
    };

    // Filename management
    void setFilename(const char* name, const char* ext);
    FilenameResult getFilename();

    // Flash operations
    bool clearFlash();
    size_t write(const char* data);
    size_t write(const uint8_t* data, size_t length);
    size_t writeLine(const char* text);

    // Read operations
    bool readSector(uint16_t sector, uint8_t* buf);
    size_t readN(uint8_t* result, size_t n);
    size_t readUntilSentinel(uint8_t* result, size_t maxLen);

   private:
    DaplinkBridge* _bridge;
};