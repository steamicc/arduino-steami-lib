// SPDX-License-Identifier: GPL-3.0-or-later

#include <Arduino.h>
#include <Wire.h>
#include <math.h>
#include <string.h>
#include <unity.h>

#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "FakeDaplinkBridge.h"
#include "SteamiAccelerometer.h"
#include "SteamiBattery.h"
#include "SteamiBoard.h"
#include "SteamiButtons.h"
#include "SteamiColor.h"
#include "SteamiCompass.h"
#include "SteamiDistance.h"
#include "SteamiGesture.h"
#include "SteamiGyroscope.h"
#include "SteamiHandle.h"
#include "SteamiHumidity.h"
#include "SteamiLight.h"
#include "SteamiMagnetometer.h"
#include "SteamiPressure.h"
#include "SteamiTemperature.h"
#include "SteamiTypes.h"
#include "steami_fake_chips.h"
#include "steami_fake_ism330dl.h"
#include "steami_fake_mcp23009.h"

static std::vector<std::string> g_lines;
static void capture(const char* line) {
    g_lines.push_back(line);
}

static SteamiBoard board(Wire, SteamiBoardPins::steami());

void setUp(void) {
    Wire = TwoWire();
    clockHook() = nullptr;
    attachedInterrupts().clear();
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

static void test_config_release_missing_battery_reports_e10(void) {
    attachFirmware();
    fw.store("{\"rev\":131}");
    steamiFakeBme280(Wire);
    Wire.setAbsent(0x55);
    SteamiBattery battery(board);
    TEST_ASSERT_FALSE(battery.begin());
    TEST_ASSERT_TRUE(board.origin() == SteamiOrigin::CONFIG);
    TEST_ASSERT_EQUAL_UINT32(0, countLogged("(E11)"));
    TEST_ASSERT_EQUAL_UINT32(1, countLogged("(E10)"));
    TEST_ASSERT_EQUAL_STRING(
        "[STeaMi] SteamiBattery: the BQ27441 battery gauge (I2C 0x55) did not answer. Plug in a "
        "battery, then reset the board. (E10)",
        g_lines.back().c_str());
}

static void test_config_release_missing_distance_reports_e10(void) {
    attachFirmware();
    fw.store("{\"rev\":131}");
    steamiFakeBme280(Wire);
    Wire.setAbsent(0x29);
    SteamiDistance distance(board);
    TEST_ASSERT_EQUAL_INT(-1, distance.read());
    TEST_ASSERT_EQUAL_UINT32(0, countLogged("(E11)"));
    TEST_ASSERT_EQUAL_STRING(
        "[STeaMi] SteamiDistance: the VL53L1X distance sensor (I2C 0x29) did not answer. Check "
        "the board or try another one. (E10)",
        g_lines.back().c_str());
}

static void test_read_after_board_end_is_safe(void) {
    steamiFakeBme280(Wire);
    SteamiTemperature temperature(board);
    TEST_ASSERT_TRUE(temperature.begin());
    board.end();
    Wire.setAbsent(0x76);
    TEST_ASSERT_TRUE(isnan(temperature.read()));
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

// --- Distance and battery ---

static void test_distance_reads_mm(void) {
    steamiFakeBme280(Wire);
    steamiFakeVl53l1x(Wire);
    SteamiDistance distance(board);
    TEST_ASSERT_TRUE(distance.begin());
    TEST_ASSERT_EQUAL_STRING("VL53L1X", distance.chip());
    steamiFakeVl53l1xDistance(Wire, 123);
    TEST_ASSERT_EQUAL_INT(123, distance.read());
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 12.3f, distance.cm());
}

static void test_distance_zero_is_minus_one(void) {
    steamiFakeBme280(Wire);
    steamiFakeVl53l1x(Wire);
    SteamiDistance distance(board);
    TEST_ASSERT_TRUE(distance.begin());
    steamiFakeVl53l1xDistance(Wire, 0);
    TEST_ASSERT_EQUAL_INT(-1, distance.read());
    TEST_ASSERT_TRUE(isnan(distance.cm()));
}

static void test_distance_absent(void) {
    steamiFakeBme280(Wire);
    Wire.setAbsent(0x29);
    SteamiDistance distance(board);
    TEST_ASSERT_FALSE(distance.begin());
    TEST_ASSERT_EQUAL_UINT32(1, countLogged("(E10)"));
    TEST_ASSERT_EQUAL_STRING(
        "[STeaMi] SteamiDistance: the VL53L1X distance sensor (I2C 0x29) did not answer. Check "
        "the board or try another one. (E10)",
        g_lines.back().c_str());
    const uint32_t traffic = Wire.totalTransactions();
    const size_t lines = g_lines.size();
    for (int i = 0; i < 10; i++) {
        TEST_ASSERT_EQUAL_INT(-1, distance.read());
        TEST_ASSERT_TRUE(isnan(distance.cm()));
    }
    TEST_ASSERT_EQUAL_UINT32(traffic, Wire.totalTransactions());
    TEST_ASSERT_EQUAL_UINT32(lines, g_lines.size());
}

static void test_battery_reads_gauge(void) {
    steamiFakeBme280(Wire);
    steamiFakeBq27441(Wire);
    SteamiBattery battery(board);
    TEST_ASSERT_TRUE(battery.begin());
    steamiFakeBq27441Measurement(Wire, 3900, 250, 87);
    TEST_ASSERT_EQUAL_UINT8(87, battery.percent());
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 3.9f, battery.voltage());
    TEST_ASSERT_EQUAL_INT16(250, battery.current());
    TEST_ASSERT_TRUE(battery.isCharging());
}

static void test_battery_discharging(void) {
    steamiFakeBme280(Wire);
    steamiFakeBq27441(Wire);
    SteamiBattery battery(board);
    TEST_ASSERT_TRUE(battery.begin());
    steamiFakeBq27441Measurement(Wire, 3900, -120, 50);
    TEST_ASSERT_EQUAL_INT16(-120, battery.current());
    TEST_ASSERT_FALSE(battery.isCharging());
}

static void test_battery_absent(void) {
    steamiFakeBme280(Wire);
    Wire.setAbsent(0x55);
    SteamiBattery battery(board);
    TEST_ASSERT_FALSE(battery.begin());
    TEST_ASSERT_EQUAL_UINT32(1, countLogged("(E10)"));
    TEST_ASSERT_EQUAL_STRING(
        "[STeaMi] SteamiBattery: the BQ27441 battery gauge (I2C 0x55) did not answer. Plug in a "
        "battery, then reset the board. (E10)",
        g_lines.back().c_str());
    const uint32_t traffic = Wire.totalTransactions();
    const size_t lines = g_lines.size();
    for (int i = 0; i < 10; i++) {
        TEST_ASSERT_EQUAL_UINT8(0, battery.percent());
        TEST_ASSERT_TRUE(isnan(battery.voltage()));
        TEST_ASSERT_EQUAL_INT16(0, battery.current());
        TEST_ASSERT_FALSE(battery.isCharging());
    }
    TEST_ASSERT_EQUAL_UINT32(traffic, Wire.totalTransactions());
    TEST_ASSERT_EQUAL_UINT32(lines, g_lines.size());
}

// --- Accelerometer and gyroscope ---

static SteamiFakeIsm330dl imu;

static void attachImu(void) {
    imu = SteamiFakeIsm330dl();
    Wire.attachDevice(ISM330DL_DEFAULT_ADDRESS, &imu);
}

static constexpr int16_t kOneG = 4098;  // +-8 g (set by SteamiAccelerometer): 0.244 mg per LSB

static void test_accelerometer_reads_g(void) {
    steamiFakeBme280(Wire);
    attachImu();
    imu.setAccelRaw(0, 0, static_cast<int16_t>(-kOneG));
    SteamiAccelerometer accelerometer(board);
    TEST_ASSERT_TRUE(accelerometer.begin());
    TEST_ASSERT_EQUAL_STRING("ISM330DL", accelerometer.chip());
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -1.0f, accelerometer.z());
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, accelerometer.x());
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, accelerometer.y());
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, accelerometer.strength());
    const SteamiVector v = accelerometer.read();
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -1.0f, v.z);
}

static void test_accelerometer_begin_sets_8g_range(void) {
    steamiFakeBme280(Wire);
    attachImu();
    SteamiGyroscope gyroscope(board);
    TEST_ASSERT_TRUE(gyroscope.begin());
    TEST_ASSERT_EQUAL_UINT8(0, (imu.registers[ISM330DL_REG_CTRL1_XL] >> 2) & 0x03);  // +-2 g
    SteamiAccelerometer accelerometer(board);
    TEST_ASSERT_TRUE(accelerometer.begin());
    TEST_ASSERT_EQUAL_UINT8(3, (imu.registers[ISM330DL_REG_CTRL1_XL] >> 2) & 0x03);  // +-8 g
    TEST_ASSERT_EQUAL_UINT8(4, imu.registers[ISM330DL_REG_CTRL1_XL] >> 4);           // 104 Hz
}

static void test_accelerometer_begin_waits_for_a_sample_at_8g(void) {
    // The output registers keep the last +-2 g sample until the sensor takes
    // the next one (about 10 ms at 104 Hz): begin() waits so that the first
    // read is not converted with the +-8 g scale (4 times too large).
    steamiFakeBme280(Wire);
    attachImu();
    SteamiGyroscope gyroscope(board);  // starts the ISM330DL at +-2 g
    TEST_ASSERT_TRUE(gyroscope.begin());
    SteamiAccelerometer accelerometer(board);
    uint32_t start = millisClock();
    TEST_ASSERT_TRUE(accelerometer.begin());
    TEST_ASSERT_UINT32_WITHIN(5, 20, millisClock() - start);
    start = millisClock();
    accelerometer.begin();
    accelerometer.read();
    TEST_ASSERT_EQUAL_UINT32(0, millisClock() - start);
}

static void test_accelerometer_absent_does_not_wait(void) {
    steamiFakeBme280(Wire);
    Wire.setAbsent(0x6B);
    board.begin();
    SteamiAccelerometer accelerometer(board);
    const uint32_t start = millisClock();
    TEST_ASSERT_FALSE(accelerometer.begin());
    TEST_ASSERT_EQUAL_UINT32(0, millisClock() - start);
}

static void test_accelerometer_is_shaken(void) {
    steamiFakeBme280(Wire);
    attachImu();
    SteamiAccelerometer accelerometer(board);
    TEST_ASSERT_TRUE(accelerometer.begin());
    imu.setAccelRaw(10245, 0, 0);  // 2.5 g at +-8 g
    TEST_ASSERT_TRUE(accelerometer.isShaken());
    TEST_ASSERT_FALSE(accelerometer.isShaken(3.0f));
}

static void test_accelerometer_and_gyroscope_share_chip(void) {
    steamiFakeBme280(Wire);
    attachImu();
    SteamiAccelerometer accelerometer(board);
    SteamiGyroscope gyroscope(board);
    TEST_ASSERT_TRUE(accelerometer.begin());
    TEST_ASSERT_TRUE(gyroscope.begin());
    TEST_ASSERT_EQUAL_INT(1, imu.resetWrites);
}

static void test_gyroscope_reads_dps(void) {
    steamiFakeBme280(Wire);
    attachImu();
    imu.setGyroRaw(1000, 0, 0);
    SteamiGyroscope gyroscope(board);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 8.75f, gyroscope.x());
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 0.0f, gyroscope.y());
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 0.0f, gyroscope.z());
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 8.75f, gyroscope.read().x);
}

static void test_accelerometer_absent(void) {
    steamiFakeBme280(Wire);
    Wire.setAbsent(0x6B);
    SteamiAccelerometer accelerometer(board);
    TEST_ASSERT_FALSE(accelerometer.begin());
    TEST_ASSERT_EQUAL_UINT32(1, countLogged("(E10)"));
    TEST_ASSERT_EQUAL_STRING(
        "[STeaMi] SteamiAccelerometer: the ISM330DL accelerometer (I2C 0x6B) did not answer. "
        "Check the board or try another one. (E10)",
        g_lines.back().c_str());
    const uint32_t traffic = Wire.totalTransactions();
    const size_t lines = g_lines.size();
    for (int i = 0; i < 10; i++) {
        const SteamiVector v = accelerometer.read();
        TEST_ASSERT_TRUE(isnan(v.x) && isnan(v.y) && isnan(v.z));
        TEST_ASSERT_TRUE(isnan(accelerometer.strength()));
        TEST_ASSERT_FALSE(accelerometer.isShaken());
    }
    TEST_ASSERT_EQUAL_UINT32(traffic, Wire.totalTransactions());
    TEST_ASSERT_EQUAL_UINT32(lines, g_lines.size());
}

static void test_accelerometer_calibrated_from_config(void) {
    attachFirmware();
    fw.store("{\"rev\":131,\"ca\":{\"ox\":0.1,\"oy\":0,\"oz\":0}}");
    steamiFakeBme280(Wire);
    attachImu();
    imu.setAccelRaw(kOneG, 0, 0);
    SteamiAccelerometer accelerometer(board);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.9f, accelerometer.x());
}

static void test_motion_read_after_board_end_is_safe(void) {
    steamiFakeBme280(Wire);
    attachImu();
    SteamiAccelerometer accelerometer(board);
    SteamiGyroscope gyroscope(board);
    TEST_ASSERT_TRUE(accelerometer.begin());
    TEST_ASSERT_TRUE(gyroscope.begin());
    board.end();
    Wire.setAbsent(0x6B);
    TEST_ASSERT_TRUE(isnan(accelerometer.x()));
    TEST_ASSERT_TRUE(isnan(gyroscope.z()));
}

// --- Magnetometer and compass ---

