// SPDX-License-Identifier: GPL-3.0-or-later

#include "SteamiHumidity.h"

#include <math.h>

namespace {

constexpr SteamiFunctionInfo kInfo = {SteamiFunction::HUMIDITY, "SteamiHumidity", "humidity sensor",
                                      "Check the board or try another one.",
                                      "read() will return NAN."};

}  // namespace

SteamiHumidity::SteamiHumidity(SteamiBoard& board) : _handle(board, kInfo) {}

float SteamiHumidity::read() {
    if (!_handle.ready()) {
        return NAN;
    }
    SteamiBoard& board = _handle.board();
    switch (_handle.chipId()) {
        case SteamiChip::BME280:
            return board.bme280()->humidity();
        case SteamiChip::WSEN_HIDS:
            return board.wsenHids()->humidity();
        default:
            return NAN;
    }
}
