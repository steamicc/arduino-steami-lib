// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "SteamiBoard.h"
#include "SteamiHandle.h"
#include "SteamiTypes.h"

/// Temperature in degrees Celsius: WSEN-HIDS, then WSEN-PADS on V1a/V1b, BME280 on V1c.
class SteamiTemperature {
   public:
    /// Stores the address of the board only; touches no hardware.
    explicit SteamiTemperature(SteamiBoard& board = Steami);

    /// False, with one message, when no temperature sensor answers. Idempotent.
    bool begin() { return _handle.begin(); }
    bool isAvailable() const { return _handle.isAvailable(); }
    SteamiStatus status() const { return _handle.status(); }
    const char* error() const { return _handle.error(); }
    const char* chip() const { return _handle.chip(); }

    /// Degrees Celsius, offset included; NAN when unavailable. Calls begin() on first use.
    float read();

    /// Added to every read() of this object; the shared driver is not changed.
    void setOffset(float celsius) { _offset = celsius; }

   private:
    SteamiHandle _handle;
    float _offset = 0.0f;
};
