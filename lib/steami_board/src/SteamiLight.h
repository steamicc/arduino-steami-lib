// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "SteamiBoard.h"
#include "SteamiHandle.h"
#include "SteamiTypes.h"

/// Ambient light (clear channel count) from the APDS-9960, shared with SteamiColor and
/// SteamiGesture. V1a and V1b only: a V1c reports E20.
class SteamiLight {
   public:
    /// Stores the address of the board only; touches no hardware.
    explicit SteamiLight(SteamiBoard& board = Steami);

    /// False, with one message, when the light sensor is absent or not on this board. Idempotent.
    bool begin() { return _handle.begin(); }
    bool isAvailable() const { return _handle.isAvailable(); }
    SteamiStatus status() const { return _handle.status(); }
    const char* error() const { return _handle.error(); }
    const char* chip() const { return _handle.chip(); }

    /// Raw clear-channel count; 0 when unavailable.
    uint16_t read();

   private:
    SteamiHandle _handle;
};
