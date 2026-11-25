// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "SteamiBoard.h"
#include "SteamiHandle.h"
#include "SteamiTypes.h"

/// Distance in mm or cm from the VL53L1X time-of-flight sensor.
class SteamiDistance {
   public:
    /// Stores the address of the board only; touches no hardware.
    explicit SteamiDistance(SteamiBoard& board = Steami);

    /// False, with one message, when the chip does not answer. Idempotent.
    bool begin() { return _handle.begin(); }
    bool isAvailable() const { return _handle.isAvailable(); }
    SteamiStatus status() const { return _handle.status(); }
    const char* error() const { return _handle.error(); }
    const char* chip() const { return _handle.chip(); }

    /// Distance in mm; -1 when unavailable or when the sensor gives no result.
    int read();

    /// Distance in cm; NAN when read() is -1.
    float cm();

   private:
    SteamiHandle _handle;
};
