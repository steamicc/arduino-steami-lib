// SPDX-License-Identifier: GPL-3.0-or-later

#include "SteamiBoard.h"

#include <Arduino.h>
#include <stdio.h>
#include <string.h>

#ifdef ARDUINO
void steamiSerialLog(const char* line) {
    if (Serial) {
        Serial.println(line);
    }
}
#endif

SteamiBoard Steami;

namespace {

// Written by the registrar of SteamiBoard.h before setup(); constant-initialised, so they hold
// these values before any dynamic initialiser runs.
SteamiRelease gForced = SteamiRelease::UNKNOWN;
bool gForcedConflict = false;

const char* const kWho = "SteamiBoard";

const char* const kUnknownBoard =
    "cannot tell which STeaMi this is: no V1a/V1b or V1c sensor answered. Add #define "
    "STEAMI_BOARD_V1C (or V1B, V1A) before #include <STeaMi.h>.";
const char* const kConflictingDefines =
    "two different STEAMI_BOARD_ defines in this sketch. Keep only one, before #include "
    "<STeaMi.h>.";

constexpr uint8_t E_UNKNOWN_BOARD = 1;
constexpr uint8_t E_UNKNOWN_REVISION = 2;
constexpr uint8_t E_CONFLICTING_DEFINES = 3;

// One chip ID read per entry, in probing order. Never 0x3B: that is the DAPLink bridge.
struct ProbeEntry {
    SteamiChip chip;
    uint8_t reg;
    uint8_t ids[3];
    uint8_t idCount;
    SteamiRelease release;
};

constexpr ProbeEntry kProbes[] = {
    {SteamiChip::BME280, 0xD0, {0x60, 0, 0}, 1, SteamiRelease::V1C},
    {SteamiChip::WSEN_HIDS, 0x0F, {0xBC, 0, 0}, 1, SteamiRelease::V1AB},
    {SteamiChip::WSEN_PADS, 0x0F, {0xB3, 0, 0}, 1, SteamiRelease::V1AB},
    {SteamiChip::APDS9960, 0x92, {0xAB, 0x9C, 0xA8}, 3, SteamiRelease::V1AB},
};

const char* releaseText(SteamiRelease r) {
    switch (r) {
        case SteamiRelease::V1A:
            return "V1a";
        case SteamiRelease::V1B:
            return "V1b";
        case SteamiRelease::V1AB:
            return "V1a/V1b";
        case SteamiRelease::V1C:
            return "V1c";
        default:
            return "unknown";
    }
}

const char* defineText(SteamiRelease r) {
    switch (r) {
        case SteamiRelease::V1A:
            return "STEAMI_BOARD_V1A";
        case SteamiRelease::V1B:
            return "STEAMI_BOARD_V1B";
        default:
            return "STEAMI_BOARD_V1C";
    }
}

}  // namespace

void steamiForceRelease(SteamiRelease r) {
    if (gForced != SteamiRelease::UNKNOWN && gForced != r) {
        gForcedConflict = true;  // E03 in begin()
    }
    gForced = r;
}

const char* steamiChipName(SteamiChip c) {
    switch (c) {
        case SteamiChip::BME280:
            return "BME280";
        case SteamiChip::WSEN_HIDS:
            return "WSEN-HIDS";
        case SteamiChip::WSEN_PADS:
            return "WSEN-PADS";
        case SteamiChip::APDS9960:
            return "APDS-9960";
        case SteamiChip::ISM330DL:
            return "ISM330DL";
        case SteamiChip::LIS2MDL:
            return "LIS2MDL";
        case SteamiChip::VL53L1X:
            return "VL53L1X";
        case SteamiChip::BQ27441:
            return "BQ27441";
        case SteamiChip::MCP23009E:
            return "MCP23009E";
        case SteamiChip::SSD1327:
            return "SSD1327";
        case SteamiChip::GC9A01:
            return "GC9A01";
        default:
            return "";
    }
}

uint8_t steamiChipAddress(SteamiChip c) {
    switch (c) {
        case SteamiChip::BME280:
            return 0x76;
        case SteamiChip::WSEN_HIDS:
            return 0x5F;
        case SteamiChip::WSEN_PADS:
            return 0x5D;
        case SteamiChip::APDS9960:
            return 0x39;
        case SteamiChip::ISM330DL:
            return 0x6B;
        case SteamiChip::LIS2MDL:
            return 0x1E;
        case SteamiChip::VL53L1X:
            return 0x29;
        case SteamiChip::BQ27441:
            return 0x55;
        case SteamiChip::MCP23009E:
            return 0x20;
        default:
            return 0;
    }
}

