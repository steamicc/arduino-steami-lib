// SPDX-License-Identifier: BSD-3-Clause
//
// PDM to PCM filter of the STeaMi IMP34DT05 driver, ported from
// STMicroelectronics' X-NUCLEO-IKS02A1-Audio library: see
// SteamiPdmFilter.cpp for the origin, the changes and the licence.

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "SteamiIMP34DT05_const.h"

// Turns a PDM bit stream (1.28 MHz) into 16-bit PCM samples (16 kHz):
//
//   1. sinc3 filter decimating by 8, computed with ST's lookup tables
//      (one output per PDM byte);
//   2. sinc4 FIR decimating by 10, whose coefficients set the volume;
//   3. first-order high-pass filter removing the DC offset.
//
// The filter keeps its state between calls, so a stream can be fed in
// blocks of any size. Pure computation: no hardware access, usable on
// the host for tests.
//
// It runs in the DMA interrupt, so it is written for speed: the sinc3
// outputs of a block go to a linear buffer, then each FIR output is a
// dot product over 19 pairs of 16-bit values (one SMLALD instruction per
// pair on a Cortex-M4, exact 64-bit sums). Results are bit-identical to
// a sample-by-sample implementation of the same three stages.
class SteamiPdmFilter {
   public:
    SteamiPdmFilter();

    // Clear the filter state (as after construction). Keeps the volume.
    void reset();

    // Volume IMP34DT05_VOLUME_MIN (mute) to IMP34DT05_VOLUME_MAX; the
    // signal is multiplied by 2 * volume. Returns false, and keeps the
    // current volume, when the value is out of range.
    bool setVolume(uint8_t volume);
    uint8_t volume() const;

    // Filter `count` PDM bytes read every `stride` bytes from `pdm`
    // (stride 2 picks one microphone out of the SAI's two slots). Each
    // byte holds 8 PDM bits, the oldest one in the MSB. Writes one PCM
    // sample per IMP34DT05_PDM_BYTES_PER_SAMPLE bytes to `pcm` and
    // returns the number of samples written; the remainder is kept for
    // the next call.
    size_t process(const uint8_t* pdm, size_t count, size_t stride, int16_t* pcm);

    // First stage alone: sinc3 output for the last three PDM bytes
    // (newest in bits 16..23 of `window`). Exposed for the tests.
    static int16_t cicOutput(uint32_t window);

    // FIR taps padded to an even count (a zero tap in front, applied to
    // the sample just older than the 37 the FIR uses), so that the dot
    // product runs over whole pairs.
    static constexpr uint8_t FIR_WINDOW = IMP34DT05_FIR_TAPS + 1;

    // sinc3 outputs kept between calls: the next FIR window minus the
    // samples still to come, i.e. FIR_KEEP plus the phase.
    static constexpr uint8_t FIR_KEEP = FIR_WINDOW - IMP34DT05_FIR_DECIMATION;
    static constexpr uint8_t FIR_HISTORY_MAX = FIR_KEEP + IMP34DT05_FIR_DECIMATION - 1;

   private:
    uint8_t _volume = IMP34DT05_VOLUME_DEFAULT;

    // Last two PDM bytes: newest in bits 16..23, previous in bits 8..15.
    uint32_t _cicWindow = 0;

    // sinc3 outputs since the last FIR output (0 to 9).
    uint8_t _firPhase = 0;

    // The FIR_KEEP + _firPhase newest sinc3 outputs, oldest first.
    alignas(4) int16_t _firHistory[FIR_HISTORY_MAX] = {};

    // High-pass filter state: previous input and previous output.
    int32_t _hpPrevIn = 0;
    int16_t _hpPrevOut = 0;

    int16_t highPass(int16_t sample);
};
