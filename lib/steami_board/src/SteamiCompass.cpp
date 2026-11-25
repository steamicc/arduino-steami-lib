// SPDX-License-Identifier: GPL-3.0-or-later

#include "SteamiCompass.h"

#include <math.h>

namespace {

constexpr SteamiFunctionInfo kInfo = {SteamiFunction::COMPASS, "SteamiCompass", "compass",
                                      "Check the board or try another one.",
                                      "heading() will return NAN."};

}  // namespace

SteamiCompass::SteamiCompass(SteamiBoard& board) : _handle(board, kInfo) {}

bool SteamiCompass::begin() {
    if (!_handle.begin()) {
        return false;
    }
    if (_hasDeclination) {
        SteamiLIS2MDL* mag = driver();
        if (mag != nullptr) {
            mag->setDeclination(_declination);
        }
    }
    return true;
}

SteamiLIS2MDL* SteamiCompass::driver() {
    if (_handle.chipId() != SteamiChip::LIS2MDL) {
        return nullptr;
    }
    return _handle.board().lis2mdl();
}

float SteamiCompass::heading() {
    if (_handle.status() == SteamiStatus::NOT_STARTED) {
        begin();
    }
    if (!_handle.isAvailable()) {
        return NAN;
    }
    SteamiLIS2MDL* mag = driver();
    return mag == nullptr ? NAN : mag->headingFlatOnly();
}

const char* SteamiCompass::direction() {
    const float angle = heading();
    if (isnan(angle)) {
        return "";
    }
    SteamiLIS2MDL* mag = driver();
    return mag == nullptr ? "" : mag->directionLabel(angle);
}

void SteamiCompass::calibrate(uint16_t seconds) {
    if (seconds == 0) {
        return;  // no sample: min/max would wipe the offsets and scales
    }
    if (_handle.status() == SteamiStatus::NOT_STARTED) {
        begin();
    }
    SteamiLIS2MDL* mag = _handle.isAvailable() ? driver() : nullptr;
    if (mag == nullptr) {
        return;
    }
    uint32_t samples = static_cast<uint32_t>(seconds) * 50U;
    if (samples > 65535U) {
        samples = 65535U;
    }
    mag->calibrateMinmax3d(static_cast<uint16_t>(samples), 20);
}

void SteamiCompass::setDeclination(float deg) {
    _declination = deg;
    _hasDeclination = true;
    if (_handle.isAvailable()) {
        SteamiLIS2MDL* mag = driver();
        if (mag != nullptr) {
            mag->setDeclination(deg);
        }
    }
}
