// SPDX-License-Identifier: GPL-3.0-or-later

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unity.h>

#include <vector>

#include "SteamiIMP34DT05.h"
#include "SteamiPdmFilter.h"
#include "SteamiPdmSai.h"
#include "golden_vectors.h"

// The tests feed the filter and the driver with PDM streams produced by a
// second-order sigma-delta modulator, the kind of modulator a MEMS
// microphone contains, then check what comes out at 16 kHz.

namespace {

constexpr float PI_D = 3.14159265358979f;
constexpr uint32_t PDM_RATE = IMP34DT05_PDM_CLOCK_HZ;

// sinc3, decimation 8: (1 + z + ... + z^7)^3.
const int SINC3[22] = {1,  3,  6,  10, 15, 21, 28, 36, 42, 46, 48,
                       48, 46, 42, 36, 28, 21, 15, 10, 6,  3,  1};

// PDM bytes (oldest bit in the MSB) encoding amplitude * sin(2 pi f t),
// plus a constant `offset`, both relative to full scale.
std::vector<uint8_t> pdmSine(float frequencyHz, float amplitude, size_t bytes,
                             float offset = 0.0f) {
    std::vector<uint8_t> out(bytes, 0);
    double integrator1 = 0.0;
    double integrator2 = 0.0;
    double feedback = 0.0;
    for (size_t n = 0; n < bytes * 8; n++) {
        double x = offset + amplitude * sin(2.0 * PI_D * frequencyHz * static_cast<double>(n) /
                                            static_cast<double>(PDM_RATE));
        integrator1 += x - feedback;
        integrator2 += integrator1 - feedback;
        bool bit = integrator2 >= 0.0;
        feedback = bit ? 1.0 : -1.0;
        if (bit) {
            out[n / 8] |= static_cast<uint8_t>(0x80 >> (n % 8));
        }
    }
    return out;
}

// Interleave a microphone stream into the two SAI slots, the other slot
// holding `idle` (0x00: an undriven data line).
std::vector<uint8_t> saiFrames(const std::vector<uint8_t>& mic, uint8_t micSlot,
                               uint8_t idle = 0x00) {
    std::vector<uint8_t> out(mic.size() * 2, idle);
    for (size_t i = 0; i < mic.size(); i++) {
        out[i * 2 + micSlot] = mic[i];
    }
    return out;
}

std::vector<int16_t> filterAll(SteamiPdmFilter& filter, const std::vector<uint8_t>& pdm) {
    std::vector<int16_t> pcm(pdm.size() / IMP34DT05_PDM_BYTES_PER_SAMPLE + 1);
    size_t n = filter.process(pdm.data(), pdm.size(), 1, pcm.data());
    pcm.resize(n);
    return pcm;
}

float rmsFrom(const std::vector<int16_t>& pcm, size_t first) {
    return SteamiIMP34DT05::computeRms(pcm.data() + first, pcm.size() - first);
}

std::vector<int16_t> sine(float frequencyHz, float amplitude, size_t count) {
    std::vector<int16_t> out(count);
    for (size_t i = 0; i < count; i++) {
        out[i] = static_cast<int16_t>(
            lroundf(amplitude * sinf(2.0f * PI_D * frequencyHz * static_cast<float>(i) /
                                     static_cast<float>(IMP34DT05_SAMPLE_RATE_16K))));
    }
    return out;
}

// Feed the driver `blocks` DMA blocks of `frames` (wrapping around).
void feedBlocks(SteamiIMP34DT05& mic, const std::vector<uint8_t>& frames, size_t blocks,
                size_t& position) {
    for (size_t b = 0; b < blocks; b++) {
        if (position + IMP34DT05_SAI_BYTES_PER_BLOCK > frames.size()) {
            position = 0;
        }
        mic.processBlock(frames.data() + position, IMP34DT05_SAI_BYTES_PER_BLOCK);
        position += IMP34DT05_SAI_BYTES_PER_BLOCK;
    }
}

constexpr size_t STARTUP_BLOCKS =
    IMP34DT05_WARMUP_BLOCKS + IMP34DT05_DETECT_BLOCKS + IMP34DT05_SETTLE_BLOCKS;

int callbackCalls = 0;
size_t callbackSamples = 0;

void countingCallback(const int16_t* /* samples */, size_t count) {
    callbackCalls++;
    callbackSamples += count;
}

}  // namespace

void setUp(void) {
    callbackCalls = 0;
    callbackSamples = 0;
}

void tearDown(void) {
    clockHook() = nullptr;
    SteamiPdmSai::hostSetPresent(false);
}

// --- Filter: ST's tables ---------------------------------------------------

void test_cic_tables_match_sinc3_coefficients(void) {
    // For any 3-byte window, ST's lookup tables must give 64 times the
    // sinc3 convolution of the 22 newest bits (bit value 1 -> +1, 0 -> -1).
    uint32_t seed = 12345;
    for (int trial = 0; trial < 2000; trial++) {
        seed = seed * 1103515245u + 12345u;
        uint32_t window = (seed >> 4) & 0xFFFFFF;
        int32_t expected = 0;
        for (int tap = 0; tap < 22; tap++) {
            // tap 0 = newest bit = LSB of the newest byte (bits 16..23).
            int byteIndex = 2 - tap / 8;
            int bitIndex = tap % 8;
            int bit = (window >> (byteIndex * 8 + bitIndex)) & 1;
            expected += SINC3[tap] * (bit ? 1 : -1);
        }
        expected *= 64;
        if (expected > 32767) {
            expected = 32767;
        }
        TEST_ASSERT_EQUAL_INT(expected, SteamiPdmFilter::cicOutput(window));
    }
}

void test_cic_saturates_instead_of_wrapping(void) {
    TEST_ASSERT_EQUAL_INT(32767, SteamiPdmFilter::cicOutput(0xFFFFFF));
    TEST_ASSERT_EQUAL_INT(-32768, SteamiPdmFilter::cicOutput(0x000000));
}

// --- Filter: behaviour -----------------------------------------------------

void test_filter_outputs_one_sample_per_ten_bytes(void) {
    SteamiPdmFilter filter;
    std::vector<uint8_t> pdm(1600, 0x55);
    std::vector<int16_t> pcm(200);
    TEST_ASSERT_EQUAL_UINT(160, filter.process(pdm.data(), 1600, 1, pcm.data()));
    // Leftover bytes are carried to the next call.
    TEST_ASSERT_EQUAL_UINT(0, filter.process(pdm.data(), 7, 1, pcm.data()));
    TEST_ASSERT_EQUAL_UINT(1, filter.process(pdm.data(), 3, 1, pcm.data()));
}

