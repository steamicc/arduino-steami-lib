// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>

#include "SteamiIMP34DT05_const.h"
#include "SteamiPdmFilter.h"

// IMP34DT05 — STMicroelectronics digital MEMS microphone (PDM output).
//
// On the STeaMi, the STM32WB55 clocks the microphone on MIC_CLK (PA3)
// and reads its bit stream on MIC_IN (PA10) with the PDM interface of
// SAI1. A DMA interrupt converts each millisecond of bits into 16 PCM
// samples (16 kHz, signed 16 bits) with ST's PDM to PCM filter, then:
//
// * keeps the samples for read() and passes them to onReceive();
// * measures the sound level over a window (rms(), peak(), levelDbfs()).
//
// The microphone has no registers, hence no deviceId(), reset() or
// calibration memory. begin() checks instead that the data line carries
// a live PDM stream.
class SteamiIMP34DT05 {
   public:
    // Sound level of the last completed window.
    struct LevelResult {
        float rms;      // RMS amplitude, PCM units (0 to 32768)
        uint16_t peak;  // largest absolute sample, PCM units
        float dbfs;     // RMS in dB relative to a full-scale sine
        float dbSpl;    // uncalibrated sound pressure level estimate, dB SPL
    };

    // Called from the DMA interrupt with each new block of samples
    // (IMP34DT05_SAMPLES_PER_BLOCK samples every millisecond). Keep it
    // short: no Serial, no delay().
    typedef void (*ReceiveCallback)(const int16_t* samples, size_t count);

    SteamiIMP34DT05();

    // Start the clock, find the microphone, wait for the first samples.
    // Only IMP34DT05_SAMPLE_RATE_16K is supported. Returns false on
    // another sample rate, on another board, or when no PDM stream comes
    // back from the microphone.
    bool begin(uint32_t sampleRate = IMP34DT05_SAMPLE_RATE_16K);

    // Stop the acquisition and release SAI1, its DMA channel and the pins.
    void end();

    // Stop / restart the PDM clock. Without clock the IMP34DT05 sleeps.
    // powerOn() returns after the microphone woke up (about 20 ms).
    void powerOn();
    void powerOff();

    // True between begin() / powerOn() and end() / powerOff().
    bool isRunning() const;

    uint32_t sampleRate() const;

    // --- Samples ---------------------------------------------------------

    // Number of samples waiting to be read (at most
    // IMP34DT05_BUFFER_SAMPLES, i.e. 64 ms).
    size_t available() const;

    // True when available() > 0.
    bool dataReady() const;

    // Copy up to `count` waiting samples, oldest first; returns how many
    // were copied. Never blocks.
    size_t read(int16_t* samples, size_t count);

    // Drop the waiting samples (e.g. to start a fresh recording).
    void clear();

    // Samples lost because the buffer was full (read() not called often
    // enough). Reset by begin() and powerOn().
    uint32_t overruns() const;

    // Register (or remove with nullptr) the interrupt callback.
    void onReceive(ReceiveCallback callback);

    // --- Volume ----------------------------------------------------------

    // Digital gain: 0 mutes, v multiplies the signal by 2 * v
    // (IMP34DT05_VOLUME_MAX = 6). Default IMP34DT05_VOLUME_DEFAULT (5,
    // +20 dB), as in ST's examples. Returns false when out of range.
    bool setVolume(uint8_t volume);
    uint8_t volume() const;

    // --- Sound level -----------------------------------------------------

    // Length of the measurement window, IMP34DT05_LEVEL_WINDOW_MIN_MS to
    // IMP34DT05_LEVEL_WINDOW_MAX_MS (default 100 ms). Short windows
    // (10 ms) catch claps, long ones (200 ms or more) give a steady level.
    void setLevelWindowMs(uint16_t windowMs);
    uint16_t levelWindowMs() const;

    // True when a window completed since the last level reading.
    bool levelReady() const;

    // Values of the last completed window; NAN (or 0 for peak()) until
    // a first window completed.
    float rms();
    uint16_t peak();

