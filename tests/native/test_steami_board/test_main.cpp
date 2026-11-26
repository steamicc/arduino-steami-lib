// SPDX-License-Identifier: GPL-3.0-or-later

#include <Arduino.h>
#include <Wire.h>
#include <string.h>
#include <unity.h>

#include <string>
#include <vector>

#include "FakeDaplinkBridge.h"
#include "SteamiBoard.h"
#include "SteamiLazy.h"
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

static_assert(steamiDecodeRevision(131).board == SteamiRelease::V1C, "rev 131 is V1c");

// The global Steami must stay constant-initialised, the bridge and config slots included.
[[maybe_unused]] static constexpr SteamiBoard kConstantBoard{};

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
    TEST_ASSERT_TRUE(steamiRoute(SteamiRelease::V1C, SteamiFunction::SCREEN, 0) ==
                     SteamiChip::GC9A01);
}

static void test_routing_v1ab_and_unknown_candidates(void) {
    TEST_ASSERT_TRUE(steamiRoute(SteamiRelease::V1AB, SteamiFunction::TEMPERATURE, 1) ==
                     SteamiChip::WSEN_PADS);
    TEST_ASSERT_TRUE(steamiRoute(SteamiRelease::V1B, SteamiFunction::SCREEN, 0) ==
                     SteamiChip::SSD1327);
    TEST_ASSERT_TRUE(steamiRoute(SteamiRelease::UNKNOWN, SteamiFunction::HUMIDITY, 0) ==
                     SteamiChip::BME280);
    TEST_ASSERT_TRUE(steamiRoute(SteamiRelease::UNKNOWN, SteamiFunction::HUMIDITY, 1) ==
                     SteamiChip::WSEN_HIDS);
    TEST_ASSERT_TRUE(steamiRoute(SteamiRelease::UNKNOWN, SteamiFunction::SCREEN, 0) ==
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
    const std::string last = board.lastError();
    TEST_ASSERT_EQUAL_UINT32(255, last.size());
    TEST_ASSERT_EQUAL_UINT32(0, last.rfind("[STeaMi] SteamiX: xxx", 0));
    TEST_ASSERT_EQUAL_STRING("x (E07)", last.c_str() + last.size() - 7);
    TEST_ASSERT_EQUAL_UINT32(1, g_lines.size());
    TEST_ASSERT_EQUAL_STRING(last.c_str(), g_lines[0].c_str());
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

// --- Release detection ---

static FakeDaplinkBridge fw;

static void attachFirmware(void) {
    fw = FakeDaplinkBridge();
    Wire.attachDevice(DAPLINK_BRIDGE_DEFAULT_ADDR, &fw);
}

static void presentBme280(void) {
    Wire.setRegister(0x76, 0xD0, 0x60);
}
static void presentHids(void) {
    Wire.setRegister(0x5F, 0x0F, 0xBC);
}
static void presentPads(void) {
    Wire.setRegister(0x5D, 0x0F, 0xB3);
}
static void presentApds(void) {
    Wire.setRegister(0x39, 0x92, 0xAB);
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

static bool loggedCode(const char* suffix) {
    return countLogged(suffix) > 0;
}

static size_t countBanners(void) {
    size_t count = 0;
    for (const std::string& line : g_lines) {
        if (line.rfind("[STeaMi] Board ", 0) == 0) {
            count++;
        }
    }
    return count;
}

static size_t countIn(const std::string& text, const std::string& what) {
    size_t count = 0;
    for (size_t at = text.find(what); at != std::string::npos; at = text.find(what, at + 1)) {
        count++;
    }
    return count;
}

// Starts the next case of a test on a fresh bus and a fresh board.
static void restart(void) {
    board.end();
    Wire = TwoWire();
    g_lines.clear();
}

static void test_config_131_gives_v1c_without_probing(void) {
    attachFirmware();
    fw.store("{\"rev\":131}");
    TEST_ASSERT_TRUE(board.begin());
    TEST_ASSERT_TRUE(board.release() == SteamiRelease::V1C);
    TEST_ASSERT_TRUE(board.origin() == SteamiOrigin::CONFIG);
    TEST_ASSERT_EQUAL_INT32(131, board.revision());
    TEST_ASSERT_FALSE(board.isPrototype());
    TEST_ASSERT_EQUAL_UINT8(1, board.schematicIndex());
    TEST_ASSERT_TRUE(board.isV1c());
    TEST_ASSERT_FALSE(board.isV1ab());
    TEST_ASSERT_NOT_NULL(board.config());
    TEST_ASSERT_EQUAL_UINT32(0, Wire.transactions(0x76));
    TEST_ASSERT_EQUAL_UINT32(0, Wire.transactions(0x5F));
    TEST_ASSERT_EQUAL_UINT32(0, Wire.transactions(0x5D));
    TEST_ASSERT_EQUAL_UINT32(0, Wire.transactions(0x39));
}

static void test_config_130_is_a_prototype(void) {
    attachFirmware();
    fw.store("{\"rev\":130}");
    TEST_ASSERT_TRUE(board.release() == SteamiRelease::V1C);
    TEST_ASSERT_TRUE(board.isPrototype());
    TEST_ASSERT_EQUAL_UINT8(0, board.schematicIndex());
}

static void test_invalid_revision_reports_e02_and_probes(void) {
    attachFirmware();
    fw.store("{\"rev\":3}");
    presentBme280();
    TEST_ASSERT_TRUE(board.release() == SteamiRelease::V1C);
    TEST_ASSERT_TRUE(board.origin() == SteamiOrigin::PROBE);
    TEST_ASSERT_EQUAL_UINT32(1, countLogged("(E02)"));
    TEST_ASSERT_FALSE(board.isPrototype());
    TEST_ASSERT_EQUAL_UINT8(0xFF, board.schematicIndex());

    restart();
    attachFirmware();
    fw.store("{\"rev\":230}");
    presentHids();
    TEST_ASSERT_TRUE(board.release() == SteamiRelease::V1AB);
    TEST_ASSERT_TRUE(board.origin() == SteamiOrigin::PROBE);
    TEST_ASSERT_EQUAL_UINT32(1, countLogged("(E02)"));
}

static void test_missing_revision_probes_silently(void) {
    attachFirmware();
    fw.store("{\"name\":\"x\"}");
    presentBme280();
    TEST_ASSERT_TRUE(board.release() == SteamiRelease::V1C);
    TEST_ASSERT_TRUE(board.origin() == SteamiOrigin::PROBE);
    TEST_ASSERT_FALSE(loggedCode("(E02)"));
    TEST_ASSERT_EQUAL_INT32(-1, board.revision());
}

static void test_legacy_firmware_never_gets_a_config_command(void) {
    attachFirmware();
    fw.legacyFirmware = true;
    presentHids();
    TEST_ASSERT_TRUE(board.begin());
    TEST_ASSERT_TRUE(board.release() == SteamiRelease::V1AB);
    TEST_ASSERT_TRUE(board.origin() == SteamiOrigin::PROBE);
    TEST_ASSERT_EQUAL_UINT32(0, fw.configCommandsOnLegacy);
    TEST_ASSERT_FALSE(fw.hung);
    TEST_ASSERT_NULL(board.config());
    TEST_ASSERT_TRUE(board.begin());
    TEST_ASSERT_EQUAL_UINT32(0, fw.configCommandsOnLegacy);
}

static void test_bridge_absent_probes(void) {
    Wire.setAbsent(DAPLINK_BRIDGE_DEFAULT_ADDR);
    presentBme280();
    TEST_ASSERT_TRUE(board.release() == SteamiRelease::V1C);
    TEST_ASSERT_TRUE(board.origin() == SteamiOrigin::PROBE);
    TEST_ASSERT_NULL(board.config());
}

static void test_probe_matrix(void) {
    presentBme280();
    TEST_ASSERT_TRUE(board.begin());
    TEST_ASSERT_TRUE(board.release() == SteamiRelease::V1C);
    TEST_ASSERT_TRUE(board.origin() == SteamiOrigin::PROBE);

    restart();
    presentHids();
    TEST_ASSERT_TRUE(board.begin());
    TEST_ASSERT_TRUE(board.release() == SteamiRelease::V1AB);
    TEST_ASSERT_TRUE(board.isV1ab());

    restart();
    presentPads();
    TEST_ASSERT_TRUE(board.begin());
    TEST_ASSERT_TRUE(board.release() == SteamiRelease::V1AB);

    restart();
    presentApds();
    TEST_ASSERT_TRUE(board.begin());
    TEST_ASSERT_TRUE(board.release() == SteamiRelease::V1AB);

    restart();
    TEST_ASSERT_FALSE(board.begin());
    TEST_ASSERT_TRUE(board.release() == SteamiRelease::UNKNOWN);
    TEST_ASSERT_TRUE(board.origin() == SteamiOrigin::NONE);
    TEST_ASSERT_TRUE(loggedCode("(E01)"));
    TEST_ASSERT_EQUAL_STRING("unknown", board.releaseName());
}

static void test_apds_ids_all_give_v1ab(void) {
    const uint8_t ids[] = {0xAB, 0x9C, 0xA8};
    for (uint8_t id : ids) {
        restart();
        Wire.setRegister(0x39, 0x92, id);
        TEST_ASSERT_TRUE(board.release() == SteamiRelease::V1AB);
    }
    restart();
    Wire.setRegister(0x39, 0x92, 0x55);
    TEST_ASSERT_TRUE(board.release() == SteamiRelease::UNKNOWN);
}

static void test_second_begin_makes_no_traffic(void) {
    presentBme280();
    TEST_ASSERT_TRUE(board.begin());
    const size_t n = Wire.totalTransactions();
    TEST_ASSERT_TRUE(board.begin());
    TEST_ASSERT_EQUAL_UINT32(n, Wire.totalTransactions());
}

static void test_runtime_override_wins_without_traffic(void) {
    presentHids();
    TEST_ASSERT_TRUE(board.begin());
    TEST_ASSERT_TRUE(board.release() == SteamiRelease::V1AB);
    const size_t n = Wire.totalTransactions();
    TEST_ASSERT_TRUE(board.begin(SteamiRelease::V1C));
    TEST_ASSERT_TRUE(board.release() == SteamiRelease::V1C);
    TEST_ASSERT_TRUE(board.origin() == SteamiOrigin::FORCED_RUNTIME);
    TEST_ASSERT_EQUAL_UINT32(n, Wire.totalTransactions());
    TEST_ASSERT_EQUAL_STRING("[STeaMi] Board V1c (forced by Steami.begin())",
                             g_lines.back().c_str());
}

static void test_runtime_override_before_begin_makes_no_traffic(void) {
    TEST_ASSERT_TRUE(board.begin(SteamiRelease::V1B));
    TEST_ASSERT_EQUAL_UINT32(0, Wire.totalTransactions());
    TEST_ASSERT_EQUAL_STRING("V1b", board.releaseName());
    TEST_ASSERT_TRUE(board.isV1ab());
    TEST_ASSERT_TRUE(board.begin());
    TEST_ASSERT_EQUAL_UINT32(0, Wire.totalTransactions());
    TEST_ASSERT_TRUE(board.origin() == SteamiOrigin::FORCED_RUNTIME);
}

static void test_queries_before_begin_detect(void) {
    presentBme280();
    TEST_ASSERT_FALSE(board.has(SteamiFunction::GESTURE));
    TEST_ASSERT_TRUE(board.has(SteamiFunction::TEMPERATURE));
    TEST_ASSERT_EQUAL_STRING("V1c", board.releaseName());
}

static void test_release_names(void) {
    TEST_ASSERT_TRUE(board.begin(SteamiRelease::V1A));
    TEST_ASSERT_EQUAL_STRING("V1a", board.releaseName());
    TEST_ASSERT_TRUE(board.begin(SteamiRelease::V1AB));
    TEST_ASSERT_EQUAL_STRING("V1a/V1b", board.releaseName());
    TEST_ASSERT_TRUE(board.has(SteamiFunction::GESTURE));
}

static void test_banner_and_print_info(void) {
    attachFirmware();
    fw.store("{\"rev\":131}");
    const char* banner = "[STeaMi] Board V1c (read from the board config, revision 131)";
    TEST_ASSERT_TRUE(board.begin());
    TEST_ASSERT_FALSE(g_lines.empty());
    TEST_ASSERT_EQUAL_STRING(banner, g_lines[0].c_str());
    TEST_ASSERT_EQUAL_UINT32(1, g_lines.size());

    // An explicit report to another output always starts with the banner.
    StringPrint p;
    board.printInfo(p);
    TEST_ASSERT_EQUAL_UINT32(0, p.text.rfind(std::string(banner) + "\n", 0));
    TEST_ASSERT_EQUAL_UINT32(1, countIn(p.text, banner));

    // The logger already delivered the banner: printInfo() sends the chips only.
    board.printInfo();
    TEST_ASSERT_EQUAL_UINT32(1, countBanners());
    TEST_ASSERT_TRUE(g_lines.size() > 1);
    TEST_ASSERT_EQUAL_STRING(banner, g_lines[0].c_str());
}

static void test_begin_then_print_info_logs_banner_and_error_once(void) {
    attachFirmware();
    fw.store("{\"rev\":3}");
    presentBme280();
    TEST_ASSERT_TRUE(board.begin());
    board.printInfo();
    TEST_ASSERT_EQUAL_UINT32(1, countBanners());
    TEST_ASSERT_EQUAL_UINT32(1, countLogged("(E02)"));
    TEST_ASSERT_EQUAL_STRING("[STeaMi] Board V1c (detected: BME280 at 0x76)", g_lines[1].c_str());
}

static void test_print_info_without_logger_replays_error(void) {
    board.setLogger(nullptr);
    attachFirmware();
    fw.store("{\"rev\":3}");
    presentBme280();
    TEST_ASSERT_TRUE(board.begin());
    StringPrint p;
    board.printInfo(p);
    TEST_ASSERT_EQUAL_UINT32(0, p.text.rfind("[STeaMi] Board V1c (detected: BME280 at 0x76)\n", 0));
    const std::string last = std::string(board.lastError()) + "\n";
    TEST_ASSERT_TRUE(last.find("(E02)") != std::string::npos);
    TEST_ASSERT_EQUAL_UINT32(1, countIn(p.text, "(E02)"));
    TEST_ASSERT_EQUAL_STRING(last.c_str(), p.text.c_str() + p.text.size() - last.size());

    // A logger set later gets the banner and the message it never delivered, once.
    board.setLogger(&capture);
    board.printInfo();
    board.printInfo();
    TEST_ASSERT_EQUAL_UINT32(1, countBanners());
    TEST_ASSERT_EQUAL_UINT32(1, countLogged("(E02)"));
}

static void test_banner_variants(void) {
    attachFirmware();
    fw.store("{\"rev\":130}");
    board.begin();
    TEST_ASSERT_EQUAL_STRING(
        "[STeaMi] Board V1c (read from the board config, revision 130, prototype)",
        g_lines.back().c_str());

    restart();
    presentBme280();
    board.begin();
    TEST_ASSERT_EQUAL_STRING("[STeaMi] Board V1c (detected: BME280 at 0x76)",
                             g_lines.back().c_str());

    restart();
    presentHids();
    board.begin();
    TEST_ASSERT_EQUAL_STRING("[STeaMi] Board V1a/V1b (detected: WSEN-HIDS at 0x5F)",
                             g_lines.back().c_str());

    restart();
    board.begin();
    TEST_ASSERT_EQUAL_STRING("[STeaMi] Board unknown (nothing detected)", g_lines.back().c_str());
    const size_t lines = g_lines.size();
    board.begin();
    TEST_ASSERT_EQUAL_UINT32(lines, g_lines.size());
}

// --- Shared chip slots ---

static void test_claim_builds_and_begins_once(void) {
    presentBme280();
    TEST_ASSERT_TRUE(board.claim(SteamiChip::BME280) == SteamiStatus::OK);
    TEST_ASSERT_TRUE(board.built(SteamiChip::BME280));
    const size_t n = Wire.transactions(0x76);
    TEST_ASSERT_TRUE(board.claim(SteamiChip::BME280) == SteamiStatus::OK);
    TEST_ASSERT_EQUAL_UINT32(n, Wire.transactions(0x76));
}

static void test_accessor_returns_same_instance(void) {
    presentBme280();
    SteamiBME280* first = board.bme280();
    TEST_ASSERT_NOT_NULL(first);
    TEST_ASSERT_TRUE(first == board.bme280());
}

static void test_not_responding_is_cached(void) {
    presentHids();
    Wire.setAbsent(0x6B);
    TEST_ASSERT_TRUE(board.release() == SteamiRelease::V1AB);
    TEST_ASSERT_TRUE(board.claim(SteamiChip::ISM330DL) == SteamiStatus::NOT_RESPONDING);
    const size_t n = Wire.transactions(0x6B);
    TEST_ASSERT_TRUE(board.claim(SteamiChip::ISM330DL) == SteamiStatus::NOT_RESPONDING);
    TEST_ASSERT_EQUAL_UINT32(n, Wire.transactions(0x6B));
    TEST_ASSERT_NULL(board.ism330dl());
}

static void test_displays_and_none_are_not_started(void) {
    presentBme280();
    TEST_ASSERT_TRUE(board.claim(SteamiChip::SSD1327) == SteamiStatus::NOT_STARTED);
    TEST_ASSERT_TRUE(board.claim(SteamiChip::GC9A01) == SteamiStatus::NOT_STARTED);
    TEST_ASSERT_TRUE(board.claim(SteamiChip::NONE) == SteamiStatus::NOT_STARTED);
    TEST_ASSERT_TRUE(board.claim(SteamiChip::COUNT) == SteamiStatus::NOT_STARTED);
    TEST_ASSERT_FALSE(board.built(SteamiChip::GC9A01));
    TEST_ASSERT_FALSE(board.built(SteamiChip::NONE));
}

static void test_claim_detects_first(void) {
    presentBme280();
    TEST_ASSERT_TRUE(board.claim(SteamiChip::BME280) == SteamiStatus::OK);
    TEST_ASSERT_TRUE(board.origin() == SteamiOrigin::PROBE);
}

static void test_every_accessor_builds_its_slot(void) {
    presentBme280();
    Wire.setAbsent(0x5F);
    Wire.setAbsent(0x5D);
    Wire.setAbsent(0x39);
    Wire.setAbsent(0x6B);
    Wire.setAbsent(0x1E);
    Wire.setAbsent(0x29);
    Wire.setAbsent(0x55);
    Wire.setAbsent(0x20);
    TEST_ASSERT_NOT_NULL(board.bme280());
    TEST_ASSERT_NULL(board.wsenHids());
    TEST_ASSERT_NULL(board.wsenPads());
    TEST_ASSERT_NULL(board.apds9960());
    TEST_ASSERT_NULL(board.ism330dl());
    TEST_ASSERT_NULL(board.lis2mdl());
    TEST_ASSERT_NULL(board.vl53l1x());
    TEST_ASSERT_NULL(board.bq27441());
    TEST_ASSERT_NULL(board.mcp23009e());
    TEST_ASSERT_TRUE(board.built(SteamiChip::MCP23009E));
    TEST_ASSERT_TRUE(board.built(SteamiChip::VL53L1X));
}

static void test_v1ab_never_builds_bme280(void) {
    presentHids();
    TEST_ASSERT_TRUE(board.release() == SteamiRelease::V1AB);
    for (uint8_t f = 0; f < static_cast<uint8_t>(SteamiFunction::COUNT); f++) {
        if (static_cast<SteamiFunction>(f) == SteamiFunction::SCREEN) {
            continue;
        }
        board.claim(steamiRoute(SteamiRelease::V1AB, static_cast<SteamiFunction>(f), 0));
    }
    TEST_ASSERT_FALSE(board.built(SteamiChip::BME280));
    TEST_ASSERT_FALSE(board.built(SteamiChip::GC9A01));
}

static void test_print_info_lists_chips(void) {
    test_v1ab_never_builds_bme280();
    StringPrint p;
    board.printInfo(p);
    TEST_ASSERT_TRUE(p.text.find("BME280") == std::string::npos);
    TEST_ASSERT_TRUE(p.text.find("WSEN-HIDS 0x5F OK\n") != std::string::npos);
    TEST_ASSERT_TRUE(p.text.find("WSEN-PADS 0x5D ") != std::string::npos);
    TEST_ASSERT_TRUE(p.text.find("APDS-9960 0x39 ") != std::string::npos);
    TEST_ASSERT_TRUE(p.text.find("MCP23009E 0x20 ") != std::string::npos);
    TEST_ASSERT_TRUE(p.text.find("GC9A01") == std::string::npos);
    TEST_ASSERT_TRUE(p.text.find("SSD1327 screen not started\n") != std::string::npos);
    TEST_ASSERT_TRUE(p.text.find("not responding") != std::string::npos);
}

static void test_print_info_lists_the_chips_of_the_release(void) {
    presentBme280();
    TEST_ASSERT_TRUE(board.release() == SteamiRelease::V1C);
    StringPrint p;
    board.printInfo(p);
    TEST_ASSERT_TRUE(p.text.find("APDS-9960") == std::string::npos);
    TEST_ASSERT_TRUE(p.text.find("WSEN-HIDS") == std::string::npos);
    TEST_ASSERT_TRUE(p.text.find("WSEN-PADS") == std::string::npos);
    const char* const v1c[] = {"BME280 0x76 ",  "ISM330DL 0x6B ", "LIS2MDL 0x1E ",
                               "VL53L1X 0x29 ", "BQ27441 0x55 ",  "MCP23009E 0x20 "};
    size_t previous = 0;
    for (const char* chip : v1c) {
        const size_t at = p.text.find(chip);
        TEST_ASSERT_TRUE(at != std::string::npos);
        TEST_ASSERT_TRUE(at >= previous);  // enum order
        previous = at;
    }
    TEST_ASSERT_TRUE(p.text.find("GC9A01 screen not started\n") != std::string::npos);
    TEST_ASSERT_TRUE(p.text.find("SSD1327") == std::string::npos);
    TEST_ASSERT_FALSE(board.built(SteamiChip::GC9A01));  // printInfo() builds no slot

    restart();
    TEST_ASSERT_FALSE(board.begin());  // unknown: all nine I2C chips
    StringPrint q;
    board.printInfo(q);
    for (size_t i = 0; i < 9; i++) {
        const SteamiChip c = static_cast<SteamiChip>(static_cast<size_t>(SteamiChip::BME280) + i);
        TEST_ASSERT_EQUAL_UINT32(1, countIn(q.text, std::string(steamiChipName(c)) + " 0x"));
    }
    TEST_ASSERT_TRUE(q.text.find(" screen ") == std::string::npos);
}

static void test_print_info_replays_last_error(void) {
    board.setLogger(nullptr);
    TEST_ASSERT_FALSE(board.begin());  // nothing answers: E01, not delivered
    StringPrint p;
    board.printInfo(p);
    const std::string last = std::string(board.lastError()) + "\n";
    TEST_ASSERT_TRUE(p.text.size() >= last.size());
    TEST_ASSERT_EQUAL_STRING(last.c_str(), p.text.c_str() + p.text.size() - last.size());

    // Delivered by the logger: not replayed into another output.
    restart();
    board.setLogger(&capture);
    TEST_ASSERT_FALSE(board.begin());
    TEST_ASSERT_EQUAL_UINT32(1, countLogged("(E01)"));
    StringPrint r;
    board.printInfo(r);
    TEST_ASSERT_TRUE(r.text.find("(E01)") == std::string::npos);

    restart();
    presentBme280();
    StringPrint q;
    board.printInfo(q);
    TEST_ASSERT_TRUE(q.text.find("(E") == std::string::npos);
}

static void test_print_info_does_not_repeat_its_own_error(void) {
    board.printInfo();  // detection inside the call reports E01 to the logger
    size_t e01 = 0;
    for (const std::string& line : g_lines) {
        if (line.size() >= 5 && line.compare(line.size() - 5, 5, "(E01)") == 0) {
            e01++;
        }
    }
    TEST_ASSERT_EQUAL_UINT32(1, e01);

    restart();
    StringPrint p;
    board.printInfo(p);
    TEST_ASSERT_TRUE(p.text.find("(E01)") == std::string::npos);
}

static void test_end_detaches_expander_interrupt(void) {
    presentBme280();
    TEST_ASSERT_NOT_NULL(board.bme280());
    board.mcp23009e();
    detachedInterrupts().clear();
    board.end();
    TEST_ASSERT_EQUAL_UINT32(1, detachedInterrupts().size());
    TEST_ASSERT_EQUAL_INT(static_cast<int>(INT_EXPANDER), detachedInterrupts()[0]);

    detachedInterrupts().clear();
    board.end();  // nothing built: nothing to detach
    TEST_ASSERT_TRUE(detachedInterrupts().empty());
}

static void test_print_info_logger_sends_banner_once(void) {
    presentBme280();
    board.printInfo();
    TEST_ASSERT_EQUAL_UINT32(1, countBanners());
    TEST_ASSERT_TRUE(g_lines.size() > 1);
}

static void test_new_message_is_replayed_once(void) {
    presentBme280();
    TEST_ASSERT_TRUE(board.begin());
    board.report("SteamiX", SteamiStatus::NOT_RESPONDING, "first.", 5);
    board.printInfo();
    TEST_ASSERT_EQUAL_UINT32(1, countLogged("(E05)"));

    // Same pair, other text: stored but not logged, so printInfo() replays it once.
    board.report("SteamiX", SteamiStatus::NOT_RESPONDING, "second.", 5);
    TEST_ASSERT_EQUAL_UINT32(1, countLogged("(E05)"));
    board.printInfo();
    TEST_ASSERT_EQUAL_STRING("[STeaMi] SteamiX: second. (E05)", g_lines.back().c_str());
    board.printInfo();
    TEST_ASSERT_EQUAL_UINT32(2, countLogged("(E05)"));
}

static void test_end_destroys_slots(void) {
    presentBme280();
    TEST_ASSERT_NOT_NULL(board.bme280());
    TEST_ASSERT_TRUE(board.built(SteamiChip::BME280));
    board.end();
    TEST_ASSERT_FALSE(board.built(SteamiChip::BME280));
    StringPrint p;
    board.printInfo(p);
    TEST_ASSERT_TRUE(p.text.find("BME280 0x76 not tried\n") != std::string::npos);

    restart();
    presentHids();
    TEST_ASSERT_NOT_NULL(board.wsenHids());
    board.end();
    TEST_ASSERT_FALSE(board.built(SteamiChip::WSEN_HIDS));
    TEST_ASSERT_TRUE(board.release() == SteamiRelease::V1AB);
    TEST_ASSERT_TRUE(board.origin() == SteamiOrigin::PROBE);
}

// --- Calibration ---

static const char* const kBmeOffsetConfig = "{\"rev\":131,\"tc\":{\"bme\":{\"g\":1.0,\"o\":-2.0}}}";

static void test_calibrate_applies_bme_key(void) {
    attachFirmware();
    fw.store(kBmeOffsetConfig);
    steamiFakeBme280(Wire);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SteamiStatus::OK),
                            static_cast<uint8_t>(board.claim(SteamiChip::BME280)));
    TEST_ASSERT_TRUE(board.calibrate(SteamiChip::BME280));
    TEST_ASSERT_TRUE(board.calibrated(SteamiChip::BME280));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 23.08f, board.bme280()->temperature());
}

