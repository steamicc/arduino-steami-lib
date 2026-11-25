// SPDX-License-Identifier: GPL-3.0-or-later

#include "SteamiLight.h"

namespace {

constexpr SteamiFunctionInfo kInfo = {SteamiFunction::LIGHT, "SteamiLight", "light sensor",
                                      "Check the board or try another one.",
                                      "read() will return 0."};

}  // namespace

SteamiLight::SteamiLight(SteamiBoard& board) : _handle(board, kInfo) {}

uint16_t SteamiLight::read() {
    if (_handle.status() == SteamiStatus::NOT_STARTED) {
        begin();
    }
    if (!_handle.isAvailable()) {
        return 0;
    }
    SteamiAPDS9960* apds = _handle.board().apds9960();
    if (apds == nullptr) {
        return 0;
    }
    uint16_t value = 0;
    return apds->ambientLight(value) ? value : 0;
}
