// SPDX-License-Identifier: GPL-3.0-or-later

#include <math.h>
#include <stdlib.h>
#include <unity.h>

#include <vector>

#include "SteamiIMP34DT05.h"
#include "SteamiPdmFilter.h"

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

void tearDown(void) {}

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

void test_forced_slot(void) {
    SteamiIMP34DT05 mic;
    TEST_ASSERT_FALSE(mic.setSlot(2));
    TEST_ASSERT_TRUE(mic.setSlot(1));
    TEST_ASSERT_EQUAL_UINT8(1, mic.slot());
    TEST_ASSERT_TRUE(mic.setSlot(IMP34DT05_SLOT_AUTO));
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
    RUN_TEST(test_detection_finds_microphone_slot);
    RUN_TEST(test_forced_slot);
    RUN_TEST(test_read_returns_oldest_samples_first);
    RUN_TEST(test_buffer_overrun_is_counted);
    RUN_TEST(test_callback_receives_each_block);
    RUN_TEST(test_level_window_measures_rms_and_peak);
    RUN_TEST(test_level_window_is_clamped);
    RUN_TEST(test_level_db_spl_is_nan_at_volume_zero);
    RUN_TEST(test_end_to_end_frequency_from_pdm);

    return UNITY_END();
}