static void test_calibrate_without_key_changes_nothing(void) {
    attachFirmware();
    fw.store("{\"rev\":131}");
    steamiFakeBme280(Wire);
    board.claim(SteamiChip::BME280);
    TEST_ASSERT_FALSE(board.calibrate(SteamiChip::BME280));
    TEST_ASSERT_FALSE(board.calibrated(SteamiChip::BME280));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 25.08f, board.bme280()->temperature());
}

static void test_calibrate_is_once_per_chip(void) {
    attachFirmware();
    fw.store(kBmeOffsetConfig);
    steamiFakeBme280(Wire);
    board.claim(SteamiChip::BME280);
    TEST_ASSERT_TRUE(board.calibrate(SteamiChip::BME280));
    const uint32_t reads = fw.readConfigCommands;
    TEST_ASSERT_TRUE(board.calibrate(SteamiChip::BME280));
    TEST_ASSERT_EQUAL_UINT32(reads, fw.readConfigCommands);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 23.08f, board.bme280()->temperature());  // not applied twice
}

static void test_calibrate_forced_loads_config_on_demand(void) {
    attachFirmware();
    fw.store(kBmeOffsetConfig);
    steamiFakeBme280(Wire);
    TEST_ASSERT_TRUE(board.begin(SteamiRelease::V1C));
    TEST_ASSERT_NULL(board.config());
    TEST_ASSERT_TRUE(board.calibrate(SteamiChip::BME280));
    TEST_ASSERT_TRUE(board.calibrated(SteamiChip::BME280));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 23.08f, board.bme280()->temperature());
}

