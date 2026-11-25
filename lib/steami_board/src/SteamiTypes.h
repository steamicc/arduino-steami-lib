// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stdint.h>

/// Codes of the messages the library reports, the "Exx" at the end of each line.
constexpr uint8_t STEAMI_E_UNKNOWN_BOARD = 1;
constexpr uint8_t STEAMI_E_UNKNOWN_REVISION = 2;
constexpr uint8_t STEAMI_E_CONFLICTING_DEFINES = 3;
constexpr uint8_t STEAMI_E_NOT_ANSWERING = 10;
constexpr uint8_t STEAMI_E_RELEASE_CONTRADICTED = 11;
constexpr uint8_t STEAMI_E_NOT_ON_THIS_BOARD = 20;

enum class SteamiRelease : uint8_t { UNKNOWN, V1A, V1B, V1AB, V1C };

enum class SteamiOrigin : uint8_t { NONE, FORCED_RUNTIME, FORCED_DEFINE, CONFIG, PROBE };

enum class SteamiStatus : uint8_t {
    OK,
    NOT_STARTED,
    NOT_ON_THIS_BOARD,
    NOT_RESPONDING,
    NO_MEMORY,
    UNKNOWN_BOARD
};

enum class SteamiChip : uint8_t {
    NONE,
    BME280,
    WSEN_HIDS,
    WSEN_PADS,
    APDS9960,
    ISM330DL,
    LIS2MDL,
    VL53L1X,
    BQ27441,
    MCP23009E,
    SSD1327,
    GC9A01,
    COUNT
};

enum class SteamiFunction : uint8_t {
    TEMPERATURE,
    HUMIDITY,
    PRESSURE,
    DISTANCE,
    ACCELEROMETER,
    GYROSCOPE,
    MAGNETOMETER,
    COMPASS,
    LIGHT,
    COLOR,
    GESTURE,
    BATTERY,
    BUTTONS,
    SCREEN,
    COUNT
};

/// Decoded board revision code (for example 131 is generation 1, release 3, index 1).
struct SteamiRevisionInfo {
    bool valid;
    uint8_t generation;
    uint8_t release;
    uint8_t index;
    SteamiRelease board;
    bool prototype;
};

/// Decodes a revision code. Valid codes are 100..999 with generation 1 and release 1..3.
constexpr SteamiRevisionInfo steamiDecodeRevision(int32_t rev) {
    SteamiRevisionInfo info{false, 0, 0, 0, SteamiRelease::UNKNOWN, false};
    if (rev < 100 || rev > 999) {
        return info;
    }
    const int32_t generation = rev / 100;
    const int32_t release = (rev / 10) % 10;
    if (generation != 1 || release < 1 || release > 3) {
        return info;
    }
    info.valid = true;
    info.generation = static_cast<uint8_t>(generation);
    info.release = static_cast<uint8_t>(release);
    info.index = static_cast<uint8_t>(rev % 10);
    info.board = release == 1   ? SteamiRelease::V1A
                 : release == 2 ? SteamiRelease::V1B
                                : SteamiRelease::V1C;
    info.prototype = info.index == 0;
    return info;
}

namespace steami_detail {

/// Returns the candidate at `n` among up to three chips, NONE past the last one.
constexpr SteamiChip pick(uint8_t n, SteamiChip a, SteamiChip b = SteamiChip::NONE,
                          SteamiChip c = SteamiChip::NONE) {
    return n == 0 ? a : n == 1 ? b : n == 2 ? c : SteamiChip::NONE;
}

}  // namespace steami_detail

/// Returns the `candidate`-th chip that can serve a function on a release, in probing order.
/// Returns SteamiChip::NONE past the last candidate.
constexpr SteamiChip steamiRoute(SteamiRelease release, SteamiFunction f, uint8_t candidate) {
    using steami_detail::pick;
    const bool v1c = release == SteamiRelease::V1C;
    const bool unknown = release == SteamiRelease::UNKNOWN;
    switch (f) {
        case SteamiFunction::TEMPERATURE:
            return v1c       ? pick(candidate, SteamiChip::BME280)
                   : unknown ? pick(candidate, SteamiChip::BME280, SteamiChip::WSEN_HIDS,
                                    SteamiChip::WSEN_PADS)
                             : pick(candidate, SteamiChip::WSEN_HIDS, SteamiChip::WSEN_PADS);
        case SteamiFunction::HUMIDITY:
            return v1c       ? pick(candidate, SteamiChip::BME280)
                   : unknown ? pick(candidate, SteamiChip::BME280, SteamiChip::WSEN_HIDS)
                             : pick(candidate, SteamiChip::WSEN_HIDS);
        case SteamiFunction::PRESSURE:
            return v1c       ? pick(candidate, SteamiChip::BME280)
                   : unknown ? pick(candidate, SteamiChip::BME280, SteamiChip::WSEN_PADS)
                             : pick(candidate, SteamiChip::WSEN_PADS);
        case SteamiFunction::DISTANCE:
            return pick(candidate, SteamiChip::VL53L1X);
        case SteamiFunction::ACCELEROMETER:
        case SteamiFunction::GYROSCOPE:
            return pick(candidate, SteamiChip::ISM330DL);
        case SteamiFunction::MAGNETOMETER:
        case SteamiFunction::COMPASS:
            return pick(candidate, SteamiChip::LIS2MDL);
        case SteamiFunction::LIGHT:
        case SteamiFunction::COLOR:
        case SteamiFunction::GESTURE:
            return v1c ? SteamiChip::NONE : pick(candidate, SteamiChip::APDS9960);
        case SteamiFunction::BATTERY:
            return pick(candidate, SteamiChip::BQ27441);
        case SteamiFunction::BUTTONS:
            return pick(candidate, SteamiChip::MCP23009E);
        case SteamiFunction::SCREEN:
            return unknown ? SteamiChip::NONE
                   : v1c   ? pick(candidate, SteamiChip::GC9A01)
                           : pick(candidate, SteamiChip::SSD1327);
        default:
            return SteamiChip::NONE;
    }
}

/// Chip name for messages ("" for NONE). Defined in SteamiBoard.cpp.
const char* steamiChipName(SteamiChip c);

/// 7-bit I2C address of a chip (0 for the displays and NONE). Defined in SteamiBoard.cpp.
uint8_t steamiChipAddress(SteamiChip c);
