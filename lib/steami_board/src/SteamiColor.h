// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "SteamiBoard.h"
#include "SteamiHandle.h"
#include "SteamiTypes.h"

/// Red, green and blue counts from the APDS-9960, shared with SteamiLight and SteamiGesture.
/// V1a and V1b only: a V1c reports E20.
class SteamiColor {
   public:
    /// Stores the address of the board only; touches no hardware.
    explicit SteamiColor(SteamiBoard& board = Steami);

    /// False, with one message, when the colour sensor is absent or not on this board. Idempotent.
    bool begin() { return _handle.begin(); }
    bool isAvailable() const { return _handle.isAvailable(); }
    SteamiStatus status() const { return _handle.status(); }
    const char* error() const { return _handle.error(); }
    const char* chip() const { return _handle.chip(); }

    /// Raw red, green and blue counts. False and three zeros when unavailable or when a channel
    /// cannot be read.
    bool read(uint16_t& r, uint16_t& g, uint16_t& b);
    uint16_t red();
    uint16_t green();
    uint16_t blue();

   private:
    SteamiHandle _handle;
};