static void test_calibrate_forced_legacy_no_config_command(void) {
    attachFirmware();
    fw.legacyFirmware = true;
    steamiFakeBme280(Wire);
    TEST_ASSERT_TRUE(board.begin(SteamiRelease::V1C));
    TEST_ASSERT_FALSE(board.calibrate(SteamiChip::BME280));
    TEST_ASSERT_EQUAL_UINT32(0, fw.readConfigCommands);
    TEST_ASSERT_FALSE(fw.hung);
}

static void test_calibrate_forced_legacy_does_not_retry_config(void) {
    attachFirmware();
    fw.legacyFirmware = true;
    steamiFakeBme280(Wire);
    TEST_ASSERT_TRUE(board.begin(SteamiRelease::V1C));
    board.wsenHids();  // claimed (and found absent) before the traffic is measured
    TEST_ASSERT_FALSE(board.calibrate(SteamiChip::BME280));
    const uint32_t traffic = Wire.totalTransactions();
    TEST_ASSERT_FALSE(board.calibrate(SteamiChip::WSEN_HIDS));
    TEST_ASSERT_EQUAL_UINT32(traffic, Wire.totalTransactions());
    TEST_ASSERT_EQUAL_UINT32(0, fw.readConfigCommands);
}

static void test_calibrate_unknown_chip_or_absent_chip_is_false(void) {
    attachFirmware();
    fw.store(kBmeOffsetConfig);
    steamiFakeBme280(Wire);
    TEST_ASSERT_FALSE(board.calibrate(SteamiChip::VL53L1X));  // no calibration, no chip
    TEST_ASSERT_FALSE(board.calibrate(SteamiChip::GC9A01));
    TEST_ASSERT_FALSE(board.calibrated(SteamiChip::VL53L1X));
}