void test_filter_output_does_not_depend_on_block_size(void) {
    std::vector<uint8_t> pdm = pdmSine(700.0f, 0.3f, 3200);
    SteamiPdmFilter whole;
    std::vector<int16_t> expected = filterAll(whole, pdm);

    SteamiPdmFilter pieces;
    std::vector<int16_t> got(expected.size() + 4);
    size_t written = 0;
    size_t sizes[] = {1, 7, 13, 160, 333};
    size_t offset = 0;
    for (size_t i = 0; offset < pdm.size(); i++) {
        size_t chunk = sizes[i % 5];
        if (offset + chunk > pdm.size()) {
            chunk = pdm.size() - offset;
        }
        written += pieces.process(pdm.data() + offset, chunk, 1, got.data() + written);
        offset += chunk;
    }
    TEST_ASSERT_EQUAL_UINT(expected.size(), written);
    TEST_ASSERT_EQUAL_INT16_ARRAY(expected.data(), got.data(), expected.size());
}

void test_filter_reads_with_stride(void) {
    std::vector<uint8_t> mic = pdmSine(1000.0f, 0.25f, 1600);
    SteamiPdmFilter direct;
    std::vector<int16_t> expected = filterAll(direct, mic);

    std::vector<uint8_t> frames = saiFrames(mic, 1, 0xFF);
    SteamiPdmFilter strided;
    std::vector<int16_t> got(expected.size() + 1);
    size_t n = strided.process(frames.data() + 1, mic.size(), 2, got.data());
    TEST_ASSERT_EQUAL_UINT(expected.size(), n);
    TEST_ASSERT_EQUAL_INT16_ARRAY(expected.data(), got.data(), n);
}

void test_filter_removes_dc_offset(void) {
    // A constant density (silence with an offset) must decay to ~0
    // through the high-pass filter.
    SteamiPdmFilter filter;
    std::vector<int16_t> pcm = filterAll(filter, pdmSine(0.0f, 0.0f, 16000, 0.2f));
    TEST_ASSERT_LESS_THAN_FLOAT(30.0f, rmsFrom(pcm, pcm.size() - 400));
}

void test_filter_gain_follows_volume(void) {
    // A 1 kHz sine at 10 % of full scale: the RMS after the filter is
    // 0.1 / sqrt(2) * 32768 * 2 * volume (DC gain of the FIR row).
    std::vector<uint8_t> pdm = pdmSine(1000.0f, 0.1f, 16000);
    for (uint8_t volume = 1; volume <= 3; volume++) {
        SteamiPdmFilter filter;
        TEST_ASSERT_TRUE(filter.setVolume(volume));
        std::vector<int16_t> pcm = filterAll(filter, pdm);
        float expected = 0.1f / 1.41421356f * 32768.0f * 2.0f * volume;
        TEST_ASSERT_FLOAT_WITHIN(expected * 0.05f, expected, rmsFrom(pcm, 400));
    }
}

void test_filter_volume_zero_mutes(void) {
    SteamiPdmFilter filter;
    TEST_ASSERT_TRUE(filter.setVolume(0));
    std::vector<int16_t> pcm = filterAll(filter, pdmSine(1000.0f, 0.5f, 3200));
    for (int16_t sample : pcm) {
        TEST_ASSERT_EQUAL_INT16(0, sample);
    }
}

void test_filter_rejects_volume_out_of_range(void) {
    SteamiPdmFilter filter;
    TEST_ASSERT_EQUAL_UINT8(IMP34DT05_VOLUME_DEFAULT, filter.volume());
    TEST_ASSERT_FALSE(filter.setVolume(IMP34DT05_VOLUME_MAX + 1));
    TEST_ASSERT_EQUAL_UINT8(IMP34DT05_VOLUME_DEFAULT, filter.volume());
}

void test_filter_saturates_loud_input(void) {
    // Near full scale at volume 6 (gain 12) the output clips, it must not
    // wrap around to the opposite sign.
    SteamiPdmFilter filter;
    filter.setVolume(6);
    std::vector<int16_t> pcm = filterAll(filter, pdmSine(200.0f, 0.6f, 16000));
    int clipped = 0;
    for (size_t i = 400; i + 1 < pcm.size(); i++) {
        if (pcm[i] == INT16_MAX || pcm[i] == INT16_MIN) {
            clipped++;
        }
        // No jump from one rail to the other between two samples.
        TEST_ASSERT_TRUE(abs(pcm[i + 1] - pcm[i]) < 40000);
    }
    TEST_ASSERT_TRUE(clipped > 0);
}

// --- Filter: reference implementation --------------------------------------

namespace {

// ST's FIR taps (pdm2pcm.cpp, fir_taps), one row per volume, as ST's
// code uses them: 37 taps, no padding.
const int16_t ST_FIR_TAPS[7][37] = {
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {7,    26,   66,   131,  229,  367,  551,  786,  1081, 1442, 1848, 2281, 2720,
     3146, 3539, 3880, 4148, 4325, 4391, 4325, 4148, 3880, 3539, 3146, 2720, 2281,
     1848, 1442, 1081, 786,  551,  367,  229,  131,  66,   26,   7},
    {13,   52,   131,  262,  459,  734,  1101, 1573, 2163, 2884, 3696, 4561, 5439,
     6291, 7078, 7759, 8297, 8651, 8782, 8651, 8297, 7759, 7078, 6291, 5439, 4561,
     3696, 2884, 2163, 1573, 1101, 734,  459,  262,  131,  52,   13},
    {20,   79,    197,   393,   688,   1101,  1652,  2359,  3244,  4325,  5544, 6842, 8159,
     9437, 10617, 11639, 12445, 12976, 13173, 12976, 12445, 11639, 10617, 9437, 8159, 6842,
     5544, 4325,  3244,  2359,  1652,  1101,  688,   393,   197,   79,    20},
    {26,    105,   262,   524,   918,   1468,  2202,  3146,  4325,  5767,  7392,  9123,  10879,
     12583, 14156, 15519, 16594, 17302, 17564, 17302, 16594, 15519, 14156, 12583, 10879, 9123,
     7392,  5767,  4325,  3146,  2202,  1468,  918,   524,   262,   105,   26},
    {33,    131,   328,   655,   1147,  1835,  2753,  3932,  5407,  7209,  9241,  11403, 13599,
     15729, 17695, 19399, 20742, 21627, 21955, 21627, 20742, 19399, 17695, 15729, 13599, 11403,
     9241,  7209,  5407,  3932,  2753,  1835,  1147,  655,   328,   131,   33},
    {39,    157,   393,   786,   1376,  2202,  3303,  4719,  6488,  8651,  11089, 13684, 16318,
     18874, 21234, 23278, 24891, 25952, 26345, 25952, 24891, 23278, 21234, 18874, 16318, 13684,
     11089, 8651,  6488,  4719,  3303,  2202,  1376,  786,   393,   157,   39},
};

int16_t clamp16(int64_t value) {
    if (value > INT16_MAX) {
        return INT16_MAX;
    }
    if (value < INT16_MIN) {
        return INT16_MIN;
    }
    return static_cast<int16_t>(value);
}

// The filter as first ported, one PDM byte at a time: sliding sinc3
// window, circular FIR history (newest first against the taps), 64-bit
// sums. The optimised SteamiPdmFilter must give exactly the same samples.
class ReferenceFilter {
   public:
    explicit ReferenceFilter(uint8_t volume) : _volume(volume) {}

