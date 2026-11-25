// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "SteamiBoard.h"
#include "SteamiHandle.h"
#include "SteamiTypes.h"

/// Angular rate in degrees per second: ISM330DL on every release.
class SteamiGyroscope {
   public:
    /// Stores the address of the board only; touches no hardware.
    explicit SteamiGyroscope(SteamiBoard& board = Steami);

    /// False, with one message, when the gyroscope does not answer. Idempotent.
    bool begin() { return _handle.begin(); }
    bool isAvailable() const { return _handle.isAvailable(); }
    SteamiStatus status() const { return _handle.status(); }
    const char* error() const { return _handle.error(); }
    const char* chip() const { return _handle.chip(); }

    /// Angular rate in degrees per second; NAN on every axis when unavailable.
    SteamiVector read();
    float x() { return read().x; }
    float y() { return read().y; }
    float z() { return read().z; }

   private:
    SteamiHandle _handle;
};
