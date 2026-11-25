// SPDX-License-Identifier: GPL-3.0-or-later

#include <Arduino.h>
#include <Wire.h>
#include <string.h>
#include <unity.h>

#include <string>
#include <vector>

#include "SteamiBoard.h"
#include "SteamiLazy.h"
#include "SteamiTypes.h"

static std::vector<std::string> g_lines;
static void capture(const char* line) {
    g_lines.push_back(line);
}

static SteamiBoard board(Wire, SteamiBoardPins::steami());

void setUp(void) {
    g_lines.clear();
    board.end();
    board.setLogger(&capture);
}
void tearDown(void) {
    board.end();
    board.setLogger(nullptr);
}

static_assert(steamiDecodeRevision(131).board == SteamiRelease::V1C, "rev 131 is V1c");

static void test_revision_codes(void) {
    SteamiRevisionInfo r = steamiDecodeRevision(110);
    TEST_ASSERT_TRUE(r.valid);
    TEST_ASSERT_EQUAL_UINT8(1, r.generation);
    TEST_ASSERT_EQUAL_UINT8(1, r.release);
    TEST_ASSERT_EQUAL_UINT8(0, r.index);
    TEST_ASSERT_TRUE(r.board == SteamiRelease::V1A);
    TEST_ASSERT_TRUE(r.prototype);

    r = steamiDecodeRevision(111);
    TEST_ASSERT_TRUE(r.valid);
    TEST_ASSERT_TRUE(r.board == SteamiRelease::V1A);
    TEST_ASSERT_FALSE(r.prototype);

    r = steamiDecodeRevision(120);
    TEST_ASSERT_TRUE(r.board == SteamiRelease::V1B);
    TEST_ASSERT_TRUE(r.prototype);
    r = steamiDecodeRevision(121);
    TEST_ASSERT_TRUE(r.board == SteamiRelease::V1B);
    TEST_ASSERT_FALSE(r.prototype);

    r = steamiDecodeRevision(130);
    TEST_ASSERT_TRUE(r.valid);
    TEST_ASSERT_TRUE(r.board == SteamiRelease::V1C);
    TEST_ASSERT_TRUE(r.prototype);
    TEST_ASSERT_EQUAL_UINT8(0, r.index);

    r = steamiDecodeRevision(131);
    TEST_ASSERT_TRUE(r.valid);
    TEST_ASSERT_EQUAL_UINT8(1, r.generation);
    TEST_ASSERT_EQUAL_UINT8(3, r.release);
    TEST_ASSERT_EQUAL_UINT8(1, r.index);
    TEST_ASSERT_TRUE(r.board == SteamiRelease::V1C);
    TEST_ASSERT_FALSE(r.prototype);

    const int32_t invalid[] = {3, 99, 100, 140, 210, 230, 1000, -131};
    for (int32_t code : invalid) {
        r = steamiDecodeRevision(code);
        TEST_ASSERT_FALSE(r.valid);
        TEST_ASSERT_TRUE(r.board == SteamiRelease::UNKNOWN);
    }
}

static void test_routing_v1c_has_no_apds_and_uses_bme280(void) {
    TEST_ASSERT_TRUE(steamiRoute(SteamiRelease::V1C, SteamiFunction::TEMPERATURE, 0) ==
                     SteamiChip::BME280);
    TEST_ASSERT_TRUE(steamiRoute(SteamiRelease::V1C, SteamiFunction::TEMPERATURE, 1) ==
                     SteamiChip::NONE);
    TEST_ASSERT_TRUE(steamiRoute(SteamiRelease::V1C, SteamiFunction::GESTURE, 0) ==
                     SteamiChip::NONE);
    TEST_ASSERT_TRUE(steamiRoute(SteamiRelease::V1C, SteamiFunction::DISPLAY, 0) ==
                     SteamiChip::GC9A01);
}

