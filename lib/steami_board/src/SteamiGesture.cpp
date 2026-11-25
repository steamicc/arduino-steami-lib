// SPDX-License-Identifier: GPL-3.0-or-later

#include "SteamiGesture.h"

namespace {

constexpr SteamiFunctionInfo kInfo = {SteamiFunction::GESTURE, "SteamiGesture", "gesture sensor",
                                      "Check the board or try another one.",
                                      "read() will return NONE."};

}  // namespace

SteamiGesture::SteamiGesture(SteamiBoard& board) : _handle(board, kInfo) {}

bool SteamiGesture::begin() {
    const bool wasStarted = _handle.status() != SteamiStatus::NOT_STARTED;
    if (!_handle.begin()) {
        return false;
    }
    if (!wasStarted) {
        SteamiAPDS9960* apds = _handle.board().apds9960();
        if (apds != nullptr) {
            apds->enableGestureSensor();
        }
    }
    return true;
}

SteamiGesture::Direction SteamiGesture::read(uint32_t timeoutMs) {
    if (_handle.status() == SteamiStatus::NOT_STARTED) {
        begin();
    }
    if (!_handle.isAvailable()) {
        return NONE;
    }
    SteamiAPDS9960* apds = _handle.board().apds9960();
    if (apds == nullptr) {
        return NONE;
    }
    switch (apds->readGesture(timeoutMs)) {
        case SteamiAPDS9960::Gesture::LEFT:
            return LEFT;
        case SteamiAPDS9960::Gesture::RIGHT:
            return RIGHT;
        case SteamiAPDS9960::Gesture::UP:
            return UP;
        case SteamiAPDS9960::Gesture::DOWN:
            return DOWN;
        case SteamiAPDS9960::Gesture::NEAR:
            return NEAR;
        case SteamiAPDS9960::Gesture::FAR:
            return FAR;
        default:
            return NONE;
    }
}

const char* SteamiGesture::name(Direction d) {
    switch (d) {
        case LEFT:
            return "LEFT";
        case RIGHT:
            return "RIGHT";
        case UP:
            return "UP";
        case DOWN:
            return "DOWN";
        case NEAR:
            return "NEAR";
        case FAR:
            return "FAR";
        default:
            return "NONE";
    }
}