    size_t process(const uint8_t* pdm, size_t count, size_t stride, int16_t* pcm) {
        size_t written = 0;
        for (size_t i = 0; i < count; i++) {
            _window = (_window >> 8) | (static_cast<uint32_t>(pdm[i * stride]) << 16);
            _newest = (_newest + 1) % IMP34DT05_FIR_TAPS;
            _history[_newest] = SteamiPdmFilter::cicOutput(_window);
            if (++_phase == IMP34DT05_FIR_DECIMATION) {
                _phase = 0;
                pcm[written++] = highPass(firOutput());
            }
        }
        return written;
    }

   private:
    uint8_t _volume;
    uint32_t _window = 0;
    int16_t _history[IMP34DT05_FIR_TAPS] = {};
    size_t _newest = 0;
    size_t _phase = 0;
    int64_t _hpPrevIn = 0;
    int64_t _hpPrevOut = 0;

    int16_t firOutput() const {
        int64_t sum = 0;
        size_t index = _newest;
        for (size_t k = 0; k < IMP34DT05_FIR_TAPS; k++) {
            sum += static_cast<int64_t>(ST_FIR_TAPS[_volume][k]) * _history[index];
            index = (index + IMP34DT05_FIR_TAPS - 1) % IMP34DT05_FIR_TAPS;
        }
        return clamp16(sum >> 15);
    }

    int16_t highPass(int16_t sample) {
        int64_t acc = (sample - _hpPrevIn) * 32768 + 32113 * _hpPrevOut;
        int16_t out = clamp16(acc >> 15);
        _hpPrevIn = sample;
        _hpPrevOut = out;
        return out;
    }
};

uint32_t nextRandom(uint32_t& seed) {
    seed = seed * 1664525u + 1013904223u;
    return seed >> 8;
}

// Feed both filters the same stream in random chunks; compare.
void expectSameAsReference(const std::vector<uint8_t>& stream, size_t stride, uint8_t volume,
                           uint32_t seed) {
    SteamiPdmFilter filter;
    TEST_ASSERT_TRUE(filter.setVolume(volume));
    ReferenceFilter reference(volume);
    size_t bytes = stream.size() / stride;
    std::vector<int16_t> expected(bytes / IMP34DT05_PDM_BYTES_PER_SAMPLE + 1);
    TEST_ASSERT_EQUAL_UINT(bytes / IMP34DT05_PDM_BYTES_PER_SAMPLE,
                           reference.process(stream.data(), bytes, stride, expected.data()));

    std::vector<int16_t> got(expected.size() + 1);
    size_t written = 0;
    size_t offset = 0;
    while (offset < bytes) {
        size_t chunk = 1 + nextRandom(seed) % 400;
        if (chunk > bytes - offset) {
            chunk = bytes - offset;
        }
        written +=
            filter.process(stream.data() + offset * stride, chunk, stride, got.data() + written);
        offset += chunk;
    }
    TEST_ASSERT_EQUAL_UINT(bytes / IMP34DT05_PDM_BYTES_PER_SAMPLE, written);
    TEST_ASSERT_EQUAL_INT16_ARRAY(expected.data(), got.data(), written);
}

}  // namespace

void test_filter_matches_reference_on_random_bytes(void) {
    // Random bytes: a white, very loud stream that also drives the FIR
    // and the high-pass filter into saturation at high volumes.
    uint32_t seed = 2024;
    for (uint8_t volume = IMP34DT05_VOLUME_MIN; volume <= IMP34DT05_VOLUME_MAX; volume++) {
        for (size_t stride = 1; stride <= 2; stride++) {
            std::vector<uint8_t> stream(6400 * stride);
            for (uint8_t& value : stream) {
                value = static_cast<uint8_t>(nextRandom(seed));
            }
            expectSameAsReference(stream, stride, volume, seed);
        }
    }
}

void test_filter_matches_reference_on_sigma_delta_streams(void) {
    // Microphone-like streams: sines of random frequency and amplitude
    // with an offset, interleaved with an idle slot as on the SAI.
    uint32_t seed = 7;
    for (int trial = 0; trial < 12; trial++) {
        float frequency = 50.0f + static_cast<float>(nextRandom(seed) % 7000);
        float amplitude = static_cast<float>(nextRandom(seed) % 700) / 1000.0f;
        float offset = (static_cast<float>(nextRandom(seed) % 200) - 100.0f) / 1000.0f;
        std::vector<uint8_t> mic = pdmSine(frequency, amplitude, 8000, offset);
        uint8_t volume = static_cast<uint8_t>(trial % (IMP34DT05_VOLUME_MAX + 1));
        expectSameAsReference(mic, 1, volume, seed);
        uint8_t slot = static_cast<uint8_t>(trial & 1);
        std::vector<uint8_t> frames = saiFrames(mic, slot, 0xFF);
        std::vector<uint8_t> slotStream(frames.begin() + slot, frames.end());
        slotStream.push_back(0xFF);  // keep the length even
        expectSameAsReference(slotStream, 2, volume, seed);
    }
}

// --- Pure helpers ----------------------------------------------------------

void test_compute_rms(void) {
    int16_t square[] = {1000, -1000, 1000, -1000};
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1000.0f, SteamiIMP34DT05::computeRms(square, 4));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, SteamiIMP34DT05::computeRms(square, 0));
    std::vector<int16_t> s = sine(1000.0f, 10000.0f, 1600);
    TEST_ASSERT_FLOAT_WITHIN(20.0f, 7071.0f, SteamiIMP34DT05::computeRms(s.data(), s.size()));
}