static void test_magnetometer_reads_ut(void) {
    steamiFakeBme280(Wire);
    steamiFakeLis2mdl(Wire, 200, -100, 50);
    SteamiMagnetometer magnetometer(board);
    TEST_ASSERT_TRUE(magnetometer.begin());
    TEST_ASSERT_EQUAL_STRING("LIS2MDL", magnetometer.chip());
    const SteamiVector v = magnetometer.read();
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 30.0f, v.x);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -15.0f, v.y);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 7.5f, v.z);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 30.0f, magnetometer.x());
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -15.0f, magnetometer.y());
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 7.5f, magnetometer.z());
    TEST_ASSERT_FLOAT_WITHIN(0.01f, sqrtf(900.0f + 225.0f + 56.25f), magnetometer.strength());
}

static void test_magnetometer_uses_hard_iron_offsets(void) {
    attachFirmware();
    fw.store("{\"rev\":131,\"cm\":{\"hx\":100,\"hy\":-50,\"hz\":0,\"sx\":1,\"sy\":1,\"sz\":1}}");
    steamiFakeBme280(Wire);
    steamiFakeLis2mdl(Wire, 200, -100, 50);
    SteamiMagnetometer magnetometer(board);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 15.0f, magnetometer.x());
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -7.5f, magnetometer.y());
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 7.5f, magnetometer.z());
}

static void test_compass_north_and_east(void) {
    steamiFakeBme280(Wire);
    steamiFakeLis2mdl(Wire, 0, 300, 0);
    SteamiCompass compass(board);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 0.0f, compass.heading());
    TEST_ASSERT_EQUAL_STRING("N", compass.direction());
    steamiFakeLis2mdl(Wire, -300, 0, 0);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 90.0f, compass.heading());
    TEST_ASSERT_EQUAL_STRING("E", compass.direction());
}

static void test_compass_declination_before_begin(void) {
    steamiFakeBme280(Wire);
    steamiFakeLis2mdl(Wire, 0, 300, 0);
    SteamiCompass compass(board);
    compass.setDeclination(10.0f);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 10.0f, compass.heading());
    compass.setDeclination(20.0f);  // already begun: applied at once
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 20.0f, compass.heading());
}

static void test_compass_and_magnetometer_share_chip(void) {
    steamiFakeBme280(Wire);
    steamiFakeLis2mdl(Wire, 0, 300, 0);
    SteamiCompass compass(board);
    SteamiMagnetometer magnetometer(board);
    TEST_ASSERT_TRUE(compass.begin());
    const uint32_t afterFirst = Wire.transactions(0x1E);
    TEST_ASSERT_TRUE(afterFirst > 0);
    TEST_ASSERT_TRUE(magnetometer.begin());
    // A second claim would read WHO_AM_I and configure the chip again: no bus traffic instead.
    TEST_ASSERT_EQUAL_UINT32(afterFirst, Wire.transactions(0x1E));
    TEST_ASSERT_TRUE(board.built(SteamiChip::LIS2MDL));
}

static void test_compass_absent(void) {
    steamiFakeBme280(Wire);
    Wire.setAbsent(0x1E);
    SteamiCompass compass(board);
    SteamiMagnetometer magnetometer(board);
    TEST_ASSERT_FALSE(compass.begin());
    TEST_ASSERT_EQUAL_UINT32(1, countLogged("(E10)"));
    TEST_ASSERT_EQUAL_STRING(
        "[STeaMi] SteamiCompass: the LIS2MDL compass (I2C 0x1E) did not answer. "
        "Check the board or try another one. (E10)",
        g_lines.back().c_str());
    const uint32_t traffic = Wire.totalTransactions();
    const size_t lines = g_lines.size();
    for (int i = 0; i < 10; i++) {
        TEST_ASSERT_TRUE(isnan(compass.heading()));
        TEST_ASSERT_EQUAL_STRING("", compass.direction());
        compass.calibrate(1);
    }
    TEST_ASSERT_EQUAL_UINT32(traffic, Wire.totalTransactions());
    TEST_ASSERT_EQUAL_UINT32(lines, g_lines.size());
    TEST_ASSERT_FALSE(magnetometer.begin());
    TEST_ASSERT_TRUE(isnan(magnetometer.strength()));
    TEST_ASSERT_TRUE(isnan(magnetometer.x()));
}

static void test_magnetometer_and_compass_nan_on_bus_error(void) {
    steamiFakeBme280(Wire);
    steamiFakeLis2mdl(Wire, 200, -100, 50);
    SteamiCompass compass(board);
    SteamiMagnetometer magnetometer(board);
    TEST_ASSERT_TRUE(compass.begin());
    TEST_ASSERT_TRUE(magnetometer.begin());
    // The chip stops answering after begin(): a plausible zero field must not come back.
    Wire.setAbsent(0x1E);
    const SteamiVector v = magnetometer.read();
    TEST_ASSERT_TRUE(isnan(v.x) && isnan(v.y) && isnan(v.z));
    TEST_ASSERT_TRUE(isnan(magnetometer.strength()));
    TEST_ASSERT_TRUE(isnan(compass.heading()));
    TEST_ASSERT_EQUAL_STRING("", compass.direction());
}

// Right edge raised by 20 degrees, top of the screen to magnetic north, field dipping 60 degrees
// (1000 LSB). ISM330DL at +-8 g: (0, -sin 20, -cos 20) g. LIS2MDL: x = R, y = T,
// z = LIS2MDL_MAG_Z_SIGN * U.
static void setRollNorth(void) {
    imu.setAccelRaw(0, static_cast<int16_t>(lroundf(-0.34202f * kOneG)),
                    static_cast<int16_t>(lroundf(-0.93969f * kOneG)));
    steamiFakeLis2mdl(Wire, -296, 500, static_cast<int16_t>(lroundf(LIS2MDL_MAG_Z_SIGN * -814.0f)));
}

static void test_compass_tilt_heading_removes_roll_error(void) {
    steamiFakeBme280(Wire);
    attachImu();
    setRollNorth();
    SteamiCompass compass(board);
    TEST_ASSERT_TRUE(compass.begin());
    const float tilt = compass.headingTiltCompensated();
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 0.0f, tilt > 180.0f ? tilt - 360.0f : tilt);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 30.6f, compass.heading());  // flat heading, about 30.6
}

static void test_compass_tilt_heading_equals_heading_when_flat(void) {
    steamiFakeBme280(Wire);
    attachImu();
    imu.setAccelRaw(0, 0, static_cast<int16_t>(-kOneG));
    steamiFakeLis2mdl(Wire, 300, -150, 450);
    SteamiCompass compass(board);
    compass.setDeclination(10.0f);
    const float flat = compass.heading();
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 253.43f, flat);
    TEST_ASSERT_TRUE(flat == compass.headingTiltCompensated());
}

static void test_compass_tilt_heading_nan_without_accelerometer(void) {
    steamiFakeBme280(Wire);
    Wire.setAbsent(0x6B);
    steamiFakeLis2mdl(Wire, 0, 300, 0);
    SteamiCompass compass(board);
    TEST_ASSERT_TRUE(compass.begin());
    TEST_ASSERT_TRUE(isnan(compass.headingTiltCompensated()));
    TEST_ASSERT_TRUE(isnan(compass.headingTiltCompensated()));
    TEST_ASSERT_EQUAL_UINT32(1, countContaining("SteamiAccelerometer"));
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 0.0f, compass.heading());  // the flat heading still works
}

static void test_compass_tilt_heading_nan_when_compass_absent(void) {
    steamiFakeBme280(Wire);
    attachImu();
    imu.setAccelRaw(0, 0, static_cast<int16_t>(-kOneG));
    Wire.setAbsent(0x1E);
    SteamiCompass compass(board);
    TEST_ASSERT_TRUE(isnan(compass.headingTiltCompensated()));
}

static void test_compass_tilt_heading_nan_on_bus_error(void) {
    steamiFakeBme280(Wire);
    attachImu();
    setRollNorth();
    SteamiCompass compass(board);
    TEST_ASSERT_FALSE(isnan(compass.headingTiltCompensated()));
    // The accelerometer stops answering: no heading from a zero gravity vector.
    Wire.setAbsent(0x6B);
    TEST_ASSERT_TRUE(isnan(compass.headingTiltCompensated()));
    TEST_ASSERT_FALSE(isnan(compass.heading()));
    // Then the compass too.
    Wire.setAbsent(0x1E);
    TEST_ASSERT_TRUE(isnan(compass.headingTiltCompensated()));
}

static const char* const kCmConfig =
    "{\"rev\":131,\"cm\":{\"hx\":100,\"hy\":0,\"hz\":0,\"sx\":1,\"sy\":1,\"sz\":1}}";

static void test_compass_heading_uses_cm(void) {
    attachFirmware();
    fw.store(kCmConfig);
    steamiFakeBme280(Wire);
    steamiFakeLis2mdl(Wire, 0, 300, 0);
    SteamiCompass compass(board);
    // atan2(-(0 - 100), 300) = 18.43 degrees.
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 18.43f, compass.heading());
}

static void test_motion_calibrations_when_forced(void) {
    attachFirmware();
    fw.store(
        "{\"rev\":131,\"ca\":{\"ox\":0.1,\"oy\":0,\"oz\":0},"
        "\"cm\":{\"hx\":100,\"hy\":0,\"hz\":0,\"sx\":1,\"sy\":1,\"sz\":1}}");
    steamiFakeBme280(Wire);
    attachImu();
    imu.setAccelRaw(kOneG, 0, 0);
    steamiFakeLis2mdl(Wire, 0, 300, 0);
    TEST_ASSERT_TRUE(board.begin(SteamiRelease::V1C));
    SteamiAccelerometer accelerometer(board);
    SteamiCompass compass(board);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.9f, accelerometer.x());
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 18.43f, compass.heading());
}

static void test_calibrate_zero_changes_nothing(void) {
    attachFirmware();
    fw.store(kCmConfig);
    steamiFakeBme280(Wire);
    steamiFakeLis2mdl(Wire, 0, 300, 0);
    SteamiCompass compass(board);
    SteamiMagnetometer magnetometer(board);
    const float before = compass.heading();
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 18.43f, before);
    const float x = magnetometer.x();
    compass.calibrate(0);
    magnetometer.calibrate(0);
    const SteamiLIS2MDL* mag = board.lis2mdl();
    TEST_ASSERT_NOT_NULL(mag);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 100.0f, mag->xOff);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, mag->xScale);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, mag->yScale);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, before, compass.heading());
    TEST_ASSERT_FLOAT_WITHIN(0.01f, x, magnetometer.x());
}

// LIS2MDL model whose field changes at a given time of the simulated clock, so that a
// calibration sees two fields. Same register model as the ISM330DL fake.
class SteamiSweepLis2mdl : public I2CDevice {
   public:
    SteamiSweepLis2mdl() {
        registers[LIS2MDL_WHO_AM_I] = LIS2MDL_WHO_AM_I_VAL;
        registers[LIS2MDL_STATUS_REG] = 0x08;  // ZYXDA
    }

    uint8_t onWrite(const std::vector<uint8_t>& frame) override {
        if (frame.empty()) {
            return 0;
        }
        pointer = frame[0] & MASK;
        for (size_t i = 1; i < frame.size(); i++) {
            registers[pointer] = frame[i];
            pointer = (pointer + 1) & MASK;
        }
        return 0;
    }

    std::vector<uint8_t> onRead(size_t quantity) override {
        std::vector<uint8_t> out;
        for (size_t i = 0; i < quantity; i++) {
            out.push_back(read(pointer));
            pointer = (pointer + 1) & MASK;
        }
        return out;
    }

    int16_t before[3] = {};
    int16_t after[3] = {};
    uint32_t switchAt = UINT32_MAX;

   private:
    static constexpr uint8_t MASK = 0x7F;

    uint8_t read(uint8_t reg) const {
        if (reg < LIS2MDL_OUTX_L_REG || reg > LIS2MDL_OUTZ_H_REG) {
            return registers[reg];
        }
        const int16_t* field = millis() < switchAt ? before : after;
        const uint8_t offset = reg - LIS2MDL_OUTX_L_REG;
        const uint16_t bits = static_cast<uint16_t>(field[offset / 2]);
        return (offset % 2 == 0) ? (bits & 0xFF) : ((bits >> 8) & 0xFF);
    }

    uint8_t registers[MASK + 1] = {};
    uint8_t pointer = 0;
};

static SteamiSweepLis2mdl sweep;

static void test_calibrate_updates_offsets(void) {
    steamiFakeBme280(Wire);
    sweep = SteamiSweepLis2mdl();
    sweep.before[0] = 200;
    sweep.before[1] = 100;
    sweep.after[0] = 0;
    sweep.after[1] = -300;
    Wire.attachDevice(LIS2MDL_I2C_ADDR, &sweep);
    SteamiMagnetometer magnetometer(board);
    TEST_ASSERT_TRUE(magnetometer.begin());
    const uint32_t start = millis();
    sweep.switchAt = start + 500;
    magnetometer.calibrate(1);  // 50 samples, 20 ms apart
    TEST_ASSERT_TRUE(millis() - start >= 1000);
    const SteamiLIS2MDL* mag = board.lis2mdl();
    TEST_ASSERT_NOT_NULL(mag);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 100.0f, mag->xOff);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -100.0f, mag->yOff);
    SteamiCompass compass(board);
    sweep.switchAt = millis() + 500;
    compass.calibrate(1);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 100.0f, mag->xOff);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -100.0f, mag->yOff);
}

// --- Light, colour and gesture (APDS-9960, V1a/V1b only) ---

static void test_light_reads_clear(void) {
    steamiFakeApds9960(Wire, 1234, 100, 200, 300);
    SteamiLight light(board);
    TEST_ASSERT_TRUE(light.begin());
    TEST_ASSERT_EQUAL_STRING("APDS-9960", light.chip());
    TEST_ASSERT_EQUAL_UINT16(1234, light.read());
}

