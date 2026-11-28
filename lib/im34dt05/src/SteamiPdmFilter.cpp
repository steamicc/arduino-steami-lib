// SPDX-License-Identifier: BSD-3-Clause
// CI filter measurement (throwaway).
//
// PDM to PCM filter of the STeaMi IMP34DT05 driver.
//
// Ported from STMicroelectronics' X-NUCLEO-IKS02A1-Audio library,
// version 1.1.1 (https://github.com/stm32duino/X-NUCLEO-IKS02A1-Audio):
// LuT_Filter_SINC3_4_DEC_8.cpp (sinc3 lookup tables), pdm2pcm.cpp (FIR
// decimator and its per-volume coefficients) and iir_hp.cpp (high-pass
// filter). The tables and coefficients are ST's, unchanged.
//
// Changes for the STeaMi (STeaMi contributors, same licence):
// - one C++ class holding the filter state instead of global variables,
//   FIR coefficients in flash (const) instead of RAM;
// - only the sinc3, "byte left MSB", sequential-byte path is kept: it is
//   the one ST's code actually runs (LuT_Filter_init() assigns its `sinc`
//   parameter to itself, so the global stays at SINC3) and the byte
//   layout of the STM32WB55 SAI PDM interface;
// - the FIR decimator is written in C++ (same arithmetic as CMSIS
//   arm_fir_decimate_q15, with exact 64-bit sums) so the code also
//   builds on the host; on a Cortex-M4 it uses the SMLALD instruction,
//   two multiply-accumulates per cycle, over taps padded to 38;
// - the stream is filtered one block at a time: sinc3 outputs into a
//   linear buffer, then the FIR outputs, instead of byte by byte;
// - LuT1 and LuT2 are packed into one table of 32-bit words and LuT3 is
//   indexed by the whole byte, so that a PDM byte costs two table reads
//   instead of three, and no mask;
// - the high-pass filter is computed in 32 bits (same results);
// - this file is compiled with -O2 (it runs in the DMA interrupt).
// The output is bit-identical to ST's arithmetic: the host tests compare
// it with a byte-by-byte implementation.
// - sums saturate to 16 bits instead of wrapping around;
// - the 4-sample output delay of pdm2pcm() is dropped: it only adds
//   latency;
// - input read with a stride, to pick one slot of the SAI frames.
//
// Original licence (LICENSE.md of the X-NUCLEO-IKS02A1-Audio library):
//
// COPYRIGHT(c) 2021 STMicroelectronics
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// Redistributions of source code must retain the above copyright notice,
// this list of conditions and the following disclaimer.
// Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
// Neither the name of STMicroelectronics nor the names of its contributors
// may be used to endorse or promote products derived from this software
// without specific prior written permission.
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
// IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
// TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
// PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
// TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "SteamiPdmFilter.h"

#include <string.h>

// The DSP extension of the Cortex-M4 (STM32WB55): SMLALD and SSAT.
// Elsewhere (host tests) portable code computes the same values.
#if defined(__ARM_FEATURE_SIMD32) && defined(__ARM_FEATURE_SAT)
#include <arm_acle.h>
#define STEAMI_PDM_FILTER_ACLE 1
#endif

// The filter runs in the DMA interrupt and the Arduino core builds for
// size (-Os): optimise this file for speed instead (about 1 KB of flash).
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize("O2")
#endif