SteamiBoardPins SteamiBoardPins::steami() {
    return {static_cast<uint32_t>(I2C_INT_SDA),  static_cast<uint32_t>(I2C_INT_SCL),
            static_cast<uint32_t>(RST_EXPANDER), static_cast<uint32_t>(INT_EXPANDER),
            static_cast<uint32_t>(A_BUTTON),     static_cast<uint32_t>(B_BUTTON),
            static_cast<uint32_t>(MENU_BUTTON)};
}

void SteamiBoard::setLogger(void (*log)(const char* line)) {
    _log = log;
}

void SteamiBoard::report(const char* who, SteamiStatus /* s */, const char* detail, uint8_t code) {
    snprintf(_lastError, sizeof(_lastError), "[STeaMi] %s: %s (E%02u)", who, detail,
             static_cast<unsigned>(code));

    for (size_t i = 0; i < _reportedCount; i++) {
        if (_reported[i].code == code && strcmp(_reported[i].who, who) == 0) {
            return;
        }
    }
    if (_reportedCount < MAX_REPORTED) {
        _reported[_reportedCount] = {who, code};
        _reportedCount++;
    }
    if (_log != nullptr) {
        _log(_lastError);
    }
}

const SteamiBoardPins& SteamiBoard::pins() {
    if (!_pinsReady) {
        _pins = SteamiBoardPins::steami();
        _pinsReady = true;
    }
    return _pins;
}

TwoWire& SteamiBoard::wire() {
    if (_wire == nullptr) {
#ifdef ARDUINO
        static TwoWire internal(I2C_INT_SDA, I2C_INT_SCL);
        _wire = &internal;
#else
        _wire = &Wire;
#endif
    }
    if (!_begun) {
        _wire->begin();
        _begun = true;
    }
    return *_wire;
}

bool SteamiBoard::begin() {
    if (!_detected) {
        detect();
    }
    return _release != SteamiRelease::UNKNOWN;
}

bool SteamiBoard::begin(SteamiRelease forced) {
    if (forced == SteamiRelease::UNKNOWN) {
        return begin();
    }
    wire();
    const bool changed =
        !_detected || _release != forced || _origin != SteamiOrigin::FORCED_RUNTIME;
    _detected = true;
    _release = forced;
    _origin = SteamiOrigin::FORCED_RUNTIME;
    if (changed) {
        logBanner();
    }
    return true;
}

SteamiRelease SteamiBoard::release() {
    begin();
    return _release;
}

bool SteamiBoard::isV1c() {
    return release() == SteamiRelease::V1C;
}

bool SteamiBoard::isV1ab() {
    const SteamiRelease r = release();
    return r == SteamiRelease::V1A || r == SteamiRelease::V1B || r == SteamiRelease::V1AB;
}

bool SteamiBoard::isPrototype() const {
    const SteamiRevisionInfo info = steamiDecodeRevision(_revision);
    return info.valid && info.prototype;
}

uint8_t SteamiBoard::schematicIndex() const {
    const SteamiRevisionInfo info = steamiDecodeRevision(_revision);
    return info.valid ? info.index : 0xFF;
}

const char* SteamiBoard::releaseName() {
    return releaseText(release());
}

bool SteamiBoard::has(SteamiFunction f) {
    return steamiRoute(release(), f, 0) != SteamiChip::NONE;
}

void SteamiBoard::detect() {
    wire();
    _detected = true;
    if (gForcedConflict) {
        report(kWho, SteamiStatus::UNKNOWN_BOARD, kConflictingDefines, E_CONFLICTING_DEFINES);
    }
    if (gForced != SteamiRelease::UNKNOWN) {
        _release = gForced;
        _origin = SteamiOrigin::FORCED_DEFINE;
    } else if (!detectFromConfig() && !detectByProbe()) {
        _release = SteamiRelease::UNKNOWN;
        _origin = SteamiOrigin::NONE;
        report(kWho, SteamiStatus::UNKNOWN_BOARD, kUnknownBoard, E_UNKNOWN_BOARD);
    }
    logBanner();
}