void test_rms_to_dbfs(void) {
    // Full-scale sine: 0 dBFS; half amplitude: -6 dBFS.
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 0.0f, SteamiIMP34DT05::rmsToDbfs(32768.0f / 1.41421356f));
    TEST_ASSERT_FLOAT_WITHIN(0.05f, -6.02f, SteamiIMP34DT05::rmsToDbfs(16384.0f / 1.41421356f));
    TEST_ASSERT_EQUAL_FLOAT(IMP34DT05_LEVEL_FLOOR_DBFS, SteamiIMP34DT05::rmsToDbfs(0.0f));
    TEST_ASSERT_TRUE(isnan(SteamiIMP34DT05::rmsToDbfs(NAN)));
}

void test_dominant_frequency_of_sines(void) {
    const float frequencies[] = {440.0f, 523.25f, 1000.0f, 2000.0f, 2730.0f, 4100.0f};
    for (float f : frequencies) {
        std::vector<int16_t> s = sine(f, 8000.0f, IMP34DT05_FFT_SIZE);
        float got = SteamiIMP34DT05::computeDominantFrequency(
            s.data(), s.size(), static_cast<float>(IMP34DT05_SAMPLE_RATE_16K));
        TEST_ASSERT_FLOAT_WITHIN(2.0f, f, got);
    }
}

void test_dominant_frequency_picks_strongest_of_two(void) {
    std::vector<int16_t> a = sine(800.0f, 2000.0f, IMP34DT05_FFT_SIZE);
    std::vector<int16_t> b = sine(2400.0f, 6000.0f, IMP34DT05_FFT_SIZE);
    for (size_t i = 0; i < a.size(); i++) {
        a[i] = static_cast<int16_t>(a[i] + b[i]);
    }
    float got = SteamiIMP34DT05::computeDominantFrequency(a.data(), a.size(), 16000.0f);
    TEST_ASSERT_FLOAT_WITHIN(2.0f, 2400.0f, got);
}

void test_dominant_frequency_of_square_wave_is_fundamental(void) {
    // tone() drives the buzzer with a square wave: odd harmonics at 1/3,
    // 1/5... of the fundamental.
    std::vector<int16_t> s(IMP34DT05_FFT_SIZE);
    for (size_t i = 0; i < s.size(); i++) {
        float phase = fmodf(1500.0f * static_cast<float>(i) / 16000.0f, 1.0f);
        s[i] = phase < 0.5f ? 6000 : -6000;
    }
    float got = SteamiIMP34DT05::computeDominantFrequency(s.data(), s.size(), 16000.0f);
    TEST_ASSERT_FLOAT_WITHIN(3.0f, 1500.0f, got);
}

void test_dominant_frequency_rejects_bad_input(void) {
    std::vector<int16_t> zeros(IMP34DT05_FFT_SIZE, 0);
    TEST_ASSERT_TRUE(isnan(SteamiIMP34DT05::computeDominantFrequency(zeros.data(), 512, 16000)));
    std::vector<int16_t> s = sine(1000.0f, 8000.0f, 1024);
    TEST_ASSERT_TRUE(isnan(SteamiIMP34DT05::computeDominantFrequency(s.data(), 500, 16000)));
    TEST_ASSERT_TRUE(isnan(SteamiIMP34DT05::computeDominantFrequency(s.data(), 1024, 16000)));
    TEST_ASSERT_TRUE(isnan(SteamiIMP34DT05::computeDominantFrequency(s.data(), 8, 16000)));
}

// --- Driver: lifecycle on the host -----------------------------------------

void test_begin_fails_without_hardware(void) {
    SteamiIMP34DT05 mic;
    TEST_ASSERT_FALSE(mic.begin());
    TEST_ASSERT_FALSE(mic.isRunning());
    TEST_ASSERT_TRUE(isnan(mic.dominantFrequencyHz()));
}

void test_begin_rejects_unsupported_sample_rate(void) {
    SteamiIMP34DT05 mic;
    TEST_ASSERT_FALSE(mic.begin(8000));
    TEST_ASSERT_EQUAL_UINT32(16000, mic.sampleRate());
}

void test_level_is_nan_before_first_window(void) {
    SteamiIMP34DT05 mic;
    TEST_ASSERT_FALSE(mic.levelReady());
    TEST_ASSERT_TRUE(isnan(mic.rms()));
    TEST_ASSERT_TRUE(isnan(mic.levelDbfs()));
    TEST_ASSERT_TRUE(isnan(mic.levelDbSpl()));
    TEST_ASSERT_EQUAL_UINT16(0, mic.peak());
}

// --- Driver: interrupt-side processing -------------------------------------

void test_warmup_and_detection_deliver_nothing(void) {
    SteamiIMP34DT05 mic;
    std::vector<uint8_t> frames = saiFrames(pdmSine(1000.0f, 0.25f, 16000), 0);
    size_t position = 0;
    feedBlocks(mic, frames, STARTUP_BLOCKS, position);
    TEST_ASSERT_EQUAL_UINT(0, mic.available());
    feedBlocks(mic, frames, 1, position);
    TEST_ASSERT_EQUAL_UINT(IMP34DT05_SAMPLES_PER_BLOCK, mic.available());
}

void test_conversion_stats_count_delivered_blocks_only(void) {
    // Warm-up, slot detection (which filters both slots) and settling
    // are left out; the host has no cycle counter, so cycles stay at 0.
    SteamiIMP34DT05 mic;
    std::vector<uint8_t> frames = saiFrames(pdmSine(1000.0f, 0.25f, 16000), 0);
    size_t position = 0;
    feedBlocks(mic, frames, STARTUP_BLOCKS, position);
    TEST_ASSERT_EQUAL_UINT32(0, mic.conversionStats().blocks);
    feedBlocks(mic, frames, 5, position);
    SteamiIMP34DT05::ConversionStats stats = mic.conversionStats();
    TEST_ASSERT_EQUAL_UINT32(5, stats.blocks);
    TEST_ASSERT_EQUAL_UINT32(0, stats.averageCycles);
    TEST_ASSERT_EQUAL_UINT32(0, stats.maxCycles);
    mic.resetConversionStats();
    TEST_ASSERT_EQUAL_UINT32(0, mic.conversionStats().blocks);
}

void test_detection_finds_microphone_slot(void) {
    for (uint8_t micSlot = 0; micSlot < 2; micSlot++) {
        for (uint8_t idle : {static_cast<uint8_t>(0x00), static_cast<uint8_t>(0xFF)}) {
            SteamiIMP34DT05 mic;
            std::vector<uint8_t> frames = saiFrames(pdmSine(1000.0f, 0.25f, 16000), micSlot, idle);
            size_t position = 0;
            feedBlocks(mic, frames, STARTUP_BLOCKS + 2, position);
            TEST_ASSERT_EQUAL_UINT8(micSlot, mic.slot());
        }
    }
}

