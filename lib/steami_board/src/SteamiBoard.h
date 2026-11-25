// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <Wire.h>
#include <stddef.h>
#include <stdint.h>

#include "SteamiLazy.h"
#include "SteamiTypes.h"

#ifdef ARDUINO
/// Default logger on a real board: prints on Serial once a host has opened it.
void steamiSerialLog(const char* line);
#define STEAMI_DEFAULT_LOGGER (&steamiSerialLog)
#else
#define STEAMI_DEFAULT_LOGGER (nullptr)
#endif

/// Pins used by the board layer.
struct SteamiBoardPins {
    uint32_t sda;
    uint32_t scl;
    uint32_t expanderReset;
    uint32_t expanderIrq;
    uint32_t buttonA;
    uint32_t buttonB;
    uint32_t buttonMenu;

    /// The pins of the STeaMi board.
    static SteamiBoardPins steami();
};

/// Board layer: release detection, shared drivers and coded messages.
/// Not copyable and not movable: the lazy slots copy raw bytes.
class SteamiBoard {
   public:
    /// The internal bus is resolved on first use, so a global stays constant-initialised.
    constexpr SteamiBoard() noexcept : _log(STEAMI_DEFAULT_LOGGER) {}

    /// Stores references and values only; touches no hardware.
    SteamiBoard(TwoWire& wire, const SteamiBoardPins& pins)
        : _wire(&wire), _pins(pins), _pinsReady(true), _log(STEAMI_DEFAULT_LOGGER) {}

    SteamiBoard(const SteamiBoard&) = delete;
    SteamiBoard& operator=(const SteamiBoard&) = delete;
    SteamiBoard(SteamiBoard&&) = delete;
    SteamiBoard& operator=(SteamiBoard&&) = delete;

    /// Sets where messages go; nullptr keeps the library silent.
    void setLogger(void (*log)(const char* line));

    /// The last message reported, "" when nothing was reported.
    const char* lastError() const { return _lastError; }

    /// Stores "[STeaMi] <who>: <detail> (E<code>)" in lastError() and sends it to the logger the
    /// first time a (who, code) pair is seen. `who` must be a string that outlives the board
    /// (a literal).
    void report(const char* who, SteamiStatus s, const char* detail, uint8_t code);

    /// The pins this board uses.
    const SteamiBoardPins& pins();

    /// The I2C bus; begins it the first time.
    TwoWire& wire();

    /// Destroys the slots and forgets reported pairs. The logger is kept.
    void end();

   private:
    static constexpr size_t MAX_REPORTED = 16;
    static constexpr size_t ERROR_SIZE = 192;

    struct Reported {
        const char* who;
        uint8_t code;
    };

    TwoWire* _wire = nullptr;
    SteamiBoardPins _pins = {};
    bool _pinsReady = false;
    bool _begun = false;
    void (*_log)(const char* line);
    char _lastError[ERROR_SIZE] = {};
    Reported _reported[MAX_REPORTED] = {};
    size_t _reportedCount = 0;
};

extern SteamiBoard Steami;