static void test_print_info_marks_calibrated_chip(void) {
    attachFirmware();
    fw.store(kBmeOffsetConfig);
    steamiFakeBme280(Wire);
    board.claim(SteamiChip::BME280);
    board.calibrate(SteamiChip::BME280);
    StringPrint p;
    board.printInfo(p);
    TEST_ASSERT_TRUE(p.text.find("BME280 0x76 OK, calibrated\n") != std::string::npos);
}

// --- Motion calibrations (ca, cm) ---

static SteamiFakeIsm330dl imu;

static void attachImu(void) {
    imu = SteamiFakeIsm330dl();
    Wire.attachDevice(ISM330DL_DEFAULT_ADDRESS, &imu);
}

static const char* const kAccelOffsetConfig = "{\"rev\":131,\"ca\":{\"ox\":0.1,\"oy\":0,\"oz\":0}}";
static const char* const kMagnetometerConfig =
    "{\"rev\":131,\"cm\":{\"hx\":100,\"hy\":-50,\"hz\":0,\"sx\":1,\"sy\":1,\"sz\":1}}";
static const char* const kMotionConfig =
    "{\"rev\":131,\"ca\":{\"ox\":0.1,\"oy\":0,\"oz\":0},"
    "\"cm\":{\"hx\":100,\"hy\":-50,\"hz\":0,\"sx\":1,\"sy\":1,\"sz\":1}}";

