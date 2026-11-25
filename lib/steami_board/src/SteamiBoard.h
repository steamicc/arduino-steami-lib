// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <Print.h>
#include <SteamiConfig.h>
#include <SteamiDaplinkBridge.h>
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

    /// Detects the release once: forced value, then the board config, then the sensors.
    /// True unless the release stays UNKNOWN. Later calls make no I2C traffic.
    bool begin();

    /// Forces the release (highest precedence), without I2C traffic. UNKNOWN is the same as
    /// begin().
    bool begin(SteamiRelease forced);

    /// The release; detects it on the first call.
    SteamiRelease release();

    bool isV1c();

    /// True for V1A, V1B and V1AB.
    bool isV1ab();

    /// Where the release comes from (NONE when UNKNOWN or not detected yet).
    SteamiOrigin origin() const { return _origin; }

    /// The revision code read from the board config, -1 when none was read.
    int32_t revision() const { return _revision; }

    /// True when the revision is valid and its schematic index is 0.
    bool isPrototype() const;

    /// Units digit of a valid revision (0 = prototype), 0xFF when there is no valid revision.
    uint8_t schematicIndex() const;

    /// "V1a", "V1b", "V1a/V1b", "V1c" or "unknown"; detects the release on the first call.
    const char* releaseName();

    /// True when a chip on this release serves the function; detects the release first.
    bool has(SteamiFunction f);

    /// The board config, non-null only after it was loaded during detection.
    SteamiConfig* config() { return _config.get(); }

    /// Prints the banner, then the I2C chips.
    void printInfo(Print& out);

    /// Same as printInfo(Print&), through the logger.
    void printInfo();

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
    static constexpr size_t LINE_SIZE = 96;

    struct Reported {
        const char* who;
        uint8_t code;
    };

    void detect();
    bool detectFromConfig();
    bool detectByProbe();
    bool readId(uint8_t address, uint8_t reg, uint8_t& id);
    void formatBanner(char* line, size_t size) const;
    void logBanner();
    void printInfoTo(void (*emit)(void* ctx, const char* line), void* ctx);

    TwoWire* _wire = nullptr;
    SteamiBoardPins _pins = {};
    bool _pinsReady = false;
    bool _begun = false;
    void (*_log)(const char* line);
    char _lastError[ERROR_SIZE] = {};
    Reported _reported[MAX_REPORTED] = {};
    size_t _reportedCount = 0;

    bool _detected = false;
    SteamiRelease _release = SteamiRelease::UNKNOWN;
    SteamiOrigin _origin = SteamiOrigin::NONE;
    int32_t _revision = -1;
    SteamiChip _probedChip = SteamiChip::NONE;
    SteamiLazy<SteamiDaplinkBridge> _bridge;
    SteamiLazy<SteamiConfig> _config;
};

extern SteamiBoard Steami;

/// Called by the registrar below; a second, different value makes begin() report E03.
void steamiForceRelease(SteamiRelease r);

#if (defined(STEAMI_BOARD_V1A) + defined(STEAMI_BOARD_V1B) + defined(STEAMI_BOARD_V1C)) > 1
#error "Define only one of STEAMI_BOARD_V1A, STEAMI_BOARD_V1B, STEAMI_BOARD_V1C."
#endif
#if defined(STEAMI_BOARD_V1C)
#define STEAMI_FORCED_RELEASE_ SteamiRelease::V1C
#elif defined(STEAMI_BOARD_V1B)
#define STEAMI_FORCED_RELEASE_ SteamiRelease::V1B
#elif defined(STEAMI_BOARD_V1A)
#define STEAMI_FORCED_RELEASE_ SteamiRelease::V1A
#endif
#ifdef STEAMI_FORCED_RELEASE_
namespace {
struct SteamiForcedReleaseRegistrar {
    SteamiForcedReleaseRegistrar() { steamiForceRelease(STEAMI_FORCED_RELEASE_); }
};
const SteamiForcedReleaseRegistrar steamiForcedReleaseRegistrar_;
}  // namespace
#endif
