// SPDX-License-Identifier: GPL-3.0-or-later

#include <Arduino.h>
#include <Print.h>
#include <Wire.h>
#include <math.h>
#include <string.h>
#include <unity.h>

#include <string>
#include <vector>

#include "../test_im34dt05/golden_vectors.h"
#include "SteamiBoard.h"
#include "SteamiHandle.h"
#include "SteamiIMP34DT05.h"
#include "SteamiMicrophone.h"
#include "SteamiPdmSai.h"
#include "SteamiTypes.h"

// SteamiMicrophone over the real driver, fed through the host stand-in for SAI1: while
// `feeding` is set, each simulated millisecond hands the driver one SAI block of a 1 kHz sine
// at 0.1 of full scale (the sine1k_a0.1 input of the golden level lines, Annex A).

namespace {

std::vector<std::string> lines;
void capture(const char* line) {
    lines.push_back(line);
}

SteamiBoard board(Wire, SteamiBoardPins::steami());

constexpr const char* kE10 =
    "[STeaMi] SteamiMicrophone: the IMP34DT05 microphone (PDM, MIC_CLK PA3 / MIC_IN PA10) sent "
    "no sound. Check the board or try another one. (E10)";

// --- PDM input, as test_im34dt05 builds it ----------------------------------

constexpr float PI_F = 3.14159265358979f;
constexpr uint32_t FNV_OFFSET = 2166136261U;
constexpr uint32_t FNV_PRIME = 16777619U;

// PDM bytes (oldest bit in the MSB) of amplitude * sin(2 pi f t), from a second-order
// sigma-delta modulator.
std::vector<uint8_t> pdmSine(float frequencyHz, float amplitude, size_t bytes) {
    std::vector<uint8_t> out(bytes, 0);
    double integrator1 = 0.0;
    double integrator2 = 0.0;
    double feedback = 0.0;
    for (size_t n = 0; n < bytes * 8; n++) {
        double x = amplitude * sin(2.0 * PI_F * frequencyHz * static_cast<double>(n) /
                                   static_cast<double>(IMP34DT05_PDM_CLOCK_HZ));
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

uint32_t fnvBytes(const std::vector<uint8_t>& bytes) {
    uint32_t hash = FNV_OFFSET;
    for (uint8_t value : bytes) {
        hash = (hash ^ value) * FNV_PRIME;
    }
    return hash;
}

// SAI frames: the microphone on slot 0, slot 1 at 0x00 (an undriven data line).
const std::vector<uint8_t>& frames() {
    static std::vector<uint8_t> out;
    if (out.empty()) {
        std::vector<uint8_t> mic = pdmSine(1000.0f, 0.1f, golden::LEVEL_BYTES_PER_SLOT);
        const size_t index = static_cast<size_t>(golden::LevelInput::SINE_1K_A010);
        TEST_ASSERT_EQUAL_HEX32_MESSAGE(golden::LEVEL_INPUT_FNV[index], fnvBytes(mic),
                                        "level input differs: generator (libm), not driver");
        out.assign(mic.size() * 2, 0x00);
        for (size_t i = 0; i < mic.size(); i++) {
            out[i * 2] = mic[i];
        }
    }
    return out;
}

bool feeding = false;
size_t position = 0;

// A board with a microphone: blocks from the start of the input, one per millisecond.
void feedFromStart() {
    position = 0;
    feeding = true;
}

void installHostSai() {
    SteamiPdmSai::hostSetPresent(true);
    clockHook() = [] {
        if (!feeding) {
            return;
        }
        const std::vector<uint8_t>& data = frames();
        if (position + IMP34DT05_SAI_BYTES_PER_BLOCK > data.size()) {
            position = 0;
        }
        SteamiPdmSai::hostFeed(data.data() + position, IMP34DT05_SAI_BYTES_PER_BLOCK);
        position += IMP34DT05_SAI_BYTES_PER_BLOCK;
    };
}

// The golden line sine1k_a0.1, volume 5, window 100 ms.
const golden::LevelVector& goldenLine() {
    for (const golden::LevelVector& line : golden::LEVEL_VECTORS) {
        if (line.input == golden::LevelInput::SINE_1K_A010 && line.volume == 5 &&
            line.windowMs == 100) {
            return line;
        }
    }
    TEST_FAIL_MESSAGE("golden line sine1k_a0.1 vol=5 win=100 missing");
    return golden::LEVEL_VECTORS[0];
}

void assertStatus(SteamiStatus expected, SteamiStatus actual) {
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(expected), static_cast<uint8_t>(actual));
}

size_t countContaining(const char* what) {
    size_t count = 0;
    for (const std::string& line : lines) {
        if (line.find(what) != std::string::npos) {
            count++;
        }
    }
    return count;
}

// Time spent by a call, in simulated milliseconds.
template <typename F>
uint32_t elapsed(F call) {
    const uint32_t start = millis();
    call();
    return millis() - start;
}

}  // namespace

void setUp(void) {
    Wire = TwoWire();
    clockHook() = nullptr;
    feeding = false;
    lines.clear();
    board.end();
    Steami.end();
    board.setLogger(&capture);
    TEST_ASSERT_TRUE(board.begin(SteamiRelease::V1C));
    installHostSai();
    // The driver is shared by every test: back to the default settings.
    SteamiMicrophone defaults(board);
    TEST_ASSERT_TRUE(defaults.setVolume(IMP34DT05_VOLUME_DEFAULT));
    defaults.setWindow(IMP34DT05_LEVEL_WINDOW_DEFAULT_MS);
}

void tearDown(void) {
    board.end();
    Steami.end();
    SteamiPdmSai::hostSetPresent(false);
    clockHook() = nullptr;
    feeding = false;
    board.setLogger(nullptr);
}

// --- Routing, probe and messages ---------------------------------------------

void test_route_all_releases(void) {
    const SteamiRelease releases[] = {SteamiRelease::UNKNOWN, SteamiRelease::V1A,
                                      SteamiRelease::V1B, SteamiRelease::V1AB, SteamiRelease::V1C};
    for (SteamiRelease release : releases) {
        TEST_ASSERT_TRUE(steamiRoute(release, SteamiFunction::MICROPHONE, 0) ==
                         SteamiChip::IMP34DT05);
        TEST_ASSERT_TRUE(steamiRoute(release, SteamiFunction::MICROPHONE, 1) == SteamiChip::NONE);
    }
    const SteamiRelease forced[] = {SteamiRelease::V1A, SteamiRelease::V1B, SteamiRelease::V1C};
    for (SteamiRelease release : forced) {
        board.end();
        TEST_ASSERT_TRUE(board.begin(release));
        TEST_ASSERT_TRUE(board.has(SteamiFunction::MICROPHONE));
    }
    // The existing values keep their numbers.
    TEST_ASSERT_EQUAL_UINT8(11, static_cast<uint8_t>(SteamiChip::GC9A01));
    TEST_ASSERT_EQUAL_UINT8(12, static_cast<uint8_t>(SteamiChip::IMP34DT05));
    TEST_ASSERT_EQUAL_UINT8(13, static_cast<uint8_t>(SteamiFunction::SCREEN));
    TEST_ASSERT_EQUAL_UINT8(14, static_cast<uint8_t>(SteamiFunction::MICROPHONE));
}

void test_route_unknown_release_still_begins(void) {
    board.end();
    TEST_ASSERT_FALSE(board.begin());  // no chip answers: UNKNOWN
    feedFromStart();
    SteamiMicrophone mic(board);
    TEST_ASSERT_TRUE(mic.begin());
    TEST_ASSERT_EQUAL_STRING("IMP34DT05", mic.chip());
}

void test_probe_starts_driver(void) {
    feedFromStart();
    SteamiMicrophone mic(board);
    assertStatus(SteamiStatus::NOT_STARTED, mic.status());
    TEST_ASSERT_TRUE(mic.begin());
    TEST_ASSERT_TRUE(mic.isAvailable());
    assertStatus(SteamiStatus::OK, mic.status());
    TEST_ASSERT_FALSE(board.built(SteamiChip::IMP34DT05));  // not a slot of the board
    assertStatus(SteamiStatus::NOT_STARTED, board.claim(SteamiChip::IMP34DT05));

    // The accessor finds the running driver and does not start it again.
    SteamiIMP34DT05* driver = nullptr;
    TEST_ASSERT_EQUAL_UINT32(0, elapsed([&] { driver = steamiImp34dt05(); }));
    TEST_ASSERT_NOT_NULL(driver);
    TEST_ASSERT_TRUE(driver->isRunning());
    TEST_ASSERT_TRUE(driver->isAlive());

    // A second object shares it, also without starting it again.
    SteamiMicrophone other(board);
    TEST_ASSERT_EQUAL_UINT32(0, elapsed([&] { TEST_ASSERT_TRUE(other.begin()); }));
    TEST_ASSERT_EQUAL_UINT32(0, countContaining("(E10)"));
}

void test_probe_replaces_claim(void) {
    static int calls = 0;
    calls = 0;
    static const SteamiFunctionInfo kInfo = {SteamiFunction::MICROPHONE,
                                             "SteamiTestMicrophone",
                                             "microphone",
                                             "Check the board or try another one.",
                                             "read() will return NAN.",
                                             [](SteamiBoard& /* b */) {
                                                 calls++;
                                                 return true;
                                             }};
    SteamiHandle handle(board, kInfo);
    TEST_ASSERT_TRUE(handle.begin());
    TEST_ASSERT_EQUAL_INT(1, calls);
    TEST_ASSERT_TRUE(handle.chipId() == SteamiChip::IMP34DT05);
    TEST_ASSERT_TRUE(handle.begin());  // idempotent
    TEST_ASSERT_EQUAL_INT(1, calls);
}

void test_chip_name_and_address(void) {
    TEST_ASSERT_EQUAL_STRING("IMP34DT05", steamiChipName(SteamiChip::IMP34DT05));
    TEST_ASSERT_EQUAL_UINT8(0, steamiChipAddress(SteamiChip::IMP34DT05));
    feedFromStart();
    SteamiMicrophone mic(board);
    TEST_ASSERT_EQUAL_STRING("", mic.chip());
    TEST_ASSERT_TRUE(mic.begin());
    TEST_ASSERT_EQUAL_STRING("IMP34DT05", mic.chip());
}

void test_board_info_has_no_microphone(void) {
    feedFromStart();
    SteamiMicrophone mic(board);
    TEST_ASSERT_TRUE(mic.begin());
    StringPrint out;
    board.printInfo(out);
    TEST_ASSERT_TRUE(out.text.find("MCP23009E") != std::string::npos);
    TEST_ASSERT_TRUE(out.text.find("IMP34DT05") == std::string::npos);
}

void test_e10_text_matches_micropython(void) {
    SteamiMicrophone mic(board);  // the SAI runs, but no block arrives
    TEST_ASSERT_FALSE(mic.begin());
    assertStatus(SteamiStatus::NOT_RESPONDING, mic.status());
    TEST_ASSERT_FALSE(lines.empty());
    TEST_ASSERT_EQUAL_STRING(kE10, lines.back().c_str());
    TEST_ASSERT_EQUAL_UINT32(1, countContaining("(E10)"));
    TEST_ASSERT_NULL(steamiImp34dt05());

    // No SAI at all: the same message, once per (class, code) pair.
    board.end();
    lines.clear();
    SteamiPdmSai::hostSetPresent(false);
    SteamiMicrophone again(board);
    TEST_ASSERT_FALSE(again.begin());
    TEST_ASSERT_EQUAL_STRING(kE10, board.lastError());
}

void test_error_strings_match_micropython(void) {
    SteamiMicrophone mic(board);
    TEST_ASSERT_EQUAL_STRING("not started", mic.error());
    feedFromStart();
    TEST_ASSERT_TRUE(mic.begin());
    TEST_ASSERT_EQUAL_STRING("", mic.error());

    mic.end();
    feeding = false;
    SteamiMicrophone silent(board);
    TEST_ASSERT_FALSE(silent.begin());
    TEST_ASSERT_EQUAL_STRING("no answer", silent.error());
}

// --- Level -------------------------------------------------------------------

void test_read_golden_spl(void) {
    const golden::LevelVector& line = goldenLine();
    TEST_ASSERT_FLOAT_WITHIN(0.0005f, 99.853f, line.spl0);
    feedFromStart();
    SteamiMicrophone mic(board);
    TEST_ASSERT_TRUE(mic.begin());
    TEST_ASSERT_FALSE(mic.levelReady());
    // The first read waits for the first window.
    float spl = NAN;
    const uint32_t waited = elapsed([&] { spl = mic.read(); });
    TEST_ASSERT_TRUE(waited >= line.windowMs);
    TEST_ASSERT_TRUE(waited <= line.windowMs + 20U);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 99.853f, spl);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, line.dbfs0, mic.dbfs());
    TEST_ASSERT_EQUAL_UINT16(line.peak0, mic.peak());

    mic.setOffset(3.0f);
    TEST_ASSERT_EQUAL_FLOAT(3.0f, mic.offset());
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 102.853f, mic.read());
    TEST_ASSERT_TRUE(mic.isLoud(102.0f));
    TEST_ASSERT_FALSE(mic.isLoud(103.0f));
    TEST_ASSERT_TRUE(mic.isLoud());  // above 70 dB by default
}

