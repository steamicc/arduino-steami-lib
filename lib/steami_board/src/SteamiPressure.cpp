// SPDX-License-Identifier: GPL-3.0-or-later

#include "SteamiPressure.h"

#include <math.h>

namespace {

constexpr SteamiFunctionInfo kInfo = {SteamiFunction::PRESSURE, "SteamiPressure", "pressure sensor",
                                      "Check the board or try another one.",
                                      "read() will return NAN."};

}  // namespace

SteamiPressure::SteamiPressure(SteamiBoard& board) : _handle(board, kInfo) {}

float SteamiPressure::read() {
    if (!_handle.ready()) {
        return NAN;
    }
    SteamiBoard& board = _handle.board();
    switch (_handle.chipId()) {
        case SteamiChip::BME280:
            return board.bme280()->pressureHpa();
        case SteamiChip::WSEN_PADS:
            return board.wsenPads()->pressureHpa();
        default:
            return NAN;
    }
}

float SteamiPressure::altitude(float seaLevelHpa) {
    return SteamiBME280::altitudeFromPressure(read(), seaLevelHpa);
}
