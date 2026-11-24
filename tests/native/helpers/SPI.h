// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>

#define MSBFIRST 1
#define SPI_MODE0 0

// Same names as the STM32 core: transfer(buf, count, SPI_TRANSMITONLY)
// sends without writing the received bytes back.
#define SPI_TRANSMITRECEIVE false
#define SPI_TRANSMITONLY true

struct SPISettings {
    SPISettings(uint32_t clock, uint8_t, uint8_t) : clock(clock) {}
    uint32_t clock;
};

// Host-side SPI mock. Every transmitted byte goes through onTransfer, so a
// test can record it together with the state of the D/C and CS pins.
// onBeginTransaction lets a test emulate the core reclaiming the pins.
class SPIClass {
   public:
    std::function<void(uint8_t)> onTransfer;
    std::function<void()> onBeginTransaction;
    uint32_t lastClock = 0;

    void begin() {}
    void beginTransaction(SPISettings settings) {
        lastClock = settings.clock;
        if (onBeginTransaction) {
            onBeginTransaction();
        }
    }
    void endTransaction() {}
    uint8_t transfer(uint8_t data) {
        record(data);
        return data;
    }
    void transfer(void* buf, size_t count, bool skipReceive = SPI_TRANSMITRECEIVE) {
        (void)skipReceive;
        const uint8_t* bytes = static_cast<const uint8_t*>(buf);
        for (size_t i = 0; i < count; i++) {
            record(bytes[i]);
        }
    }

   private:
    void record(uint8_t data) {
        if (onTransfer) {
            onTransfer(data);
        }
    }
};

extern SPIClass SPI;
