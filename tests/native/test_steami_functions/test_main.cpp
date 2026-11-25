// SPDX-License-Identifier: GPL-3.0-or-later

#include <Arduino.h>
#include <Wire.h>
#include <math.h>
#include <string.h>
#include <unity.h>

#include <string>
#include <vector>

#include "FakeDaplinkBridge.h"
#include "SteamiBoard.h"
#include "SteamiHandle.h"
#include "SteamiHumidity.h"
#include "SteamiPressure.h"
#include "SteamiTemperature.h"
#include "SteamiTypes.h"
#include "steami_fake_chips.h"

static std::vector<std::string> g_lines;
static void capture(const char* line) {
    g_lines.push_back(line);
}

static SteamiBoard board(Wire, SteamiBoardPins::steami());

void setUp(void) {
    Wire = TwoWire();
    g_lines.clear();
    board.end();
    board.setLogger(&capture);
}
void tearDown(void) {
    board.end();
    board.setLogger(nullptr);
}

static FakeDaplinkBridge fw;

static void attachFirmware(void) {
    fw = FakeDaplinkBridge();
    Wire.attachDevice(DAPLINK_BRIDGE_DEFAULT_ADDR, &fw);
}

static size_t countLogged(const char* suffix) {
    const size_t n = strlen(suffix);
    size_t count = 0;
    for (const std::string& line : g_lines) {
        if (line.size() >= n && line.compare(line.size() - n, n, suffix) == 0) {
            count++;
        }
    }
    return count;
}

static size_t countContaining(const char* what) {
    size_t count = 0;
    for (const std::string& line : g_lines) {
        if (line.find(what) != std::string::npos) {
            count++;
        }
    }
    return count;
}

static void assertStatus(SteamiStatus expected, SteamiStatus actual) {
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(expected), static_cast<uint8_t>(actual));
}

static const char* const kBmeOffsetConfig = "{\"rev\":131,\"tc\":{\"bme\":{\"g\":1.0,\"o\":-2.0}}}";

static const SteamiFunctionInfo kGesture = {SteamiFunction::GESTURE, "SteamiGesture",
                                            "gesture sensor", "Check the board or try another one.",
                                            "read() will return NONE."};
static const SteamiFunctionInfo kScreen = {SteamiFunction::SCREEN, "SteamiTestScreen", "screen",
                                           "Check the board or try another one.",
                                           "nothing will be drawn."};

// --- Routing and reads ---

static void test_temperature_on_v1c_reads_bme280(void) {
    steamiFakeBme280(Wire);
    SteamiTemperature temperature(board);
    TEST_ASSERT_TRUE(temperature.begin());
    TEST_ASSERT_TRUE(temperature.isAvailable());
    assertStatus(SteamiStatus::OK, temperature.status());
    TEST_ASSERT_EQUAL_STRING("BME280", temperature.chip());
    TEST_ASSERT_EQUAL_STRING("", temperature.error());
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 25.08f, temperature.read());
}

static void test_humidity_and_pressure_on_v1c(void) {
    steamiFakeBme280(Wire);
    SteamiHumidity humidity(board);
    SteamiPressure pressure(board);
    TEST_ASSERT_TRUE(humidity.begin());
    TEST_ASSERT_TRUE(pressure.begin());
    TEST_ASSERT_EQUAL_STRING("BME280", humidity.chip());
    TEST_ASSERT_EQUAL_STRING("BME280", pressure.chip());
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 55.35f, humidity.read());
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1006.53f, pressure.read());
    const float expected = SteamiBME280::altitudeFromPressure(1006.5325f, 1013.25f);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, expected, pressure.altitude(1013.25f));
    TEST_ASSERT_FLOAT_WITHIN(0.1f, expected, pressure.altitude());
}

static void test_shared_bme280_begins_once(void) {
    steamiFakeBme280(Wire);
    SteamiTemperature temperature(board);
    SteamiHumidity humidity(board);
    SteamiPressure pressure(board);
    TEST_ASSERT_TRUE(temperature.begin());
    const uint32_t afterFirst = Wire.transactions(0x76);
    TEST_ASSERT_TRUE(humidity.begin());
    TEST_ASSERT_TRUE(pressure.begin());
    TEST_ASSERT_EQUAL_UINT32(afterFirst, Wire.transactions(0x76));
    TEST_ASSERT_TRUE(board.built(SteamiChip::BME280));
}