bool SteamiBoard::detectFromConfig() {
    SteamiDaplinkBridge& bridge = _bridge.emplace(wire());
    // Older DAPLink firmwares lock up on config commands: no SteamiConfig call before this.
    if (!bridge.begin() || !bridge.hasConfigZone()) {
        return false;
    }
    SteamiConfig& cfg = _config.emplace(bridge);
    if (!cfg.load()) {
        _config.reset();
        return false;
    }
    int32_t rev = -1;
    if (!cfg.boardRevision(rev)) {
        return false;  // no revision written: probe, without a message
    }
    _revision = rev;
    const SteamiRevisionInfo info = steamiDecodeRevision(rev);
    if (!info.valid) {
        char detail[ERROR_SIZE];
        snprintf(detail, sizeof(detail),
                 "unknown board revision %ld in the board config. Update the STeaMi library; the "
                 "board is detected from its sensors instead.",
                 static_cast<long>(rev));
        report(kWho, SteamiStatus::UNKNOWN_BOARD, detail, E_UNKNOWN_REVISION);
        return false;
    }
    _release = info.board;
    _origin = SteamiOrigin::CONFIG;
    return true;
}

bool SteamiBoard::detectByProbe() {
    for (const ProbeEntry& p : kProbes) {
        uint8_t id = 0;
        if (!readId(steamiChipAddress(p.chip), p.reg, id)) {
            continue;
        }
        for (uint8_t i = 0; i < p.idCount; i++) {
            if (id == p.ids[i]) {
                _release = p.release;
                _origin = SteamiOrigin::PROBE;
                _probedChip = p.chip;
                return true;
            }
        }
    }
    return false;
}

bool SteamiBoard::readId(uint8_t address, uint8_t reg, uint8_t& id) {
    TwoWire& bus = wire();
    bus.beginTransmission(address);
    bus.write(reg);
    if (bus.endTransmission(false) != 0 || bus.requestFrom(address, static_cast<uint8_t>(1)) != 1) {
        return false;
    }
    id = static_cast<uint8_t>(bus.read());
    return true;
}

void SteamiBoard::formatBanner(char* line, size_t size) const {
    const char* name = releaseText(_release);
    switch (_origin) {
        case SteamiOrigin::FORCED_RUNTIME:
            snprintf(line, size, "[STeaMi] Board %s (forced by Steami.begin())", name);
            break;
        case SteamiOrigin::FORCED_DEFINE:
            snprintf(line, size, "[STeaMi] Board %s (forced by %s)", name, defineText(_release));
            break;
        case SteamiOrigin::CONFIG:
            snprintf(line, size, "[STeaMi] Board %s (read from the board config, revision %ld%s)",
                     name, static_cast<long>(_revision), isPrototype() ? ", prototype" : "");
            break;
        case SteamiOrigin::PROBE:
            snprintf(line, size, "[STeaMi] Board %s (detected: %s at 0x%02X)", name,
                     steamiChipName(_probedChip),
                     static_cast<unsigned>(steamiChipAddress(_probedChip)));
            break;
        default:
            snprintf(line, size, "[STeaMi] Board %s (nothing detected)", name);
            break;
    }
}

void SteamiBoard::logBanner() {
    if (_log != nullptr) {
        char line[LINE_SIZE];
        formatBanner(line, sizeof(line));
        _log(line);
    }
}

SteamiBoard::ChipState& SteamiBoard::stateOf(SteamiChip c) {
    return _chipState[static_cast<size_t>(c) - static_cast<size_t>(SteamiChip::BME280)];
}

SteamiBoard::ChipState SteamiBoard::stateOf(SteamiChip c) const {
    return _chipState[static_cast<size_t>(c) - static_cast<size_t>(SteamiChip::BME280)];
}

bool SteamiBoard::built(SteamiChip c) const {
    switch (c) {
        case SteamiChip::BME280:
            return _bme280.built();
        case SteamiChip::WSEN_HIDS:
            return _wsenHids.built();
        case SteamiChip::WSEN_PADS:
            return _wsenPads.built();
        case SteamiChip::APDS9960:
            return _apds9960.built();
        case SteamiChip::ISM330DL:
            return _ism330dl.built();
        case SteamiChip::LIS2MDL:
            return _lis2mdl.built();
        case SteamiChip::VL53L1X:
            return _vl53l1x.built();
        case SteamiChip::BQ27441:
            return _bq27441.built();
        case SteamiChip::MCP23009E:
            return _mcp23009e.built();
        default:
            return false;
    }
}