namespace {

// sinc3 filter, decimation 8: 22 taps (1 3 6 10 ... 6 3 1, sum 512)
// scaled by 64, so that a full-scale PDM stream gives +/-32768. Each
// table holds, for every value of one PDM byte, the contribution of its
// bits (+1 for a 1, -1 for a 0): LUT1 for the newest byte (taps 0..7),
// LUT2 for the previous one (taps 8..15), LUT3 for the 6 newest bits of
// the oldest byte (taps 16..21). ST's LuT1[0], LuT2[0] and LuT3[0].
constexpr int16_t LUT1[256] = {
    -7680, -7552, -7296, -7168, -6912, -6784, -6528, -6400, -6400, -6272, -6016, -5888, -5632,
    -5504, -5248, -5120, -5760, -5632, -5376, -5248, -4992, -4864, -4608, -4480, -4480, -4352,
    -4096, -3968, -3712, -3584, -3328, -3200, -4992, -4864, -4608, -4480, -4224, -4096, -3840,
    -3712, -3712, -3584, -3328, -3200, -2944, -2816, -2560, -2432, -3072, -2944, -2688, -2560,
    -2304, -2176, -1920, -1792, -1792, -1664, -1408, -1280, -1024, -896,  -640,  -512,  -4096,
    -3968, -3712, -3584, -3328, -3200, -2944, -2816, -2816, -2688, -2432, -2304, -2048, -1920,
    -1664, -1536, -2176, -2048, -1792, -1664, -1408, -1280, -1024, -896,  -896,  -768,  -512,
    -384,  -128,  0,     256,   384,   -1408, -1280, -1024, -896,  -640,  -512,  -256,  -128,
    -128,  0,     256,   384,   640,   768,   1024,  1152,  512,   640,   896,   1024,  1280,
    1408,  1664,  1792,  1792,  1920,  2176,  2304,  2560,  2688,  2944,  3072,  -3072, -2944,
    -2688, -2560, -2304, -2176, -1920, -1792, -1792, -1664, -1408, -1280, -1024, -896,  -640,
    -512,  -1152, -1024, -768,  -640,  -384,  -256,  0,     128,   128,   256,   512,   640,
    896,   1024,  1280,  1408,  -384,  -256,  0,     128,   384,   512,   768,   896,   896,
    1024,  1280,  1408,  1664,  1792,  2048,  2176,  1536,  1664,  1920,  2048,  2304,  2432,
    2688,  2816,  2816,  2944,  3200,  3328,  3584,  3712,  3968,  4096,  512,   640,   896,
    1024,  1280,  1408,  1664,  1792,  1792,  1920,  2176,  2304,  2560,  2688,  2944,  3072,
    2432,  2560,  2816,  2944,  3200,  3328,  3584,  3712,  3712,  3840,  4096,  4224,  4480,
    4608,  4864,  4992,  3200,  3328,  3584,  3712,  3968,  4096,  4352,  4480,  4480,  4608,
    4864,  4992,  5248,  5376,  5632,  5760,  5120,  5248,  5504,  5632,  5888,  6016,  6272,
    6400,  6400,  6528,  6784,  6912,  7168,  7296,  7552,  7680,
};

constexpr int16_t LUT2[256] = {
    -21504, -16128, -15616, -10240, -15360, -9984,  -9472,  -4096, -15360, -9984,  -9472,  -4096,
    -9216,  -3840,  -3328,  2048,   -15616, -10240, -9728,  -4352, -9472,  -4096,  -3584,  1792,
    -9472,  -4096,  -3584,  1792,   -3328,  2048,   2560,   7936,  -16128, -10752, -10240, -4864,
    -9984,  -4608,  -4096,  1280,   -9984,  -4608,  -4096,  1280,  -3840,  1536,   2048,   7424,
    -10240, -4864,  -4352,  1024,   -4096,  1280,   1792,   7168,  -4096,  1280,   1792,   7168,
    2048,   7424,   7936,   13312,  -16896, -11520, -11008, -5632, -10752, -5376,  -4864,  512,
    -10752, -5376,  -4864,  512,    -4608,  768,    1280,   6656,  -11008, -5632,  -5120,  256,
    -4864,  512,    1024,   6400,   -4864,  512,    1024,   6400,  1280,   6656,   7168,   12544,
    -11520, -6144,  -5632,  -256,   -5376,  0,      512,    5888,  -5376,  0,      512,    5888,
    768,    6144,   6656,   12032,  -5632,  -256,   256,    5632,  512,    5888,   6400,   11776,
    512,    5888,   6400,   11776,  6656,   12032,  12544,  17920, -17920, -12544, -12032, -6656,
    -11776, -6400,  -5888,  -512,   -11776, -6400,  -5888,  -512,  -5632,  -256,   256,    5632,
    -12032, -6656,  -6144,  -768,   -5888,  -512,   0,      5376,  -5888,  -512,   0,      5376,
    256,    5632,   6144,   11520,  -12544, -7168,  -6656,  -1280, -6400,  -1024,  -512,   4864,
    -6400,  -1024,  -512,   4864,   -256,   5120,   5632,   11008, -6656,  -1280,  -768,   4608,
    -512,   4864,   5376,   10752,  -512,   4864,   5376,   10752, 5632,   11008,  11520,  16896,
    -13312, -7936,  -7424,  -2048,  -7168,  -1792,  -1280,  4096,  -7168,  -1792,  -1280,  4096,
    -1024,  4352,   4864,   10240,  -7424,  -2048,  -1536,  3840,  -1280,  4096,   4608,   9984,
    -1280,  4096,   4608,   9984,   4864,   10240,  10752,  16128, -7936,  -2560,  -2048,  3328,
    -1792,  3584,   4096,   9472,   -1792,  3584,   4096,   9472,  4352,   9728,   10240,  15616,
    -2048,  3328,   3840,   9216,   4096,   9472,   9984,   15360, 4096,   9472,   9984,   15360,
    10240,  15616,  16128,  21504,
};

constexpr int16_t LUT3[64] = {
    -3584, -896,  -1664, 1024,  -2304, 384,   -384,  2304,  -2816, -128,  -896,  1792,  -1536,
    1152,  384,   3072,  -3200, -512,  -1280, 1408,  -1920, 768,   0,     2688,  -2432, 256,
    -512,  2176,  -1152, 1536,  768,   3456,  -3456, -768,  -1536, 1152,  -2176, 512,   -256,
    2432,  -2688, 0,     -768,  1920,  -1408, 1280,  512,   3200,  -3072, -384,  -1152, 1536,
    -1792, 896,   128,   2816,  -2304, 384,   -384,  2304,  -1024, 1664,  896,   3584,
};

// FIR decimator, decimation 10: sinc4 (37 taps), one row per volume.
// Row v has a DC gain of 2 * v (Q15). ST's fir_taps from pdm2pcm.cpp,
// with a zero in front (SteamiPdmFilter::FIR_WINDOW). The taps are
// symmetric, so the row also reads oldest sample first.
alignas(4) const int16_t FIR_TAPS[IMP34DT05_VOLUME_MAX + 1][SteamiPdmFilter::FIR_WINDOW] = {
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0,    7,    26,   66,   131,  229,  367,  551,  786,  1081, 1442, 1848, 2281,
     2720, 3146, 3539, 3880, 4148, 4325, 4391, 4325, 4148, 3880, 3539, 3146, 2720,
     2281, 1848, 1442, 1081, 786,  551,  367,  229,  131,  66,   26,   7},
    {0,    13,   52,   131,  262,  459,  734,  1101, 1573, 2163, 2884, 3696, 4561,
     5439, 6291, 7078, 7759, 8297, 8651, 8782, 8651, 8297, 7759, 7078, 6291, 5439,
     4561, 3696, 2884, 2163, 1573, 1101, 734,  459,  262,  131,  52,   13},
    {0,    20,   79,    197,   393,   688,   1101,  1652,  2359,  3244,  4325,  5544, 6842,
     8159, 9437, 10617, 11639, 12445, 12976, 13173, 12976, 12445, 11639, 10617, 9437, 8159,
     6842, 5544, 4325,  3244,  2359,  1652,  1101,  688,   393,   197,   79,    20},
    {0,     26,    105,   262,   524,   918,   1468,  2202,  3146,  4325,  5767,  7392,  9123,
     10879, 12583, 14156, 15519, 16594, 17302, 17564, 17302, 16594, 15519, 14156, 12583, 10879,
     9123,  7392,  5767,  4325,  3146,  2202,  1468,  918,   524,   262,   105,   26},
    {0,     33,    131,   328,   655,   1147,  1835,  2753,  3932,  5407,  7209,  9241,  11403,
     13599, 15729, 17695, 19399, 20742, 21627, 21955, 21627, 20742, 19399, 17695, 15729, 13599,
     11403, 9241,  7209,  5407,  3932,  2753,  1835,  1147,  655,   328,   131,   33},
    {0,     39,    157,   393,   786,   1376,  2202,  3303,  4719,  6488,  8651,  11089, 13684,
     16318, 18874, 21234, 23278, 24891, 25952, 26345, 25952, 24891, 23278, 21234, 18874, 16318,
     13684, 11089, 8651,  6488,  4719,  3303,  2202,  1376,  786,   393,   157,   39},
};