void test_read_nan_when_stopped(void) {
    feedFromStart();
    SteamiMicrophone mic(board);
    TEST_ASSERT_TRUE(mic.begin());
    TEST_ASSERT_FALSE(isnan(mic.read()));

    // Stopped behind the object's back, through the accessor.
    SteamiIMP34DT05* driver = steamiImp34dt05();
    TEST_ASSERT_NOT_NULL(driver);
    driver->end();
    delay(200);
    int16_t samples[16];
    TEST_ASSERT_TRUE(isnan(mic.read()));
    TEST_ASSERT_TRUE(isnan(mic.dbfs()));
    TEST_ASSERT_EQUAL_UINT16(0, mic.peak());
    TEST_ASSERT_FALSE(mic.levelReady());
    TEST_ASSERT_FALSE(mic.isLoud(0.0f));
    TEST_ASSERT_TRUE(isnan(mic.frequency(0.0f)));
    TEST_ASSERT_EQUAL_UINT32(0, mic.readInto(samples, 16));
    TEST_ASSERT_FALSE(driver->isRunning());  // no restart
    assertStatus(SteamiStatus::OK, mic.status());
}

void test_read_repeats_last_window(void) {
    feedFromStart();
    SteamiMicrophone mic(board);
    TEST_ASSERT_TRUE(mic.begin());
    const float first = mic.read();
    TEST_ASSERT_FALSE(isnan(first));

    // Faster than the window: no wait, the last completed window again.
    float again = NAN;
    TEST_ASSERT_EQUAL_UINT32(0, elapsed([&] { again = mic.read(); }));
    TEST_ASSERT_EQUAL_FLOAT(first, again);
    TEST_ASSERT_FALSE(mic.levelReady());

    // No block for a while (the stream still counts as alive): still no wait, never NAN.
    feeding = false;
    delay(30);
    TEST_ASSERT_EQUAL_UINT32(0, elapsed([&] { again = mic.read(); }));
    TEST_ASSERT_EQUAL_FLOAT(first, again);

    // Slower than the window: the latest one, still without waiting.
    feeding = true;
    delay(350);
    TEST_ASSERT_TRUE(mic.levelReady());
    TEST_ASSERT_EQUAL_UINT32(0, elapsed([&] { again = mic.read(); }));
    TEST_ASSERT_FLOAT_WITHIN(0.05f, first, again);
}

