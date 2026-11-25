// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "SteamiBoard.h"
#include "SteamiHandle.h"
#include "SteamiTypes.h"

/// Air pressure in hPa: WSEN-PADS on V1a/V1b, BME280 on V1c.
class SteamiPressure {
   public:
    /// Stores the address of the board only; touches no hardware.
    explicit SteamiPressure(SteamiBoard& board = Steami);

    /// False, with one message, when no pressure sensor answers. Idempotent.
    bool begin() { return _handle.begin(); }
    bool isAvailable() const { return _handle.isAvailable(); }
    SteamiStatus status() const { return _handle.status(); }
    const char* error() const { return _handle.error(); }
    const char* chip() const { return _handle.chip(); }

    /// Pressure in hPa; NAN when unavailable. Calls begin() on first use.
    float read();

    /// Altitude in metres from read() and the sea-level pressure, the same formula on every
    /// release; NAN when unavailable.
    float altitude(float seaLevelHpa = 1013.25f);

   private:
    SteamiHandle _handle;
};