static void test_color_reads_rgb(void) {
    steamiFakeApds9960(Wire, 1234, 100, 200, 300);
    SteamiColor color(board);
    TEST_ASSERT_EQUAL_UINT16(100, color.red());
    TEST_ASSERT_EQUAL_UINT16(200, color.green());
    TEST_ASSERT_EQUAL_UINT16(300, color.blue());
    uint16_t r = 1, g = 1, b = 1;
    TEST_ASSERT_TRUE(color.read(r, g, b));
    TEST_ASSERT_EQUAL_UINT16(100, r);
    TEST_ASSERT_EQUAL_UINT16(200, g);
    TEST_ASSERT_EQUAL_UINT16(300, b);
}

static void test_light_color_gesture_share_chip(void) {
    steamiFakeApds9960(Wire, 10, 20, 30, 40);
    SteamiLight light(board);
    SteamiColor color(board);
    SteamiGesture gesture(board);
    TEST_ASSERT_TRUE(light.begin());
    const uint32_t afterFirst = Wire.transactions(0x39);
    TEST_ASSERT_TRUE(afterFirst > 0);
    TEST_ASSERT_TRUE(color.begin());
    TEST_ASSERT_EQUAL_UINT32(afterFirst, Wire.transactions(0x39));
    TEST_ASSERT_TRUE(board.built(SteamiChip::APDS9960));
    TEST_ASSERT_TRUE(gesture.begin());
    TEST_ASSERT_TRUE(board.built(SteamiChip::APDS9960));
}

static void test_gesture_begin_enables_engine(void) {
    steamiFakeApds9960(Wire, 10, 20, 30, 40);
    SteamiGesture gesture(board);
    TEST_ASSERT_TRUE(gesture.begin());
    const uint8_t enable = Wire.getRegister(0x39, 0x80);
    TEST_ASSERT_EQUAL_UINT8(APDS9960_ENABLE_PON | APDS9960_ENABLE_GEN,
                            enable & (APDS9960_ENABLE_PON | APDS9960_ENABLE_GEN));
    // Idempotent: a second begin() does not touch the chip again.
    const uint32_t traffic = Wire.transactions(0x39);
    TEST_ASSERT_TRUE(gesture.begin());
    TEST_ASSERT_EQUAL_UINT32(traffic, Wire.transactions(0x39));
}

static void test_gesture_none_without_motion(void) {
    steamiFakeApds9960(Wire, 10, 20, 30, 40);
    SteamiGesture gesture(board);
    TEST_ASSERT_TRUE(gesture.read() == SteamiGesture::NONE);
    TEST_ASSERT_TRUE(gesture.read(5) == SteamiGesture::NONE);
}

static void test_gesture_names(void) {
    TEST_ASSERT_EQUAL_STRING("NONE", SteamiGesture::name(SteamiGesture::NONE));
    TEST_ASSERT_EQUAL_STRING("LEFT", SteamiGesture::name(SteamiGesture::LEFT));
    TEST_ASSERT_EQUAL_STRING("RIGHT", SteamiGesture::name(SteamiGesture::RIGHT));
    TEST_ASSERT_EQUAL_STRING("UP", SteamiGesture::name(SteamiGesture::UP));
    TEST_ASSERT_EQUAL_STRING("DOWN", SteamiGesture::name(SteamiGesture::DOWN));
    TEST_ASSERT_EQUAL_STRING("NEAR", SteamiGesture::name(SteamiGesture::NEAR));
    TEST_ASSERT_EQUAL_STRING("FAR", SteamiGesture::name(SteamiGesture::FAR));
}

static void test_apds_classes_e20_on_v1c(void) {
    steamiFakeBme280(Wire);
    SteamiLight light(board);
    SteamiColor color(board);
    SteamiGesture gesture(board);
    TEST_ASSERT_FALSE(light.begin());
    TEST_ASSERT_FALSE(color.begin());
    TEST_ASSERT_FALSE(gesture.begin());
    assertStatus(SteamiStatus::NOT_ON_THIS_BOARD, light.status());
    assertStatus(SteamiStatus::NOT_ON_THIS_BOARD, color.status());
    assertStatus(SteamiStatus::NOT_ON_THIS_BOARD, gesture.status());
    // The board prints its own detection line first; the three E20 lines follow it.
    TEST_ASSERT_EQUAL_UINT32(4, g_lines.size());
    TEST_ASSERT_EQUAL_UINT32(3, countLogged("(E20)"));
    TEST_ASSERT_EQUAL_STRING(
        "[STeaMi] SteamiLight: this STeaMi is a V1c, it has no light sensor (APDS-9960 is on "
        "V1a/V1b only). read() will return 0. (E20)",
        g_lines[1].c_str());
    TEST_ASSERT_EQUAL_STRING(
        "[STeaMi] SteamiColor: this STeaMi is a V1c, it has no colour sensor (APDS-9960 is on "
        "V1a/V1b only). read() will return false. (E20)",
        g_lines[2].c_str());
    TEST_ASSERT_EQUAL_STRING(
        "[STeaMi] SteamiGesture: this STeaMi is a V1c, it has no gesture sensor (APDS-9960 is on "
        "V1a/V1b only). read() will return NONE. (E20)",
        g_lines[3].c_str());
    TEST_ASSERT_EQUAL_UINT16(0, light.read());
    uint16_t r = 7, g = 7, b = 7;
    TEST_ASSERT_FALSE(color.read(r, g, b));
    TEST_ASSERT_EQUAL_UINT16(0, r);
    TEST_ASSERT_EQUAL_UINT16(0, g);
    TEST_ASSERT_EQUAL_UINT16(0, b);
    TEST_ASSERT_EQUAL_UINT16(0, color.red());
    TEST_ASSERT_TRUE(gesture.read() == SteamiGesture::NONE);
    TEST_ASSERT_EQUAL_UINT32(4, g_lines.size());
    TEST_ASSERT_EQUAL_UINT32(0, Wire.transactions(0x39));
}

static void test_apds_forced_v1ab_absent_is_e11(void) {
    TEST_ASSERT_TRUE(board.begin(SteamiRelease::V1B));
    Wire.setAbsent(0x39);
    SteamiColor color(board);
    TEST_ASSERT_FALSE(color.begin());
    const size_t lines = g_lines.size();
    TEST_ASSERT_EQUAL_UINT32(1, countLogged("(E11)"));
    const std::string& line = g_lines.back();
    TEST_ASSERT_TRUE(line.find("APDS-9960 colour sensor (I2C 0x39)") != std::string::npos);
    const uint32_t traffic = Wire.totalTransactions();
    uint16_t r = 1, g = 1, b = 1;
    TEST_ASSERT_FALSE(color.read(r, g, b));
    TEST_ASSERT_EQUAL_UINT32(traffic, Wire.totalTransactions());
    TEST_ASSERT_EQUAL_UINT32(lines, g_lines.size());
}

// --- Buttons ---

static void releaseMcuButtons(void) {
    gpioPinState()[A_BUTTON] = HIGH;
    gpioPinState()[B_BUTTON] = HIGH;
    gpioPinState()[MENU_BUTTON] = HIGH;
}

static void setDpad(uint8_t gpio) {
    Wire.setRegister(MCP23009_I2C_ADDR, MCP23009_GPIO, gpio);
}

// Model of the expander with its interrupt latch (INTF, INTCAP), for the D-PAD taps.
static FakeMCP23009 expanderChip;

static void attachExpander(void) {
    expanderChip.reset();
    Wire.attachDevice(MCP23009_I2C_ADDR, &expanderChip);
}

// Moves an MCU button pin and runs its interrupt, as the edge would on the board.
static void setButtonPin(int pin, int level) {
    gpioPinState()[pin] = level;
    fireInterrupt(pin);
}

static void test_buttons_read_a_and_dpad(void) {
    steamiFakeBme280(Wire);
    steamiFakeMcp23009e(Wire, 0xF0);
    releaseMcuButtons();
    SteamiButtons buttons(board);
    TEST_ASSERT_TRUE(buttons.begin());
    TEST_ASSERT_FALSE(buttons.isPressed(SteamiButton::A));
    gpioPinState()[A_BUTTON] = LOW;
    setDpad(0xF0 & ~(1 << MCP23009_BTN_UP));
    TEST_ASSERT_TRUE(buttons.isPressed(SteamiButton::A));  // seen on the first call
    delay(30);
    TEST_ASSERT_TRUE(buttons.isPressed(SteamiButton::A));
    TEST_ASSERT_TRUE(buttons.isPressed(SteamiButton::UP));
    TEST_ASSERT_FALSE(buttons.isPressed(SteamiButton::B));
    TEST_ASSERT_FALSE(buttons.isPressed(SteamiButton::DOWN));
    TEST_ASSERT_FALSE(buttons.isPressed(SteamiButton::NONE));
}

static void test_was_pressed_once_per_press(void) {
    steamiFakeBme280(Wire);
    steamiFakeMcp23009e(Wire, 0xF0);
    releaseMcuButtons();
    SteamiButtons buttons(board);
    TEST_ASSERT_TRUE(buttons.begin());
    buttons.isPressed(SteamiButton::A);
    gpioPinState()[A_BUTTON] = LOW;
    int count = 0;
    for (int t = 0; t < 200; t += 10) {
        if (buttons.wasPressed(SteamiButton::A)) {
            count++;
        }
        delay(10);
    }
    TEST_ASSERT_EQUAL_INT(1, count);
    TEST_ASSERT_TRUE(buttons.isPressed(SteamiButton::A));
}

static void test_was_released_after_release(void) {
    steamiFakeBme280(Wire);
    steamiFakeMcp23009e(Wire, 0xF0);
    releaseMcuButtons();
    SteamiButtons buttons(board);
    TEST_ASSERT_TRUE(buttons.begin());
    gpioPinState()[A_BUTTON] = LOW;
    buttons.isPressed(SteamiButton::A);
    delay(30);
    TEST_ASSERT_TRUE(buttons.isPressed(SteamiButton::A));
    TEST_ASSERT_FALSE(buttons.wasReleased(SteamiButton::A));
    gpioPinState()[A_BUTTON] = HIGH;
    buttons.isPressed(SteamiButton::A);
    delay(30);
    TEST_ASSERT_TRUE(buttons.wasReleased(SteamiButton::A));
    TEST_ASSERT_FALSE(buttons.wasReleased(SteamiButton::A));
    TEST_ASSERT_FALSE(buttons.isPressed(SteamiButton::A));
}

// Lock-out debounce: a change is taken at once, then the bounces of the next 20 ms are ignored.
static void test_bounces_after_a_press_give_one_press(void) {
    steamiFakeBme280(Wire);
    steamiFakeMcp23009e(Wire, 0xF0);
    releaseMcuButtons();
    SteamiButtons buttons(board);
    TEST_ASSERT_TRUE(buttons.begin());
    TEST_ASSERT_FALSE(buttons.isPressed(SteamiButton::A));
    delay(50);
    int presses = 0;
    int releases = 0;
    const int bounce[] = {LOW, HIGH, LOW, HIGH, LOW};
    for (int level : bounce) {  // 0, 3, 6, 9 and 12 ms after the press
        gpioPinState()[A_BUTTON] = level;
        presses += buttons.wasPressed(SteamiButton::A) ? 1 : 0;
        releases += buttons.wasReleased(SteamiButton::A) ? 1 : 0;
        delay(3);
    }
    for (int t = 0; t < 200; t += 10) {  // held
        presses += buttons.wasPressed(SteamiButton::A) ? 1 : 0;
        releases += buttons.wasReleased(SteamiButton::A) ? 1 : 0;
        delay(10);
    }
    TEST_ASSERT_EQUAL_INT(1, presses);
    TEST_ASSERT_EQUAL_INT(0, releases);
    TEST_ASSERT_TRUE(buttons.isPressed(SteamiButton::A));
    const int releaseBounce[] = {HIGH, LOW, HIGH, LOW, HIGH};
    for (int level : releaseBounce) {
        gpioPinState()[A_BUTTON] = level;
        presses += buttons.wasPressed(SteamiButton::A) ? 1 : 0;
        releases += buttons.wasReleased(SteamiButton::A) ? 1 : 0;
        delay(3);
    }
    for (int t = 0; t < 100; t += 10) {
        presses += buttons.wasPressed(SteamiButton::A) ? 1 : 0;
        releases += buttons.wasReleased(SteamiButton::A) ? 1 : 0;
        delay(10);
    }
    TEST_ASSERT_EQUAL_INT(1, presses);
    TEST_ASSERT_EQUAL_INT(1, releases);
    TEST_ASSERT_FALSE(buttons.isPressed(SteamiButton::A));
}

// A 100 ms tap in a loop that calls the buttons every 200 ms: one call falls inside the tap.
static void test_tap_caught_by_a_single_sample(void) {
    steamiFakeBme280(Wire);
    steamiFakeMcp23009e(Wire, 0xF0);
    releaseMcuButtons();
    SteamiButtons buttons(board);
    TEST_ASSERT_TRUE(buttons.begin());
    TEST_ASSERT_FALSE(buttons.wasPressed(SteamiButton::A));
    delay(150);
    gpioPinState()[A_BUTTON] = LOW;
    delay(50);
    TEST_ASSERT_TRUE(buttons.wasPressed(SteamiButton::A));  // the only call during the tap
    delay(50);
    gpioPinState()[A_BUTTON] = HIGH;
    delay(150);
    TEST_ASSERT_TRUE(buttons.wasReleased(SteamiButton::A));
    TEST_ASSERT_FALSE(buttons.wasPressed(SteamiButton::A));
}