void test_read_nan_when_no_window_in_time(void) {
    feedFromStart();
    SteamiMicrophone mic(board);
    mic.setWindow(4000);
    TEST_ASSERT_EQUAL_UINT16(4000, mic.window());
    TEST_ASSERT_TRUE(mic.begin());
    // The stream dies right after the start: the wait gives up after window() + 20 ms.
    feeding = false;
    float spl = 0.0f;
    const uint32_t waited = elapsed([&] { spl = mic.read(); });
    TEST_ASSERT_TRUE(isnan(spl));
    TEST_ASSERT_TRUE(waited <= 4000U + 20U);
}

void test_volume_zero(void) {
    feedFromStart();
    SteamiMicrophone mic(board);
    TEST_ASSERT_TRUE(mic.begin());
    TEST_ASSERT_FALSE(mic.setVolume(7));
    TEST_ASSERT_TRUE(mic.setVolume(0));
    TEST_ASSERT_EQUAL_UINT8(0, mic.volume());
    delay(250);  // two whole windows at volume 0
    TEST_ASSERT_TRUE(isnan(mic.read()));
    TEST_ASSERT_EQUAL_FLOAT(-120.0f, mic.dbfs());
    TEST_ASSERT_EQUAL_UINT16(0, mic.peak());
    TEST_ASSERT_FALSE(mic.isLoud(0.0f));
    TEST_ASSERT_TRUE(isnan(mic.frequency(0.0f)));
    TEST_ASSERT_TRUE(isnan(mic.frequency(-1000.0f)));
}

