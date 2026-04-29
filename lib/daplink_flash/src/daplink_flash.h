// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <Arduino.h>
#include <Wire.h>

#include "daplink_bridge.h"
#include "daplink_flash_const.h"

class DaplinkFlash {
   public:
    DaplinkFlash(daplink_bridge& bridge);

    struct FilenameResult {
        char name[FILENAME_LEN + 1];  // +1 pour le null terminator
        char ext[EXT_LEN + 1];
    };

    // Filename management
    void setFilename(const char* name, const char* ext);
    FilenameResult getFilename();

    // Flash operations
    void clearFlash();
    size_t write(const char* data);
    size_t write(const uint8_t* data, size_t length);
    size_t writeLine(const char* text);

    // Read operations
    void readSector(uint16_t sector, uint8_t* buf);
    size_t read(uint8_t* result, size_t maxLen, bool limitLen = false);

   private:
    daplink_bridge* _bridge;
};