static void test_temperature_on_v1ab_reads_wsen_hids(void) {
    steamiFakeWsenHids(Wire, 21.5f, 40.0f);
    SteamiTemperature temperature(board);
    SteamiHumidity humidity(board);
    TEST_ASSERT_TRUE(temperature.begin());
    TEST_ASSERT_TRUE(board.release() == SteamiRelease::V1AB);
    TEST_ASSERT_EQUAL_STRING("WSEN-HIDS", temperature.chip());
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 21.5f, temperature.read());
    TEST_ASSERT_TRUE(humidity.begin());
    TEST_ASSERT_EQUAL_STRING("WSEN-HIDS", humidity.chip());
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 40.0f, humidity.read());
}

static void test_temperature_falls_back_to_wsen_pads(void) {
    steamiFakeWsenPads(Wire, 22.0f, 990.0f);
    TEST_ASSERT_TRUE(board.begin());
    TEST_ASSERT_TRUE(board.release() == SteamiRelease::V1AB);
    Wire.setAbsent(0x5F);
    g_lines.clear();
    SteamiTemperature temperature(board);
    SteamiPressure pressure(board);
    TEST_ASSERT_TRUE(temperature.begin());
    TEST_ASSERT_EQUAL_STRING("WSEN-PADS", temperature.chip());
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 22.0f, temperature.read());
    TEST_ASSERT_TRUE(pressure.begin());
    TEST_ASSERT_EQUAL_STRING("WSEN-PADS", pressure.chip());
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 990.0f, pressure.read());
    TEST_ASSERT_FLOAT_WITHIN(0.1f, SteamiBME280::altitudeFromPressure(990.0f, 1013.25f),
                             pressure.altitude());
    TEST_ASSERT_EQUAL_UINT32(0, g_lines.size());
}

static void test_unknown_release_tries_every_candidate(void) {
    TEST_ASSERT_FALSE(board.begin());
    TEST_ASSERT_TRUE(board.release() == SteamiRelease::UNKNOWN);
    TEST_ASSERT_EQUAL_UINT32(1, countLogged("(E01)"));
    steamiFakeWsenPads(Wire, 22.0f, 990.0f);
    SteamiTemperature temperature(board);
    TEST_ASSERT_TRUE(temperature.begin());
    TEST_ASSERT_EQUAL_STRING("WSEN-PADS", temperature.chip());
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 22.0f, temperature.read());
    TEST_ASSERT_EQUAL_UINT32(0, countLogged("(E10)"));
}

// --- Messages ---

static void test_absent_chip_reports_e10_once(void) {
    steamiFakeBme280(Wire);
    TEST_ASSERT_TRUE(board.begin());
    TEST_ASSERT_TRUE(board.release() == SteamiRelease::V1C);
    Wire.setAbsent(0x76);
    SteamiTemperature temperature(board);
    TEST_ASSERT_FALSE(temperature.begin());
    TEST_ASSERT_FALSE(temperature.isAvailable());
    assertStatus(SteamiStatus::NOT_RESPONDING, temperature.status());
    TEST_ASSERT_EQUAL_STRING("", temperature.chip());
    TEST_ASSERT_EQUAL_STRING("no answer", temperature.error());
    TEST_ASSERT_EQUAL_UINT32(1,
                             countLogged("the BME280 temperature sensor (I2C 0x76) did not answer. "
                                         "Check the board or try another one. (E10)"));
    TEST_ASSERT_EQUAL_STRING(
        "[STeaMi] SteamiTemperature: the BME280 temperature sensor (I2C 0x76) did not answer. "
        "Check the board or try another one. (E10)",
        g_lines.back().c_str());
    TEST_ASSERT_FALSE(temperature.begin());
    TEST_ASSERT_EQUAL_UINT32(1, countLogged("(E10)"));
}

