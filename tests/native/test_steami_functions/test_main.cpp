// SPDX-License-Identifier: GPL-3.0-or-later

#include <Arduino.h>
#include <Wire.h>
#include <math.h>
#include <string.h>
#include <unity.h>

#include <string>
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

// The native mock cannot change a pin during the wait, so B goes low before the call, unseen:
// the first sample of the wait takes it.
static void test_wait_for_press_returns_button(void) {
    steamiFakeBme280(Wire);
    steamiFakeMcp23009e(Wire, 0xF0);
    releaseMcuButtons();
    SteamiButtons buttons(board);
    TEST_ASSERT_TRUE(buttons.begin());
    buttons.isPressed(SteamiButton::B);
    delay(50);
    gpioPinState()[B_BUTTON] = LOW;
    const uint32_t start = millis();
    TEST_ASSERT_TRUE(buttons.waitForPress(500) == SteamiButton::B);
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
    RUN_TEST(test_compass_heading_uses_cm);
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
    RUN_TEST(test_all_classes_on_v1c_print_only_e20);
    return UNITY_END();
}