// The tables the sinc3 loop reads, derived at compile time from ST's.
// PACKED: LUT1 in the low half-word, LUT2 in the high one, so that one
// read serves a byte as the newest and then as the previous one.
struct PackedTable {
    int32_t value[256];
};

constexpr PackedTable makePackedTable() noexcept {
    PackedTable table = {};
    for (int i = 0; i < 256; i++) {
        uint32_t low = static_cast<uint16_t>(LUT1[i]);
        uint32_t high = static_cast<uint16_t>(LUT2[i]);
        table.value[i] = static_cast<int32_t>((high << 16) | low);
    }
    return table;
}

// OLDEST: LUT3 indexed by the whole byte (its 6 low bits select the
// entry), which saves masking the byte.
struct OldestTable {
    int16_t value[256];
};

constexpr OldestTable makeOldestTable() noexcept {
    OldestTable table = {};
    for (int i = 0; i < 256; i++) {
        table.value[i] = LUT3[i & 0x3F];
    }
    return table;
}

// On the STM32WB55 these tables (1.5 KB) live in RAM: at 32 MHz a flash
// read takes a wait state, and the 256-byte data cache of the flash
// cannot hold them for random byte values. Elsewhere, in flash.
#if defined(ARDUINO_ARCH_STM32)
#define STEAMI_PDM_FILTER_RAM(name) __attribute__((section(".data.steami_pdm_" name)))
#else
#define STEAMI_PDM_FILTER_RAM(name)
#endif