static void test_routing_v1ab_and_unknown_candidates(void) {
    TEST_ASSERT_TRUE(steamiRoute(SteamiRelease::V1AB, SteamiFunction::TEMPERATURE, 1) ==
                     SteamiChip::WSEN_PADS);
    TEST_ASSERT_TRUE(steamiRoute(SteamiRelease::V1B, SteamiFunction::DISPLAY, 0) ==
                     SteamiChip::SSD1327);
    TEST_ASSERT_TRUE(steamiRoute(SteamiRelease::UNKNOWN, SteamiFunction::HUMIDITY, 0) ==
                     SteamiChip::BME280);
    TEST_ASSERT_TRUE(steamiRoute(SteamiRelease::UNKNOWN, SteamiFunction::HUMIDITY, 1) ==
                     SteamiChip::WSEN_HIDS);
    TEST_ASSERT_TRUE(steamiRoute(SteamiRelease::UNKNOWN, SteamiFunction::DISPLAY, 0) ==
                     SteamiChip::NONE);
}

struct Probe {
    static int alive;
    int v;
    explicit Probe(int x) : v(x) { ++alive; }
    ~Probe() { --alive; }
};
int Probe::alive = 0;

static void test_lazy_slot_builds_once_and_resets(void) {
    SteamiLazy<Probe> slot;
    TEST_ASSERT_FALSE(slot.built());
    slot.emplace(4);
    slot.emplace(9);
    TEST_ASSERT_TRUE(slot.built());
    TEST_ASSERT_EQUAL_INT(4, slot.get()->v);
    TEST_ASSERT_EQUAL_INT(1, Probe::alive);
    slot.reset();
    TEST_ASSERT_EQUAL_INT(0, Probe::alive);
    TEST_ASSERT_FALSE(slot.built());
}

static void test_constructor_touches_nothing(void) {
    Wire = TwoWire();
    gpioPinMode().clear();
    SteamiBoard fresh(Wire, SteamiBoardPins::steami());
    TEST_ASSERT_EQUAL_UINT32(0, Wire.totalTransactions());
    TEST_ASSERT_TRUE(gpioPinMode().empty());
    TEST_ASSERT_EQUAL_STRING("", fresh.lastError());
}

static void test_pins_steami(void) {
    SteamiBoardPins p = SteamiBoardPins::steami();
    TEST_ASSERT_EQUAL_UINT32(I2C_INT_SDA, p.sda);
    TEST_ASSERT_EQUAL_UINT32(I2C_INT_SCL, p.scl);
    TEST_ASSERT_EQUAL_UINT32(RST_EXPANDER, p.expanderReset);
    TEST_ASSERT_EQUAL_UINT32(INT_EXPANDER, p.expanderIrq);
    TEST_ASSERT_EQUAL_UINT32(A_BUTTON, p.buttonA);
    TEST_ASSERT_EQUAL_UINT32(B_BUTTON, p.buttonB);
    TEST_ASSERT_EQUAL_UINT32(MENU_BUTTON, p.buttonMenu);
}

static const char* const kGesture = "this STeaMi is a V1c, it has no gesture sensor.";
static const char* const kGestureLine =
    "[STeaMi] SteamiGesture: this STeaMi is a V1c, it has no gesture sensor. (E20)";

static void test_report_formats_and_logs_once(void) {
    board.report("SteamiGesture", SteamiStatus::NOT_ON_THIS_BOARD, kGesture, 20);
    board.report("SteamiGesture", SteamiStatus::NOT_ON_THIS_BOARD, kGesture, 20);
    TEST_ASSERT_EQUAL_UINT32(1, g_lines.size());
    TEST_ASSERT_EQUAL_STRING(kGestureLine, g_lines[0].c_str());
    TEST_ASSERT_EQUAL_STRING(kGestureLine, board.lastError());
}

static void test_report_same_code_other_class_logs_again(void) {
    board.report("SteamiGesture", SteamiStatus::NOT_ON_THIS_BOARD, kGesture, 20);
    board.report("SteamiLight", SteamiStatus::NOT_ON_THIS_BOARD, kGesture, 20);
    TEST_ASSERT_EQUAL_UINT32(2, g_lines.size());
}