void test_detection_rejects_random_slot(void) {
    // The undriven edge may read a random bit stream (the line picks up
    // the clock): it toggles as much as the microphone, but is far louder
    // once filtered than a quiet room (a -60 dBFS tone here).
    std::vector<uint8_t> quiet = pdmSine(500.0f, 0.001f, 16000);
    for (uint8_t micSlot = 0; micSlot < 2; micSlot++) {
        std::vector<uint8_t> frames = saiFrames(quiet, micSlot);
        // Random bytes, none flat: the two slots toggle equally often.
        srand(1234);
        for (size_t i = 0; i < quiet.size(); i++) {
            frames[i * 2 + (1 - micSlot)] = static_cast<uint8_t>(1 + rand() % 254);
        }
        SteamiIMP34DT05 mic;
        size_t position = 0;
        feedBlocks(mic, frames, STARTUP_BLOCKS + 2, position);
        TEST_ASSERT_EQUAL_UINT8(micSlot, mic.slot());
        TEST_ASSERT_EQUAL_UINT(2 * IMP34DT05_SAMPLES_PER_BLOCK, mic.available());
    }
}

void test_forced_slot(void) {
    SteamiIMP34DT05 mic;
    TEST_ASSERT_FALSE(mic.setSlot(2));
    TEST_ASSERT_TRUE(mic.setSlot(1));
    TEST_ASSERT_EQUAL_UINT8(1, mic.slot());
    TEST_ASSERT_TRUE(mic.setSlot(IMP34DT05_SLOT_AUTO));
}

void test_auto_slot_after_forced_slot_detects_again(void) {
    SteamiIMP34DT05 mic;
    TEST_ASSERT_TRUE(mic.setSlot(1));
    TEST_ASSERT_TRUE(mic.setSlot(IMP34DT05_SLOT_AUTO));
    std::vector<uint8_t> frames = saiFrames(pdmSine(1000.0f, 0.25f, 16000), 0);
    size_t position = 0;
    feedBlocks(mic, frames, STARTUP_BLOCKS + 2, position);
    TEST_ASSERT_EQUAL_UINT8(0, mic.slot());
}

void test_read_returns_oldest_samples_first(void) {
    SteamiIMP34DT05 mic;
    std::vector<uint8_t> frames = saiFrames(pdmSine(1000.0f, 0.25f, 16000), 1);
    size_t position = 0;
    feedBlocks(mic, frames, STARTUP_BLOCKS + 4, position);
    TEST_ASSERT_EQUAL_UINT(64, mic.available());
    TEST_ASSERT_TRUE(mic.dataReady());

    int16_t first[10];
    int16_t rest[100];
    TEST_ASSERT_EQUAL_UINT(10, mic.read(first, 10));
    TEST_ASSERT_EQUAL_UINT(54, mic.read(rest, 100));
    TEST_ASSERT_EQUAL_UINT(0, mic.available());
    TEST_ASSERT_FALSE(mic.dataReady());

    feedBlocks(mic, frames, 1, position);
    mic.clear();
    TEST_ASSERT_EQUAL_UINT(0, mic.available());
}

void test_buffer_overrun_is_counted(void) {
    SteamiIMP34DT05 mic;
    std::vector<uint8_t> frames = saiFrames(pdmSine(1000.0f, 0.25f, 16000), 0);
    size_t position = 0;
    size_t blocks = IMP34DT05_BUFFER_SAMPLES / IMP34DT05_SAMPLES_PER_BLOCK + 3;
    feedBlocks(mic, frames, STARTUP_BLOCKS + blocks, position);
    TEST_ASSERT_EQUAL_UINT(IMP34DT05_BUFFER_SAMPLES, mic.available());
    TEST_ASSERT_EQUAL_UINT32(3 * IMP34DT05_SAMPLES_PER_BLOCK, mic.overruns());
}

void test_callback_receives_each_block(void) {
    SteamiIMP34DT05 mic;
    mic.onReceive(countingCallback);
    std::vector<uint8_t> frames = saiFrames(pdmSine(1000.0f, 0.25f, 16000), 0);
    size_t position = 0;
    feedBlocks(mic, frames, STARTUP_BLOCKS + 5, position);
    TEST_ASSERT_EQUAL_INT(5, callbackCalls);
    TEST_ASSERT_EQUAL_UINT(5 * IMP34DT05_SAMPLES_PER_BLOCK, callbackSamples);

    mic.onReceive(nullptr);
    feedBlocks(mic, frames, 2, position);
    TEST_ASSERT_EQUAL_INT(5, callbackCalls);
}

void test_level_window_measures_rms_and_peak(void) {
    SteamiIMP34DT05 mic;
    mic.setVolume(1);
    mic.setLevelWindowMs(20);
    TEST_ASSERT_EQUAL_UINT16(20, mic.levelWindowMs());
    std::vector<uint8_t> frames = saiFrames(pdmSine(1000.0f, 0.25f, 16000), 0);
    size_t position = 0;
    feedBlocks(mic, frames, STARTUP_BLOCKS + 19, position);
    TEST_ASSERT_FALSE(mic.levelReady());
    feedBlocks(mic, frames, 1, position);
    TEST_ASSERT_TRUE(mic.levelReady());

    SteamiIMP34DT05::LevelResult r = mic.level();
    TEST_ASSERT_FALSE(mic.levelReady());
    float amplitude = 0.25f * 32768.0f * 2.0f;  // volume 1: gain 2
    TEST_ASSERT_FLOAT_WITHIN(amplitude * 0.05f, amplitude / 1.41421356f, r.rms);
    TEST_ASSERT_UINT16_WITHIN(static_cast<uint16_t>(amplitude * 0.08f),
                              static_cast<uint16_t>(amplitude), r.peak);
    // 0.25 of full scale at gain 2: 20 log10(0.5) = -6 dBFS.
    TEST_ASSERT_FLOAT_WITHIN(0.5f, -6.0f, r.dbfs);
    // dB SPL = dBFS - gain (6 dB) + 26 + 94.
    TEST_ASSERT_FLOAT_WITHIN(0.6f, -6.0f - 6.02f + 120.0f, r.dbSpl);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, r.dbfs, mic.levelDbfs());

    mic.setLevelOffset(-3.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, r.dbSpl - 3.0f, mic.levelDbSpl());
}

void test_level_window_is_clamped(void) {
    SteamiIMP34DT05 mic;
    mic.setLevelWindowMs(0);
    TEST_ASSERT_EQUAL_UINT16(IMP34DT05_LEVEL_WINDOW_MIN_MS, mic.levelWindowMs());
    mic.setLevelWindowMs(60000);
    TEST_ASSERT_EQUAL_UINT16(IMP34DT05_LEVEL_WINDOW_MAX_MS, mic.levelWindowMs());
}