static void test_dpad_bus_error_reads_released(void) {
    steamiFakeBme280(Wire);
    steamiFakeMcp23009e(Wire, 0xF0 & ~(1 << MCP23009_BTN_UP));
    releaseMcuButtons();
    SteamiButtons buttons(board);
    TEST_ASSERT_TRUE(buttons.begin());
    buttons.isPressed(SteamiButton::UP);
    delay(30);
    TEST_ASSERT_TRUE(buttons.isPressed(SteamiButton::UP));
    Wire.setAbsent(MCP23009_I2C_ADDR);
    gpioPinState()[A_BUTTON] = LOW;
    buttons.isPressed(SteamiButton::UP);
    delay(30);
    TEST_ASSERT_FALSE(buttons.isPressed(SteamiButton::UP));
    TEST_ASSERT_FALSE(buttons.isPressed(SteamiButton::DOWN));
    TEST_ASSERT_FALSE(buttons.isPressed(SteamiButton::LEFT));
    TEST_ASSERT_FALSE(buttons.isPressed(SteamiButton::RIGHT));
    TEST_ASSERT_TRUE(buttons.isPressed(SteamiButton::A));
}

static void test_buttons_without_expander(void) {
    steamiFakeBme280(Wire);
    Wire.setAbsent(MCP23009_I2C_ADDR);
    releaseMcuButtons();
    SteamiButtons buttons(board);
    TEST_ASSERT_FALSE(buttons.begin());
    TEST_ASSERT_EQUAL_UINT32(1, countLogged("(E10)"));
    TEST_ASSERT_EQUAL_STRING(
        "[STeaMi] SteamiButtons: the MCP23009E D-PAD expander (I2C 0x20) did not answer. "
        "The A, B and MENU buttons still work. (E10)",
        g_lines.back().c_str());
    gpioPinState()[A_BUTTON] = LOW;
    buttons.isPressed(SteamiButton::A);
    delay(30);
    TEST_ASSERT_TRUE(buttons.isPressed(SteamiButton::A));
    TEST_ASSERT_FALSE(buttons.isPressed(SteamiButton::UP));
    const size_t lines = g_lines.size();
    TEST_ASSERT_FALSE(buttons.begin());
    TEST_ASSERT_EQUAL_UINT32(lines, g_lines.size());
}

static void test_wait_for_press_timeout(void) {
    steamiFakeBme280(Wire);
    steamiFakeMcp23009e(Wire, 0xF0);
    releaseMcuButtons();
    SteamiButtons buttons(board);
    TEST_ASSERT_TRUE(buttons.begin());
    const uint32_t start = millis();
    TEST_ASSERT_TRUE(buttons.waitForPress(100) == SteamiButton::NONE);
    const uint32_t elapsed = millis() - start;
    TEST_ASSERT_TRUE(elapsed >= 100 && elapsed <= 110);
}

// Every sample of the wait reads the expander: its read hook presses B during the wait.
static void test_wait_for_press_returns_button(void) {
    steamiFakeBme280(Wire);
    attachExpander();
    releaseMcuButtons();
    SteamiButtons buttons(board);
    TEST_ASSERT_TRUE(buttons.begin());
    buttons.isPressed(SteamiButton::B);
    delay(50);
    int reads = 0;
    expanderChip.onReadHook = [&reads](uint8_t reg) {
        if (reg == MCP23009_GPIO && ++reads == 5) {
            setButtonPin(B_BUTTON, LOW);
        }
    };
    const uint32_t start = millis();
    TEST_ASSERT_TRUE(buttons.waitForPress(500) == SteamiButton::B);
    expanderChip.onReadHook = nullptr;
    TEST_ASSERT_TRUE(millis() - start < 500);
    TEST_ASSERT_FALSE(buttons.wasPressed(SteamiButton::B));
}

static void test_wait_for_press_drops_a_latched_press(void) {
    steamiFakeBme280(Wire);
    steamiFakeMcp23009e(Wire, 0xF0);
    releaseMcuButtons();
    SteamiButtons buttons(board);
    TEST_ASSERT_TRUE(buttons.begin());
    TEST_ASSERT_FALSE(buttons.isPressed(SteamiButton::A));
    delay(50);
    gpioPinState()[A_BUTTON] = LOW;
    TEST_ASSERT_TRUE(buttons.isPressed(SteamiButton::A));  // latched before the call
    TEST_ASSERT_TRUE(buttons.waitForPress(100) == SteamiButton::NONE);
}

static void test_button_held_at_start_is_not_a_press(void) {
    steamiFakeBme280(Wire);
    steamiFakeMcp23009e(Wire, 0xF0);
    releaseMcuButtons();
    gpioPinState()[A_BUTTON] = LOW;
    SteamiButtons buttons(board);
    TEST_ASSERT_TRUE(buttons.begin());
    TEST_ASSERT_FALSE(buttons.wasPressed(SteamiButton::A));
    delay(50);
    TEST_ASSERT_FALSE(buttons.wasPressed(SteamiButton::A));
    TEST_ASSERT_TRUE(buttons.waitForPress(100) == SteamiButton::NONE);
}

// --- Buttons: taps between two calls (issue #251) ---

static void test_buttons_attach_change_interrupts_on_a_b_menu(void) {
    steamiFakeBme280(Wire);
    attachExpander();
    releaseMcuButtons();
    SteamiButtons buttons(board);
    TEST_ASSERT_TRUE(buttons.begin());
    const int pins[] = {A_BUTTON, B_BUTTON, MENU_BUTTON};
    for (int pin : pins) {
        auto it = attachedInterrupts().find(pin);
        TEST_ASSERT_TRUE(it != attachedInterrupts().end());
        TEST_ASSERT_EQUAL_INT(CHANGE, it->second.mode);
    }
    // MENU (PA0) and INT_EXPANDER (PB0) share EXTI line 0: the line goes to MENU.
    TEST_ASSERT_TRUE(attachedInterrupts().find(INT_EXPANDER) == attachedInterrupts().end());
    TEST_ASSERT_EQUAL_UINT32(3, attachedInterrupts().size());
}

static void test_mcu_tap_between_two_calls_is_caught(void) {
    steamiFakeBme280(Wire);
    attachExpander();
    releaseMcuButtons();
    SteamiButtons buttons(board);
    TEST_ASSERT_TRUE(buttons.begin());
    const int pins[] = {A_BUTTON, B_BUTTON, MENU_BUTTON};
    const SteamiButton names[] = {SteamiButton::A, SteamiButton::B, SteamiButton::MENU};
    for (int k = 0; k < 3; k++) {
        TEST_ASSERT_FALSE(buttons.wasPressed(names[k]));
        delay(150);
        setButtonPin(pins[k], LOW);  // a 100 ms tap between two calls 300 ms apart
        delay(100);
        setButtonPin(pins[k], HIGH);
        delay(50);
        TEST_ASSERT_FALSE(buttons.isPressed(names[k]));
        TEST_ASSERT_TRUE(buttons.wasPressed(names[k]));
        TEST_ASSERT_TRUE(buttons.wasReleased(names[k]));
        TEST_ASSERT_FALSE(buttons.wasPressed(names[k]));
        TEST_ASSERT_FALSE(buttons.wasReleased(names[k]));
    }
}

// Each edge of a bounce runs the interrupt: the lock-out keeps one press and one release.
static void test_mcu_bounces_between_two_calls_give_one_press(void) {
    steamiFakeBme280(Wire);
    attachExpander();
    releaseMcuButtons();
    SteamiButtons buttons(board);
    TEST_ASSERT_TRUE(buttons.begin());
    TEST_ASSERT_FALSE(buttons.isPressed(SteamiButton::A));
    delay(50);
    const int pressBounce[] = {LOW, HIGH, LOW, HIGH, LOW};
    for (int level : pressBounce) {
        setButtonPin(A_BUTTON, level);
        delay(1);
    }
    delay(100);
    const int releaseBounce[] = {HIGH, LOW, HIGH, LOW, HIGH};
    for (int level : releaseBounce) {
        setButtonPin(A_BUTTON, level);
        delay(2);
    }
    delay(200);
    int presses = 0;
    int releases = 0;
    for (int t = 0; t < 5; t++) {
        presses += buttons.wasPressed(SteamiButton::A) ? 1 : 0;
        releases += buttons.wasReleased(SteamiButton::A) ? 1 : 0;
        delay(30);
    }
    TEST_ASSERT_EQUAL_INT(1, presses);
    TEST_ASSERT_EQUAL_INT(1, releases);
    TEST_ASSERT_FALSE(buttons.isPressed(SteamiButton::A));
}

// A bounce 2 ms after a release the interrupt took is not a press.
static void test_mcu_bounce_after_a_release_is_not_a_press(void) {
    steamiFakeBme280(Wire);
    attachExpander();
    releaseMcuButtons();
    SteamiButtons buttons(board);
    TEST_ASSERT_TRUE(buttons.begin());
    TEST_ASSERT_FALSE(buttons.isPressed(SteamiButton::B));
    delay(50);
    setButtonPin(B_BUTTON, LOW);
    delay(50);
    TEST_ASSERT_TRUE(buttons.wasPressed(SteamiButton::B));
    delay(100);
    setButtonPin(B_BUTTON, HIGH);
    delay(2);
    setButtonPin(B_BUTTON, LOW);
    delay(1);
    setButtonPin(B_BUTTON, HIGH);
    delay(100);
    TEST_ASSERT_TRUE(buttons.wasReleased(SteamiButton::B));
    TEST_ASSERT_FALSE(buttons.wasPressed(SteamiButton::B));
    TEST_ASSERT_FALSE(buttons.isPressed(SteamiButton::B));
}

// wasPressed() and wasReleased() stay flags: two taps between two calls give one press and one
// release. getPresses() counts them (issue #341).
static void test_two_mcu_taps_between_two_calls_give_one_press_and_count_two(void) {
    steamiFakeBme280(Wire);
    attachExpander();
    releaseMcuButtons();
    SteamiButtons buttons(board);
    TEST_ASSERT_TRUE(buttons.begin());
    TEST_ASSERT_FALSE(buttons.isPressed(SteamiButton::MENU));
    delay(50);
    for (int k = 0; k < 2; k++) {
        setButtonPin(MENU_BUTTON, LOW);
        delay(60);
        setButtonPin(MENU_BUTTON, HIGH);
        delay(60);
    }
    TEST_ASSERT_TRUE(buttons.wasPressed(SteamiButton::MENU));
    TEST_ASSERT_FALSE(buttons.wasPressed(SteamiButton::MENU));
    TEST_ASSERT_TRUE(buttons.wasReleased(SteamiButton::MENU));
    TEST_ASSERT_FALSE(buttons.wasReleased(SteamiButton::MENU));
    TEST_ASSERT_EQUAL_UINT8(2, buttons.getPresses(SteamiButton::MENU));
    TEST_ASSERT_EQUAL_UINT8(0, buttons.getPresses(SteamiButton::MENU));
    TEST_ASSERT_EQUAL_UINT8(0, buttons.getPresses(SteamiButton::NONE));
}

// Presses a sketch does not read are not replayed later by wasPressed(): a sketch whose mode 1
// ignores B and whose mode 2 runs an action on wasPressed(B) runs it once, not 30 times.
// getPresses() still has them all, and counts on its own, whatever wasPressed() took.
static void test_unread_presses_are_not_replayed_by_was_pressed(void) {
    steamiFakeBme280(Wire);
    attachExpander();
    releaseMcuButtons();
    SteamiButtons buttons(board);
    TEST_ASSERT_TRUE(buttons.begin());
    TEST_ASSERT_FALSE(buttons.isPressed(SteamiButton::A));
    delay(50);
    for (int k = 0; k < 30; k++) {
        setButtonPin(B_BUTTON, LOW);
        delay(80);
        setButtonPin(B_BUTTON, HIGH);
        delay(120);
        TEST_ASSERT_FALSE(buttons.wasPressed(SteamiButton::A));
    }
    TEST_ASSERT_TRUE(buttons.wasPressed(SteamiButton::B));
    TEST_ASSERT_FALSE(buttons.wasPressed(SteamiButton::B));
    TEST_ASSERT_EQUAL_UINT8(30, buttons.getPresses(SteamiButton::B));
    TEST_ASSERT_EQUAL_UINT8(0, buttons.getPresses(SteamiButton::B));
    TEST_ASSERT_EQUAL_UINT8(0, buttons.getPresses(SteamiButton::A));
}

// Held at one call, then released and tapped again before the next one: the release and the new
// press are both reported.
static void test_mcu_release_then_tap_between_two_calls(void) {
    steamiFakeBme280(Wire);
    attachExpander();
    releaseMcuButtons();
    SteamiButtons buttons(board);
    TEST_ASSERT_TRUE(buttons.begin());
    TEST_ASSERT_FALSE(buttons.isPressed(SteamiButton::A));
    delay(50);
    setButtonPin(A_BUTTON, LOW);
    delay(50);
    TEST_ASSERT_TRUE(buttons.wasPressed(SteamiButton::A));
    setButtonPin(A_BUTTON, HIGH);
    delay(80);
    setButtonPin(A_BUTTON, LOW);
    delay(80);
    setButtonPin(A_BUTTON, HIGH);
    delay(80);
    TEST_ASSERT_FALSE(buttons.isPressed(SteamiButton::A));
    TEST_ASSERT_TRUE(buttons.wasReleased(SteamiButton::A));
    TEST_ASSERT_TRUE(buttons.wasPressed(SteamiButton::A));
}

// A tap shorter than the lock-out: the interrupt takes the press, ignores the release, and the
// next call reads the pin released.
static void test_mcu_tap_shorter_than_the_lock_out(void) {
    steamiFakeBme280(Wire);
    attachExpander();
    releaseMcuButtons();
    SteamiButtons buttons(board);
    TEST_ASSERT_TRUE(buttons.begin());
    TEST_ASSERT_FALSE(buttons.isPressed(SteamiButton::A));
    delay(50);
    setButtonPin(A_BUTTON, LOW);
    delay(10);
    setButtonPin(A_BUTTON, HIGH);
    delay(200);
    TEST_ASSERT_FALSE(buttons.isPressed(SteamiButton::A));
    TEST_ASSERT_TRUE(buttons.wasPressed(SteamiButton::A));
    TEST_ASSERT_TRUE(buttons.wasReleased(SteamiButton::A));
}

