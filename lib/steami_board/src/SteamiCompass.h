// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "SteamiAccelerometer.h"
#include "SteamiBoard.h"
#include "SteamiHandle.h"
#include "SteamiTypes.h"

/// Heading of the board held flat, or tilted with headingTiltCompensated(): LIS2MDL on every
/// release, shared with SteamiMagnetometer.
class SteamiCompass {
   public:
    /// Stores the address of the board only; touches no hardware.
    explicit SteamiCompass(SteamiBoard& board = Steami);

    /// False, with one message, when the compass does not answer. Idempotent. On the first
    /// success it applies the declination set before.
    bool begin();
    bool isAvailable() const { return _handle.isAvailable(); }
    SteamiStatus status() const { return _handle.status(); }
    const char* error() const { return _handle.error(); }
    const char* chip() const { return _handle.chip(); }

    /// Degrees clockwise from north in [0, 360), screen up and flat; NAN when unavailable or when
    /// the I2C read fails.
    float heading();

    /// Same heading, compensated for the tilt of the board with the ISM330DL accelerometer
    /// (started as by SteamiAccelerometer::begin()); equal to heading() when the board is flat.
    /// Valid up to about 60 degrees of tilt and only after a 3D calibration: calibrate(), the
    /// board config, or SteamiLIS2MDL::calibrateMinmax3d(), not calibrateMinmax2d(). NAN when
    /// the compass or the accelerometer is unavailable or a read fails.
    float headingTiltCompensated();

    /// "N", "NE", "E"... for heading(); "" when unavailable.
    const char* direction();

    /// Min/max calibration over `seconds`: turn the board in every direction meanwhile. Does
    /// nothing when unavailable or for 0.
    void calibrate(uint16_t seconds = 12);

    /// Magnetic declination in degrees added to the heading. May be called before begin().
    void setDeclination(float deg);

   private:
    SteamiLIS2MDL* driver();

    SteamiHandle _handle;
    SteamiAccelerometer _accel;  // headingTiltCompensated() only
    float _declination = 0.0f;
    bool _hasDeclination = false;
};