void test_frequency_threshold_includes_offset(void) {
    feedFromStart();
    SteamiMicrophone mic(board);
    TEST_ASSERT_TRUE(mic.begin());
    // The input reads about 99.85 dB SPL.
    float hz = 0.0f;
    const uint32_t blocked = elapsed([&] { hz = mic.frequency(); });
    TEST_ASSERT_FLOAT_WITHIN(5.0f, 1000.0f, hz);
    TEST_ASSERT_TRUE(blocked >= 32 && blocked <= 40);
    TEST_ASSERT_TRUE(isnan(mic.frequency(100.0f)));
    mic.setOffset(1.0f);
    TEST_ASSERT_FLOAT_WITHIN(5.0f, 1000.0f, mic.frequency(100.0f));
    TEST_ASSERT_TRUE(isnan(mic.frequency(101.0f)));
}

void test_read_into_records_new_samples(void) {
    feedFromStart();
    SteamiMicrophone mic(board);
    int16_t samples[160] = {};
    TEST_ASSERT_TRUE(mic.begin());
    size_t count = 0;
    const uint32_t blocked = elapsed([&] { count = mic.readInto(samples, 160); });
    TEST_ASSERT_EQUAL_UINT32(160, count);
    TEST_ASSERT_TRUE(blocked >= 10 && blocked <= 12);
    int16_t largest = 0;
    for (int16_t s : samples) {
        largest = s > largest ? s : largest;
    }
    TEST_ASSERT_TRUE(largest > 1000);
    TEST_ASSERT_EQUAL_UINT32(0, mic.readInto(samples, 0));
}