    // 20 * log10(rms * sqrt(2) / 32768): a full-scale sine reads 0 dBFS
    // (AES17 convention, used by the datasheet sensitivity). Never below
    // IMP34DT05_LEVEL_FLOOR_DBFS.
    float levelDbfs();

    // Sound pressure level estimated from the datasheet sensitivity
    // (-26 dBFS at 94 dB SPL) and the volume, plus setLevelOffset().
    // Uncalibrated: compare with a sound level meter and set the offset.
    // NAN at volume 0.
    float levelDbSpl();

    // The four values above from the same window, at once.
    LevelResult level();

    // Additive dB correction applied by levelDbSpl().
    void setLevelOffset(float offsetDb);

    // --- Frequency -------------------------------------------------------

    // Record IMP34DT05_FFT_SIZE samples (32 ms) and return the frequency
    // of the strongest component, in Hz (about 1 Hz resolution after
    // interpolation). Blocks about 35 ms; clears the samples waiting for
    // read(). NAN when not running or on silence.
    float dominantFrequencyHz();

    // --- Microphone slot -------------------------------------------------

    // The SAI reads two microphones per data line (one on each clock
    // edge); begin() picks the slot that carries the STeaMi microphone.
    // setSlot(0 or 1) forces a slot, setSlot(IMP34DT05_SLOT_AUTO)
    // restores detection at the next begin() / powerOn().
    bool setSlot(uint8_t slot);
    uint8_t slot() const;

    // --- Pure helpers (no hardware) --------------------------------------

    // RMS of `count` samples (0 when count is 0).
    static float computeRms(const int16_t* samples, size_t count);

    // dBFS of an RMS value, as levelDbfs().
    static float rmsToDbfs(float rms);

    // Frequency of the strongest component of `count` samples taken at
    // `sampleRateHz`: Hann window, radix-2 FFT, Gaussian interpolation
    // of the peak. `count` must be a power of two from 16 to
    // IMP34DT05_FFT_MAX_SIZE. NAN on a bad size or on silence.
    static float computeDominantFrequency(const int16_t* samples, size_t count, float sampleRateHz);

    // Process one block of raw SAI data (IMP34DT05_SAI_BYTES_PER_BLOCK
    // bytes, slots 0 and 1 alternating). Called by the DMA interrupt;
    // public so that host tests can feed synthetic PDM streams.
    void processBlock(const uint8_t* data, size_t length);

   private:
    SteamiPdmFilter _filter;
    ReceiveCallback _callback = nullptr;

    bool _begun = false;
    volatile bool _running = false;

    // Slot detection.
    uint8_t _slotMode = IMP34DT05_SLOT_AUTO;
    volatile uint8_t _slot = 0;
    volatile bool _slotLocked = false;
    volatile bool _streamAlive = false;
    volatile uint32_t _blocks = 0;
    uint32_t _slotScore[IMP34DT05_SAI_SLOTS] = {};
    volatile uint16_t _settleBlocks = 0;

    // Sample buffer: written by the interrupt, read by read().
    int16_t _buffer[IMP34DT05_BUFFER_SAMPLES] = {};
    volatile uint16_t _head = 0;
    volatile uint16_t _tail = 0;
    volatile uint32_t _overruns = 0;

    // Level window being accumulated by the interrupt.
    uint16_t _windowMs = IMP34DT05_LEVEL_WINDOW_DEFAULT_MS;
    uint32_t _windowSamples = 0;
    uint64_t _accSquares = 0;
    uint32_t _accCount = 0;
    uint16_t _accPeak = 0;

    // Last completed window.
    volatile float _meanSquare = 0.0f;
    volatile uint16_t _windowPeak = 0;
    volatile bool _levelValid = false;
    volatile bool _levelReady = false;
    float _levelOffset = 0.0f;

    bool startAndWait();
    void resetStream();
    void resetLevel();
    void publish(const int16_t* samples, size_t count);
    float dbSplFromDbfs(float dbfs) const;
};
