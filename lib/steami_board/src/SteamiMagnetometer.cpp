// SPDX-License-Identifier: GPL-3.0-or-later

#include "SteamiMagnetometer.h"

#include <math.h>

namespace {

constexpr SteamiFunctionInfo kInfo = {SteamiFunction::MAGNETOMETER, "SteamiMagnetometer",
                                      "magnetometer", "Check the board or try another one.",
                                      "read() will return NAN."};

constexpr float kMicroteslaPerLsb = 0.15f;

}  // namespace

SteamiMagnetometer::SteamiMagnetometer(SteamiBoard& board) : _handle(board, kInfo) {}

SteamiVector SteamiMagnetometer::read() {
    SteamiVector out = {NAN, NAN, NAN};
    if (!_handle.ready() || _handle.chipId() != SteamiChip::LIS2MDL) {
        return out;
    }
    SteamiLIS2MDL* mag = _handle.board().lis2mdl();
    if (mag == nullptr) {
        return out;
    }
    MagneticField raw;
    if (!mag->magneticField(raw)) {
        return out;  // bus error or data-ready timeout
    }
    out = {(static_cast<float>(raw.x) - mag->xOff) * kMicroteslaPerLsb,
           (static_cast<float>(raw.y) - mag->yOff) * kMicroteslaPerLsb,
           (static_cast<float>(raw.z) - mag->zOff) * kMicroteslaPerLsb};
    return out;
}

float SteamiMagnetometer::strength() {
    const SteamiVector v = read();
    return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
}

void SteamiMagnetometer::calibrate(uint16_t seconds) {
    if (seconds == 0) {
        return;  // no sample: min/max would wipe the offsets and scales
    }
    if (!_handle.ready() || _handle.chipId() != SteamiChip::LIS2MDL) {
        return;
    }
    SteamiLIS2MDL* mag = _handle.board().lis2mdl();
    if (mag == nullptr) {
        return;
    }
    uint32_t samples = static_cast<uint32_t>(seconds) * 50U;
    if (samples > 65535U) {
        samples = 65535U;
    }
    mag->calibrateMinmax3d(static_cast<uint16_t>(samples), 20);
}
