// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstddef>
#include <cstdint>

#define MSBFIRST 1
#define SPI_MODE0 0

struct SPISettings {
    SPISettings(uint32_t, uint8_t, uint8_t) {}
};

class SPIClass {
   public:
    void beginTransaction(SPISettings) {}
    void endTransaction() {}
    uint8_t transfer(uint8_t data) { return data; }
    void transfer(uint8_t*, size_t) {}
};

extern SPIClass SPI;