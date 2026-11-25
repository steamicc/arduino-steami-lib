// SPDX-License-Identifier: GPL-3.0-or-later

#include "SteamiDistance.h"

#include <math.h>

namespace {

constexpr SteamiFunctionInfo kInfo = {SteamiFunction::DISTANCE, "SteamiDistance", "distance sensor",
                                      "Check the board or try another one.",
                                      "read() will return -1."};

}  // namespace

SteamiDistance::SteamiDistance(SteamiBoard& board) : _handle(board, kInfo) {}

int SteamiDistance::read() {
    if (!_handle.ready() || _handle.chipId() != SteamiChip::VL53L1X) {
        return -1;
    }
    const uint16_t mm = _handle.board().vl53l1x()->read();
    return mm == 0 ? -1 : static_cast<int>(mm);
}

float SteamiDistance::cm() {
    const int mm = read();
    return mm < 0 ? NAN : static_cast<float>(mm) / 10.0f;
}
