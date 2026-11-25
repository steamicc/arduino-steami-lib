// SPDX-License-Identifier: GPL-3.0-or-later

#include "SteamiColor.h"

namespace {

constexpr SteamiFunctionInfo kInfo = {SteamiFunction::COLOR, "SteamiColor", "colour sensor",
                                      "Check the board or try another one.",
                                      "read() will return false."};

}  // namespace

SteamiColor::SteamiColor(SteamiBoard& board) : _handle(board, kInfo) {}

bool SteamiColor::read(uint16_t& r, uint16_t& g, uint16_t& b) {
    r = g = b = 0;
    if (_handle.status() == SteamiStatus::NOT_STARTED) {
        begin();
    }
    if (!_handle.isAvailable()) {
        return false;
    }
    SteamiAPDS9960* apds = _handle.board().apds9960();
    if (apds == nullptr) {
        return false;
    }
    uint16_t vr = 0, vg = 0, vb = 0;
    if (!apds->redLight(vr) || !apds->greenLight(vg) || !apds->blueLight(vb)) {
        return false;
    }
    r = vr;
    g = vg;
    b = vb;
    return true;
}

uint16_t SteamiColor::red() {
    uint16_t r, g, b;
    read(r, g, b);
    return r;
}

uint16_t SteamiColor::green() {
    uint16_t r, g, b;
    read(r, g, b);
    return g;
}

uint16_t SteamiColor::blue() {
    uint16_t r, g, b;
    read(r, g, b);
    return b;
}
