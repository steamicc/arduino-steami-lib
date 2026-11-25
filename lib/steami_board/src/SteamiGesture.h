// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "SteamiBoard.h"
#include "SteamiHandle.h"
#include "SteamiTypes.h"

/// Hand gestures from the APDS-9960, shared with SteamiLight and SteamiColor. V1a and V1b only:
/// a V1c reports E20.
///
/// begin() turns the gesture engine on, and the chip serves the gesture first: while a hand is
/// over the sensor, the light and colour readings pause and resume afterwards.
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

    /// The gesture in progress, read for at most `timeoutMs`; NONE when the sensor has not seen
    /// a gesture (it does not wait for one to start) or when unavailable. Call it on every
    /// loop() turn; with 0 it only decodes what the chip already holds and can cut a gesture.
    Direction read(uint32_t timeoutMs = 0);

    /// "NONE", "LEFT", "RIGHT", "UP", "DOWN", "NEAR" or "FAR".
    static const char* name(Direction d);

   private:
    SteamiHandle _handle;
};
