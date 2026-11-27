// SPDX-License-Identifier: GPL-3.0-or-later

#include "SteamiAccelerometer.h"

#include <Arduino.h>
#include <math.h>

namespace {

constexpr SteamiFunctionInfo kInfo = {SteamiFunction::ACCELEROMETER, "SteamiAccelerometer",
                                      "accelerometer", "Check the board or try another one.",
                                      "read() will return NAN."};

// After switching the range, wait for two samples at 104 Hz: until then the
// output registers hold the last +-2 g sample, which the +-8 g scale would
// read 4 times too large.
constexpr uint32_t kSettleMs = 20;

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
            delay(kSettleMs);
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
