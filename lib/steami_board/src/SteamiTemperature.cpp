// SPDX-License-Identifier: GPL-3.0-or-later

#include "SteamiTemperature.h"

#include <math.h>

namespace {

constexpr SteamiFunctionInfo kInfo = {SteamiFunction::TEMPERATURE, "SteamiTemperature",
                                      "temperature sensor", "Check the board or try another one.",
                                      "read() will return NAN."};

}  // namespace

SteamiTemperature::SteamiTemperature(SteamiBoard& board) : _handle(board, kInfo) {}

float SteamiTemperature::read() {
    if (!_handle.ready()) {
        return NAN;
    }
    SteamiBoard& board = _handle.board();
    switch (_handle.chipId()) {
        case SteamiChip::BME280:
            return board.bme280()->temperature() + _offset;
        case SteamiChip::WSEN_HIDS:
            return board.wsenHids()->temperature() + _offset;
        case SteamiChip::WSEN_PADS:
            return board.wsenPads()->temperature() + _offset;
        default:
            return NAN;
    }
}
