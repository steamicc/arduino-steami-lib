// SPDX-License-Identifier: GPL-3.0-or-later

/*
 * Hardware unit validation for the IMP34DT05 microphone on a real STeaMi.
 *
 * No sound is required: the tests check that the PDM stream comes back,
 * at the right rate, and that its level is plausible for a room (above
 * the microphone noise floor, below clipping). The CPU load test prints
 * the share of the CPU the PDM to PCM conversion takes, measured twice:
 * with the DWT cycle counter around the conversion, and by the slowdown
 * of a busy loop. The buzzer test plays tones from 2 to 4 kHz and
 * expects to hear each one, within 3 % (HSI clock tolerance plus
 * margin). Lower tones are left out: the piezo buzzer resonates near
 * 3 kHz, so below 2 kHz one of their harmonics is louder than the tone
 * itself (1000 Hz is heard at 3 kHz, 1500 Hz at 3 kHz too).
 */

#include <Arduino.h>
#include <SteamiIMP34DT05.h>
#include <math.h>
#include <unity.h>

SteamiIMP34DT05 mic;

volatile uint32_t callbackSamples = 0;

static void countSamples(const int16_t* /* samples */, size_t count) {
    callbackSamples = callbackSamples + count;
}

void setUp(void) {}

void tearDown(void) {}

void test_im34dt05_begin() {
    TEST_ASSERT_TRUE_MESSAGE(mic.begin(), "no PDM stream on MIC_IN (PA10)");
    TEST_ASSERT_TRUE(mic.isRunning());
    TEST_ASSERT_TRUE(mic.slot() < 2);
}

void test_im34dt05_sample_rate() {
    // 16 samples per millisecond: about 8000 in 500 ms.
    mic.onReceive(countSamples);
    callbackSamples = 0;
    delay(500);
    mic.onReceive(nullptr);
    TEST_ASSERT_UINT32_WITHIN(240, 8000, callbackSamples);
}

void test_im34dt05_read_buffer() {
    // The previous test left the buffer full: overruns() counts the
    // samples it dropped since begin(), clear() does not reset it.
    mic.clear();
    uint32_t overrunsBefore = mic.overruns();
    delay(20);
    size_t waiting = mic.available();
    TEST_ASSERT_UINT32_WITHIN(64, 320, waiting);
    int16_t samples[64];
    TEST_ASSERT_EQUAL_UINT(64, mic.read(samples, 64));
    TEST_ASSERT_EQUAL_UINT32(overrunsBefore, mic.overruns());
}

void test_im34dt05_level_plausible() {
    mic.setLevelWindowMs(100);
    delay(250);
    float dbfs = mic.levelDbfs();
    TEST_ASSERT_FALSE(isnan(dbfs));
    // Not all-zero (dead stream) and not clipping in a normal room.
    TEST_ASSERT_GREATER_THAN_FLOAT(-100.0f, dbfs);
    TEST_ASSERT_LESS_THAN_FLOAT(-3.0f, dbfs);
}

void test_im34dt05_power_cycle() {
    mic.powerOff();
    TEST_ASSERT_FALSE(mic.isRunning());
    mic.clear();
    delay(20);
    TEST_ASSERT_EQUAL_UINT(0, mic.available());
    mic.powerOn();
    TEST_ASSERT_TRUE(mic.isRunning());
    delay(20);
    TEST_ASSERT_TRUE(mic.available() > 0);
}

// Busy-loop iterations completed in `ms` milliseconds.
static uint32_t spinCount(uint32_t ms) {
    volatile uint32_t count = 0;
    uint32_t start = millis();
    while (millis() - start < ms) {
        count = count + 1;
    }
    return count;
}

// Share of the CPU, in per mille, that `cycles` per millisecond take.
static uint32_t cyclesToPermille(uint32_t cycles) {
    uint32_t cyclesPerMs = SystemCoreClock / 1000;
    return static_cast<uint32_t>((static_cast<uint64_t>(cycles) * 1000) / cyclesPerMs);
}