STEAMI_PDM_FILTER_RAM("packed") const PackedTable PACKED = makePackedTable();
STEAMI_PDM_FILTER_RAM("oldest") const OldestTable OLDEST = makeOldestTable();

constexpr int32_t HP_FEEDBACK = 32113;  // round(0.98 * 2^15): pole of the high-pass filter

constexpr size_t CHUNK = IMP34DT05_PDM_BYTES_PER_BLOCK;  // sinc3 outputs per pass

int16_t saturate16(int32_t value) {
#ifdef STEAMI_PDM_FILTER_ACLE
    return static_cast<int16_t>(__ssat(value, 16));
#else
    if (value > INT16_MAX) {
        return INT16_MAX;
    }
    if (value < INT16_MIN) {
        return INT16_MIN;
    }
    return static_cast<int16_t>(value);
#endif
}

// A FIR sum back to 16 bits. |sum| <= 32768 * (sum of the taps, at most
// 393213) < 2^34, so sum >> 15 fits in 32 bits before the saturation.
int16_t firResult(int64_t sum) {
    return saturate16(static_cast<int32_t>(sum >> 15));
}

// Low (LUT1) and high (LUT2) halves of a packed table entry.
int32_t lowHalf(int32_t packed) {
    return static_cast<int16_t>(packed & 0xFFFF);
}

int32_t highHalf(int32_t packed) {
    return packed >> 16;  // arithmetic shift: keeps the sign
}

// Two 16-bit values from an even index of an aligned array, the first
// one in the low half (little-endian, as on the Cortex-M4).
uint32_t loadPair(const int16_t* values) {
    uint32_t pair;
    memcpy(&pair, values, sizeof(pair));
    return pair;
}

// acc + low(x) * low(y) + high(x) * high(y): the SMLALD instruction.
int64_t dualMultiplyAccumulate(uint32_t x, uint32_t y, int64_t acc) {
#ifdef STEAMI_PDM_FILTER_ACLE
    return __smlald(static_cast<int32_t>(x), static_cast<int32_t>(y), acc);
#else
    int32_t xLow = static_cast<int16_t>(x & 0xFFFF);
    int32_t xHigh = static_cast<int16_t>(x >> 16);
    int32_t yLow = static_cast<int16_t>(y & 0xFFFF);
    int32_t yHigh = static_cast<int16_t>(y >> 16);
    return acc + static_cast<int64_t>(xLow * yLow) + static_cast<int64_t>(xHigh * yHigh);
#endif
}

// Stage 1: one sinc3 output per PDM byte, `count` bytes read every
// `stride` bytes. `window` holds the last two bytes (newest in bits
// 16..23, previous in bits 8..15) and is updated. Kept out of process()
// so that its loop has all the registers it needs.
#if defined(__GNUC__)
__attribute__((noinline))
#endif
void sinc3Block(const uint8_t* pdm, size_t count, size_t stride, int16_t* out, uint32_t& window) {
    uint32_t newest = (window >> 16) & 0xFF;
    uint32_t previous = (window >> 8) & 0xFF;
    // Packed table entry of the newest byte; sinc3 term of the low 6
    // bits of the previous byte, the oldest one at the next byte.
    int32_t packed = PACKED.value[newest];
    int32_t oldestTerm = OLDEST.value[previous];

    // Two bytes per turn: the second one's oldest byte is `newest`.
    const size_t step = 2 * stride;
    for (size_t pairs = count / 2; pairs > 0; pairs--) {
        uint32_t first = pdm[0];
        uint32_t second = pdm[stride];
        pdm += step;
        int32_t olderTerms = highHalf(packed) + oldestTerm;
        packed = PACKED.value[first];
        out[0] = saturate16(lowHalf(packed) + olderTerms);
        olderTerms = highHalf(packed) + OLDEST.value[newest];
        packed = PACKED.value[second];
        out[1] = saturate16(lowHalf(packed) + olderTerms);
        out += 2;
        oldestTerm = OLDEST.value[first];
        previous = first;
        newest = second;
    }
    if ((count & 1) != 0) {
        uint32_t next = *pdm;
        int32_t olderTerms = highHalf(packed) + oldestTerm;
        packed = PACKED.value[next];
        *out = saturate16(lowHalf(packed) + olderTerms);
        previous = newest;
        newest = next;
    }
    window = (newest << 16) | (previous << 8);
}

