// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "SteamiBoard.h"
#include "SteamiHandle.h"
#include "SteamiTypes.h"

/// Magnetic field in microtesla: LIS2MDL on every release, shared with SteamiCompass.
class SteamiMagnetometer {
   public:
    /// Stores the address of the board only; touches no hardware.
    explicit SteamiMagnetometer(SteamiBoard& board = Steami);

    /// False, with one message, when the magnetometer does not answer. Idempotent.
    bool begin() { return _handle.begin(); }
    bool isAvailable() const { return _handle.isAvailable(); }
    SteamiStatus status() const { return _handle.status(); }
    const char* error() const { return _handle.error(); }
    const char* chip() const { return _handle.chip(); }

    /// Field in microtesla; NAN on every axis when unavailable or when the I2C read fails. The
    /// hard-iron offsets are removed only after a calibration (`cm` in the board config, or
    /// calibrate()): before that the field includes the board's own and can be much larger.
    SteamiVector read();
    float x() { return read().x; }
    float y() { return read().y; }
    float z() { return read().z; }

    /// Norm of read() in microtesla, NAN when unavailable.
    float strength();

    /// Min/max calibration over `seconds` (50 samples per second): turn the board in every
    /// direction meanwhile. Updates the shared driver; does nothing when unavailable or for 0.
    void calibrate(uint16_t seconds = 12);

   private:
    SteamiHandle _handle;
};
