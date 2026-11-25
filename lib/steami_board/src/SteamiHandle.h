// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "SteamiBoard.h"
#include "SteamiTypes.h"

/// What a per-function class tells its handle: the function and the words of its messages.
struct SteamiFunctionInfo {
    SteamiFunction function;
    const char* who;       // class name literal, e.g. "SteamiTemperature"
    const char* noun;      // "temperature sensor", "battery gauge"...
    const char* hint;      // E10 advice, e.g. "Check the board or try another one."
    const char* fallback;  // e.g. "read() will return NAN."
};

/// Common part of the per-function classes: finds the chip that serves a function on this
/// release, claims it on the board, applies its calibration, and reports E10, E11 or E20 once.
class SteamiHandle {
   public:
    /// Stores the addresses only; touches no hardware. `info` must outlive the handle.
    constexpr SteamiHandle(SteamiBoard& board, const SteamiFunctionInfo& info) noexcept
        : _board(&board), _info(&info) {}

    /// Tries the candidates of the function in order and keeps the first that answers. True when
    /// one answered. Idempotent: once tried, returns status() == OK without bus traffic.
    bool begin();

    /// Calls begin() on first use; true when the function is available.
    bool ready();

    bool isAvailable() const { return _status == SteamiStatus::OK; }
    SteamiStatus status() const { return _status; }

    /// "" when OK, "not started", "not on this board", "no answer" or "unknown board".
    const char* error() const;

    /// Name of the chip in use, "" when none.
    const char* chip() const { return steamiChipName(_chip); }
    SteamiChip chipId() const { return _chip; }
    SteamiBoard& board() const { return *_board; }

   private:
    void reportNotOnThisBoard(SteamiRelease release);
    void reportNotAnswering(SteamiRelease release, uint8_t candidates);

    SteamiBoard* _board;
    const SteamiFunctionInfo* _info;
    SteamiChip _chip = SteamiChip::NONE;
    SteamiStatus _status = SteamiStatus::NOT_STARTED;
};
