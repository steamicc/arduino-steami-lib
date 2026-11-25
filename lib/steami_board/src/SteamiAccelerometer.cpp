// SPDX-License-Identifier: GPL-3.0-or-later

#include "SteamiAccelerometer.h"

#include <math.h>

namespace {

constexpr SteamiFunctionInfo kInfo = {SteamiFunction::ACCELEROMETER, "SteamiAccelerometer",
                                      "accelerometer", "Check the board or try another one.",
                                      "read() will return NAN."};

}  // namespace

SteamiAccelerometer::SteamiAccelerometer(SteamiBoard& board) : _handle(board, kInfo) {}

SteamiVector SteamiAccelerometer::read() {
    SteamiVector out = {NAN, NAN, NAN};
    if (!_handle.ready() || _handle.chipId() != SteamiChip::ISM330DL) {
        return out;
    }
    SteamiISM330DL* imu = _handle.board().ism330dl();
    if (imu == nullptr) {
        return out;
    }
    SteamiISM330DL::Vector3 value{};
    if (imu->accelerationG(value)) {
        out = {value.x, value.y, value.z};
    }
    return out;
}

float SteamiAccelerometer::strength() {
    const SteamiVector v = read();
    return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
}