static void test_end_forgets_reported_pairs(void) {
    board.report("SteamiGesture", SteamiStatus::NOT_ON_THIS_BOARD, kGesture, 20);
    board.end();
    TEST_ASSERT_EQUAL_STRING("", board.lastError());
    board.report("SteamiGesture", SteamiStatus::NOT_ON_THIS_BOARD, kGesture, 20);
    TEST_ASSERT_EQUAL_UINT32(2, g_lines.size());
}

static void test_report_without_logger_still_stores_last_error(void) {
    board.setLogger(nullptr);
    board.report("SteamiX", SteamiStatus::NOT_RESPONDING, "no answer.", 5);
    TEST_ASSERT_EQUAL_STRING("[STeaMi] SteamiX: no answer. (E05)", board.lastError());
    TEST_ASSERT_TRUE(g_lines.empty());
}

static void test_last_error_is_truncated(void) {
    std::string longText(400, 'x');
    board.report("SteamiX", SteamiStatus::NOT_RESPONDING, longText.c_str(), 7);
    TEST_ASSERT_EQUAL_UINT32(191, strlen(board.lastError()));
    TEST_ASSERT_EQUAL_UINT32(1, g_lines.size());
}

static void test_more_than_16_pairs_do_not_overflow(void) {
    for (uint8_t i = 0; i < 20; i++) {
        board.report("SteamiX", SteamiStatus::NOT_RESPONDING, "d", i);
    }
    TEST_ASSERT_EQUAL_UINT32(20, g_lines.size());
    board.report("SteamiX", SteamiStatus::NOT_RESPONDING, "d", 0);
    TEST_ASSERT_EQUAL_UINT32(20, g_lines.size());
}

static void test_wire_begins_bus_and_returns_it(void) {
    TEST_ASSERT_TRUE(&board.wire() == &Wire);
    TEST_ASSERT_TRUE(&Steami.wire() == &Wire);
}

static void test_chip_names_and_addresses(void) {
    TEST_ASSERT_EQUAL_STRING("BME280", steamiChipName(SteamiChip::BME280));
    TEST_ASSERT_EQUAL_STRING("APDS-9960", steamiChipName(SteamiChip::APDS9960));
    TEST_ASSERT_EQUAL_STRING("GC9A01", steamiChipName(SteamiChip::GC9A01));
    TEST_ASSERT_EQUAL_STRING("", steamiChipName(SteamiChip::NONE));
    TEST_ASSERT_EQUAL_STRING("", steamiChipName(SteamiChip::COUNT));
    TEST_ASSERT_EQUAL_UINT8(0x76, steamiChipAddress(SteamiChip::BME280));
    TEST_ASSERT_EQUAL_UINT8(0x20, steamiChipAddress(SteamiChip::MCP23009E));
    TEST_ASSERT_EQUAL_UINT8(0, steamiChipAddress(SteamiChip::SSD1327));
    TEST_ASSERT_EQUAL_UINT8(0, steamiChipAddress(SteamiChip::NONE));
}

static void test_sizeof_board(void) {
    TEST_ASSERT_LESS_THAN(2560, sizeof(SteamiBoard));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_constructor_touches_nothing);
    RUN_TEST(test_pins_steami);
    RUN_TEST(test_report_formats_and_logs_once);
    RUN_TEST(test_report_same_code_other_class_logs_again);
    RUN_TEST(test_end_forgets_reported_pairs);
    RUN_TEST(test_report_without_logger_still_stores_last_error);
    RUN_TEST(test_last_error_is_truncated);
    RUN_TEST(test_more_than_16_pairs_do_not_overflow);
    RUN_TEST(test_wire_begins_bus_and_returns_it);
    RUN_TEST(test_chip_names_and_addresses);
    RUN_TEST(test_sizeof_board);
    RUN_TEST(test_revision_codes);
    RUN_TEST(test_routing_v1c_has_no_apds_and_uses_bme280);
    RUN_TEST(test_routing_v1ab_and_unknown_candidates);
    RUN_TEST(test_lazy_slot_builds_once_and_resets);
    return UNITY_END();
}
