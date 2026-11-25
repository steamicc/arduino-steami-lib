// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stdint.h>

#include "SteamiBoard.h"
#include "SteamiHandle.h"
#include "SteamiTypes.h"

/// Battery state from the BQ27441 fuel gauge.
class SteamiBattery {
   public:
    /// Stores the address of the board only; touches no hardware.
    explicit SteamiBattery(SteamiBoard& board = Steami);

    /// False, with one message, when the chip does not answer. Idempotent.
    bool begin() { return _handle.begin(); }
    bool isAvailable() const { return _handle.isAvailable(); }
    SteamiStatus status() const { return _handle.status(); }
    const char* error() const { return _handle.error(); }
    const char* chip() const { return _handle.chip(); }

    /// State of charge in %; 0 when unavailable.
    uint8_t percent();

    /// Battery voltage in V; NAN when unavailable or when the gauge reads 0 mV.
    float voltage();

    /// Average current in mA, positive while charging; 0 when unavailable.
    int16_t current();

    /// True while the battery is charging (current() > 0).
    bool isCharging();

   private:
    SteamiHandle _handle;
};
