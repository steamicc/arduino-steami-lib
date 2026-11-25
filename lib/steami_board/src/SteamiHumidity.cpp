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
        case SteamiChip::BME280: {
            SteamiBME280* bme = board.bme280();
            return bme == nullptr ? NAN : bme->humidity();
        }
        case SteamiChip::WSEN_HIDS: {
            SteamiWsenHids* hids = board.wsenHids();
            return hids == nullptr ? NAN : hids->humidity();
        }
        default:
            return NAN;
    }
}
