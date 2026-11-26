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
// onAnyTransfer sees every byte of every instance, so a test also sees a bus
// that a library keeps in a function-local static.
class SPIClass {
   public:
    std::function<void(uint8_t)> onTransfer;
    std::function<void()> onBeginTransaction;
    uint32_t lastClock = 0;
    uint32_t mosiPin = 0, misoPin = 0, sclkPin = 0;
    // Epoch of the last begin(). Each test bumps testEpoch in setUp(), so a
    // bus begun by an earlier test does not count as begun.
    uint32_t begunEpoch = 0;

    static inline uint32_t testEpoch = 1;
    static inline std::function<void(const SPIClass& bus, uint8_t data)> onAnyTransfer;

    SPIClass() = default;
    SPIClass(uint32_t mosi, uint32_t miso, uint32_t sclk)
        : mosiPin(mosi), misoPin(miso), sclkPin(sclk) {}

    void begin() { begunEpoch = testEpoch; }
    bool begun() const { return begunEpoch == testEpoch; }
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
        if (onAnyTransfer) {
            onAnyTransfer(*this, data);
        }
    }
};

extern SPIClass SPI;