void test_level_db_spl_is_nan_at_volume_zero(void) {
    SteamiIMP34DT05 mic;
    mic.setVolume(0);
    mic.setLevelWindowMs(10);
    std::vector<uint8_t> frames = saiFrames(pdmSine(1000.0f, 0.25f, 16000), 0);
    size_t position = 0;
    feedBlocks(mic, frames, STARTUP_BLOCKS + 10, position);
    SteamiIMP34DT05::LevelResult r = mic.level();
    TEST_ASSERT_EQUAL_FLOAT(0.0f, r.rms);
    TEST_ASSERT_EQUAL_FLOAT(IMP34DT05_LEVEL_FLOOR_DBFS, r.dbfs);
    TEST_ASSERT_TRUE(isnan(r.dbSpl));
}

void test_end_to_end_frequency_from_pdm(void) {
    // PDM sine -> driver -> 512 samples -> FFT: the frequency must survive
    // the whole chain (filter, decimation, buffer).
    const float frequencies[] = {440.0f, 1000.0f, 3000.0f};
    for (float f : frequencies) {
        SteamiIMP34DT05 mic;
        std::vector<uint8_t> frames = saiFrames(pdmSine(f, 0.2f, 160 * 200), 1);
        size_t position = 0;
        feedBlocks(mic, frames, STARTUP_BLOCKS + 40, position);
        mic.clear();
        feedBlocks(mic, frames, IMP34DT05_FFT_SIZE / IMP34DT05_SAMPLES_PER_BLOCK, position);
        int16_t samples[IMP34DT05_FFT_SIZE];
        TEST_ASSERT_EQUAL_UINT(IMP34DT05_FFT_SIZE, mic.read(samples, IMP34DT05_FFT_SIZE));
        float got =
            SteamiIMP34DT05::computeDominantFrequency(samples, IMP34DT05_FFT_SIZE, 16000.0f);
        TEST_ASSERT_FLOAT_WITHIN(3.0f, f, got);
    }
}

// --- Cross-language golden vectors (Annex A, shared with MicroPython) -------

namespace {

constexpr uint32_t FNV_OFFSET = 2166136261U;
constexpr uint32_t FNV_PRIME = 16777619U;

uint32_t fnvAddByte(uint32_t hash, uint8_t value) {
    return (hash ^ value) * FNV_PRIME;
}

uint32_t fnvBytes(const std::vector<uint8_t>& bytes) {
    uint32_t hash = FNV_OFFSET;
    for (uint8_t value : bytes) {
        hash = fnvAddByte(hash, value);
    }
    return hash;
}

uint32_t fnvAddLe16(uint32_t hash, uint16_t value) {
    hash = fnvAddByte(hash, static_cast<uint8_t>(value & 0xFF));
    return fnvAddByte(hash, static_cast<uint8_t>(value >> 8));
}

uint32_t fnvAddLe32(uint32_t hash, uint32_t value) {
    hash = fnvAddLe16(hash, static_cast<uint16_t>(value & 0xFFFF));
    return fnvAddLe16(hash, static_cast<uint16_t>(value >> 16));
}

uint32_t fnvSamples(const int16_t* samples, size_t count) {
    uint32_t hash = FNV_OFFSET;
    for (size_t i = 0; i < count; i++) {
        hash = fnvAddLe16(hash, static_cast<uint16_t>(samples[i]));
    }
    return hash;
}

uint32_t floatBits(float value) {
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

// SAI frames of a level input (microphone on slot 0, slot 1 at 0x00),
// built once: the 54 level lines share three inputs.
const std::vector<uint8_t>& levelFrames(golden::LevelInput input) {
    static std::vector<uint8_t> frames[3];
    size_t index = static_cast<size_t>(input);
    if (frames[index].empty()) {
        std::vector<uint8_t> mic;
        switch (input) {
            case golden::LevelInput::SINE_1K_A025:
                mic = pdmSine(1000.0f, 0.25f, golden::LEVEL_BYTES_PER_SLOT);
                break;
            case golden::LevelInput::SINE_1K_A010:
                mic = pdmSine(1000.0f, 0.1f, golden::LEVEL_BYTES_PER_SLOT);
                break;
            case golden::LevelInput::RANDOM_11: {
                mic.resize(golden::LEVEL_BYTES_PER_SLOT);
                uint32_t seed = 11;
                for (uint8_t& value : mic) {
                    value = static_cast<uint8_t>(nextRandom(seed));
                }
                break;
            }
        }
        TEST_ASSERT_EQUAL_HEX32_MESSAGE(golden::LEVEL_INPUT_FNV[index], fnvBytes(mic),
                                        "level input differs: generator (libm), not driver");
        frames[index] = saiFrames(mic, 0, 0x00);
    }
    return frames[index];
}

// Windows read by a level line: 20 for 1 ms, 10 for 10 ms, 3 for 100 ms.
unsigned levelWindowsWanted(uint16_t windowMs) {
    if (windowMs == 1) {
        return 20;
    }
    return windowMs == 10 ? 10 : 3;
}

struct LevelRun {
    size_t blocks = 0;
    unsigned windows = 0;
    uint32_t hash = FNV_OFFSET;
    SteamiIMP34DT05::LevelResult first = {};
    uint8_t slot = 0;
};

// One level line on a new driver, blocks fed to processBlock() one by one.
LevelRun runLevelVector(const golden::LevelVector& vector, float offsetDb) {
    const std::vector<uint8_t>& frames = levelFrames(vector.input);
    SteamiIMP34DT05 mic;
    TEST_ASSERT_TRUE(mic.setVolume(vector.volume));
    mic.setLevelWindowMs(vector.windowMs);
    mic.setLevelOffset(offsetDb);
    const unsigned wanted = levelWindowsWanted(vector.windowMs);
    LevelRun run;
    size_t position = 0;
    while (run.windows < wanted && position + IMP34DT05_SAI_BYTES_PER_BLOCK <= frames.size()) {
        mic.processBlock(frames.data() + position, IMP34DT05_SAI_BYTES_PER_BLOCK);
        position += IMP34DT05_SAI_BYTES_PER_BLOCK;
        run.blocks++;
        if (mic.levelReady()) {
            SteamiIMP34DT05::LevelResult result = mic.level();
            if (run.windows == 0) {
                run.first = result;
            }
            run.hash = fnvAddLe32(run.hash, floatBits(result.rms));
            run.hash = fnvAddLe16(run.hash, result.peak);
            run.windows++;
        }
    }
    run.slot = mic.slot();
    return run;
}

// Host SAI fed by the simulated clock: one block per millisecond, as the
// DMA interrupt delivers them on the board.
const std::vector<uint8_t>* hostFrames = nullptr;
size_t hostPosition = 0;
bool hostFeeding = false;

void startHostSai(const std::vector<uint8_t>& frames) {
    hostFrames = &frames;
    hostPosition = 0;
    hostFeeding = true;
    SteamiPdmSai::hostSetPresent(true);
    clockHook() = [] {
        if (!hostFeeding) {
            return;
        }
        if (hostPosition + IMP34DT05_SAI_BYTES_PER_BLOCK > hostFrames->size()) {
            hostPosition = 0;
        }
        SteamiPdmSai::hostFeed(hostFrames->data() + hostPosition, IMP34DT05_SAI_BYTES_PER_BLOCK);
        hostPosition += IMP34DT05_SAI_BYTES_PER_BLOCK;
    };
}

}  // namespace

void test_cross_filter_vectors(void) {
    uint32_t randomSeed = 2024;
    uint32_t sigmaDeltaSeed = 7;
    char message[48];
    for (size_t line = 0; line < golden::FILTER_VECTOR_COUNT; line++) {
        const golden::FilterVector& vector = golden::FILTER_VECTORS[line];
        snprintf(message, sizeof(message), "filter line %zu", line);
        std::vector<uint8_t> stream;
        switch (vector.input) {
            case golden::FilterInput::RANDOM:
                stream.resize(6400 * static_cast<size_t>(vector.stride));
                for (uint8_t& value : stream) {
                    value = static_cast<uint8_t>(nextRandom(randomSeed));
                }
                break;
            case golden::FilterInput::SIGMA_DELTA: {
                float frequency = 50.0f + static_cast<float>(nextRandom(sigmaDeltaSeed) % 7000);
                float amplitude = static_cast<float>(nextRandom(sigmaDeltaSeed) % 700) / 1000.0f;
                float offset =
                    (static_cast<float>(nextRandom(sigmaDeltaSeed) % 200) - 100.0f) / 1000.0f;
                TEST_ASSERT_EQUAL_FLOAT_MESSAGE(vector.frequencyHz, frequency, message);
                TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.0005f, vector.amplitude, amplitude, message);
                TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.0005f, vector.offset, offset, message);
                stream = pdmSine(frequency, amplitude, 8000, offset);
                break;
            }
            case golden::FilterInput::SINE_1K:
                stream = pdmSine(vector.frequencyHz, vector.amplitude, 16000);
                break;
        }
        TEST_ASSERT_EQUAL_HEX32_MESSAGE(vector.inFnv, fnvBytes(stream), message);

