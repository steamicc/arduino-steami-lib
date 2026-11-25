// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <Print.h>
#include <SteamiAPDS9960.h>
#include <SteamiBME280.h>
#include <SteamiBQ27441.h>
#include <SteamiConfig.h>
#include <SteamiDaplinkBridge.h>
#include <SteamiISM330DL.h>
#include <SteamiLIS2MDL.h>
#include <SteamiMCP23009E.h>
#include <SteamiVL53L1X.h>
#include <SteamiWsenHids.h>
#include <SteamiWsenPads.h>
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

    /// The board config, non-null only once it was loaded: during detection, or on demand by
    /// calibrate() when the release is forced.
    SteamiConfig* config() { return _config.get(); }

    /// Builds the driver of a chip on the first call and begins it once; the result is cached.
    /// OK or NOT_RESPONDING for the nine I2C chips, NOT_STARTED for the displays and NONE.
    /// Detects the release first when that has not happened.
    SteamiStatus claim(SteamiChip c);

    /// Applies the config calibration of a chip once: claims the chip, loads the board config on
    /// demand and applies the temperature calibration (`tc`) of BME280, WSEN-HIDS and WSEN-PADS,
    /// the accelerometer offsets (`ca`) of ISM330DL and the hard and soft iron (`cm`) of LIS2MDL.
    /// True when a calibration was applied, now or on an earlier call; false for the other chips,
    /// a chip that does not answer, and a config without calibration for it.
    bool calibrate(SteamiChip c);

    /// True when a calibration was applied to the chip.
    bool calibrated(SteamiChip c) const;

    /// True when the slot of the chip is constructed.
    bool built(SteamiChip c) const;

    /// The shared driver of a chip, nullptr unless claim() gives OK.
    SteamiBME280* bme280() { return claimed(SteamiChip::BME280, _bme280); }
    SteamiWsenHids* wsenHids() { return claimed(SteamiChip::WSEN_HIDS, _wsenHids); }
    SteamiWsenPads* wsenPads() { return claimed(SteamiChip::WSEN_PADS, _wsenPads); }
    SteamiAPDS9960* apds9960() { return claimed(SteamiChip::APDS9960, _apds9960); }
    SteamiISM330DL* ism330dl() { return claimed(SteamiChip::ISM330DL, _ism330dl); }
    SteamiLIS2MDL* lis2mdl() { return claimed(SteamiChip::LIS2MDL, _lis2mdl); }
    SteamiVL53L1X* vl53l1x() { return claimed(SteamiChip::VL53L1X, _vl53l1x); }
    SteamiBQ27441* bq27441() { return claimed(SteamiChip::BQ27441, _bq27441); }
    SteamiMCP23009E* mcp23009e() { return claimed(SteamiChip::MCP23009E, _mcp23009e); }

    /// Prints the banner, the I2C chips of the release and the last message to `out`. The banner
    /// is always printed; the last message only when the logger did not deliver it.
    void printInfo(Print& out);

    /// Same list through the logger, without repeating the banner or the last message when the
    /// logger already delivered them.
    void printInfo();

    /// Sets where messages go; nullptr keeps the library silent. A logger set here counts as
    /// delivering every line; the default one only while Serial is started.
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

    /// Destroys the slots and forgets reported pairs and delivered lines. The logger is kept.
    void end();

   private:
    static constexpr size_t MAX_REPORTED = 24;
    static constexpr size_t ERROR_SIZE = 256;  // the longest E11 line is about 220 bytes
    static constexpr size_t LINE_SIZE = 96;

    struct Reported {
        const char* who;
        uint8_t code;
    };

    void detect();
    bool detectFromConfig();
    bool loadConfig();
    bool detectByProbe();
    bool readId(uint8_t address, uint8_t reg, uint8_t& id);
    void formatBanner(char* line, size_t size) const;
    void logBanner();
    bool deliver(const char* line);
    void printInfoTo(bool (*emit)(void* ctx, const char* line), void* ctx, bool toLogger);

    enum class ChipState : uint8_t { NOT_TRIED, OK, NOT_RESPONDING };
    static constexpr size_t I2C_CHIPS = 9;  // BME280 .. MCP23009E

    ChipState& stateOf(SteamiChip c);
    ChipState stateOf(SteamiChip c) const;

    template <typename T>
    T* claimed(SteamiChip c, SteamiLazy<T>& slot) {
        return claim(c) == SteamiStatus::OK ? slot.get() : nullptr;
    }

    TwoWire* _wire = nullptr;
    SteamiBoardPins _pins = {};
    bool _pinsReady = false;
    bool _begun = false;
    void (*_log)(const char* line);
    char _lastError[ERROR_SIZE] = {};
    bool _bannerDelivered = false;  // the logger delivered the current banner
    bool _errorDelivered = false;   // the logger delivered the current last error
    Reported _reported[MAX_REPORTED] = {};
    size_t _reportedCount = 0;

    bool _detected = false;
    SteamiRelease _release = SteamiRelease::UNKNOWN;
    SteamiOrigin _origin = SteamiOrigin::NONE;
    int32_t _revision = -1;
    SteamiChip _probedChip = SteamiChip::NONE;
    SteamiLazy<SteamiDaplinkBridge> _bridge;
    SteamiLazy<SteamiConfig> _config;
    bool _configTried = false;         // loadConfig() ran since begin()/end()
    uint16_t _calibrationTried = 0;    // bit per chip, set before the config is read
    uint16_t _calibrationApplied = 0;  // bit per chip
    static_assert(static_cast<unsigned>(SteamiChip::COUNT) <= 16,
                  "the calibration masks hold one bit per chip: widen them past 16 chips");
    ChipState _chipState[I2C_CHIPS] = {};
    SteamiLazy<SteamiBME280> _bme280;
    SteamiLazy<SteamiWsenHids> _wsenHids;
    SteamiLazy<SteamiWsenPads> _wsenPads;
    SteamiLazy<SteamiAPDS9960> _apds9960;
    SteamiLazy<SteamiISM330DL> _ism330dl;
    SteamiLazy<SteamiLIS2MDL> _lis2mdl;
    SteamiLazy<SteamiVL53L1X> _vl53l1x;
    SteamiLazy<SteamiBQ27441> _bq27441;
    SteamiLazy<SteamiMCP23009E> _mcp23009e;
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