// The expander latch keeps the tap: the next call finds the press, reads the expander again
// 20 ms later and finds the release, as for A.
static void test_dpad_tap_between_two_calls_is_caught(void) {
    steamiFakeBme280(Wire);
    attachExpander();
    releaseMcuButtons();
    SteamiButtons buttons(board);
    TEST_ASSERT_TRUE(buttons.begin());
    const uint8_t bits[] = {MCP23009_BTN_UP, MCP23009_BTN_DOWN, MCP23009_BTN_LEFT,
                            MCP23009_BTN_RIGHT};
    const SteamiButton names[] = {SteamiButton::UP, SteamiButton::DOWN, SteamiButton::LEFT,
                                  SteamiButton::RIGHT};
    for (int k = 0; k < 4; k++) {
        TEST_ASSERT_FALSE(buttons.wasPressed(names[k]));
        delay(150);
        expanderChip.press(bits[k]);
        delay(100);
        expanderChip.release(bits[k]);
        delay(50);
        TEST_ASSERT_TRUE(buttons.wasPressed(names[k]));
        TEST_ASSERT_FALSE(buttons.isPressed(names[k]));
        TEST_ASSERT_TRUE(buttons.wasReleased(names[k]));
        delay(300);
        TEST_ASSERT_FALSE(buttons.isPressed(names[k]));
        TEST_ASSERT_FALSE(buttons.wasReleased(names[k]));
        TEST_ASSERT_FALSE(buttons.wasPressed(names[k]));
    }
}

// A call reads the D-PAD in the middle of a release bounce, and the tail of the bounce comes
// just after that read: the expander latches it, the read made 20 ms later finds it and takes it
// for a bounce, not for a release then a press.
static void test_dpad_read_during_a_release_bounce_gives_no_false_press(void) {
    steamiFakeBme280(Wire);
    attachExpander();
    releaseMcuButtons();
    SteamiButtons buttons(board);
    TEST_ASSERT_TRUE(buttons.begin());
    TEST_ASSERT_FALSE(buttons.isPressed(SteamiButton::DOWN));
    delay(50);
    int presses = 0;
    int releases = 0;
    expanderChip.press(MCP23009_BTN_DOWN);
    delay(30);
    presses += buttons.wasPressed(SteamiButton::DOWN) ? 1 : 0;
    delay(100);
    expanderChip.release(MCP23009_BTN_DOWN);
    bool tail = false;
    expanderChip.onReadHook = [&tail](uint8_t reg) {
        if (reg == MCP23009_GPIO && !tail) {
            tail = true;  // tail of the bounce, just after the read that found the release
            expanderChip.press(MCP23009_BTN_DOWN);
            expanderChip.release(MCP23009_BTN_DOWN);
        }
    };
    presses += buttons.wasPressed(SteamiButton::DOWN) ? 1 : 0;  // read in the bounce
    expanderChip.onReadHook = nullptr;
    TEST_ASSERT_TRUE(tail);
    releases += buttons.wasReleased(SteamiButton::DOWN) ? 1 : 0;
    delay(30);
    for (int t = 0; t < 4; t++) {
        presses += buttons.wasPressed(SteamiButton::DOWN) ? 1 : 0;
        releases += buttons.wasReleased(SteamiButton::DOWN) ? 1 : 0;
        delay(30);
    }
    TEST_ASSERT_EQUAL_INT(1, presses);
    TEST_ASSERT_EQUAL_INT(1, releases);
    TEST_ASSERT_FALSE(buttons.isPressed(SteamiButton::DOWN));
}

// A failed read would give 0, every active-low D-PAD button pressed: it gives no press.
static void test_dpad_bus_error_gives_no_false_press(void) {
    steamiFakeBme280(Wire);
    attachExpander();
    releaseMcuButtons();
    SteamiButtons buttons(board);
    TEST_ASSERT_TRUE(buttons.begin());
    TEST_ASSERT_FALSE(buttons.isPressed(SteamiButton::UP));
    delay(50);
    expanderChip.failReads = true;
    int presses = 0;
    const SteamiButton dpad[] = {SteamiButton::UP, SteamiButton::DOWN, SteamiButton::LEFT,
                                 SteamiButton::RIGHT};
    for (int t = 0; t < 5; t++) {
        for (SteamiButton b : dpad) {
            presses += buttons.wasPressed(b) ? 1 : 0;
            presses += buttons.isPressed(b) ? 1 : 0;
        }
        delay(30);
    }
    TEST_ASSERT_EQUAL_INT(0, presses);
    expanderChip.failReads = false;
    expanderChip.press(MCP23009_BTN_LEFT);
    delay(30);
    TEST_ASSERT_TRUE(buttons.wasPressed(SteamiButton::LEFT));
}

// A bus error while begin() registers the D-PAD leaves the directions unregistered in the driver:
// the next calls register them again once the bus answers, and read their level then.
static void test_dpad_registration_bus_error_is_retried(void) {
    steamiFakeBme280(Wire);
    attachExpander();
    releaseMcuButtons();
    expanderChip.failReadRegs = 1u << MCP23009_INTCON;  // every registration fails
    SteamiButtons buttons(board);
    TEST_ASSERT_TRUE(buttons.begin());
    TEST_ASSERT_FALSE(buttons.isPressed(SteamiButton::UP));
    delay(50);
    expanderChip.press(MCP23009_BTN_UP);  // held while the bus fails
    delay(30);
    TEST_ASSERT_FALSE(buttons.wasPressed(SteamiButton::UP));
    expanderChip.failReadRegs = 0;
    delay(30);
    TEST_ASSERT_TRUE(buttons.wasPressed(SteamiButton::UP));  // level read at the registration
    TEST_ASSERT_TRUE(buttons.isPressed(SteamiButton::UP));
    expanderChip.release(MCP23009_BTN_UP);
    buttons.isPressed(SteamiButton::UP);
    delay(30);
    TEST_ASSERT_TRUE(buttons.wasReleased(SteamiButton::UP));
    expanderChip.press(MCP23009_BTN_LEFT);  // a tap between two calls, through the latch
    delay(50);
    expanderChip.release(MCP23009_BTN_LEFT);
    delay(50);
    TEST_ASSERT_TRUE(buttons.wasPressed(SteamiButton::LEFT));
}

// The GPINTEN read of the third registration (LEFT) fails once: the other directions stay
// registered and LEFT is registered again at the next call.
static void test_dpad_one_failed_registration_is_retried(void) {
    steamiFakeBme280(Wire);
    attachExpander();
    releaseMcuButtons();
    int intconWrites = 0;  // one per registration, before its GPINTEN read
    expanderChip.onWriteHook = [&intconWrites](uint8_t reg, uint8_t) {
        if (reg == MCP23009_INTCON) {
            intconWrites++;
            expanderChip.failReadRegs = intconWrites == 3 ? 1u << MCP23009_GPINTEN : 0;
        }
    };
    SteamiButtons buttons(board);
    TEST_ASSERT_TRUE(buttons.begin());
    TEST_ASSERT_EQUAL_INT(4, intconWrites);
    expanderChip.failReadRegs = 0;
    expanderChip.onWriteHook = nullptr;
    TEST_ASSERT_FALSE(buttons.isPressed(SteamiButton::LEFT));
    delay(50);
    const uint8_t bits[] = {MCP23009_BTN_UP, MCP23009_BTN_DOWN, MCP23009_BTN_LEFT,
                            MCP23009_BTN_RIGHT};
    const SteamiButton names[] = {SteamiButton::UP, SteamiButton::DOWN, SteamiButton::LEFT,
                                  SteamiButton::RIGHT};
    for (int k = 0; k < 4; k++) {
        expanderChip.press(bits[k]);
        delay(30);
        TEST_ASSERT_TRUE(buttons.wasPressed(names[k]));
        expanderChip.release(bits[k]);
        buttons.isPressed(names[k]);
        delay(30);
        TEST_ASSERT_TRUE(buttons.wasReleased(names[k]));
    }
}

static void test_dpad_held_at_start_is_not_a_press(void) {
    steamiFakeBme280(Wire);
    attachExpander();
    releaseMcuButtons();
    expanderChip.press(MCP23009_BTN_UP);
    SteamiButtons buttons(board);
    TEST_ASSERT_TRUE(buttons.begin());
    TEST_ASSERT_FALSE(buttons.wasPressed(SteamiButton::UP));
    TEST_ASSERT_TRUE(buttons.isPressed(SteamiButton::UP));
    delay(50);
    TEST_ASSERT_FALSE(buttons.wasPressed(SteamiButton::UP));
    expanderChip.release(MCP23009_BTN_UP);
    buttons.isPressed(SteamiButton::UP);
    delay(30);
    TEST_ASSERT_TRUE(buttons.wasReleased(SteamiButton::UP));
    TEST_ASSERT_FALSE(buttons.wasPressed(SteamiButton::UP));
}

// Taps made before the call, latched by the interrupt or by the expander, are dropped.
static void test_wait_for_press_drops_taps_made_before_the_call(void) {
    steamiFakeBme280(Wire);
    attachExpander();
    releaseMcuButtons();
    SteamiButtons buttons(board);
    TEST_ASSERT_TRUE(buttons.begin());
    TEST_ASSERT_FALSE(buttons.isPressed(SteamiButton::A));
    delay(50);
    setButtonPin(A_BUTTON, LOW);
    delay(60);
    setButtonPin(A_BUTTON, HIGH);
    delay(60);
    expanderChip.press(MCP23009_BTN_LEFT);
    delay(60);
    expanderChip.release(MCP23009_BTN_LEFT);
    delay(60);
    TEST_ASSERT_TRUE(buttons.waitForPress(100) == SteamiButton::NONE);
}

static void test_wait_for_press_returns_a_dpad_press_made_during_the_wait(void) {
    steamiFakeBme280(Wire);
    attachExpander();
    releaseMcuButtons();
    SteamiButtons buttons(board);
    TEST_ASSERT_TRUE(buttons.begin());
    TEST_ASSERT_FALSE(buttons.isPressed(SteamiButton::RIGHT));
    delay(50);
    int reads = 0;
    expanderChip.onReadHook = [&reads](uint8_t reg) {
        if (reg == MCP23009_GPIO && ++reads == 5) {
            expanderChip.press(MCP23009_BTN_RIGHT);
        }
    };
    const uint32_t start = millis();
    TEST_ASSERT_TRUE(buttons.waitForPress(500) == SteamiButton::RIGHT);
    expanderChip.onReadHook = nullptr;
    TEST_ASSERT_TRUE(millis() - start < 500);
}

// The calls read the expander through interruptEvent(): the pad callbacks of the sketch still run.
static void test_buttons_deliver_the_pad_callbacks_of_the_sketch(void) {
    steamiFakeBme280(Wire);
    attachExpander();
    releaseMcuButtons();
    expanderChip.release(0);
    SteamiButtons buttons(board);
    TEST_ASSERT_TRUE(buttons.begin());
    TEST_ASSERT_FALSE(buttons.isPressed(SteamiButton::A));
    SteamiMCP23009E* expander = board.mcp23009e();
    TEST_ASSERT_NOT_NULL(expander);
    int falls = 0;
    expander->setup(0, MCP23009_DIR_INPUT, MCP23009_PULLUP);
    expander->interruptOnFalling(0, [&falls]() { falls++; });
    expanderChip.press(0);
    delay(30);
    TEST_ASSERT_FALSE(buttons.isPressed(SteamiButton::A));
    TEST_ASSERT_EQUAL_INT(1, falls);
    TEST_ASSERT_FALSE(buttons.wasPressed(SteamiButton::UP));
    expander->disableInterrupt(0);
}

// A sketch that registers its own callback on a D-PAD bit, or disables its interrupt, does not stop
// the D-PAD of SteamiButtons: both keep working.
static void test_dpad_survives_the_pad_callbacks_of_the_sketch(void) {
    steamiFakeBme280(Wire);
    attachExpander();
    releaseMcuButtons();
    SteamiButtons buttons(board);
    TEST_ASSERT_TRUE(buttons.begin());
    TEST_ASSERT_FALSE(buttons.isPressed(SteamiButton::UP));
    SteamiMCP23009E* expander = board.mcp23009e();
    TEST_ASSERT_NOT_NULL(expander);
    std::vector<int> levels;
    expander->interruptOnChange(MCP23009_BTN_UP,
                                [&levels](uint8_t level) { levels.push_back(level); });
    expander->disableInterrupt(MCP23009_BTN_DOWN);
    delay(50);
    expanderChip.press(MCP23009_BTN_UP);
    delay(60);
    expanderChip.release(MCP23009_BTN_UP);
    delay(60);
    TEST_ASSERT_TRUE(buttons.wasPressed(SteamiButton::UP));
    expanderChip.press(MCP23009_BTN_DOWN);
    delay(60);
    expanderChip.release(MCP23009_BTN_DOWN);
    delay(60);
    TEST_ASSERT_TRUE(buttons.wasPressed(SteamiButton::DOWN));
    TEST_ASSERT_EQUAL_INT(2, static_cast<int>(levels.size()));  // the press, then the release
    TEST_ASSERT_EQUAL_INT(MCP23009_LOGIC_LOW, levels[0]);
    TEST_ASSERT_EQUAL_INT(MCP23009_LOGIC_HIGH, levels[1]);
    delay(30);
    TEST_ASSERT_FALSE(buttons.isPressed(SteamiButton::UP));
    TEST_ASSERT_TRUE(buttons.wasReleased(SteamiButton::UP));
    TEST_ASSERT_TRUE(buttons.wasReleased(SteamiButton::DOWN));
    expander->disableInterrupt(MCP23009_BTN_UP);
    TEST_ASSERT_TRUE((expander->watched() & (1 << MCP23009_BTN_UP)) != 0);
}