void test_settings(void) {
    SteamiMicrophone mic(board);
    TEST_ASSERT_EQUAL_UINT16(100, mic.window());
    TEST_ASSERT_EQUAL_UINT8(5, mic.volume());
    TEST_ASSERT_EQUAL_FLOAT(0.0f, mic.offset());
    mic.setWindow(0);
    TEST_ASSERT_EQUAL_UINT16(1, mic.window());
    mic.setWindow(5000);
    TEST_ASSERT_EQUAL_UINT16(4000, mic.window());
    mic.setWindow(200);
    // The setting lives in the shared driver.
    SteamiMicrophone other(board);
    TEST_ASSERT_EQUAL_UINT16(200, other.window());
}

void test_level_ready_shared(void) {
    feedFromStart();
    SteamiMicrophone a;
    SteamiMicrophone b;
    TEST_ASSERT_TRUE(a.begin());
    TEST_ASSERT_TRUE(b.begin());
    SteamiIMP34DT05* driver = steamiImp34dt05();
    TEST_ASSERT_NOT_NULL(driver);
    delay(110);
    TEST_ASSERT_TRUE(a.levelReady());
    TEST_ASSERT_TRUE(b.levelReady());
    TEST_ASSERT_TRUE(driver->levelReady());
    TEST_ASSERT_TRUE(a.levelReady());  // levelReady() does not consume
    TEST_ASSERT_FALSE(isnan(a.read()));
    TEST_ASSERT_FALSE(b.levelReady());
    TEST_ASSERT_FALSE(driver->levelReady());

    delay(100);
    TEST_ASSERT_TRUE(a.levelReady());
    (void)b.dbfs();
    TEST_ASSERT_FALSE(a.levelReady());
    delay(100);
    (void)b.peak();
    TEST_ASSERT_FALSE(a.levelReady());
    delay(100);
    (void)b.isLoud();
    TEST_ASSERT_FALSE(a.levelReady());
    delay(100);
    (void)driver->level();
    TEST_ASSERT_FALSE(b.levelReady());
}