static void test_unknown_release_lists_candidates_in_e10(void) {
    SteamiTemperature temperature(board);
    TEST_ASSERT_FALSE(temperature.begin());
    assertStatus(SteamiStatus::NOT_RESPONDING, temperature.status());
    TEST_ASSERT_EQUAL_STRING("no answer", temperature.error());
    TEST_ASSERT_EQUAL_UINT32(1, countLogged("(E01)"));
    TEST_ASSERT_EQUAL_STRING(
        "[STeaMi] SteamiTemperature: no temperature sensor answered (BME280, WSEN-HIDS, "
        "WSEN-PADS). Check the board or try another one. (E10)",
        g_lines.back().c_str());

    SteamiHumidity humidity(board);
    TEST_ASSERT_FALSE(humidity.begin());
    TEST_ASSERT_EQUAL_STRING(
        "[STeaMi] SteamiHumidity: no humidity sensor answered (BME280, WSEN-HIDS). Check the "
        "board or try another one. (E10)",
        g_lines.back().c_str());
}

static void test_several_candidates_on_v1ab_list_in_e10(void) {
    steamiFakeWsenHids(Wire, 21.5f, 40.0f);
    TEST_ASSERT_TRUE(board.begin());
    Wire.setAbsent(0x5F);
    Wire.setAbsent(0x5D);
    SteamiTemperature temperature(board);
    TEST_ASSERT_FALSE(temperature.begin());
    TEST_ASSERT_EQUAL_STRING(
        "[STeaMi] SteamiTemperature: no temperature sensor answered (WSEN-HIDS, WSEN-PADS). Check "
        "the board or try another one. (E10)",
        g_lines.back().c_str());
    SteamiHumidity humidity(board);
    TEST_ASSERT_FALSE(humidity.begin());
    TEST_ASSERT_EQUAL_STRING(
        "[STeaMi] SteamiHumidity: the WSEN-HIDS humidity sensor (I2C 0x5F) did not answer. Check "
        "the board or try another one. (E10)",
        g_lines.back().c_str());
}

static void test_forced_release_contradicted_reports_e11(void) {
    TEST_ASSERT_TRUE(board.begin(SteamiRelease::V1C));
    SteamiTemperature temperature(board);
    TEST_ASSERT_FALSE(temperature.begin());
    assertStatus(SteamiStatus::NOT_RESPONDING, temperature.status());
    TEST_ASSERT_EQUAL_UINT32(0, countLogged("(E10)"));
    TEST_ASSERT_EQUAL_UINT32(1, countLogged("(E11)"));
    TEST_ASSERT_EQUAL_STRING(
        "[STeaMi] SteamiTemperature: this STeaMi is set as a V1c (forced by Steami.begin()), but "
        "its BME280 temperature sensor (I2C 0x76) did not answer. Check the #define, "
        "Steami.begin() or the board config. (E11)",
        g_lines.back().c_str());
}

static void test_config_release_contradicted_reports_e11(void) {
    attachFirmware();
    fw.store("{\"rev\":121}");
    SteamiTemperature temperature(board);
    TEST_ASSERT_FALSE(temperature.begin());
    TEST_ASSERT_TRUE(board.origin() == SteamiOrigin::CONFIG);
    TEST_ASSERT_EQUAL_STRING(
        "[STeaMi] SteamiTemperature: this STeaMi is set as a V1b (read from the board config), "
        "but its WSEN-HIDS temperature sensor (I2C 0x5F) did not answer. Check the #define, "
        "Steami.begin() or the board config. (E11)",
        g_lines.back().c_str());
}

static void test_failed_function_is_silent_and_quiet(void) {
    SteamiTemperature temperature(board);
    SteamiHumidity humidity(board);
    SteamiPressure pressure(board);
    TEST_ASSERT_FALSE(temperature.begin());
    TEST_ASSERT_FALSE(humidity.begin());
    TEST_ASSERT_FALSE(pressure.begin());
    const uint32_t traffic = Wire.totalTransactions();
    const size_t lines = g_lines.size();
    for (int i = 0; i < 10; i++) {
        TEST_ASSERT_TRUE(isnan(temperature.read()));
        TEST_ASSERT_TRUE(isnan(humidity.read()));
        TEST_ASSERT_TRUE(isnan(pressure.read()));
        TEST_ASSERT_TRUE(isnan(pressure.altitude()));
        TEST_ASSERT_FALSE(temperature.begin());
    }
    TEST_ASSERT_EQUAL_UINT32(traffic, Wire.totalTransactions());
    TEST_ASSERT_EQUAL_UINT32(lines, g_lines.size());
}