// A reset of the expander by the sketch removes the watch of SteamiButtons: the next call sets the
// D-PAD up again.
static void test_dpad_follows_again_after_a_reset_of_the_expander(void) {
    steamiFakeBme280(Wire);
    attachExpander();
    releaseMcuButtons();
    SteamiButtons buttons(board);
    TEST_ASSERT_TRUE(buttons.begin());
    TEST_ASSERT_FALSE(buttons.isPressed(SteamiButton::LEFT));
    SteamiMCP23009E* expander = board.mcp23009e();
    TEST_ASSERT_NOT_NULL(expander);
    expander->reset();
    TEST_ASSERT_EQUAL_HEX8(0, expander->watched());
    TEST_ASSERT_FALSE(buttons.isPressed(SteamiButton::LEFT));
    TEST_ASSERT_EQUAL_HEX8(0xF0, expander->watched());
    delay(50);
    expanderChip.press(MCP23009_BTN_LEFT);
    delay(60);
    expanderChip.release(MCP23009_BTN_LEFT);
    delay(60);
    TEST_ASSERT_TRUE(buttons.wasPressed(SteamiButton::LEFT));
}

// The loop keeps calling SteamiButtons while the expander is held in reset by powerOff(): the call
// after powerOn() still sets the D-PAD up again (pull-ups and interrupts back on the chip).
static void test_dpad_follows_again_after_calls_during_a_power_off(void) {
    steamiFakeBme280(Wire);
    attachExpander();
    releaseMcuButtons();
    SteamiButtons buttons(board);
    TEST_ASSERT_TRUE(buttons.begin());
    SteamiMCP23009E* expander = board.mcp23009e();
    TEST_ASSERT_NOT_NULL(expander);
    expander->powerOff();
    expanderChip.held = true;
    TEST_ASSERT_FALSE(buttons.isPressed(SteamiButton::LEFT));
    TEST_ASSERT_FALSE(buttons.wasPressed(SteamiButton::LEFT));
    TEST_ASSERT_EQUAL_HEX8(0, expander->watched());
    expander->powerOn();
    expanderChip.reset();  // power-on defaults: no pull-up, no interrupt
    TEST_ASSERT_FALSE(buttons.isPressed(SteamiButton::LEFT));
    TEST_ASSERT_EQUAL_HEX8(0xF0, expander->watched());
    TEST_ASSERT_EQUAL_HEX8(0xF0, expanderChip.regs[MCP23009_GPPU] & 0xF0);
    TEST_ASSERT_EQUAL_HEX8(0xF0, expanderChip.regs[MCP23009_GPINTEN] & 0xF0);
    delay(50);
    expanderChip.press(MCP23009_BTN_LEFT);
    expanderChip.release(MCP23009_BTN_LEFT);  // a tap between two calls: in the latch
    TEST_ASSERT_TRUE(buttons.wasPressed(SteamiButton::LEFT));
}

// Two objects: the interrupts and the D-PAD events go to the one called last.
static void test_buttons_called_last_get_the_interrupts(void) {
    steamiFakeBme280(Wire);
    attachExpander();
    releaseMcuButtons();
    SteamiButtons outer(board);
    TEST_ASSERT_TRUE(outer.begin());
    TEST_ASSERT_FALSE(outer.isPressed(SteamiButton::A));
    {
        SteamiButtons inner(board);
        TEST_ASSERT_TRUE(inner.begin());
        TEST_ASSERT_FALSE(inner.isPressed(SteamiButton::A));
    }
    TEST_ASSERT_FALSE(outer.isPressed(SteamiButton::A));
    delay(50);
    setButtonPin(A_BUTTON, LOW);
    delay(60);
    setButtonPin(A_BUTTON, HIGH);
    expanderChip.press(MCP23009_BTN_UP);
    expanderChip.release(MCP23009_BTN_UP);
    delay(60);
    TEST_ASSERT_TRUE(outer.wasPressed(SteamiButton::A));
    TEST_ASSERT_TRUE(outer.wasPressed(SteamiButton::UP));
}

// The interrupt handlers and the D-PAD callbacks outlive the object: they then do nothing.
static void test_interrupts_after_the_buttons_are_gone_do_nothing(void) {
    steamiFakeBme280(Wire);
    attachExpander();
    releaseMcuButtons();
    {
        SteamiButtons buttons(board);
        TEST_ASSERT_TRUE(buttons.begin());
        TEST_ASSERT_FALSE(buttons.isPressed(SteamiButton::A));
    }
    setButtonPin(A_BUTTON, LOW);
    setButtonPin(A_BUTTON, HIGH);
    expanderChip.press(MCP23009_BTN_UP);
    expanderChip.release(MCP23009_BTN_UP);
    TEST_ASSERT_TRUE(board.mcp23009e()->interruptEvent());

    SteamiButtons again(board);
    TEST_ASSERT_TRUE(again.begin());
    TEST_ASSERT_FALSE(again.isPressed(SteamiButton::A));
    delay(50);
    setButtonPin(A_BUTTON, LOW);
    delay(60);
    setButtonPin(A_BUTTON, HIGH);
    delay(60);
    TEST_ASSERT_TRUE(again.wasPressed(SteamiButton::A));
}

// --- Buttons on the board's timeline (issue #341) ---
//
// The finger moves at given times, run as the clock reaches them: while the sketch waits
// (delay()) and while it reads the expander (FakeMCP23009::readUs: about 400 us per register read
// at 100 kHz, so 1.2 ms per call and 17 ms for the 14 calls of a turn). The loop is the one of the
// integration test: getPresses() and wasReleased() for the seven buttons, then the wait (300 ms;
// about 520 ms in the second hardware run, which redrew the screen at each turn). getPresses()
// counts every press; wasReleased() is a flag, so the releases count only when at most one tap
// falls in a turn of the loop.

struct Timeline {
    std::vector<std::pair<uint32_t, std::function<void()>>> actions;  // sorted by time
    size_t next = 0;

    void at(uint32_t t, std::function<void()> fn) {
        auto it = actions.begin() + static_cast<long>(next);
        while (it != actions.end() && it->first <= t) {
            ++it;
        }
        actions.insert(it, {t, std::move(fn)});
    }
    void runDue() {
        while (next < actions.size() && actions[next].first <= millis()) {
            std::function<void()> fn = actions[next].second;
            next++;
            fn();
        }
    }
    // A tap of `ms` on A, B or MENU, from `t`: each edge runs the interrupt.
    void mcuTap(uint32_t t, int pin, uint32_t ms) {
        at(t, [pin]() { setButtonPin(pin, LOW); });
        at(t + ms, [pin]() { setButtonPin(pin, HIGH); });
    }
    // A tap of `ms` on a D-PAD button, from `t`.
    void dpadTap(uint32_t t, uint8_t bit, uint32_t ms) {
        at(t, [bit]() { expanderChip.press(bit); });
        at(t + ms, [bit]() { expanderChip.release(bit); });
    }
    // The same, with a 2 ms bounce at the press and at the release.
    void bouncyDpadTap(uint32_t t, uint8_t bit, uint32_t ms) {
        dpadTap(t, bit, 1);
        dpadTap(t + 2, bit, ms - 2);
        at(t + ms + 1, [bit]() { expanderChip.press(bit); });
        at(t + ms + 2, [bit]() { expanderChip.release(bit); });
    }
};

struct TapCounts {
    uint32_t presses[7] = {};
    uint32_t releases[7] = {};
};

static const SteamiButton kAllButtons[7] = {
    SteamiButton::A,    SteamiButton::B,    SteamiButton::MENU, SteamiButton::UP,
    SteamiButton::DOWN, SteamiButton::LEFT, SteamiButton::RIGHT};

// The loop of the integration test, until `until`, with the timeline running.
static void slowLoop(SteamiButtons& buttons, Timeline& timeline, uint32_t until, uint32_t periodMs,
                     TapCounts& counts) {
    clockHook() = [&timeline]() { timeline.runDue(); };
    while (static_cast<int32_t>(millis() - until) < 0) {
        timeline.runDue();
        for (int k = 0; k < 7; k++) {
            counts.presses[k] += buttons.getPresses(kAllButtons[k]);
            counts.releases[k] += buttons.wasReleased(kAllButtons[k]) ? 1 : 0;
        }
        delay(periodMs);
    }
    clockHook() = nullptr;
}

// A board started again for each object: attachExpander() resets the chip, as a power-on would,
// while the driver of the earlier object still takes the D-PAD pins as enabled, and watchChanges()
// enables only the pins not enabled yet.
static void attachFreshExpander(void) {
    board.end();
    attachExpander();
}

static uint32_t startTimeline(SteamiButtons& buttons) {
    attachFreshExpander();
    releaseMcuButtons();
    TEST_ASSERT_TRUE(buttons.begin());
    TEST_ASSERT_FALSE(buttons.isPressed(SteamiButton::A));
    expanderChip.readUs = 400;
    delay(50);
    return millis();
}

// `releases`: the taps are slower than the loop, so the release flags count them too.
static void assertOnlyOn(const TapCounts& counts, int target, uint32_t taps, bool releases = true) {
    char text[64];
    for (int k = 0; k < 7; k++) {
        const uint32_t expected = k == target ? taps : 0;
        snprintf(text, sizeof(text), "presses of button %d", k);
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(expected, counts.presses[k], text);
        if (releases || k != target) {
            snprintf(text, sizeof(text), "releases of button %d", k);
            TEST_ASSERT_EQUAL_UINT32_MESSAGE(expected, counts.releases[k], text);
        }
    }
}

// Hardware run 2: about 30 quick taps on A for the counter to reach 10. Four taps a second with a
// loop of 300 ms: some turns of the loop see two taps, and every tap must still be reported.
static void test_quick_taps_on_a_faster_than_the_loop_are_all_reported(void) {
    steamiFakeBme280(Wire);
    SteamiButtons buttons(board);
    const uint32_t t0 = startTimeline(buttons);
    Timeline timeline;
    for (uint32_t k = 0; k < 10; k++) {
        timeline.mcuTap(t0 + 100 + k * 250, A_BUTTON, 80);
    }
    TapCounts counts;
    slowLoop(buttons, timeline, t0 + 100 + 10 * 250 + 3000, 300, counts);
    assertOnlyOn(counts, 0, 10, false);
    TEST_ASSERT_FALSE(buttons.isPressed(SteamiButton::A));
}

// The same with the loop of hardware run 2 (about 520 ms) and three taps a second, on B and MENU.
static void test_quick_taps_on_b_and_menu_with_a_520_ms_loop_are_all_reported(void) {
    steamiFakeBme280(Wire);
    const int pins[2] = {B_BUTTON, MENU_BUTTON};
    for (int n = 0; n < 2; n++) {
        SteamiButtons buttons(board);
        const uint32_t t0 = startTimeline(buttons);
        Timeline timeline;
        for (uint32_t k = 0; k < 10; k++) {
            timeline.mcuTap(t0 + 100 + k * 337, pins[n], 60 + 10 * (k % 5));
        }
        TapCounts counts;
        slowLoop(buttons, timeline, t0 + 100 + 10 * 337 + 3000, 520, counts);
        assertOnlyOn(counts, 1 + n, 10, false);
    }
}

// Hardware run 1: UP 2 presses, DOWN, LEFT and RIGHT 1 press each, for 10 quick taps each with
// the 300 ms loop. A tap made before the next call is reported, whatever came before it.
static void test_quick_taps_on_the_dpad_are_all_reported(void) {
    steamiFakeBme280(Wire);
    const uint8_t bits[4] = {MCP23009_BTN_UP, MCP23009_BTN_DOWN, MCP23009_BTN_LEFT,
                             MCP23009_BTN_RIGHT};
    for (int d = 0; d < 4; d++) {
        SteamiButtons buttons(board);
        const uint32_t t0 = startTimeline(buttons);
        Timeline timeline;
        for (uint32_t k = 0; k < 10; k++) {
            timeline.dpadTap(t0 + 100 + k * 450 + 40 * (k % 3), bits[d], 80);
        }
        TapCounts counts;
        slowLoop(buttons, timeline, t0 + 100 + 10 * 450 + 1500, 300, counts);
        assertOnlyOn(counts, 3 + d, 10);
        TEST_ASSERT_FALSE(buttons.isPressed(kAllButtons[3 + d]));
    }
}

// Hardware run 2: 4 UP for 10 taps, about once a second, with the 520 ms loop.
static void test_dpad_taps_once_a_second_with_a_520_ms_loop_are_all_reported(void) {
    steamiFakeBme280(Wire);
    SteamiButtons buttons(board);
    const uint32_t t0 = startTimeline(buttons);
    Timeline timeline;
    uint32_t at = t0 + 300;
    for (uint32_t k = 0; k < 10; k++) {
        timeline.dpadTap(at, MCP23009_BTN_UP, 80);
        at += 620 + 140 * (k % 3);  // more than one turn of the loop between two taps
    }
    TapCounts counts;
    slowLoop(buttons, timeline, at + 1500, 520, counts);
    assertOnlyOn(counts, 3, 10);
}

// Contact bounces at the press and at the release, read or not by a call: one press each.
static void test_bouncy_dpad_taps_give_one_press_each(void) {
    steamiFakeBme280(Wire);
    SteamiButtons buttons(board);
    const uint32_t t0 = startTimeline(buttons);
    Timeline timeline;
    for (uint32_t k = 0; k < 20; k++) {
        timeline.bouncyDpadTap(t0 + 100 + k * 377, MCP23009_BTN_LEFT, 70 + 7 * (k % 4));
    }
    TapCounts counts;
    slowLoop(buttons, timeline, t0 + 100 + 20 * 377 + 1500, 300, counts);
    assertOnlyOn(counts, 5, 20);
}