        SteamiPdmFilter filter;
        TEST_ASSERT_TRUE(filter.setVolume(vector.volume));
        size_t bytes = stream.size() / vector.stride;
        std::vector<int16_t> pcm(bytes / IMP34DT05_PDM_BYTES_PER_SAMPLE + 2);
        size_t count = filter.process(stream.data(), bytes, vector.stride, pcm.data());
        TEST_ASSERT_EQUAL_UINT_MESSAGE(vector.count, count, message);
        TEST_ASSERT_EQUAL_HEX32_MESSAGE(vector.outFnv, fnvSamples(pcm.data(), count), message);
        if (vector.hasEdges) {
            TEST_ASSERT_EQUAL_INT16_ARRAY_MESSAGE(vector.first, pcm.data(), 6, message);
            TEST_ASSERT_EQUAL_INT16_MESSAGE(vector.last, pcm[count - 1], message);
        }
    }
}

void test_cross_fft_vectors(void) {
    for (const golden::FftVector& vector : golden::FFT_VECTORS) {
        std::vector<int16_t> samples = sine(vector.inputHz, 8000.0f, IMP34DT05_FFT_SIZE);
        float got =
            SteamiIMP34DT05::computeDominantFrequency(samples.data(), samples.size(), 16000.0f);
        TEST_ASSERT_FLOAT_WITHIN(0.01f, vector.expectedHz, got);
    }
}

void test_cross_level_vectors(void) {
    char message[48];
    for (size_t line = 0; line < golden::LEVEL_VECTOR_COUNT; line++) {
        const golden::LevelVector& vector = golden::LEVEL_VECTORS[line];
        snprintf(message, sizeof(message), "level line %zu", line);
        LevelRun run = runLevelVector(vector, 0.0f);
        TEST_ASSERT_EQUAL_UINT_MESSAGE(vector.blocks, run.blocks, message);
        TEST_ASSERT_EQUAL_UINT_MESSAGE(vector.windows, run.windows, message);
        TEST_ASSERT_EQUAL_HEX32_MESSAGE(vector.lvFnv, run.hash, message);
        TEST_ASSERT_EQUAL_HEX32_MESSAGE(vector.rms0Bits, floatBits(run.first.rms), message);
        TEST_ASSERT_EQUAL_UINT16_MESSAGE(vector.peak0, run.first.peak, message);
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(vector.slot, run.slot, message);
        TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, vector.dbfs0, run.first.dbfs, message);
        TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, vector.spl0, run.first.dbSpl, message);
    }
}

void test_dbfs_to_db_spl_matches_spl0(void) {
    char message[48];
    for (size_t line = 0; line < golden::LEVEL_VECTOR_COUNT; line++) {
        const golden::LevelVector& vector = golden::LEVEL_VECTORS[line];
        snprintf(message, sizeof(message), "level line %zu", line);
        TEST_ASSERT_FLOAT_WITHIN_MESSAGE(
            0.01f, vector.spl0, SteamiIMP34DT05::dbfsToDbSpl(vector.dbfs0, vector.volume), message);
        // The same from the exact rms of the first window.
        float rms;
        memcpy(&rms, &vector.rms0Bits, sizeof(rms));
        float dbfs = SteamiIMP34DT05::rmsToDbfs(rms);
        TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, vector.dbfs0, dbfs, message);
        TEST_ASSERT_FLOAT_WITHIN_MESSAGE(
            0.01f, vector.spl0, SteamiIMP34DT05::dbfsToDbSpl(dbfs, vector.volume), message);
    }
    // At volume 5 (x10, +20 dB): dBFS + 100.
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 94.0f, SteamiIMP34DT05::dbfsToDbSpl(-6.0f, 5));
    TEST_ASSERT_TRUE(isnan(SteamiIMP34DT05::dbfsToDbSpl(-6.0f, 0)));
    TEST_ASSERT_TRUE(isnan(SteamiIMP34DT05::dbfsToDbSpl(NAN, 5)));
}