static constexpr int16_t kOneG = 16393;  // +-2 g: 0.061 mg per LSB

static void test_fake_ism330dl_lets_claim_succeed(void) {
    attachImu();
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SteamiStatus::OK),
                            static_cast<uint8_t>(board.claim(SteamiChip::ISM330DL)));
    TEST_ASSERT_EQUAL_INT(1, imu.resetWrites);
    imu.setAccelRaw(kOneG, 0, static_cast<int16_t>(-kOneG));
    imu.setGyroRaw(1000, 0, 0);
    SteamiISM330DL::Vector3 a{};
    TEST_ASSERT_TRUE(board.ism330dl()->accelerationG(a));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, a.x);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, a.y);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -1.0f, a.z);
    SteamiISM330DL::Vector3 g{};
    TEST_ASSERT_TRUE(board.ism330dl()->gyroscopeDps(g));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 8.75f, g.x);
}

static void test_calibrate_applies_ca(void) {
    attachFirmware();
    fw.store(kAccelOffsetConfig);
    attachImu();
    imu.setAccelRaw(kOneG, 0, 0);
    TEST_ASSERT_TRUE(board.calibrate(SteamiChip::ISM330DL));
    TEST_ASSERT_TRUE(board.calibrated(SteamiChip::ISM330DL));
    SteamiISM330DL::Vector3 a{};
    TEST_ASSERT_TRUE(board.ism330dl()->accelerationG(a));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.9f, a.x);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, a.y);
}