// The release of a D-PAD button is reported by the call that reads it, as for A: isPressed()
// does not stay true one more turn of the loop (the long presses on UP looked longer than on A).
static void test_dpad_release_is_reported_by_the_call_that_reads_it(void) {
    steamiFakeBme280(Wire);
    SteamiButtons buttons(board);
    startTimeline(buttons);
    expanderChip.press(MCP23009_BTN_UP);
    TEST_ASSERT_TRUE(buttons.wasPressed(SteamiButton::UP));
    delay(1000);
    TEST_ASSERT_TRUE(buttons.isPressed(SteamiButton::UP));
    expanderChip.release(MCP23009_BTN_UP);
    TEST_ASSERT_FALSE(buttons.isPressed(SteamiButton::UP));
    TEST_ASSERT_TRUE(buttons.wasReleased(SteamiButton::UP));
}

// Hardware run 2: 3 DOWN during taps on UP. The D-PAD is one lever: released from UP, it can
// touch DOWN for a moment on its way back. A call that reads the release of UP just before that
// contact clears the interrupt of UP, and the expander latches the contact.
static void test_dpad_lever_rebound_after_a_release_is_not_a_press(void) {
    steamiFakeBme280(Wire);
    SteamiButtons buttons(board);
    startTimeline(buttons);
    expanderChip.press(MCP23009_BTN_UP);
    TEST_ASSERT_TRUE(buttons.wasPressed(SteamiButton::UP));
    delay(200);
    Timeline timeline;
    expanderChip.release(MCP23009_BTN_UP);
    // The call reads the release, then the lever touches DOWN 3 ms later, for 2 ms.
    timeline.dpadTap(millis() + 3, MCP23009_BTN_DOWN, 2);
    TapCounts counts;
    slowLoop(buttons, timeline, millis() + 1500, 300, counts);
    TEST_ASSERT_EQUAL_UINT32(0, counts.presses[4]);
    TEST_ASSERT_EQUAL_UINT32(1, counts.releases[3]);
    TEST_ASSERT_FALSE(buttons.isPressed(SteamiButton::DOWN));
}

// The same contact, still there when the call reads the release of UP.
static void test_dpad_lever_rebound_read_with_the_release_is_not_a_press(void) {
    steamiFakeBme280(Wire);
    SteamiButtons buttons(board);
    startTimeline(buttons);
    expanderChip.press(MCP23009_BTN_UP);
    TEST_ASSERT_TRUE(buttons.wasPressed(SteamiButton::UP));
    delay(200);
    Timeline timeline;
    expanderChip.release(MCP23009_BTN_UP);
    expanderChip.press(MCP23009_BTN_DOWN);
    timeline.at(millis() + 3, []() { expanderChip.release(MCP23009_BTN_DOWN); });
    TapCounts counts;
    slowLoop(buttons, timeline, millis() + 1500, 300, counts);
    TEST_ASSERT_EQUAL_UINT32(0, counts.presses[4]);
    TEST_ASSERT_EQUAL_UINT32(1, counts.releases[3]);
    TEST_ASSERT_FALSE(buttons.isPressed(SteamiButton::DOWN));
}

// A real press on another direction right after a release is reported.
static void test_dpad_other_direction_held_after_a_release_is_a_press(void) {
    steamiFakeBme280(Wire);
    SteamiButtons buttons(board);
    startTimeline(buttons);
    expanderChip.press(MCP23009_BTN_UP);
    TEST_ASSERT_TRUE(buttons.wasPressed(SteamiButton::UP));
    delay(200);
    Timeline timeline;
    expanderChip.release(MCP23009_BTN_UP);
    expanderChip.press(MCP23009_BTN_DOWN);
    timeline.at(millis() + 200, []() { expanderChip.release(MCP23009_BTN_DOWN); });
    TapCounts counts;
    slowLoop(buttons, timeline, millis() + 1500, 300, counts);
    TEST_ASSERT_EQUAL_UINT32(1, counts.releases[3]);
    TEST_ASSERT_EQUAL_UINT32(1, counts.presses[4]);
    TEST_ASSERT_EQUAL_UINT32(1, counts.releases[4]);
}

// Long presses with the 300 ms loop: isPressed() is true at every call between the press and the
// release, on A and on UP (the hardware run reported 1 to 4 out of 10 on A: the test, not the
// buttons, see the integration test).
static void test_long_presses_stay_pressed_at_every_call(void) {
    steamiFakeBme280(Wire);
    SteamiButtons buttons(board);
    const uint32_t t0 = startTimeline(buttons);
    Timeline timeline;
    for (uint32_t k = 0; k < 4; k++) {
        timeline.mcuTap(t0 + 100 + k * 2000, A_BUTTON, 1250);
        timeline.dpadTap(t0 + 1100 + k * 2000, MCP23009_BTN_UP, 1250);
    }
    clockHook() = [&timeline]() { timeline.runDue(); };
    const SteamiButton names[2] = {SteamiButton::A, SteamiButton::UP};
    uint32_t presses[2] = {};
    uint32_t releases[2] = {};
    bool held[2] = {};
    uint32_t drops = 0;
    while (millis() - t0 < 4 * 2000 + 1500) {
        for (int n = 0; n < 2; n++) {
            if (buttons.wasPressed(names[n])) {
                presses[n]++;
                held[n] = true;
            }
            const bool down = buttons.isPressed(names[n]);
            if (buttons.wasReleased(names[n])) {
                releases[n]++;
                held[n] = false;
            } else if (held[n] && !down) {
                drops++;
            }
        }
        delay(300);
    }
    clockHook() = nullptr;
    TEST_ASSERT_EQUAL_UINT32(0, drops);
    TEST_ASSERT_EQUAL_UINT32(4, presses[0]);
    TEST_ASSERT_EQUAL_UINT32(4, releases[0]);
    TEST_ASSERT_EQUAL_UINT32(4, presses[1]);
    TEST_ASSERT_EQUAL_UINT32(4, releases[1]);
}

// Review of #346: a call finds a whole UP tap in the latch (pressed, then released before the
// read), and the second tap is pressed `offMs` after the start of the call, while the call waits
// to confirm the first one. Gives the presses and releases of UP over the next turns.
static void secondTapAfterARead(uint32_t offMs, uint32_t& presses, uint32_t& releases) {
    setUp();
    steamiFakeBme280(Wire);
    SteamiButtons buttons(board);
    const uint32_t t0 = startTimeline(buttons);
    Timeline timeline;
    timeline.dpadTap(t0 + 100, MCP23009_BTN_UP, 80);
    clockHook() = [&timeline]() { timeline.runDue(); };
    delay(300);  // the first tap is over, no call yet
    timeline.dpadTap(millis() + offMs, MCP23009_BTN_UP, 80);
    TapCounts counts;
    slowLoop(buttons, timeline, millis() + 1500, 300, counts);
    presses = counts.presses[3];
    releases = counts.releases[3];
    TEST_ASSERT_FALSE(buttons.isPressed(SteamiButton::UP));
}

// The confirmation read 20 ms after a whole tap finds UP pressed again: that is the release of the
// first tap and the press of a second one, not a bounce. Only a press made before the read
// (offset 0: the chip latches the first change only) or within the 5 ms that follow it (taken for
// a bounce of the first press) merges.
static void test_second_dpad_tap_just_after_a_read_is_not_merged(void) {
    std::string scan;
    bool merged = false;
    for (uint32_t off = 0; off <= 40; off++) {
        uint32_t presses = 0;
        uint32_t releases = 0;
        secondTapAfterARead(off, presses, releases);
        scan += std::to_string(off) + ":" + std::to_string(presses) + "/" +
                std::to_string(releases) + " ";
        TEST_ASSERT_EQUAL_UINT32(presses, releases);
        TEST_ASSERT_TRUE(presses == 1 || presses == 2);
        if (off >= 8 && presses != 2) {
            merged = true;
        }
    }
    TEST_MESSAGE(scan.c_str());
    TEST_ASSERT_FALSE_MESSAGE(merged, "a second tap pressed 8 ms or more after the read merged");
}

// A call reads the D-PAD in the middle of a press bounce: the latch has the press, the port reads
// open, and the contact closes again just after the read. One press, still held: not a tap and a
// second press.
static void test_dpad_read_during_a_press_bounce_gives_one_press(void) {
    steamiFakeBme280(Wire);
    SteamiButtons buttons(board);
    startTimeline(buttons);
    expanderChip.press(MCP23009_BTN_RIGHT);
    expanderChip.release(MCP23009_BTN_RIGHT);
    bool closed = false;
    expanderChip.onReadHook = [&closed](uint8_t reg) {
        if (reg == MCP23009_GPIO && !closed) {
            closed = true;
            expanderChip.press(MCP23009_BTN_RIGHT);
        }
    };
    TEST_ASSERT_EQUAL_UINT8(1, buttons.getPresses(SteamiButton::RIGHT));
    expanderChip.onReadHook = nullptr;
    TEST_ASSERT_TRUE(closed);
    TEST_ASSERT_TRUE(buttons.isPressed(SteamiButton::RIGHT));
    TEST_ASSERT_FALSE(buttons.wasReleased(SteamiButton::RIGHT));
    delay(200);
    expanderChip.release(MCP23009_BTN_RIGHT);
    delay(50);
    TEST_ASSERT_EQUAL_UINT8(0, buttons.getPresses(SteamiButton::RIGHT));
    TEST_ASSERT_TRUE(buttons.wasReleased(SteamiButton::RIGHT));
    TEST_ASSERT_FALSE(buttons.isPressed(SteamiButton::RIGHT));
}

// --- Buttons: a bus error at the D-PAD registration (#332), then the taps of #341 ---

// startTimeline() with every D-PAD registration of begin() failing (INTCON read), so that the
// calls register the D-PAD again once the bus answers.
static uint32_t startTimelineWithFailedRegistration(SteamiButtons& buttons) {
    attachFreshExpander();
    releaseMcuButtons();
    expanderChip.failReadRegs = 1u << MCP23009_INTCON;
    TEST_ASSERT_TRUE(buttons.begin());
    TEST_ASSERT_FALSE(buttons.isPressed(SteamiButton::A));
    expanderChip.readUs = 400;
    delay(50);
    return millis();
}

// The bus answers again while the loop of the integration test runs: the call that registers the
// D-PAD again starts the latch, and every quick tap made after it is counted, as without the
// error.
static void test_dpad_quick_taps_after_a_registration_bus_error_are_all_counted(void) {
    steamiFakeBme280(Wire);
    const uint8_t bits[4] = {MCP23009_BTN_UP, MCP23009_BTN_DOWN, MCP23009_BTN_LEFT,
                             MCP23009_BTN_RIGHT};
    for (int d = 0; d < 4; d++) {
        SteamiButtons buttons(board);
        const uint32_t t0 = startTimelineWithFailedRegistration(buttons);
        Timeline timeline;
        timeline.at(t0 + 150, []() { expanderChip.failReadRegs = 0; });
        for (uint32_t k = 0; k < 10; k++) {
            timeline.dpadTap(t0 + 700 + k * 450 + 40 * (k % 3), bits[d], 80);
        }
        TapCounts counts;
        slowLoop(buttons, timeline, t0 + 700 + 10 * 450 + 1500, 300, counts);
        assertOnlyOn(counts, 3 + d, 10);
        TEST_ASSERT_FALSE(buttons.isPressed(kAllButtons[3 + d]));
    }
}

// LEFT is held while the bus fails: the registration made when it answers again reads it, which
// gives one press; its release and the quick taps after it are all counted, and the taps on A made
// while the D-PAD was unregistered too.
static void test_dpad_held_through_a_registration_bus_error_then_quick_taps(void) {
    steamiFakeBme280(Wire);
    SteamiButtons buttons(board);
    const uint32_t t0 = startTimelineWithFailedRegistration(buttons);
    Timeline timeline;
    timeline.at(t0 + 50, []() { expanderChip.press(MCP23009_BTN_LEFT); });
    timeline.mcuTap(t0 + 100, A_BUTTON, 60);
    timeline.mcuTap(t0 + 250, A_BUTTON, 60);
    timeline.at(t0 + 500, []() { expanderChip.failReadRegs = 0; });
    timeline.at(t0 + 1100, []() { expanderChip.release(MCP23009_BTN_LEFT); });
    for (uint32_t k = 0; k < 10; k++) {
        timeline.dpadTap(t0 + 1500 + k * 450 + 40 * (k % 3), MCP23009_BTN_LEFT, 80);
    }
    TapCounts counts;
    slowLoop(buttons, timeline, t0 + 1500 + 10 * 450 + 1500, 300, counts);
    for (int k = 0; k < 7; k++) {
        char text[64];
        const uint32_t expected = k == 5 ? 11 : (k == 0 ? 2 : 0);
        snprintf(text, sizeof(text), "presses of button %d", k);
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(expected, counts.presses[k], text);
        if (k != 0) {  // the two taps on A fall in one turn of the loop: one release flag
            snprintf(text, sizeof(text), "releases of button %d", k);
            TEST_ASSERT_EQUAL_UINT32_MESSAGE(expected, counts.releases[k], text);
        }
    }
    TEST_ASSERT_FALSE(buttons.isPressed(SteamiButton::LEFT));
}

