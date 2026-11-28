// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <SteamiIMP34DT05.h>
#include <stddef.h>
#include <stdint.h>

#include "SteamiBoard.h"
#include "SteamiHandle.h"
#include "SteamiTypes.h"

/// Sound level in dB SPL and dominant frequency: the IMP34DT05 PDM microphone, on every release.
/// Every object shares one driver, also returned by steamiImp34dt05().
class SteamiMicrophone {
   public:
    /// Stores the address of the board only; touches no hardware.
    explicit SteamiMicrophone(SteamiBoard& board = Steami);

    /// Starts the shared driver when it is not running. False, with one message, when no PDM
    /// stream comes back from the microphone. Idempotent while the driver runs.
    bool begin();
    bool isAvailable() const { return _handle.isAvailable(); }
    SteamiStatus status() const { return _handle.status(); }
    const char* error() const { return _handle.error(); }
    const char* chip() const { return _handle.chip(); }  // "IMP34DT05"

    /// Stops the shared driver (SAI1, its DMA channel and the pins) and sets status() back to
    /// NOT_STARTED.
    void end();

    /// Estimated dB SPL of the last completed window, offsets included; NAN when unavailable or
    /// at volume 0. The first call after begin() waits for the first window (at most window() +
    /// 20 ms), later calls never wait. Calls begin() on first use.
    float read();

    /// dBFS of the same window, never below -120; NAN when unavailable.
    float dbfs();

    /// Largest absolute sample of the same window, 0 to 32768; 0 when unavailable.
    uint16_t peak();

    /// True when a window completed since the last level reading (read(), dbfs(), peak() or
    /// isLoud(), by any object or through steamiImp34dt05()). Does not consume it.
    bool levelReady();

    /// Length of the level window, 1 to 4000 ms (100 by default).
    void setWindow(uint16_t ms);
    uint16_t window() const;

    /// Added to every read() of this object, on top of the calibration of the driver.
    void setOffset(float db) { _offset = db; }
    float offset() const { return _offset; }

    /// Digital gain, 0 (mute) to 6, 5 by default. False when out of range.
    bool setVolume(uint8_t v);
    uint8_t volume() const;

    /// Frequency of the strongest component of 512 new samples, in Hz. Blocks about 35 ms. NAN
    /// when unavailable, at volume 0, or when those samples read below `minDb` dB SPL (offsets
    /// included, as read()).
    float frequency(float minDb = 50.0f);

    /// Drops the waiting samples, then copies `count` new ones (16 per millisecond) into
    /// `samples`, blocking about count / 16 ms. Returns how many were copied, 0 when unavailable.
    size_t readInto(int16_t* samples, size_t count);

    /// True when read() is above `thresholdDb`.
    bool isLoud(float thresholdDb = 70.0f);

   private:
    SteamiIMP34DT05* liveDriver();
    SteamiIMP34DT05::LevelResult currentLevel();

    SteamiHandle _handle;
    float _offset = 0.0f;
    bool _waitFirst = false;  // the next level reading waits for a first window
};

/// The shared driver, started (and calibrated) on first use; nullptr when no PDM stream comes
/// back from the microphone. The same driver as every SteamiMicrophone.
SteamiIMP34DT05* steamiImp34dt05();