// Stage 2: `outputs` FIR outputs from the sinc3 outputs in `cic` (4-byte
// aligned, oldest first), the window of output n starting at sample
// 10 * n. Two outputs per turn share the loads of the taps.
#if defined(__GNUC__)
__attribute__((noinline))
#endif
void firBlock(const int16_t* cic, size_t outputs, const int16_t* taps, int16_t* out) {
    constexpr size_t NEXT = IMP34DT05_FIR_DECIMATION;  // offset of the next window
    for (; outputs >= 2; outputs -= 2) {
        int64_t first = 0;
        int64_t second = 0;
#pragma GCC unroll 19
        for (uint8_t k = 0; k < SteamiPdmFilter::FIR_WINDOW; k += 2) {
            uint32_t tap = loadPair(taps + k);
            first = dualMultiplyAccumulate(loadPair(cic + k), tap, first);
            second = dualMultiplyAccumulate(loadPair(cic + NEXT + k), tap, second);
        }
        out[0] = firResult(first);
        out[1] = firResult(second);
        out += 2;
        cic += 2 * NEXT;
    }
    if (outputs != 0) {
        int64_t sum = 0;
#pragma GCC unroll 19
        for (uint8_t k = 0; k < SteamiPdmFilter::FIR_WINDOW; k += 2) {
            sum = dualMultiplyAccumulate(loadPair(cic + k), loadPair(taps + k), sum);
        }
        *out = firResult(sum);
    }
}

}  // namespace

SteamiPdmFilter::SteamiPdmFilter() = default;

void SteamiPdmFilter::reset() {
    _cicWindow = 0;
    _firPhase = 0;
    for (int16_t& value : _firHistory) {
        value = 0;
    }
    _hpPrevIn = 0;
    _hpPrevOut = 0;
}

bool SteamiPdmFilter::setVolume(uint8_t volume) {
    if (volume > IMP34DT05_VOLUME_MAX) {
        return false;
    }
    _volume = volume;
    return true;
}

uint8_t SteamiPdmFilter::volume() const {
    return _volume;
}

int16_t SteamiPdmFilter::cicOutput(uint32_t window) {
    int32_t sum = lowHalf(PACKED.value[(window >> 16) & 0xFF]) +
                  highHalf(PACKED.value[(window >> 8) & 0xFF]) + OLDEST.value[window & 0xFF];
    return saturate16(sum);
}

int16_t SteamiPdmFilter::highPass(int16_t sample) {
    // y[n] = x[n] - x[n-1] + 0.98 y[n-1], in Q15 like ST's iir_hp():
    // ((x - x1) * 2^15 + a * y1) >> 15 == (x - x1) + ((a * y1) >> 15),
    // which fits in 32 bits.
    int32_t out = (static_cast<int32_t>(sample) - _hpPrevIn) + ((HP_FEEDBACK * _hpPrevOut) >> 15);
    int16_t result = saturate16(out);
    _hpPrevIn = sample;
    _hpPrevOut = result;
    return result;
}

size_t SteamiPdmFilter::process(const uint8_t* pdm, size_t count, size_t stride, int16_t* pcm) {
    // sinc3 outputs: the kept history, then up to CHUNK new ones. The FIR
    // windows start at 0, 10, 20... (even: whole 32-bit pairs).
    alignas(4) int16_t cic[FIR_HISTORY_MAX + CHUNK];
    size_t kept = FIR_KEEP + _firPhase;
    memcpy(cic, _firHistory, kept * sizeof(int16_t));

    const int16_t* taps = FIR_TAPS[_volume];
    size_t written = 0;
    while (count > 0) {
        size_t n = count < CHUNK ? count : CHUNK;
        sinc3Block(pdm, n, stride, cic + kept, _cicWindow);
        pdm += n * stride;
        count -= n;

        // Stages 2 and 3: one PCM sample per complete FIR window.
        size_t total = kept + n;
        size_t outputs = 0;
        if (total >= FIR_WINDOW) {
            outputs = (total - FIR_WINDOW) / IMP34DT05_FIR_DECIMATION + 1;
        }
        firBlock(cic, outputs, taps, pcm + written);
        for (size_t i = 0; i < outputs; i++) {
            pcm[written + i] = highPass(pcm[written + i]);
        }
        written += outputs;

        size_t start = outputs * IMP34DT05_FIR_DECIMATION;
        kept = total - start;
        memmove(cic, cic + start, kept * sizeof(int16_t));
    }

    memcpy(_firHistory, cic, kept * sizeof(int16_t));
    _firPhase = static_cast<uint8_t>(kept - FIR_KEEP);
    return written;
}
