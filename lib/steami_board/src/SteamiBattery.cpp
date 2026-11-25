// SPDX-License-Identifier: GPL-3.0-or-later

#include "SteamiBattery.h"

#include <math.h>

namespace {

constexpr SteamiFunctionInfo kInfo = {SteamiFunction::BATTERY, "SteamiBattery", "battery gauge",
                                      "Plug in a battery, then reset the board.",
                                      "percent() will return 0."};

}  // namespace

SteamiBattery::SteamiBattery(SteamiBoard& board) : _handle(board, kInfo) {}

uint8_t SteamiBattery::percent() {
    if (!_handle.ready() || _handle.chipId() != SteamiChip::BQ27441) {
        return 0;
    }
    SteamiBQ27441* gauge = _handle.board().bq27441();
    return gauge == nullptr ? 0 : static_cast<uint8_t>(gauge->stateOfCharge());
}

float SteamiBattery::voltage() {
    if (!_handle.ready() || _handle.chipId() != SteamiChip::BQ27441) {
        return NAN;
    }
    SteamiBQ27441* gauge = _handle.board().bq27441();
    if (gauge == nullptr) {
        return NAN;
    }
    const uint16_t mv = gauge->voltageMv();
    return mv == 0 ? NAN : static_cast<float>(mv) / 1000.0f;
}

int16_t SteamiBattery::current() {
    if (!_handle.ready() || _handle.chipId() != SteamiChip::BQ27441) {
        return 0;
    }
    SteamiBQ27441* gauge = _handle.board().bq27441();
    return gauge == nullptr ? 0 : static_cast<int16_t>(gauge->currentAverage());
}

bool SteamiBattery::isCharging() {
    return current() > 0;
}