// --- Restarts ----------------------------------------------------------------

void test_no_relaunch_after_power_off(void) {
    feedFromStart();
    SteamiMicrophone mic(board);
    TEST_ASSERT_TRUE(mic.begin());
    TEST_ASSERT_FALSE(isnan(mic.read()));
    SteamiIMP34DT05* driver = steamiImp34dt05();
    TEST_ASSERT_NOT_NULL(driver);
    driver->powerOff();
    delay(100);
    TEST_ASSERT_TRUE(isnan(mic.read()));
    TEST_ASSERT_FALSE(driver->isRunning());
    TEST_ASSERT_TRUE(isnan(mic.read()));
    TEST_ASSERT_FALSE(driver->isRunning());

    // begin() on a powered-off driver starts it again.
    SteamiMicrophone fresh(board);
    TEST_ASSERT_TRUE(fresh.begin());
    TEST_ASSERT_TRUE(driver->isRunning());
    TEST_ASSERT_FALSE(isnan(fresh.read()));
    TEST_ASSERT_TRUE(mic.begin());
    TEST_ASSERT_FALSE(isnan(mic.read()));
}

void test_relaunch_when_not_alive(void) {
    feedFromStart();
    SteamiMicrophone mic(board);
    TEST_ASSERT_TRUE(mic.begin());
    const float before = mic.read();
    TEST_ASSERT_FALSE(isnan(before));
    SteamiIMP34DT05* driver = steamiImp34dt05();
    TEST_ASSERT_NOT_NULL(driver);

    // The DMA stops delivering blocks while the driver runs.
    feeding = false;
    delay(60);
    TEST_ASSERT_TRUE(driver->isRunning());
    TEST_ASSERT_FALSE(driver->isAlive());

    // The next read restarts the stream (46 blocks) and waits for its first window.
    feedFromStart();
    float spl = NAN;
    const uint32_t waited = elapsed([&] { spl = mic.read(); });
    TEST_ASSERT_FALSE(isnan(spl));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 99.853f, spl);
    const uint32_t startBlocks =
        IMP34DT05_WARMUP_BLOCKS + IMP34DT05_DETECT_BLOCKS + IMP34DT05_SETTLE_BLOCKS;
    TEST_ASSERT_TRUE(waited >= startBlocks + 100U);
    TEST_ASSERT_TRUE(driver->isAlive());

    // A stream that does not come back: NAN, then no further restart while stopped.
    feeding = false;
    delay(60);
    TEST_ASSERT_TRUE(isnan(mic.read()));
    TEST_ASSERT_FALSE(driver->isRunning());
    TEST_ASSERT_EQUAL_UINT32(0, elapsed([&] { TEST_ASSERT_TRUE(isnan(mic.read())); }));
}

