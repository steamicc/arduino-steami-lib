// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "SteamiBoard.h"
#include "SteamiHandle.h"
#include "SteamiTypes.h"

/// Acceleration in g (screen up and flat reads z of about -1): ISM330DL on every release.
class SteamiAccelerometer {
   public:
    /// Stores the address of the board only; touches no hardware.
    explicit SteamiAccelerometer(SteamiBoard& board = Steami);

    /// False, with one message, when the accelerometer does not answer. Idempotent.
    bool begin() { return _handle.begin(); }
    bool isAvailable() const { return _handle.isAvailable(); }
    SteamiStatus status() const { return _handle.status(); }
    const char* error() const { return _handle.error(); }
    const char* chip() const { return _handle.chip(); }

    /// Acceleration in g, calibration offset included; NAN on every axis when unavailable.
    SteamiVector read();
    float x() { return read().x; }
    float y() { return read().y; }
    float z() { return read().z; }

    /// Norm of read() in g, NAN when unavailable.
    float strength();

    /// True when strength() is above `g`.
    bool isShaken(float g = 2.0f) { return strength() > g; }

   private:
    SteamiHandle _handle;
};