// #341 meets #348: the quick taps of the slow loop are all counted while the sketch has its own
// callback on UP, and after it resets the expander halfway and registers that callback again.
static void test_dpad_quick_taps_all_counted_beside_a_sketch_callback_and_a_reset(void) {
    steamiFakeBme280(Wire);
    SteamiButtons buttons(board);
    const uint32_t t0 = startTimeline(buttons);
    SteamiMCP23009E* expander = board.mcp23009e();
    TEST_ASSERT_NOT_NULL(expander);
    std::vector<int> levels;
    auto record = [&levels](uint8_t level) { levels.push_back(level); };
    TEST_ASSERT_TRUE(expander->interruptOnChange(MCP23009_BTN_UP, record));
    Timeline timeline;
    for (uint32_t k = 0; k < 10; k++) {
        timeline.dpadTap(t0 + 100 + k * 450 + 40 * (k % 3), MCP23009_BTN_UP, 80);
    }
    TapCounts first;
    slowLoop(buttons, timeline, t0 + 2000, 300, first);      // ends between the 5th and the 6th tap
    TEST_ASSERT_FALSE(buttons.isPressed(SteamiButton::UP));  // the 5th one read
    expander->reset();
    TEST_ASSERT_TRUE(expander->interruptOnChange(MCP23009_BTN_UP, record));
    TapCounts second;
    slowLoop(buttons, timeline, t0 + 100 + 10 * 450 + 1500, 300, second);
    TapCounts counts;
    for (int k = 0; k < 7; k++) {
        counts.presses[k] = first.presses[k] + second.presses[k];
        counts.releases[k] = first.releases[k] + second.releases[k];
    }
    assertOnlyOn(counts, 3, 10);
    TEST_ASSERT_EQUAL_INT(20, static_cast<int>(levels.size()));  // the press, then the release
    for (size_t k = 0; k < levels.size(); k++) {
        TEST_ASSERT_EQUAL_INT(k % 2 == 0 ? MCP23009_LOGIC_LOW : MCP23009_LOGIC_HIGH, levels[k]);
    }
    TEST_ASSERT_EQUAL_HEX8(0xF0, expander->watched());
}

// #332 meets #348: the four D-PAD pins are enabled, but the GPIO read that gives their level fails
// in begin(). The next call reads it again: UP, held from the start, reads pressed (held at start:
// not a press), and the quick taps on LEFT that follow are all counted, beside a sketch callback
// on LEFT.
static void test_dpad_level_read_error_in_begin_is_retried_then_quick_taps(void) {
    steamiFakeBme280(Wire);
    attachFreshExpander();
    releaseMcuButtons();
    expanderChip.press(MCP23009_BTN_UP);
    int gpintenWrites = 0;  // one per enabled pin; the level read comes after the fourth
    expanderChip.onWriteHook = [&gpintenWrites](uint8_t reg, uint8_t) {
        if (reg == MCP23009_GPINTEN && ++gpintenWrites == 4) {
            expanderChip.failReadRegs = 1u << MCP23009_GPIO;
        }
    };
    SteamiButtons buttons(board);
    TEST_ASSERT_TRUE(buttons.begin());
    TEST_ASSERT_EQUAL_INT(4, gpintenWrites);
    expanderChip.onWriteHook = nullptr;
    expanderChip.failReadRegs = 0;
    SteamiMCP23009E* expander = board.mcp23009e();
    TEST_ASSERT_NOT_NULL(expander);
    TEST_ASSERT_EQUAL_HEX8(0xF0, expander->watched());
    TEST_ASSERT_TRUE(buttons.isPressed(SteamiButton::UP));
    TEST_ASSERT_FALSE(buttons.wasPressed(SteamiButton::UP));
    std::vector<int> levels;
    TEST_ASSERT_TRUE(expander->interruptOnChange(
        MCP23009_BTN_LEFT, [&levels](uint8_t level) { levels.push_back(level); }));
    expanderChip.readUs = 400;
    delay(50);
    const uint32_t t0 = millis();
    Timeline timeline;
    timeline.at(t0 + 50, []() { expanderChip.release(MCP23009_BTN_UP); });
    for (uint32_t k = 0; k < 10; k++) {
        timeline.dpadTap(t0 + 500 + k * 450 + 40 * (k % 3), MCP23009_BTN_LEFT, 80);
    }
    TapCounts counts;
    slowLoop(buttons, timeline, t0 + 500 + 10 * 450 + 1500, 300, counts);
    TEST_ASSERT_EQUAL_UINT32(0, counts.presses[3]);
    TEST_ASSERT_EQUAL_UINT32(1, counts.releases[3]);  // UP, held from the start
    TEST_ASSERT_EQUAL_UINT32(10, counts.presses[5]);
    TEST_ASSERT_EQUAL_UINT32(10, counts.releases[5]);
    TEST_ASSERT_EQUAL_INT(20, static_cast<int>(levels.size()));
    TEST_ASSERT_FALSE(buttons.isPressed(SteamiButton::UP));
    expander->disableInterrupt(MCP23009_BTN_LEFT);
    TEST_ASSERT_EQUAL_HEX8(0xF0, expander->watched());
}

// --- Every class together ---

static void test_all_classes_on_v1c_print_only_e20(void) {
    steamiFakeBme280(Wire);
    attachImu();
    steamiFakeLis2mdl(Wire, 0, 300, 0);
    steamiFakeVl53l1x(Wire);
    steamiFakeBq27441(Wire);
    steamiFakeMcp23009e(Wire, 0xF0);
    releaseMcuButtons();
    SteamiTemperature temperature(board);
    SteamiHumidity humidity(board);
    SteamiPressure pressure(board);
    SteamiDistance distance(board);
    SteamiBattery battery(board);
    SteamiAccelerometer accelerometer(board);
    SteamiGyroscope gyroscope(board);
    SteamiMagnetometer magnetometer(board);
    SteamiCompass compass(board);
    SteamiLight light(board);
    SteamiColor color(board);
    SteamiGesture gesture(board);
    SteamiButtons buttons(board);
    TEST_ASSERT_TRUE(temperature.begin());
    TEST_ASSERT_TRUE(humidity.begin());
    TEST_ASSERT_TRUE(pressure.begin());
    TEST_ASSERT_TRUE(distance.begin());
    TEST_ASSERT_TRUE(battery.begin());
    TEST_ASSERT_TRUE(accelerometer.begin());
    TEST_ASSERT_TRUE(gyroscope.begin());
    TEST_ASSERT_TRUE(magnetometer.begin());
    TEST_ASSERT_TRUE(compass.begin());
    TEST_ASSERT_FALSE(light.begin());
    TEST_ASSERT_FALSE(color.begin());
    TEST_ASSERT_FALSE(gesture.begin());
    TEST_ASSERT_TRUE(buttons.begin());
    TEST_ASSERT_EQUAL_UINT32(4, g_lines.size());
    TEST_ASSERT_EQUAL_STRING("[STeaMi] Board V1c (detected: BME280 at 0x76)", g_lines[0].c_str());
    TEST_ASSERT_TRUE(g_lines[1].rfind("[STeaMi] SteamiLight: ", 0) == 0);
    TEST_ASSERT_TRUE(g_lines[2].rfind("[STeaMi] SteamiColor: ", 0) == 0);
    TEST_ASSERT_TRUE(g_lines[3].rfind("[STeaMi] SteamiGesture: ", 0) == 0);
    TEST_ASSERT_EQUAL_UINT32(3, countLogged("(E20)"));
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
    RUN_TEST(test_config_release_missing_battery_reports_e10);
    RUN_TEST(test_config_release_missing_distance_reports_e10);
    RUN_TEST(test_read_after_board_end_is_safe);
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
    RUN_TEST(test_distance_reads_mm);
    RUN_TEST(test_distance_zero_is_minus_one);
    RUN_TEST(test_distance_absent);
    RUN_TEST(test_battery_reads_gauge);
    RUN_TEST(test_battery_discharging);
    RUN_TEST(test_battery_absent);
    RUN_TEST(test_accelerometer_reads_g);
    RUN_TEST(test_accelerometer_begin_sets_8g_range);
    RUN_TEST(test_accelerometer_begin_waits_for_a_sample_at_8g);
    RUN_TEST(test_accelerometer_absent_does_not_wait);
    RUN_TEST(test_accelerometer_is_shaken);
    RUN_TEST(test_accelerometer_and_gyroscope_share_chip);
    RUN_TEST(test_gyroscope_reads_dps);
    RUN_TEST(test_accelerometer_absent);
    RUN_TEST(test_accelerometer_calibrated_from_config);
    RUN_TEST(test_motion_read_after_board_end_is_safe);
    RUN_TEST(test_magnetometer_reads_ut);
    RUN_TEST(test_magnetometer_uses_hard_iron_offsets);
    RUN_TEST(test_compass_north_and_east);
    RUN_TEST(test_compass_declination_before_begin);
    RUN_TEST(test_compass_and_magnetometer_share_chip);
    RUN_TEST(test_compass_absent);
    RUN_TEST(test_magnetometer_and_compass_nan_on_bus_error);
    RUN_TEST(test_compass_heading_uses_cm);
    RUN_TEST(test_compass_tilt_heading_removes_roll_error);
    RUN_TEST(test_compass_tilt_heading_equals_heading_when_flat);
    RUN_TEST(test_compass_tilt_heading_nan_without_accelerometer);
    RUN_TEST(test_compass_tilt_heading_nan_when_compass_absent);
    RUN_TEST(test_compass_tilt_heading_nan_on_bus_error);
    RUN_TEST(test_motion_calibrations_when_forced);
    RUN_TEST(test_calibrate_zero_changes_nothing);
    RUN_TEST(test_calibrate_updates_offsets);
    RUN_TEST(test_light_reads_clear);
    RUN_TEST(test_color_reads_rgb);
    RUN_TEST(test_light_color_gesture_share_chip);
    RUN_TEST(test_gesture_begin_enables_engine);
    RUN_TEST(test_gesture_none_without_motion);
    RUN_TEST(test_gesture_names);
    RUN_TEST(test_apds_classes_e20_on_v1c);
    RUN_TEST(test_apds_forced_v1ab_absent_is_e11);
    RUN_TEST(test_buttons_read_a_and_dpad);
    RUN_TEST(test_was_pressed_once_per_press);
    RUN_TEST(test_was_released_after_release);
    RUN_TEST(test_bounces_after_a_press_give_one_press);
    RUN_TEST(test_tap_caught_by_a_single_sample);
    RUN_TEST(test_dpad_bus_error_reads_released);
    RUN_TEST(test_buttons_without_expander);
    RUN_TEST(test_wait_for_press_timeout);
    RUN_TEST(test_wait_for_press_returns_button);
    RUN_TEST(test_wait_for_press_drops_a_latched_press);
    RUN_TEST(test_button_held_at_start_is_not_a_press);
    RUN_TEST(test_buttons_attach_change_interrupts_on_a_b_menu);
    RUN_TEST(test_mcu_tap_between_two_calls_is_caught);
    RUN_TEST(test_mcu_bounces_between_two_calls_give_one_press);
    RUN_TEST(test_mcu_bounce_after_a_release_is_not_a_press);
    RUN_TEST(test_two_mcu_taps_between_two_calls_give_one_press_and_count_two);
    RUN_TEST(test_unread_presses_are_not_replayed_by_was_pressed);
    RUN_TEST(test_mcu_release_then_tap_between_two_calls);
    RUN_TEST(test_mcu_tap_shorter_than_the_lock_out);
    RUN_TEST(test_dpad_tap_between_two_calls_is_caught);
    RUN_TEST(test_dpad_read_during_a_release_bounce_gives_no_false_press);
    RUN_TEST(test_dpad_bus_error_gives_no_false_press);
    RUN_TEST(test_dpad_registration_bus_error_is_retried);
    RUN_TEST(test_dpad_one_failed_registration_is_retried);
    RUN_TEST(test_dpad_held_at_start_is_not_a_press);
    RUN_TEST(test_wait_for_press_drops_taps_made_before_the_call);
    RUN_TEST(test_wait_for_press_returns_a_dpad_press_made_during_the_wait);
    RUN_TEST(test_buttons_deliver_the_pad_callbacks_of_the_sketch);
    RUN_TEST(test_dpad_survives_the_pad_callbacks_of_the_sketch);
    RUN_TEST(test_dpad_follows_again_after_a_reset_of_the_expander);
    RUN_TEST(test_dpad_follows_again_after_calls_during_a_power_off);
    RUN_TEST(test_buttons_called_last_get_the_interrupts);
    RUN_TEST(test_interrupts_after_the_buttons_are_gone_do_nothing);
    RUN_TEST(test_quick_taps_on_a_faster_than_the_loop_are_all_reported);
    RUN_TEST(test_quick_taps_on_b_and_menu_with_a_520_ms_loop_are_all_reported);
    RUN_TEST(test_quick_taps_on_the_dpad_are_all_reported);
    RUN_TEST(test_dpad_taps_once_a_second_with_a_520_ms_loop_are_all_reported);
    RUN_TEST(test_bouncy_dpad_taps_give_one_press_each);
    RUN_TEST(test_dpad_release_is_reported_by_the_call_that_reads_it);
    RUN_TEST(test_dpad_lever_rebound_after_a_release_is_not_a_press);
    RUN_TEST(test_dpad_lever_rebound_read_with_the_release_is_not_a_press);
    RUN_TEST(test_dpad_other_direction_held_after_a_release_is_a_press);
    RUN_TEST(test_long_presses_stay_pressed_at_every_call);
    RUN_TEST(test_second_dpad_tap_just_after_a_read_is_not_merged);
    RUN_TEST(test_dpad_read_during_a_press_bounce_gives_one_press);
    RUN_TEST(test_dpad_quick_taps_after_a_registration_bus_error_are_all_counted);
    RUN_TEST(test_dpad_held_through_a_registration_bus_error_then_quick_taps);
    RUN_TEST(test_dpad_quick_taps_all_counted_beside_a_sketch_callback_and_a_reset);
    RUN_TEST(test_dpad_level_read_error_in_begin_is_retried_then_quick_taps);
    RUN_TEST(test_all_classes_on_v1c_print_only_e20);
    return UNITY_END();
}
