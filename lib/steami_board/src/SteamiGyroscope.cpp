// SPDX-License-Identifier: GPL-3.0-or-later

#include "SteamiGyroscope.h"

#include <math.h>

namespace {

constexpr SteamiFunctionInfo kInfo = {SteamiFunction::GYROSCOPE, "SteamiGyroscope", "gyroscope",
                                      "Check the board or try another one.",
                                      "read() will return NAN."};

}  // namespace

SteamiGyroscope::SteamiGyroscope(SteamiBoard& board) : _handle(board, kInfo) {}

SteamiVector SteamiGyroscope::read() {
    SteamiVector out = {NAN, NAN, NAN};
    if (!_handle.ready() || _handle.chipId() != SteamiChip::ISM330DL) {
        return out;
    }
    SteamiISM330DL* imu = _handle.board().ism330dl();
    if (imu == nullptr) {
        return out;
    }
    SteamiISM330DL::Vector3 value{};
    if (imu->gyroscopeDps(value)) {
        out = {value.x, value.y, value.z};
    }
    return out;
}