void test_steami_end_stops_microphone(void) {
    feedFromStart();
    SteamiMicrophone mic(board);
    TEST_ASSERT_TRUE(mic.begin());
    TEST_ASSERT_FALSE(isnan(mic.read()));
    SteamiIMP34DT05* driver = steamiImp34dt05();
    TEST_ASSERT_NOT_NULL(driver);

    board.end();
    TEST_ASSERT_FALSE(driver->isRunning());
    TEST_ASSERT_TRUE(isnan(mic.read()));
    TEST_ASSERT_FALSE(driver->isRunning());

    TEST_ASSERT_TRUE(board.begin(SteamiRelease::V1C));
    TEST_ASSERT_TRUE(mic.begin());
    TEST_ASSERT_TRUE(driver->isRunning());
    TEST_ASSERT_FALSE(isnan(mic.read()));

    // The hook is cleared by end(): a second end() with the driver started elsewhere leaves it.
    board.end();
    TEST_ASSERT_FALSE(driver->isRunning());
    SteamiMicrophone onSteami;
    TEST_ASSERT_TRUE(onSteami.begin());
    board.end();
    TEST_ASSERT_TRUE(driver->isRunning());
    Steami.end();
    TEST_ASSERT_FALSE(driver->isRunning());
}

void test_end_then_begin_restarts(void) {
    feedFromStart();
    SteamiMicrophone mic(board);
    TEST_ASSERT_TRUE(mic.begin());
    TEST_ASSERT_FALSE(isnan(mic.read()));
    SteamiIMP34DT05* driver = steamiImp34dt05();
    TEST_ASSERT_NOT_NULL(driver);

    mic.end();
    assertStatus(SteamiStatus::NOT_STARTED, mic.status());
    TEST_ASSERT_EQUAL_STRING("not started", mic.error());
    TEST_ASSERT_FALSE(driver->isRunning());

    TEST_ASSERT_TRUE(mic.begin());
    TEST_ASSERT_TRUE(driver->isRunning());
    TEST_ASSERT_FALSE(mic.levelReady());
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 99.853f, mic.read());
}

void test_existing_classes_unchanged(void) {
    // Aggregate initialisation of the five fields of the other classes still compiles.
    constexpr SteamiFunctionInfo kInfo = {
        SteamiFunction::TEMPERATURE, "SteamiTestTemp", "temperature sensor",
        "Check the board or try another one.", "read() will return NAN."};
    TEST_ASSERT_TRUE(kInfo.probe == nullptr);
    TEST_ASSERT_EQUAL_STRING("SteamiTestTemp", kInfo.who);
    // Without a probe, the handle claims the chip on the board as before.
    SteamiHandle handle(board, kInfo);
    TEST_ASSERT_FALSE(handle.begin());  // no BME280 on the bus
    assertStatus(SteamiStatus::NOT_RESPONDING, handle.status());
    TEST_ASSERT_EQUAL_UINT32(
        1, countContaining("BME280 temperature sensor (I2C 0x76) did not answer"));
}

int main(int /* argc */, char** /* argv */) {
    UNITY_BEGIN();
    RUN_TEST(test_route_all_releases);
    RUN_TEST(test_route_unknown_release_still_begins);
    RUN_TEST(test_probe_starts_driver);
    RUN_TEST(test_probe_replaces_claim);
    RUN_TEST(test_chip_name_and_address);
    RUN_TEST(test_board_info_has_no_microphone);
    RUN_TEST(test_e10_text_matches_micropython);
    RUN_TEST(test_error_strings_match_micropython);
    RUN_TEST(test_read_golden_spl);
    RUN_TEST(test_read_nan_when_stopped);
    RUN_TEST(test_read_repeats_last_window);
    RUN_TEST(test_read_nan_when_no_window_in_time);
    RUN_TEST(test_volume_zero);
    RUN_TEST(test_frequency_threshold_includes_offset);
    RUN_TEST(test_read_into_records_new_samples);
    RUN_TEST(test_settings);
    RUN_TEST(test_level_ready_shared);
    RUN_TEST(test_no_relaunch_after_power_off);
    RUN_TEST(test_relaunch_when_not_alive);
    RUN_TEST(test_steami_end_stops_microphone);
    RUN_TEST(test_end_then_begin_restarts);
    RUN_TEST(test_existing_classes_unchanged);
    return UNITY_END();
}
