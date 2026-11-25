// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "SteamiBoard.h"
#include "SteamiHandle.h"
#include "SteamiTypes.h"

/// Relative humidity in %: WSEN-HIDS on V1a/V1b, BME280 on V1c.
class SteamiHumidity {
   public:
    /// Stores the address of the board only; touches no hardware.
    explicit SteamiHumidity(SteamiBoard& board = Steami);

    /// False, with one message, when no humidity sensor answers. Idempotent.
    bool begin() { return _handle.begin(); }
    bool isAvailable() const { return _handle.isAvailable(); }
    SteamiStatus status() const { return _handle.status(); }
    const char* error() const { return _handle.error(); }
    const char* chip() const { return _handle.chip(); }

    /// Relative humidity in %; NAN when unavailable. Calls begin() on first use.
    float read();

   private:
    SteamiHandle _handle;
};