void test_level_db_spl_uses_level_offset(void) {
    SteamiIMP34DT05 fresh;
    TEST_ASSERT_EQUAL_FLOAT(0.0f, fresh.levelOffset());

    // Every level line with a +3 dB driver offset.
    char message[48];
    for (size_t line = 0; line < golden::LEVEL_VECTOR_COUNT; line++) {
        const golden::LevelVector& vector = golden::LEVEL_VECTORS[line];
        snprintf(message, sizeof(message), "level line %zu", line);
        LevelRun run = runLevelVector(vector, 3.0f);
        TEST_ASSERT_FLOAT_WITHIN_MESSAGE(0.01f, vector.spl0 + 3.0f, run.first.dbSpl, message);
    }

    // levelDbSpl() itself, on the first line at volume 5 with a 10 ms window.
    const golden::LevelVector* vector = nullptr;
    for (const golden::LevelVector& candidate : golden::LEVEL_VECTORS) {
        if (candidate.volume == 5 && candidate.windowMs == 10) {
            vector = &candidate;
            break;
        }
    }
    TEST_ASSERT_NOT_NULL(vector);
    const std::vector<uint8_t>& frames = levelFrames(vector->input);
    SteamiIMP34DT05 mic;
    TEST_ASSERT_TRUE(mic.setVolume(vector->volume));
    mic.setLevelWindowMs(vector->windowMs);
    mic.setLevelOffset(3.0f);
    TEST_ASSERT_EQUAL_FLOAT(3.0f, mic.levelOffset());
    size_t position = 0;
    feedBlocks(mic, frames, vector->blocks - (vector->windows - 1) * vector->windowMs, position);
    TEST_ASSERT_TRUE(mic.levelReady());
    TEST_ASSERT_FLOAT_WITHIN(0.01f, vector->spl0 + 3.0f, mic.levelDbSpl());
    TEST_ASSERT_EQUAL_FLOAT(3.0f, mic.levelOffset());
}

void test_is_alive_follows_blocks(void) {
    SteamiIMP34DT05 mic;
    TEST_ASSERT_FALSE(mic.isAlive());  // not running

    startHostSai(levelFrames(golden::LevelInput::SINE_1K_A025));
    TEST_ASSERT_TRUE(mic.begin());
    TEST_ASSERT_TRUE(mic.isAlive());
    delay(200);
    TEST_ASSERT_TRUE(mic.isAlive());

    // The DMA stops delivering: alive for 50 ms after the last block.
    hostFeeding = false;
    delay(50);
    TEST_ASSERT_TRUE(mic.isAlive());
    delay(1);
    TEST_ASSERT_FALSE(mic.isAlive());
    TEST_ASSERT_TRUE(mic.isRunning());

    hostFeeding = true;
    delay(1);
    TEST_ASSERT_TRUE(mic.isAlive());

    // A block 0 ms ago, but the driver is stopped.
    mic.powerOff();
    TEST_ASSERT_FALSE(mic.isAlive());
    mic.powerOn();
    TEST_ASSERT_TRUE(mic.isRunning());
    TEST_ASSERT_TRUE(mic.isAlive());
    mic.end();
    TEST_ASSERT_FALSE(mic.isAlive());
}

void test_host_seam_begin_succeeds(void) {
    startHostSai(levelFrames(golden::LevelInput::SINE_1K_A025));
    TEST_ASSERT_TRUE(SteamiPdmSai::supported());
    SteamiIMP34DT05 mic;
    TEST_ASSERT_TRUE(mic.begin());
    TEST_ASSERT_TRUE(mic.isRunning());
    TEST_ASSERT_EQUAL_UINT8(0, mic.slot());
    TEST_ASSERT_FALSE(mic.levelReady());
    delay(IMP34DT05_LEVEL_WINDOW_DEFAULT_MS);
    TEST_ASSERT_TRUE(mic.levelReady());
    TEST_ASSERT_TRUE(mic.levelDbSpl() > 90.0f);
    mic.end();
    TEST_ASSERT_FALSE(mic.isRunning());
}

int main(int /* argc */, char** /* argv */) {
    UNITY_BEGIN();

    RUN_TEST(test_cic_tables_match_sinc3_coefficients);
    RUN_TEST(test_cic_saturates_instead_of_wrapping);

    RUN_TEST(test_filter_outputs_one_sample_per_ten_bytes);
    RUN_TEST(test_filter_output_does_not_depend_on_block_size);
    RUN_TEST(test_filter_reads_with_stride);
    RUN_TEST(test_filter_removes_dc_offset);
    RUN_TEST(test_filter_gain_follows_volume);
    RUN_TEST(test_filter_volume_zero_mutes);
    RUN_TEST(test_filter_rejects_volume_out_of_range);
    RUN_TEST(test_filter_saturates_loud_input);
    RUN_TEST(test_filter_matches_reference_on_random_bytes);
    RUN_TEST(test_filter_matches_reference_on_sigma_delta_streams);

    RUN_TEST(test_compute_rms);
    RUN_TEST(test_rms_to_dbfs);
    RUN_TEST(test_dominant_frequency_of_sines);
    RUN_TEST(test_dominant_frequency_picks_strongest_of_two);
    RUN_TEST(test_dominant_frequency_of_square_wave_is_fundamental);
    RUN_TEST(test_dominant_frequency_rejects_bad_input);

    RUN_TEST(test_begin_fails_without_hardware);
    RUN_TEST(test_begin_rejects_unsupported_sample_rate);
    RUN_TEST(test_level_is_nan_before_first_window);

    RUN_TEST(test_warmup_and_detection_deliver_nothing);
    RUN_TEST(test_conversion_stats_count_delivered_blocks_only);
    RUN_TEST(test_detection_finds_microphone_slot);
    RUN_TEST(test_detection_rejects_random_slot);
    RUN_TEST(test_forced_slot);
    RUN_TEST(test_auto_slot_after_forced_slot_detects_again);
    RUN_TEST(test_read_returns_oldest_samples_first);
    RUN_TEST(test_buffer_overrun_is_counted);
    RUN_TEST(test_callback_receives_each_block);
    RUN_TEST(test_level_window_measures_rms_and_peak);
    RUN_TEST(test_level_window_is_clamped);
    RUN_TEST(test_level_db_spl_is_nan_at_volume_zero);
    RUN_TEST(test_end_to_end_frequency_from_pdm);

    RUN_TEST(test_cross_filter_vectors);
    RUN_TEST(test_cross_fft_vectors);
    RUN_TEST(test_cross_level_vectors);
    RUN_TEST(test_dbfs_to_db_spl_matches_spl0);
    RUN_TEST(test_level_db_spl_uses_level_offset);
    RUN_TEST(test_is_alive_follows_blocks);
    RUN_TEST(test_host_seam_begin_succeeds);

    return UNITY_END();
}
