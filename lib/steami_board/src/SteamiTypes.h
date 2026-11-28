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
constexpr uint8_t STEAMI_E_DISPLAY_UNKNOWN_BOARD = 30;
constexpr uint8_t STEAMI_E_NO_MEMORY = 31;

/// Crocodile pads on the MCP23009E expander: their GPIO number on the expander, not MCU pins.
/// Use them with Steami.mcp23009e(), for example setup(STEAMI_PAD_P24, MCP23009_DIR_INPUT).
constexpr uint8_t STEAMI_PAD_P28 = 0;
constexpr uint8_t STEAMI_PAD_P29 = 1;
constexpr uint8_t STEAMI_PAD_P24 = 2;
constexpr uint8_t STEAMI_PAD_P25 = 3;

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
    IMP34DT05,
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
    MICROPHONE,
    COUNT
};

/// A three-axis reading (acceleration, angular rate or magnetic field) in the unit of its class.
struct SteamiVector {
    float x;
    float y;
    float z;
};

/// A button of the D-pad or the A, B and MENU buttons, NONE when none is pressed.
enum class SteamiButton : uint8_t { NONE, A, B, MENU, UP, DOWN, LEFT, RIGHT };

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
        case SteamiFunction::MICROPHONE:
            return pick(candidate, SteamiChip::IMP34DT05);
        default:
            return SteamiChip::NONE;
    }
}

/// Chip name for messages ("" for NONE). Defined in SteamiBoard.cpp.
const char* steamiChipName(SteamiChip c);

/// 7-bit I2C address of a chip (0 for the displays, the microphone and NONE). Defined in
/// SteamiBoard.cpp.
uint8_t steamiChipAddress(SteamiChip c);

/// State of a display status for messages: "OK", "not started", "unknown board", "no memory",
/// otherwise "not available". Defined in SteamiBoard.cpp.
const char* steamiDisplayStateText(SteamiStatus s);