SteamiStatus SteamiBoard::claim(SteamiChip c) {
    if (c < SteamiChip::BME280 || c > SteamiChip::MCP23009E) {
        return SteamiStatus::NOT_STARTED;  // displays, NONE and COUNT have no shared driver
    }
    begin();
    ChipState& state = stateOf(c);
    if (state == ChipState::NOT_TRIED) {
        TwoWire& bus = wire();
        bool ok = false;
        switch (c) {
            case SteamiChip::BME280:
                ok = _bme280.emplace(bus).begin();
                break;
            case SteamiChip::WSEN_HIDS:
                ok = _wsenHids.emplace(bus).begin();
                break;
            case SteamiChip::WSEN_PADS:
                ok = _wsenPads.emplace(bus).begin();
                break;
            case SteamiChip::APDS9960:
                ok = _apds9960.emplace(bus).begin();
                break;
            case SteamiChip::ISM330DL:
                ok = _ism330dl.emplace(bus).begin();
                break;
            case SteamiChip::LIS2MDL:
                ok = _lis2mdl.emplace(bus).begin();
                break;
            case SteamiChip::VL53L1X:
                ok = _vl53l1x.emplace(bus).begin();
                break;
            case SteamiChip::BQ27441:
                ok = _bq27441.emplace(bus).begin();
                break;
            default:
                ok = _mcp23009e
                         .emplace(bus, static_cast<uint8_t>(pins().expanderReset),
                                  MCP23009_I2C_ADDR, static_cast<int>(pins().expanderIrq))
                         .begin();
                break;
        }
        state = ok ? ChipState::OK : ChipState::NOT_RESPONDING;
    }
    return state == ChipState::OK ? SteamiStatus::OK : SteamiStatus::NOT_RESPONDING;
}

void SteamiBoard::printInfoTo(void (*emit)(void* ctx, const char* line), void* ctx,
                              bool withBanner) {
    // Only a message reported before this call is replayed: detection logs its own.
    char before[ERROR_SIZE];
    memcpy(before, _lastError, sizeof(before));
    begin();
    char line[LINE_SIZE];
    if (withBanner) {
        formatBanner(line, sizeof(line));
        emit(ctx, line);
    }
    for (size_t i = 0; i < I2C_CHIPS; i++) {
        const SteamiChip c = static_cast<SteamiChip>(static_cast<size_t>(SteamiChip::BME280) + i);
        const ChipState state = stateOf(c);
        snprintf(line, sizeof(line), "%s 0x%02X %s", steamiChipName(c),
                 static_cast<unsigned>(steamiChipAddress(c)),
                 state == ChipState::OK               ? "OK"
                 : state == ChipState::NOT_RESPONDING ? "not responding"
                                                      : "not tried");
        emit(ctx, line);
    }
    if (before[0] != '\0') {
        emit(ctx, before);
    }
}

void SteamiBoard::printInfo(Print& out) {
    printInfoTo([](void* ctx, const char* line) { static_cast<Print*>(ctx)->println(line); }, &out,
                true);
}

void SteamiBoard::printInfo() {
    // Detection logs the banner itself: do not send it twice on a first call.
    const bool withBanner = _detected;
    printInfoTo(
        [](void* ctx, const char* line) {
            void (*log)(const char*) = static_cast<SteamiBoard*>(ctx)->_log;
            if (log != nullptr) {
                log(line);
            }
        },
        this, withBanner);
}

void SteamiBoard::end() {
    if (_mcp23009e.built()) {
        // The driver attaches a handler that captures its address and never detaches it.
        detachInterrupt(digitalPinToInterrupt(static_cast<int>(pins().expanderIrq)));
    }
    _mcp23009e.reset();
    _bq27441.reset();
    _vl53l1x.reset();
    _lis2mdl.reset();
    _ism330dl.reset();
    _apds9960.reset();
    _wsenPads.reset();
    _wsenHids.reset();
    _bme280.reset();
    for (ChipState& s : _chipState) {
        s = ChipState::NOT_TRIED;
    }
    _config.reset();  // before the bridge it points to
    _bridge.reset();
    _detected = false;
    _release = SteamiRelease::UNKNOWN;
    _origin = SteamiOrigin::NONE;
    _revision = -1;
    _probedChip = SteamiChip::NONE;
    _lastError[0] = '\0';
    _reportedCount = 0;
    _begun = false;
}