void test_im34dt05_cpu_load() {
    // Measured once the stream runs steadily: powerOn() returns after the
    // warm-up and settling blocks, and the conversion statistics leave
    // out the slot detection (begin() only) and those blocks anyway.
    mic.powerOff();
    uint32_t idle = spinCount(500);
    mic.powerOn();
    TEST_ASSERT_TRUE(mic.isRunning());
    mic.resetConversionStats();

    // 1. Busy loop: it runs slower while the DMA interrupt converts the
    //    stream. Includes the interrupt entry and the HAL DMA handler.
    uint32_t busy = spinCount(500);
    uint32_t spinPermille =
        idle > busy ? static_cast<uint32_t>((static_cast<uint64_t>(idle - busy) * 1000) / idle) : 0;

    // 2. DWT cycle counter around each block's conversion (same 500 ms).
    SteamiIMP34DT05::ConversionStats stats = mic.conversionStats();
    uint32_t averagePermille = cyclesToPermille(stats.averageCycles);
    uint32_t maxPermille = cyclesToPermille(stats.maxCycles);

    char msg[160];
    snprintf(msg, sizeof(msg),
             "PDM to PCM conversion at %lu MHz: %lu cycles per 1 ms block (%lu.%lu %% of the "
             "CPU, longest block %lu.%lu %%, %lu blocks); busy loop: %lu.%lu %%",
             static_cast<unsigned long>(SystemCoreClock / 1000000),
             static_cast<unsigned long>(stats.averageCycles),
             static_cast<unsigned long>(averagePermille / 10),
             static_cast<unsigned long>(averagePermille % 10),
             static_cast<unsigned long>(maxPermille / 10),
             static_cast<unsigned long>(maxPermille % 10), static_cast<unsigned long>(stats.blocks),
             static_cast<unsigned long>(spinPermille / 10),
             static_cast<unsigned long>(spinPermille % 10));
    TEST_MESSAGE(msg);

    // About 500 blocks in 500 ms, all of them after the start-up phase.
    TEST_ASSERT_UINT32_WITHIN(20, 500, stats.blocks);
    // Each block must be done well before the next one (1 ms).
    TEST_ASSERT_LESS_THAN_UINT32(1000, maxPermille);
    // Under 30 % on average, whichever way it is measured.
    TEST_ASSERT_LESS_THAN_UINT32(300, averagePermille);
    TEST_ASSERT_LESS_THAN_UINT32(300, spinPermille);
}

void test_im34dt05_hears_buzzer() {
    // The buzzer's useful range: it resonates near 3 kHz, and below
    // 2 kHz a harmonic near 3 kHz dominates what the microphone hears.
    const uint16_t tones[] = {2000, 2500, 3000, 3500, 4000};
    for (uint16_t played : tones) {
        tone(SPEAKER, played);
        delay(100);
        float heard = mic.dominantFrequencyHz();
        noTone(SPEAKER);
        delay(50);
        TEST_ASSERT_FALSE(isnan(heard));
        // No %f: the core's printf is built without float support.
        long tenths = lroundf(heard * 10.0f);
        char msg[64];
        snprintf(msg, sizeof(msg), "played %u Hz, heard %ld.%ld Hz", static_cast<unsigned>(played),
                 tenths / 10, tenths % 10);
        TEST_MESSAGE(msg);
        TEST_ASSERT_FLOAT_WITHIN(0.03f * played, played, heard);
    }
}

void test_im34dt05_end() {
    mic.end();
    TEST_ASSERT_FALSE(mic.isRunning());
    TEST_ASSERT_TRUE(isnan(mic.dominantFrequencyHz()));
}

void setup() {
    delay(2000);

    UNITY_BEGIN();
    RUN_TEST(test_im34dt05_begin);
    RUN_TEST(test_im34dt05_sample_rate);
    RUN_TEST(test_im34dt05_read_buffer);
    RUN_TEST(test_im34dt05_level_plausible);
    RUN_TEST(test_im34dt05_power_cycle);
    RUN_TEST(test_im34dt05_cpu_load);
    RUN_TEST(test_im34dt05_hears_buzzer);
    RUN_TEST(test_im34dt05_end);
    UNITY_END();
}

void loop() {}
