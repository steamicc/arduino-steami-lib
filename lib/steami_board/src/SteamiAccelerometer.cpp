// SPDX-License-Identifier: GPL-3.0-or-later

#include "SteamiAccelerometer.h"

#include <math.h>

namespace {

constexpr SteamiFunctionInfo kInfo = {SteamiFunction::ACCELEROMETER, "SteamiAccelerometer",
                                      "accelerometer", "Check the board or try another one.",
                                      "read() will return NAN."};

}  // namespace

SteamiAccelerometer::SteamiAccelerometer(SteamiBoard& board) : _handle(board, kInfo) {}

bool SteamiAccelerometer::begin() {
    const bool wasStarted = _handle.status() != SteamiStatus::NOT_STARTED;
    if (!_handle.begin()) {
        return false;
    }
    if (!wasStarted) {
        SteamiISM330DL* imu = _handle.board().ism330dl();
        if (imu != nullptr) {
            imu->configureAccel(SteamiISM330DL::AccelOdr::HZ_104, SteamiISM330DL::AccelScale::G_8);
        }
    }
    return true;
}

SteamiVector SteamiAccelerometer::read() {
    SteamiVector out = {NAN, NAN, NAN};
    if (_handle.status() == SteamiStatus::NOT_STARTED) {
        begin();
    }
    if (!_handle.isAvailable() || _handle.chipId() != SteamiChip::ISM330DL) {
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