static void test_two_objects_one_message(void) {
    TEST_ASSERT_TRUE(board.begin(SteamiRelease::V1AB));
    SteamiTemperature first(board);
    SteamiTemperature second(board);
    TEST_ASSERT_FALSE(first.begin());
    TEST_ASSERT_FALSE(second.begin());
    TEST_ASSERT_TRUE(isnan(second.read()));
    TEST_ASSERT_EQUAL_UINT32(1, countLogged("(E11)"));

    board.end();
    g_lines.clear();
    Wire = TwoWire();
    SteamiTemperature third(board);
    SteamiTemperature fourth(board);
    TEST_ASSERT_FALSE(third.begin());
    TEST_ASSERT_FALSE(fourth.begin());
    TEST_ASSERT_EQUAL_UINT32(1, countLogged("(E10)"));
}

// --- Common shape ---

static void test_constructor_touches_nothing(void) {
    SteamiTemperature temperature(board);
    SteamiHumidity humidity(board);
    SteamiPressure pressure(board);
    TEST_ASSERT_EQUAL_UINT32(0, Wire.totalTransactions());
    assertStatus(SteamiStatus::NOT_STARTED, temperature.status());
    TEST_ASSERT_EQUAL_STRING("not started", temperature.error());
    TEST_ASSERT_EQUAL_STRING("", humidity.chip());
    TEST_ASSERT_FALSE(pressure.isAvailable());
}

static void test_read_before_begin_auto_begins(void) {
    steamiFakeBme280(Wire);
    SteamiTemperature temperature(board);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 25.08f, temperature.read());
    assertStatus(SteamiStatus::OK, temperature.status());
    SteamiPressure pressure(board);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, SteamiBME280::altitudeFromPressure(1006.5325f, 1013.25f),
                             pressure.altitude());
    assertStatus(SteamiStatus::OK, pressure.status());
}

static void test_begin_is_idempotent(void) {
    steamiFakeBme280(Wire);
    SteamiTemperature temperature(board);
    TEST_ASSERT_TRUE(temperature.begin());
    const uint32_t traffic = Wire.totalTransactions();
    TEST_ASSERT_TRUE(temperature.begin());
    TEST_ASSERT_EQUAL_UINT32(traffic, Wire.totalTransactions());
}

static void test_set_offset_does_not_touch_shared_driver(void) {
    steamiFakeBme280(Wire);
    SteamiTemperature temperature(board);
    temperature.setOffset(1.5f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 26.58f, temperature.read());
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 25.08f, board.bme280()->temperature());
    SteamiTemperature other(board);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 25.08f, other.read());
}

// --- Calibration ---

static void test_temperature_calibrated_from_config(void) {
    attachFirmware();
    fw.store(kBmeOffsetConfig);
    steamiFakeBme280(Wire);
    SteamiTemperature temperature(board);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 23.08f, temperature.read());
    StringPrint p;
    board.printInfo(p);
    TEST_ASSERT_TRUE(p.text.find("BME280 0x76 OK, calibrated\n") != std::string::npos);
}

static void test_temperature_calibrated_when_forced(void) {
    attachFirmware();
    fw.store(kBmeOffsetConfig);
    steamiFakeBme280(Wire);
    TEST_ASSERT_TRUE(board.begin(SteamiRelease::V1C));
    SteamiTemperature temperature(board);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 23.08f, temperature.read());
}

static void test_hids_calibration_key_hid(void) {
    attachFirmware();
    fw.store("{\"tc\":{\"hid\":{\"g\":1.0,\"o\":0.5}}}");
    steamiFakeWsenHids(Wire, 21.5f, 40.0f);
    SteamiTemperature temperature(board);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 22.0f, temperature.read());
    TEST_ASSERT_TRUE(board.release() == SteamiRelease::V1AB);
    TEST_ASSERT_TRUE(board.calibrated(SteamiChip::WSEN_HIDS));
}