static void test_calibrate_applies_cm(void) {
    attachFirmware();
    fw.store(kMagnetometerConfig);
    steamiFakeLis2mdl(Wire, 300, 200, 100);
    TEST_ASSERT_TRUE(board.calibrate(SteamiChip::LIS2MDL));
    TEST_ASSERT_TRUE(board.calibrated(SteamiChip::LIS2MDL));
    TEST_ASSERT_EQUAL_FLOAT(100.0f, board.lis2mdl()->xOff);
    TEST_ASSERT_EQUAL_FLOAT(-50.0f, board.lis2mdl()->yOff);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, board.lis2mdl()->zOff);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, board.lis2mdl()->xScale);
}

static void test_calibrate_motion_without_keys_changes_nothing(void) {
    attachFirmware();
    fw.store("{\"rev\":131}");
    attachImu();
    imu.setAccelRaw(kOneG, 0, 0);
    steamiFakeLis2mdl(Wire, 300, 200, 100);
    TEST_ASSERT_FALSE(board.calibrate(SteamiChip::ISM330DL));
    TEST_ASSERT_FALSE(board.calibrate(SteamiChip::LIS2MDL));
    TEST_ASSERT_FALSE(board.calibrated(SteamiChip::ISM330DL));
    TEST_ASSERT_FALSE(board.calibrated(SteamiChip::LIS2MDL));
    SteamiISM330DL::Vector3 a{};
    TEST_ASSERT_TRUE(board.ism330dl()->accelerationG(a));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, a.x);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, board.lis2mdl()->xOff);
}

