// SPDX-License-Identifier: GPL-3.0-or-later

#include "SteamiBattery.h"

#include <math.h>

namespace {

constexpr SteamiFunctionInfo kInfo = {SteamiFunction::BATTERY, "SteamiBattery", "battery gauge",
                                      "Plug in a battery or check the board.",
                                      "percent() will return 0."};

}  // namespace

SteamiBattery::SteamiBattery(SteamiBoard& board) : _handle(board, kInfo) {}

uint8_t SteamiBattery::percent() {
    if (!_handle.ready() || _handle.chipId() != SteamiChip::BQ27441) {
        return 0;
    }
    return static_cast<uint8_t>(_handle.board().bq27441()->stateOfCharge());
}

float SteamiBattery::voltage() {
    if (!_handle.ready() || _handle.chipId() != SteamiChip::BQ27441) {
        return NAN;
    }
    const uint16_t mv = _handle.board().bq27441()->voltageMv();
    return mv == 0 ? NAN : static_cast<float>(mv) / 1000.0f;
}

int16_t SteamiBattery::current() {
    if (!_handle.ready() || _handle.chipId() != SteamiChip::BQ27441) {
        return 0;
    }
    return static_cast<int16_t>(_handle.board().bq27441()->currentAverage());
}

bool SteamiBattery::isCharging() {
    return current() > 0;
}