// --- Handle ---

static void test_handle_e20_on_v1c(void) {
    TEST_ASSERT_TRUE(board.begin(SteamiRelease::V1C));
    SteamiHandle handle(board, kGesture);
    TEST_ASSERT_FALSE(handle.begin());
    TEST_ASSERT_FALSE(handle.ready());
    assertStatus(SteamiStatus::NOT_ON_THIS_BOARD, handle.status());
    TEST_ASSERT_EQUAL_STRING("not on this board", handle.error());
    TEST_ASSERT_EQUAL_STRING("", handle.chip());
    TEST_ASSERT_TRUE(handle.chipId() == SteamiChip::NONE);
    TEST_ASSERT_EQUAL_STRING(
        "[STeaMi] SteamiGesture: this STeaMi is a V1c, it has no gesture sensor (APDS-9960 is on "
        "V1a/V1b only). read() will return NONE. (E20)",
        g_lines.back().c_str());
    TEST_ASSERT_EQUAL_UINT32(0, Wire.transactions(0x39));
    TEST_ASSERT_EQUAL_UINT32(1, countLogged("(E20)"));
}

static void test_handle_unknown_board_without_candidate_is_silent(void) {
    TEST_ASSERT_FALSE(board.begin());
    const size_t lines = g_lines.size();
    SteamiHandle handle(board, kScreen);
    TEST_ASSERT_FALSE(handle.begin());
    assertStatus(SteamiStatus::UNKNOWN_BOARD, handle.status());
    TEST_ASSERT_EQUAL_STRING("unknown board", handle.error());
    TEST_ASSERT_EQUAL_UINT32(lines, g_lines.size());
    TEST_ASSERT_EQUAL_UINT32(0, countContaining("SteamiTestScreen"));
}

static void test_handle_ready_begins_on_first_use(void) {
    steamiFakeBme280(Wire);
    static const SteamiFunctionInfo kTemp = {
        SteamiFunction::TEMPERATURE, "SteamiTestTemp", "temperature sensor",
        "Check the board or try another one.", "read() will return NAN."};
    SteamiHandle handle(board, kTemp);
    assertStatus(SteamiStatus::NOT_STARTED, handle.status());
    TEST_ASSERT_TRUE(handle.ready());
    TEST_ASSERT_TRUE(handle.isAvailable());
    TEST_ASSERT_TRUE(handle.chipId() == SteamiChip::BME280);
    TEST_ASSERT_EQUAL_STRING("BME280", handle.chip());
    TEST_ASSERT_TRUE(&handle.board() == &board);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_temperature_on_v1c_reads_bme280);
    RUN_TEST(test_humidity_and_pressure_on_v1c);
    RUN_TEST(test_shared_bme280_begins_once);
    RUN_TEST(test_temperature_on_v1ab_reads_wsen_hids);
    RUN_TEST(test_temperature_falls_back_to_wsen_pads);
    RUN_TEST(test_unknown_release_tries_every_candidate);
    RUN_TEST(test_absent_chip_reports_e10_once);
    RUN_TEST(test_unknown_release_lists_candidates_in_e10);
    RUN_TEST(test_several_candidates_on_v1ab_list_in_e10);
    RUN_TEST(test_forced_release_contradicted_reports_e11);
    RUN_TEST(test_config_release_contradicted_reports_e11);
    RUN_TEST(test_failed_function_is_silent_and_quiet);
    RUN_TEST(test_two_objects_one_message);
    RUN_TEST(test_constructor_touches_nothing);
    RUN_TEST(test_read_before_begin_auto_begins);
    RUN_TEST(test_begin_is_idempotent);
    RUN_TEST(test_set_offset_does_not_touch_shared_driver);
    RUN_TEST(test_temperature_calibrated_from_config);
    RUN_TEST(test_temperature_calibrated_when_forced);
    RUN_TEST(test_hids_calibration_key_hid);
    RUN_TEST(test_handle_e20_on_v1c);
    RUN_TEST(test_handle_unknown_board_without_candidate_is_silent);
    RUN_TEST(test_handle_ready_begins_on_first_use);
    return UNITY_END();
}
