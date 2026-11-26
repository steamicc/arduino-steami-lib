// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stddef.h>
#include <stdint.h>

// IMP34DT05 — STMicroelectronics MEMS microphone with a PDM output.
//
// The microphone has no register bank: it streams one bit per clock
// period (pulse density modulation). The STM32WB55 clocks it and reads
// the bit stream with the PDM interface of SAI1, then the driver turns
// the bits into 16-bit PCM samples.

// --- Audio format ---------------------------------------------------------

// PCM sample rate delivered by the driver (the only rate supported).
constexpr uint32_t IMP34DT05_SAMPLE_RATE_16K = 16000;

// PDM clock sent to the microphone on MIC_CLK (PA3). The IMP34DT05
// datasheet asks for 1.2 to 3.25 MHz in normal mode; 1.28 MHz divided
// by 80 gives 16 kHz, the configuration of ST's filter.
constexpr uint32_t IMP34DT05_PDM_CLOCK_HZ = 1280000;

// PDM bits per PCM sample: the CIC (lookup-table) stage decimates by 8,
// the FIR stage by 10.
constexpr uint8_t IMP34DT05_CIC_DECIMATION = 8;
constexpr uint8_t IMP34DT05_FIR_DECIMATION = 10;
constexpr uint8_t IMP34DT05_DECIMATION = IMP34DT05_CIC_DECIMATION * IMP34DT05_FIR_DECIMATION;

// PDM bytes (8 bits each, oldest bit in the MSB) consumed per PCM sample.
constexpr uint8_t IMP34DT05_PDM_BYTES_PER_SAMPLE = IMP34DT05_DECIMATION / 8;

// Number of taps of the FIR decimator (sinc4, decimation 10).
constexpr uint8_t IMP34DT05_FIR_TAPS = 37;

// --- Volume (digital gain of the FIR stage) -------------------------------

// Volume 0 mutes the output; volume v multiplies the signal by 2 * v
// (+6 dB at 1, +20 dB at 5, +21.6 dB at 6). Same scale as ST's
// pdm2pcm_volume().
constexpr uint8_t IMP34DT05_VOLUME_MIN = 0;
constexpr uint8_t IMP34DT05_VOLUME_MAX = 6;
constexpr uint8_t IMP34DT05_VOLUME_DEFAULT = 5;

// --- SAI1 acquisition -----------------------------------------------------

// The DMA interrupt fires every millisecond with one block.
constexpr uint8_t IMP34DT05_BLOCK_MS = 1;
constexpr uint16_t IMP34DT05_SAMPLES_PER_BLOCK =
    (IMP34DT05_SAMPLE_RATE_16K / 1000) * IMP34DT05_BLOCK_MS;

// The SAI PDM interface serves one microphone pair per data line: each
// SAI frame holds one byte per microphone of the pair (slot 0 and
// slot 1). The STeaMi has a single microphone, the driver finds which
// slot carries it.
constexpr uint8_t IMP34DT05_SAI_SLOTS = 2;
constexpr uint16_t IMP34DT05_PDM_BYTES_PER_BLOCK =
    IMP34DT05_SAMPLES_PER_BLOCK * IMP34DT05_PDM_BYTES_PER_SAMPLE;
constexpr uint16_t IMP34DT05_SAI_BYTES_PER_BLOCK =
    IMP34DT05_PDM_BYTES_PER_BLOCK * IMP34DT05_SAI_SLOTS;

// SAI1 kernel clock: PLLSAI1 "P" output at 64 MHz (VCO 128 MHz, P = 2).
// The SAI bit clock is 64 MHz / 25 = 2.56 MHz, two bits (one per
// microphone of the pair) per PDM clock period: 1.28 MHz on MIC_CLK.
constexpr uint32_t IMP34DT05_SAI_KERNEL_CLOCK_HZ = 64000000;
constexpr uint32_t IMP34DT05_PLLSAI1_VCO_HZ = 128000000;
constexpr uint8_t IMP34DT05_SAI_MCKDIV = 25;
constexpr uint8_t IMP34DT05_SAI_FRAME_BITS = 16;

// Priority of the DMA interrupt (0 = most urgent, 15 = least).
constexpr uint8_t IMP34DT05_DMA_IRQ_PRIORITY = 6;

// Slot selection: automatic detection, or a fixed slot.
constexpr uint8_t IMP34DT05_SLOT_AUTO = 0xFF;

// --- Driver timing --------------------------------------------------------

// Blocks dropped after the clock starts: the IMP34DT05 needs up to
// 10 ms to wake up, then the filters settle.
constexpr uint16_t IMP34DT05_WARMUP_BLOCKS = 20;

// Blocks used to find the slot that carries the microphone.
constexpr uint16_t IMP34DT05_DETECT_BLOCKS = 16;

// Blocks filtered but not delivered after a (re)start or a slot change,
// while the high-pass filter forgets the DC offset.
constexpr uint16_t IMP34DT05_SETTLE_BLOCKS = 10;

// How long begin() and powerOn() wait for the first samples.
constexpr uint32_t IMP34DT05_START_TIMEOUT_MS = 200;

// --- Sample buffer --------------------------------------------------------

// Samples kept for read() (64 ms at 16 kHz). Must be a power of two.
constexpr uint16_t IMP34DT05_BUFFER_SAMPLES = 1024;

// --- Sound level ----------------------------------------------------------

constexpr float IMP34DT05_FULL_SCALE = 32768.0f;
constexpr uint16_t IMP34DT05_LEVEL_WINDOW_DEFAULT_MS = 100;
constexpr uint16_t IMP34DT05_LEVEL_WINDOW_MIN_MS = 1;
constexpr uint16_t IMP34DT05_LEVEL_WINDOW_MAX_MS = 4000;

// levelDbfs() never returns less than this (silence, or an all-zero
// window at volume 0).
constexpr float IMP34DT05_LEVEL_FLOOR_DBFS = -120.0f;

// Datasheet sensitivity: -26 dBFS for a 94 dB SPL, 1 kHz tone (typical,
// +/- 1 dB). Used by levelDbSpl(), an uncalibrated estimate.
constexpr float IMP34DT05_SENSITIVITY_DBFS = -26.0f;
constexpr float IMP34DT05_SENSITIVITY_REF_DB_SPL = 94.0f;

// --- Dominant frequency ---------------------------------------------------

// Samples analysed by dominantFrequencyHz() (32 ms, 31.25 Hz per bin
// before interpolation), also the largest size computeDominantFrequency()
// accepts (its work arrays, 4 KB, live on the stack).
constexpr uint16_t IMP34DT05_FFT_SIZE = 512;
constexpr uint16_t IMP34DT05_FFT_MAX_SIZE = 512;
