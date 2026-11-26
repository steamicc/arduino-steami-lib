// SPDX-License-Identifier: GPL-3.0-or-later

/*
 * Hardware unit validation for the IMP34DT05 microphone on a real STeaMi.
 *
 * No sound is required: the tests check that the PDM stream comes back,
 * at the right rate, and that its level is plausible for a room (above
 * the microphone noise floor, below clipping). The CPU load test prints
 * the share of the CPU the PDM to PCM conversion takes. The buzzer test
 * plays a tone and expects to hear it, within 3 % (HSI clock tolerance
 * plus margin).
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

void test_im34dt05_cpu_load() {
    // The loop runs slower while the DMA interrupt converts the stream:
    // the difference is the share of the CPU the conversion takes.
    mic.powerOff();
    uint32_t idle = spinCount(500);
    mic.powerOn();
    TEST_ASSERT_TRUE(mic.isRunning());
    uint32_t busy = spinCount(500);
    uint32_t permille =
        idle > busy ? static_cast<uint32_t>((static_cast<uint64_t>(idle - busy) * 1000) / idle) : 0;
    char msg[96];
    snprintf(msg, sizeof(msg), "PDM to PCM conversion: %lu.%lu %% of the CPU (%lu us per ms)",
             static_cast<unsigned long>(permille / 10), static_cast<unsigned long>(permille % 10),
             static_cast<unsigned long>(permille));
    TEST_MESSAGE(msg);
    TEST_ASSERT_LESS_THAN_UINT32(500, permille);
}

void test_im34dt05_hears_buzzer() {
    const uint16_t played = 2000;
    tone(SPEAKER, played);
    delay(100);
    float heard = mic.dominantFrequencyHz();
    noTone(SPEAKER);
    TEST_ASSERT_FALSE(isnan(heard));
    TEST_ASSERT_FLOAT_WITHIN(0.03f * played, played, heard);
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