static void test_print_info_marks_ca_and_cm(void) {
    attachFirmware();
    fw.store(kMotionConfig);
    attachImu();
    steamiFakeLis2mdl(Wire, 300, 200, 100);
    TEST_ASSERT_TRUE(board.calibrate(SteamiChip::ISM330DL));
    TEST_ASSERT_TRUE(board.calibrate(SteamiChip::LIS2MDL));
    StringPrint p;
    board.printInfo(p);
    TEST_ASSERT_TRUE(p.text.find("ISM330DL 0x6B OK, calibrated\n") != std::string::npos);
    TEST_ASSERT_TRUE(p.text.find("LIS2MDL 0x1E OK, calibrated\n") != std::string::npos);
}

static void test_report_keeps_twenty_pairs(void) {
    for (uint8_t i = 0; i < 20; i++) {
        board.report("SteamiX", SteamiStatus::NOT_RESPONDING, "d", i);
    }
    for (uint8_t i = 0; i < 20; i++) {
        board.report("SteamiX", SteamiStatus::NOT_RESPONDING, "d", i);
    }
    TEST_ASSERT_EQUAL_UINT32(20, g_lines.size());
}

// Must run last: nothing clears the value the registrar writes.
static void test_registrar_value_and_conflict(void) {
    steamiForceRelease(SteamiRelease::V1C);
    presentHids();
    TEST_ASSERT_TRUE(board.begin());
    TEST_ASSERT_TRUE(board.release() == SteamiRelease::V1C);
    TEST_ASSERT_TRUE(board.origin() == SteamiOrigin::FORCED_DEFINE);
    TEST_ASSERT_EQUAL_UINT32(0, Wire.totalTransactions());
    TEST_ASSERT_FALSE(loggedCode("(E03)"));
    TEST_ASSERT_EQUAL_STRING("[STeaMi] Board V1c (forced by STEAMI_BOARD_V1C)",
                             g_lines.back().c_str());

    restart();
    TEST_ASSERT_TRUE(board.begin(SteamiRelease::V1B));
    TEST_ASSERT_TRUE(board.origin() == SteamiOrigin::FORCED_RUNTIME);

    restart();
    steamiForceRelease(SteamiRelease::V1A);
    TEST_ASSERT_TRUE(board.begin());
    TEST_ASSERT_TRUE(loggedCode("(E03)"));
    TEST_ASSERT_TRUE(board.origin() == SteamiOrigin::FORCED_DEFINE);
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
    RUN_TEST(test_config_131_gives_v1c_without_probing);
    RUN_TEST(test_config_130_is_a_prototype);
    RUN_TEST(test_invalid_revision_reports_e02_and_probes);
    RUN_TEST(test_missing_revision_probes_silently);
    RUN_TEST(test_legacy_firmware_never_gets_a_config_command);
    RUN_TEST(test_bridge_absent_probes);
    RUN_TEST(test_probe_matrix);
    RUN_TEST(test_apds_ids_all_give_v1ab);
    RUN_TEST(test_second_begin_makes_no_traffic);
    RUN_TEST(test_runtime_override_wins_without_traffic);
    RUN_TEST(test_runtime_override_before_begin_makes_no_traffic);
    RUN_TEST(test_queries_before_begin_detect);
    RUN_TEST(test_release_names);
    RUN_TEST(test_banner_and_print_info);
    RUN_TEST(test_begin_then_print_info_logs_banner_and_error_once);
    RUN_TEST(test_print_info_without_logger_replays_error);
    RUN_TEST(test_banner_variants);
    RUN_TEST(test_claim_builds_and_begins_once);
    RUN_TEST(test_accessor_returns_same_instance);
    RUN_TEST(test_not_responding_is_cached);
    RUN_TEST(test_displays_and_none_are_not_started);
    RUN_TEST(test_claim_detects_first);
    RUN_TEST(test_every_accessor_builds_its_slot);
    RUN_TEST(test_v1ab_never_builds_bme280);
    RUN_TEST(test_print_info_lists_chips);
    RUN_TEST(test_print_info_lists_the_chips_of_the_release);
    RUN_TEST(test_print_info_replays_last_error);
    RUN_TEST(test_print_info_logger_sends_banner_once);
    RUN_TEST(test_new_message_is_replayed_once);
    RUN_TEST(test_print_info_does_not_repeat_its_own_error);
    RUN_TEST(test_end_detaches_expander_interrupt);
    RUN_TEST(test_end_destroys_slots);
    RUN_TEST(test_calibrate_applies_bme_key);
    RUN_TEST(test_calibrate_without_key_changes_nothing);
    RUN_TEST(test_calibrate_is_once_per_chip);
    RUN_TEST(test_calibrate_forced_loads_config_on_demand);
    RUN_TEST(test_calibrate_forced_legacy_no_config_command);
    RUN_TEST(test_calibrate_forced_legacy_does_not_retry_config);
    RUN_TEST(test_calibrate_unknown_chip_or_absent_chip_is_false);
    RUN_TEST(test_print_info_marks_calibrated_chip);
    RUN_TEST(test_fake_ism330dl_lets_claim_succeed);
    RUN_TEST(test_calibrate_applies_ca);
    RUN_TEST(test_calibrate_applies_cm);
    RUN_TEST(test_calibrate_motion_without_keys_changes_nothing);
    RUN_TEST(test_print_info_marks_ca_and_cm);
    RUN_TEST(test_report_keeps_twenty_pairs);
    RUN_TEST(test_registrar_value_and_conflict);  // last: the registrar value cannot be cleared
    return UNITY_END();
}
