// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "SteamiBoard.h"
#include "SteamiHandle.h"
#include "SteamiTypes.h"

/// Hand gestures from the APDS-9960, shared with SteamiLight and SteamiColor. V1a and V1b only:
/// a V1c reports E20.
///
/// While the gesture engine tracks a hand, the light and colour readings freeze: the chip
/// serves the gesture first. begin() turns that engine on, so read the light and the colour
/// before the gesture begin() when both matter.
class SteamiGesture {
   public:
    /// Same order as the driver's SteamiAPDS9960::Gesture.
    enum Direction : uint8_t { NONE, LEFT, RIGHT, UP, DOWN, NEAR, FAR };

    /// Stores the address of the board only; touches no hardware.
    explicit SteamiGesture(SteamiBoard& board = Steami);

    /// False, with one message, when the gesture sensor is absent or not on this board.
    /// Idempotent. On the first success it enables the gesture engine.
    bool begin();
    bool isAvailable() const { return _handle.isAvailable(); }
    SteamiStatus status() const { return _handle.status(); }
    const char* error() const { return _handle.error(); }
    const char* chip() const { return _handle.chip(); }

    /// The gesture seen, waiting up to `timeoutMs` for one; NONE when there is none or when
    /// unavailable. With 0 it only looks at what the chip already holds.
    Direction read(uint32_t timeoutMs = 0);

    /// "NONE", "LEFT", "RIGHT", "UP", "DOWN", "NEAR" or "FAR".
    static const char* name(Direction d);

   private:
    SteamiHandle _handle;
};
