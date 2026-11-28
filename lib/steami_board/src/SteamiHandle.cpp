// SPDX-License-Identifier: GPL-3.0-or-later

#include "SteamiHandle.h"

#include <stdio.h>
#include <string.h>

namespace {

// The longest detail is the E11 one with the longest names (about 180 bytes with the NUL), and
// report() keeps up to 220 bytes of detail (ERROR_SIZE minus prefix, class and " (Exx)").
constexpr size_t kDetailSize = 192;

// Where the release comes from, for E11; nullptr when it was detected from the sensors.
const char* trustedOriginText(SteamiOrigin origin) {
    switch (origin) {
        case SteamiOrigin::FORCED_RUNTIME:
            return "forced by Steami.begin()";
        case SteamiOrigin::FORCED_DEFINE:
            return "forced by #define";
        case SteamiOrigin::CONFIG:
            return "read from the board config";
        default:
            return nullptr;
    }
}

}  // namespace

bool SteamiHandle::begin() {
    if (_status != SteamiStatus::NOT_STARTED) {
        return _status == SteamiStatus::OK;
    }
    const SteamiRelease release = _board->release();
    const SteamiFunction function = _info->function;
    if (steamiRoute(release, function, 0) == SteamiChip::NONE) {
        if (release == SteamiRelease::UNKNOWN) {
            _status = SteamiStatus::UNKNOWN_BOARD;  // the board already reported E01
            return false;
        }
        _status = SteamiStatus::NOT_ON_THIS_BOARD;
        reportNotOnThisBoard(release);
        return false;
    }
    uint8_t candidates = 0;
    for (SteamiChip c = steamiRoute(release, function, 0); c != SteamiChip::NONE;
         c = steamiRoute(release, function, candidates)) {
        candidates++;
        const bool ok =
            _info->probe != nullptr ? _info->probe(*_board) : _board->claim(c) == SteamiStatus::OK;
        if (ok) {
            _board->calibrate(c);
            _chip = c;
            _status = SteamiStatus::OK;
            return true;
        }
    }
    _status = SteamiStatus::NOT_RESPONDING;
    reportNotAnswering(release, candidates);
    return false;
}

bool SteamiHandle::ready() {
    if (_status == SteamiStatus::NOT_STARTED) {
        begin();
    }
    return _status == SteamiStatus::OK;
}

const char* SteamiHandle::error() const {
    switch (_status) {
        case SteamiStatus::OK:
            return "";
        case SteamiStatus::NOT_STARTED:
            return "not started";
        case SteamiStatus::NOT_ON_THIS_BOARD:
            return "not on this board";
        case SteamiStatus::NOT_RESPONDING:
            return "no answer";
        case SteamiStatus::NO_MEMORY:
            return "no memory";
        default:
            return "unknown board";
    }
}

void SteamiHandle::reportNotOnThisBoard(SteamiRelease release) {
    const bool v1c = release == SteamiRelease::V1C;
    const SteamiRelease other = v1c ? SteamiRelease::V1AB : SteamiRelease::V1C;
    char detail[kDetailSize];
    snprintf(detail, sizeof(detail), "this STeaMi is a %s, it has no %s (%s is on %s only). %s",
             _board->releaseName(), _info->noun,
             steamiChipName(steamiRoute(other, _info->function, 0)), v1c ? "V1a/V1b" : "V1c",
             _info->fallback);
    _board->report(_info->who, SteamiStatus::NOT_ON_THIS_BOARD, detail, STEAMI_E_NOT_ON_THIS_BOARD);
}

void SteamiHandle::reportNotAnswering(SteamiRelease release, uint8_t candidates) {
    const SteamiFunction function = _info->function;
    const SteamiChip first = steamiRoute(release, function, 0);
    // E11 only when the release decides which chip serves the function; a chip that is on every
    // release (distance, battery) never contradicts it.
    const bool dependsOnRelease = steamiRoute(SteamiRelease::V1C, function, 0) !=
                                  steamiRoute(SteamiRelease::V1AB, function, 0);
    const char* origin = dependsOnRelease ? trustedOriginText(_board->origin()) : nullptr;
    char detail[kDetailSize];
    if (origin != nullptr) {
        snprintf(detail, sizeof(detail),
                 "this STeaMi is set as a %s (%s), but its %s %s (I2C 0x%02X) did not answer. "
                 "Check the #define, Steami.begin() or the board config.",
                 _board->releaseName(), origin, steamiChipName(first), _info->noun,
                 static_cast<unsigned>(steamiChipAddress(first)));
        _board->report(_info->who, SteamiStatus::NOT_RESPONDING, detail,
                       STEAMI_E_RELEASE_CONTRADICTED);
        return;
    }
    if (candidates == 1 && steamiChipAddress(first) == 0) {
        // Not on the I2C bus: the microphone, whose probe waits for a PDM stream.
        snprintf(detail, sizeof(detail),
                 "the %s %s (PDM, MIC_CLK PA3 / MIC_IN PA10) sent no sound. %s",
                 steamiChipName(first), _info->noun, _info->hint);
    } else if (candidates == 1) {
        snprintf(detail, sizeof(detail), "the %s %s (I2C 0x%02X) did not answer. %s",
                 steamiChipName(first), _info->noun,
                 static_cast<unsigned>(steamiChipAddress(first)), _info->hint);
    } else {
        char names[64] = {};
        for (uint8_t i = 0; i < candidates; i++) {
            if (i > 0) {
                strncat(names, ", ", sizeof(names) - strlen(names) - 1);
            }
            strncat(names, steamiChipName(steamiRoute(release, function, i)),
                    sizeof(names) - strlen(names) - 1);
        }
        snprintf(detail, sizeof(detail), "no %s answered (%s). %s", _info->noun, names,
                 _info->hint);
    }
    _board->report(_info->who, SteamiStatus::NOT_RESPONDING, detail, STEAMI_E_NOT_ANSWERING);
}
