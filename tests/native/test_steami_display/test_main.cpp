// SPDX-License-Identifier: GPL-3.0-or-later

#include <Arduino.h>
#include <Wire.h>
#include <math.h>
#include <stdint.h>
#include <string.h>
#include <unity.h>

#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

#include "SteamiBoard.h"
#include "SteamiColor.h"
#include "SteamiDisplay.h"
#include "SteamiDisplayBackend.h"
#include "SteamiFont.h"
#include "SteamiMemoryBackend.h"
#include "SteamiRgb.h"
#include "SteamiTrig.h"
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

static void assertStatus(SteamiStatus expected, SteamiStatus actual) {
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(expected), static_cast<uint8_t>(actual));
}

// Values printed by golden_colors.py.
static void test_rgb565_of_every_colour(void) {
    TEST_ASSERT_EQUAL_HEX16(0x0000, STEAMI_BLACK.rgb565());
    TEST_ASSERT_EQUAL_HEX16(0x632C, STEAMI_DARK.rgb565());
    TEST_ASSERT_EQUAL_HEX16(0x9CD3, STEAMI_GRAY.rgb565());
    TEST_ASSERT_EQUAL_HEX16(0xBDD7, STEAMI_LIGHT.rgb565());
    TEST_ASSERT_EQUAL_HEX16(0xFFFF, STEAMI_WHITE.rgb565());
    TEST_ASSERT_EQUAL_HEX16(0x77EE, STEAMI_GREEN.rgb565());
    TEST_ASSERT_EQUAL_HEX16(0xFAAA, STEAMI_RED.rgb565());
    TEST_ASSERT_EQUAL_HEX16(0x52BF, STEAMI_BLUE.rgb565());
    TEST_ASSERT_EQUAL_HEX16(0xFFEA, STEAMI_YELLOW.rgb565());
    TEST_ASSERT_EQUAL_HEX16(0x3186, STEAMI_GRID_DARK.rgb565());
}

static void test_gray4_of_every_colour(void) {
    TEST_ASSERT_EQUAL_UINT8(0, STEAMI_BLACK.gray4());
    TEST_ASSERT_EQUAL_UINT8(3, STEAMI_GRID_DARK.gray4());
    TEST_ASSERT_EQUAL_UINT8(6, STEAMI_DARK.gray4());
    TEST_ASSERT_EQUAL_UINT8(9, STEAMI_GRAY.gray4());
    TEST_ASSERT_EQUAL_UINT8(11, STEAMI_LIGHT.gray4());
    TEST_ASSERT_EQUAL_UINT8(15, STEAMI_WHITE.gray4());
    TEST_ASSERT_EQUAL_UINT8(12, STEAMI_GREEN.gray4());
    TEST_ASSERT_EQUAL_UINT8(8, STEAMI_RED.gray4());
    TEST_ASSERT_EQUAL_UINT8(14, STEAMI_YELLOW.gray4());
    TEST_ASSERT_EQUAL_UINT8(6, STEAMI_BLUE.gray4());
}

static void test_gray_levels_round_trip(void) {
    for (uint8_t level = 0; level <= 15; level++) {
        TEST_ASSERT_EQUAL_UINT8(level, SteamiRgb::gray(level).gray4());
    }
    TEST_ASSERT_TRUE(SteamiRgb::gray(16) == SteamiRgb::gray(15));
    TEST_ASSERT_TRUE(SteamiRgb::gray(15) == STEAMI_WHITE);
    TEST_ASSERT_TRUE(SteamiRgb::gray(3) == STEAMI_GRID_DARK);
    TEST_ASSERT_TRUE(SteamiRgb::gray(6) == STEAMI_DARK);
}

static void test_from_rgb565_and_equality(void) {
    TEST_ASSERT_TRUE(SteamiRgb::fromRgb565(0xFAAA) == STEAMI_RED);
    const uint16_t values[] = {0, 0x1234, 0xFFFF};
    for (uint16_t v : values) {
        TEST_ASSERT_EQUAL_HEX16(v, SteamiRgb::fromRgb565(v).rgb565());
    }
    TEST_ASSERT_TRUE(STEAMI_BLUE != STEAMI_DARK);
    TEST_ASSERT_FALSE(STEAMI_BLUE == STEAMI_DARK);
}

static_assert(STEAMI_RED.gray4() == 8, "gray4 is constexpr");
static_assert(sizeof(SteamiRgb) == 2, "one RGB565 word");

static void test_color_rgb_scales_largest_channel(void) {
    steamiFakeApds9960(Wire, 1234, 100, 200, 300);
    SteamiColor color(board);
    TEST_ASSERT_TRUE(color.rgb() == SteamiRgb(85, 170, 255));
}

static void test_color_rgb_rounds_to_nearest(void) {
    steamiFakeApds9960(Wire, 1, 100, 50, 0);
    SteamiColor color(board);
    TEST_ASSERT_TRUE(color.rgb() == SteamiRgb(255, 128, 0));  // 127.5 rounds up (g6 32, not 31)
}

static void test_color_rgb_zero_is_black(void) {
    steamiFakeApds9960(Wire, 0, 0, 0, 0);
    SteamiColor color(board);
    TEST_ASSERT_TRUE(color.rgb() == STEAMI_BLACK);
}

static void test_color_rgb_unavailable_is_black(void) {
    TEST_ASSERT_TRUE(board.begin(SteamiRelease::V1C));
    SteamiColor color(board);
    TEST_ASSERT_TRUE(color.rgb() == STEAMI_BLACK);
    assertStatus(SteamiStatus::NOT_ON_THIS_BOARD, color.status());
    TEST_ASSERT_EQUAL_size_t(1, countLogged("(E20)"));
}

static void test_sin_table_matches_rounded_sine(void) {
    for (int32_t d = 0; d <= 90; d++) {  // test code may use double
        TEST_ASSERT_EQUAL_INT16((int16_t)lround(16384.0 * sin(d * M_PI / 180.0)), steamiSinQ14(d));
    }
}

static void test_sin_symmetries_and_normalisation(void) {
    for (int32_t d = -720; d <= 720; d++) {
        TEST_ASSERT_EQUAL_INT16(-steamiSinQ14(d), steamiSinQ14(-d));
        TEST_ASSERT_EQUAL_INT16(steamiSinQ14(d), steamiSinQ14(180 - d));
        TEST_ASSERT_EQUAL_INT16(steamiSinQ14(d), steamiSinQ14(d + 360));
        TEST_ASSERT_EQUAL_INT16(steamiSinQ14(d + 90), steamiCosQ14(d));
    }
    TEST_ASSERT_EQUAL_INT16(16384, steamiSinQ14(90));
    TEST_ASSERT_EQUAL_INT16(-16384, steamiSinQ14(270));
    TEST_ASSERT_EQUAL_INT16(-16384, steamiSinQ14(-450));
    // The two calls below must not trigger a UBSan report (checked by native_asan).
    const int16_t lowest = steamiSinQ14(INT32_MIN);
    const int16_t highest = steamiCosQ14(INT32_MAX);
    TEST_ASSERT_TRUE(lowest >= -16384 && lowest <= 16384);
    TEST_ASSERT_TRUE(highest >= -16384 && highest <= 16384);
}

static void test_round_q14(void) {
    TEST_ASSERT_EQUAL_INT32(1, steamiRoundQ14(8192));
    TEST_ASSERT_EQUAL_INT32(0, steamiRoundQ14(8191));
    TEST_ASSERT_EQUAL_INT32(-1, steamiRoundQ14(-8192));
    TEST_ASSERT_EQUAL_INT32(0, steamiRoundQ14(-8191));
    TEST_ASSERT_EQUAL_INT32(100, steamiRoundQ14(16384 * 100));
    TEST_ASSERT_EQUAL_INT32(-4, steamiRoundQ14(-(16384 * 3 + 8192)));
}

static void test_isqrt(void) {
    for (uint32_t v = 0; v <= 70000; v++) {
        const uint64_t r = steamiIsqrt(v);
        TEST_ASSERT_TRUE(r * r <= v);
        TEST_ASSERT_TRUE(v < (r + 1) * (r + 1));
    }
    TEST_ASSERT_EQUAL_UINT32(65535, steamiIsqrt(4294967295u));
    TEST_ASSERT_EQUAL_UINT32(65535, steamiIsqrt(4294836225u));
}

static uint32_t weightedSum(const uint8_t* (*glyph)(uint8_t), uint8_t cols) {
    uint32_t sum = 0, i = 0;
    for (uint8_t c = 32; c <= 126; c++)
        for (uint8_t k = 0; k < cols; k++)
            sum += ++i * glyph(c)[k];
    return sum;
}

static void test_8x8_table_is_micropython_font(void) {
    TEST_ASSERT_EQUAL_UINT32(16797326,
                             weightedSum(steamiGlyph8x8, 8));  // petme128 @ b0a1b60, 32..126
    const uint8_t a[8] = {0x00, 0x7c, 0x7e, 0x0b, 0x0b, 0x7e, 0x7c, 0x00};
    TEST_ASSERT_EQUAL_HEX8_ARRAY(a, steamiGlyph8x8('A'), 8);
    TEST_ASSERT_EQUAL_HEX8(0x60, steamiGlyph8x8('.')[3]);
    TEST_ASSERT_EQUAL_HEX8(0x60, steamiGlyph8x8('.')[4]);
}

static void test_5x7_table_is_driver_font(void) {
    TEST_ASSERT_EQUAL_UINT32(5520505, weightedSum(steamiGlyph5x7, 5));  // kFont5x7, 32..126
    const uint8_t a[5] = {0x7E, 0x11, 0x11, 0x11, 0x7E};
    TEST_ASSERT_EQUAL_HEX8_ARRAY(a, steamiGlyph5x7('A'), 5);
}

static void test_out_of_range_codes_give_question_mark(void) {
    const uint8_t codes[] = {0, 10, 31, 128, 255};
    for (uint8_t code : codes) {
        TEST_ASSERT_EQUAL_PTR(steamiGlyph8x8('?'), steamiGlyph8x8(code));
        TEST_ASSERT_EQUAL_PTR(steamiGlyph5x7('?'), steamiGlyph5x7(code));
    }
}

static void test_degree_glyphs(void) {
    const uint8_t big[8] = {0x00, 0x06, 0x09, 0x09, 0x06, 0x00, 0x00, 0x00};
    const uint8_t small[5] = {0x00, 0x06, 0x09, 0x09, 0x06};
    TEST_ASSERT_EQUAL_UINT8(127, STEAMI_GLYPH_DEGREE);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(big, steamiGlyph8x8(STEAMI_GLYPH_DEGREE), 8);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(small, steamiGlyph5x7(STEAMI_GLYPH_DEGREE), 5);
}

// Decodes s and checks every glyph, the final 0, the pointer left on the NUL
// (a further call returns 0 without moving) and steamiGlyphCount().
static void assertGlyphs(const char* s, std::initializer_list<uint8_t> expected) {
    const char* p = s;
    for (uint8_t code : expected) {
        TEST_ASSERT_EQUAL_UINT8(code, steamiNextGlyph(p));
    }
    TEST_ASSERT_EQUAL_UINT8(0, steamiNextGlyph(p));
    TEST_ASSERT_EQUAL_PTR(s + strlen(s), p);
    TEST_ASSERT_EQUAL_UINT8(0, steamiNextGlyph(p));
    TEST_ASSERT_EQUAL_PTR(s + strlen(s), p);
    TEST_ASSERT_EQUAL_size_t(expected.size(), steamiGlyphCount(s));
}

static std::string utf8(uint32_t cp) {
    std::string out;
    if (cp < 0x80) {
        out += static_cast<char>(cp);
    } else if (cp < 0x800) {
        out += static_cast<char>(0xC0 | (cp >> 6));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else {
        out += static_cast<char>(0xE0 | (cp >> 12));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    }
    return out;
}

static void test_utf8_degree_and_folding(void) {
    assertGlyphs(
        "\xC2\xB0"
        "C",
        {STEAMI_GLYPH_DEGREE, 'C'});  // °C
    assertGlyphs("\xC3\xA9", {'e'});  // é
    assertGlyphs("\xC3\x89", {'E'});  // É
    assertGlyphs("\xC5\xB8", {'Y'});  // Ÿ
    assertGlyphs("\xC3\xBF", {'y'});  // ÿ
    struct Fold {
        uint32_t cp;
        char base;
    };
    const Fold folds[] = {
        {0xE0, 'a'}, {0xE2, 'a'}, {0xE4, 'a'}, {0xE7, 'c'},  {0xE8, 'e'}, {0xE9, 'e'}, {0xEA, 'e'},
        {0xEB, 'e'}, {0xEE, 'i'}, {0xEF, 'i'}, {0xF4, 'o'},  {0xF6, 'o'}, {0xF9, 'u'}, {0xFB, 'u'},
        {0xFC, 'u'}, {0xFF, 'y'}, {0xC0, 'A'}, {0xC2, 'A'},  {0xC4, 'A'}, {0xC7, 'C'}, {0xC8, 'E'},
        {0xC9, 'E'}, {0xCA, 'E'}, {0xCB, 'E'}, {0xCE, 'I'},  {0xCF, 'I'}, {0xD4, 'O'}, {0xD6, 'O'},
        {0xD9, 'U'}, {0xDB, 'U'}, {0xDC, 'U'}, {0x178, 'Y'},
    };
    for (const Fold& f : folds) {
        const std::string s = utf8(f.cp);
        assertGlyphs(s.c_str(), {static_cast<uint8_t>(f.base)});
    }
    // Latin-1 letters next to the folded ones stay unsupported.
    const uint32_t others[] = {0xC1, 0xC3, 0xC5, 0xD1, 0xE1, 0xE3, 0xE5, 0xF1, 0xF8, 0xFD, 0x153};
    for (uint32_t cp : others) {
        const std::string s = utf8(cp);
        assertGlyphs(s.c_str(), {'?'});
    }
    assertGlyphs("Temp\xC3\xA9rature", {'T', 'e', 'm', 'p', 'e', 'r', 'a', 't', 'u', 'r', 'e'});
    TEST_ASSERT_EQUAL_size_t(11, steamiGlyphCount("Temp\xC3\xA9rature"));
}

static void assertAllQuestionMarks(const char* s, size_t n) {
    const char* p = s;
    for (size_t i = 0; i < n; i++) {
        const uint8_t code = steamiNextGlyph(p);
        TEST_ASSERT_EQUAL_UINT8('?', code);
        TEST_ASSERT_NOT_EQUAL('A', code);
    }
    TEST_ASSERT_EQUAL_UINT8(0, steamiNextGlyph(p));
    TEST_ASSERT_EQUAL_PTR(s + strlen(s), p);
    TEST_ASSERT_EQUAL_size_t(n, steamiGlyphCount(s));
}

static void test_utf8_invalid_bytes(void) {
    assertAllQuestionMarks("\xFF", 1);
    assertGlyphs(
        "\x80"
        "A",
        {'?', 'A'});
    assertAllQuestionMarks("\xC0\xAF", 2);          // overlong '/'
    assertAllQuestionMarks("\xC1\x81", 2);          // overlong 'A'
    assertAllQuestionMarks("\xE0\x81\x81", 3);      // overlong 'A'
    assertAllQuestionMarks("\xF0\x80\x81\x81", 4);  // overlong 'A'
    assertAllQuestionMarks("\xED\xA0\x80", 3);      // surrogate U+D800
    assertAllQuestionMarks("\xF4\x90\x80\x80", 4);  // U+110000
    assertAllQuestionMarks("\xF5\x80\x80\x80", 4);  // lead byte above F4
    assertAllQuestionMarks("\xE2\x82\xAC", 1);      // valid but unsupported: euro sign
    assertAllQuestionMarks("\xF0\x9F\x98\x80", 1);  // valid but unsupported: U+1F600
    assertAllQuestionMarks("\xED\x9F\xBF", 1);      // U+D7FF, last before the surrogates
    assertAllQuestionMarks("\xF4\x8F\xBF\xBF", 1);  // U+10FFFF
    assertAllQuestionMarks("\n", 1);
    assertAllQuestionMarks("\x1F", 1);
    assertAllQuestionMarks("\x7F", 1);
    assertGlyphs(" ~", {' ', '~'});
}

static void test_utf8_truncated_sequences(void) {
    assertGlyphs("A\xC3", {'A', '?'});
    assertGlyphs("\xE2\x82", {'?', '?'});
    assertGlyphs("\xF0\x9F", {'?', '?'});
    assertGlyphs("\xF0\x9F\x98", {'?', '?', '?'});
    assertGlyphs(
        "\xE2\x82"
        "A",
        {'?', '?', 'A'});
    assertGlyphs(
        "\xC3"
        "A",
        {'?', 'A'});
}

static void test_glyph_count_nullptr_and_empty(void) {
    TEST_ASSERT_EQUAL_size_t(0, steamiGlyphCount(nullptr));
    TEST_ASSERT_EQUAL_size_t(0, steamiGlyphCount(""));
    const char* p = nullptr;
    TEST_ASSERT_EQUAL_UINT8(0, steamiNextGlyph(p));
    TEST_ASSERT_NULL(p);
    const char* empty = "";
    p = empty;
    TEST_ASSERT_EQUAL_UINT8(0, steamiNextGlyph(p));
    TEST_ASSERT_EQUAL_PTR(empty, p);
}

static void test_memory_backend_counts_out_of_bounds(void) {
    SteamiMemoryBackend mem(128, 128);
    mem.pixel(-1, 0, 0xFFFF);
    mem.pixel(128, 5, 0xFFFF);
    mem.fillRect(120, 0, 9, 1, 0xFFFF);
    mem.fillRect(0, 0, 0, 1, 0xFFFF);
    TEST_ASSERT_EQUAL_UINT32(4, mem.outOfBounds());
    TEST_ASSERT_EQUAL_size_t(0, mem.litCount());
}

static void test_memory_backend_grey_mode_stores_oled_levels(void) {
    SteamiMemoryBackend mem(128, 128, false);
    mem.pixel(1, 1, STEAMI_BLUE.rgb565());
    TEST_ASSERT_EQUAL_HEX16(SteamiRgb::gray(6).rgb565(), mem.at(1, 1));
    TEST_ASSERT_FALSE(mem.isColor());
}

static void test_memory_backend_writes_ppm(void) {
    SteamiMemoryBackend mem(2, 1);
    mem.pixel(0, 0, STEAMI_RED.rgb565());
    const char* path = "steami_display_test.ppm";
    TEST_ASSERT_TRUE(mem.writePpm(path));
    FILE* f = fopen(path, "rb");
    TEST_ASSERT_NOT_NULL(f);
    unsigned char buf[64] = {};
    const size_t n = fread(buf, 1, sizeof buf, f);
    fclose(f);
    remove(path);
    const char header[] = "P6\n2 1\n255\n";
    const size_t h = sizeof header - 1;
    TEST_ASSERT_EQUAL_size_t(h + 6, n);
    TEST_ASSERT_EQUAL_MEMORY(header, buf, h);
    const unsigned char expected[6] = {255, 85, 82, 0, 0, 0};
    TEST_ASSERT_EQUAL_MEMORY(expected, buf + h, 6);
}

static void test_board_without_backend_is_unknown_board(void) {
    TEST_ASSERT_NULL(board.display());
    assertStatus(SteamiStatus::UNKNOWN_BOARD, board.beginDisplay());
    TEST_ASSERT_EQUAL_size_t(0, g_lines.size());  // the board itself reports nothing
}

static void test_board_begins_injected_backend_once(void) {
    SteamiMemoryBackend mem(240, 240);
    board.setDisplayBackend(mem);
    TEST_ASSERT_EQUAL_PTR(&mem, board.display());
    assertStatus(SteamiStatus::OK, board.beginDisplay());
    assertStatus(SteamiStatus::OK, board.beginDisplay());
    TEST_ASSERT_EQUAL_UINT32(1, mem.beginCalls());
}

static void test_board_failed_begin_is_no_memory(void) {
    SteamiMemoryBackend mem(128, 128, true, false);
    board.setDisplayBackend(mem);
    assertStatus(SteamiStatus::NO_MEMORY, board.beginDisplay());
    assertStatus(SteamiStatus::NO_MEMORY, board.beginDisplay());
    TEST_ASSERT_EQUAL_UINT32(1, mem.beginCalls());  // a failed begin is cached, not retried
    TEST_ASSERT_EQUAL_size_t(0, g_lines.size());    // the board itself reports nothing
}

static void test_set_backend_after_begin_display_is_ignored(void) {
    SteamiMemoryBackend a(128, 128);
    SteamiMemoryBackend b(128, 128);
    board.setDisplayBackend(a);
    assertStatus(SteamiStatus::OK, board.beginDisplay());
    board.setDisplayBackend(b);
    TEST_ASSERT_EQUAL_PTR(&a, board.display());
    TEST_ASSERT_EQUAL_UINT32(0, b.beginCalls());
}

static void test_unknown_board_locks_injection_until_end(void) {
    SteamiMemoryBackend a(128, 128);
    assertStatus(SteamiStatus::UNKNOWN_BOARD, board.beginDisplay());
    board.setDisplayBackend(a);
    TEST_ASSERT_NULL(board.display());
    TEST_ASSERT_EQUAL_UINT32(0, a.beginCalls());
    board.end();
    board.setDisplayBackend(a);
    assertStatus(SteamiStatus::OK, board.beginDisplay());
    TEST_ASSERT_EQUAL_UINT32(1, a.beginCalls());
}

static void test_board_end_forgets_backend_and_status(void) {
    SteamiMemoryBackend a(128, 128);
    board.setDisplayBackend(a);
    assertStatus(SteamiStatus::OK, board.beginDisplay());
    board.end();
    TEST_ASSERT_NULL(board.display());
    board.setDisplayBackend(a);
    assertStatus(SteamiStatus::OK, board.beginDisplay());
    TEST_ASSERT_EQUAL_UINT32(2, a.beginCalls());
}

// ---- SteamiDisplay core ----

[[maybe_unused]] static constexpr SteamiDisplay kConstInitDisplay(board);  // constant-initialised
static_assert(sizeof(SteamiDisplay) <= 3 * sizeof(void*), "12 B on the 32-bit target");
static_assert(SteamiDisplay::CHAR_W == 8 && SteamiDisplay::CHAR_H == 8, "5.3");

/// The protected face of SteamiDisplay, as SteamiScreen will use it.
class TestDisplay : public SteamiDisplay {
   public:
    explicit TestDisplay(SteamiBoard& b) : SteamiDisplay(b, "SteamiTestDisplay") {}
    using SteamiDisplay::strokeCircle;
    using SteamiDisplay::glyph;
    using SteamiDisplay::strokeLine;
    using SteamiDisplay::textSmall;
    using SteamiDisplay::textSmallWidth;
};

/// Runs `check` on a 128 colour, a 128 grey, a 240 colour and a 240 grey panel, each with a fresh
/// board state, backend and display, then asserts that nothing reached the backend off the panel.
static void forEachPanel(void (*check)(SteamiMemoryBackend&, TestDisplay&)) {
    const int16_t sizes[] = {128, 240};
    for (int16_t size : sizes) {
        for (bool color : {true, false}) {
            board.end();
            board.setLogger(&capture);
            SteamiMemoryBackend mem(size, size, color);
            board.setDisplayBackend(mem);
            TestDisplay d(board);
            check(mem, d);
            TEST_ASSERT_EQUAL_UINT32(0, mem.outOfBounds());
        }
    }
    board.end();
}

static std::vector<uint16_t> snapshot(const SteamiMemoryBackend& mem) {
    return mem.pixels();
}

static void expectGeometry(int16_t w, int16_t h, int16_t cx, int16_t cy, int16_t r) {
    board.end();
    board.setLogger(&capture);
    SteamiMemoryBackend mem(w, h);
    board.setDisplayBackend(mem);
    SteamiDisplay d(board);
    TEST_ASSERT_EQUAL_INT16(w, d.width());
    TEST_ASSERT_EQUAL_INT16(h, d.height());
    TEST_ASSERT_EQUAL_INT16(cx, d.centerX());
    TEST_ASSERT_EQUAL_INT16(cy, d.centerY());
    TEST_ASSERT_EQUAL_INT16(r, d.radius());
    TEST_ASSERT_TRUE(d.isColor());
    TEST_ASSERT_EQUAL_UINT32(0, mem.beginCalls());
}

static void test_geometry_before_begin(void) {
    expectGeometry(128, 128, 64, 64, 64);
    expectGeometry(240, 240, 120, 120, 120);
    expectGeometry(200, 100, 100, 50, 50);
    expectGeometry(100, 200, 50, 100, 50);

    board.end();
    SteamiDisplay fresh(board);
    assertStatus(SteamiStatus::NOT_STARTED, fresh.status());
    TEST_ASSERT_EQUAL_STRING("not started", fresh.error());
    TEST_ASSERT_FALSE(fresh.isAvailable());
    TEST_ASSERT_EQUAL_INT16(0, fresh.width());
    TEST_ASSERT_EQUAL_INT16(0, fresh.height());
    TEST_ASSERT_EQUAL_INT16(0, fresh.radius());
    TEST_ASSERT_FALSE(fresh.isColor());
}

static void test_no_backend_reports_e30_once(void) {
    SteamiDisplay a(board), b(board);
    TEST_ASSERT_FALSE(a.begin());
    TEST_ASSERT_FALSE(b.begin());
    assertStatus(SteamiStatus::UNKNOWN_BOARD, a.status());
    TEST_ASSERT_EQUAL_STRING("unknown board", a.error());
    TEST_ASSERT_FALSE(a.isAvailable());
    TEST_ASSERT_EQUAL_size_t(1, g_lines.size());
    TEST_ASSERT_EQUAL_STRING(
        "[STeaMi] SteamiDisplay: cannot tell if this board is a V1a/V1b or a V1c. "
        "Add #define STEAMI_BOARD_V1C (or V1B) before #include <STeaMi.h>. (E30)",
        g_lines[0].c_str());
}

static void test_failed_begin_reports_e31_and_draws_nothing(void) {
    SteamiMemoryBackend mem(240, 240, true, false);
    board.setDisplayBackend(mem);
    SteamiDisplay d(board);
    TEST_ASSERT_FALSE(d.begin());
    assertStatus(SteamiStatus::NO_MEMORY, d.status());
    TEST_ASSERT_EQUAL_STRING("no memory", d.error());
    TEST_ASSERT_EQUAL_size_t(1, g_lines.size());
    TEST_ASSERT_EQUAL_STRING(
        "[STeaMi] SteamiDisplay: not enough RAM for the colour screen (115200 bytes in one block). "
        "Call screen.begin() first in setup(). (E31)",
        g_lines[0].c_str());
    d.clear(STEAMI_WHITE);
    d.pixel(1, 1);
    d.show();
    TEST_ASSERT_EQUAL_UINT32(0, mem.fillCalls());
    TEST_ASSERT_EQUAL_UINT32(0, mem.pixelCalls());
    TEST_ASSERT_EQUAL_UINT32(0, mem.fillRectCalls());
    TEST_ASSERT_EQUAL_UINT32(0, mem.showCalls());
    TEST_ASSERT_EQUAL_size_t(0, mem.litCount());
    TEST_ASSERT_EQUAL_UINT32(1, mem.beginCalls());
}

static void test_auto_begin_on_first_draw(void) {
    SteamiMemoryBackend mem(128, 128);
    board.setDisplayBackend(mem);
    SteamiDisplay d(board);
    d.pixel(3, 4, STEAMI_RED);
    TEST_ASSERT_EQUAL_UINT32(1, mem.beginCalls());
    TEST_ASSERT_EQUAL_HEX16(STEAMI_RED.rgb565(), mem.at(3, 4));
    TEST_ASSERT_TRUE(d.isAvailable());
    assertStatus(SteamiStatus::OK, d.status());
    TEST_ASSERT_EQUAL_STRING("", d.error());
    d.begin();
    d.pixel(5, 5);
    TEST_ASSERT_EQUAL_UINT32(1, mem.beginCalls());
}

static void test_backend_accessor(void) {
    SteamiDisplay d(board);
    TEST_ASSERT_NULL(d.backend());
    SteamiMemoryBackend mem(128, 128);
    board.setDisplayBackend(mem);
    TEST_ASSERT_EQUAL_PTR(&mem, d.backend());
    TEST_ASSERT_EQUAL_UINT32(0, mem.beginCalls());
}

static void test_default_board_is_steami(void) {
    Steami.end();
    Steami.setLogger(&capture);
    SteamiMemoryBackend mem(128, 128);
    Steami.setDisplayBackend(mem);
    SteamiDisplay d;
    d.pixel(1, 1);
    TEST_ASSERT_TRUE(mem.lit(1, 1));
    Steami.end();
    Steami.setLogger(nullptr);
}

static void pixelClippingCheck(SteamiMemoryBackend& mem, TestDisplay& d) {
    d.pixel(-1, 0);
    d.pixel(0, -1);
    d.pixel(300, 5);
    d.pixel(5, 32767);
    d.pixel(d.width(), 0);
    d.pixel(0, d.height());
    d.pixel(-32768, -32768);
    TEST_ASSERT_EQUAL_size_t(0, mem.litCount());
    d.pixel(0, 0);
    d.pixel(static_cast<int16_t>(d.width() - 1), static_cast<int16_t>(d.height() - 1));
    TEST_ASSERT_TRUE(mem.lit(0, 0));
    TEST_ASSERT_TRUE(
        mem.lit(static_cast<int16_t>(d.width() - 1), static_cast<int16_t>(d.height() - 1)));
    TEST_ASSERT_EQUAL_UINT32(2, mem.pixelCalls());
}

static void test_pixel_clipping(void) {
    forEachPanel(pixelClippingCheck);
}

static void test_primitives_never_lift_colours(void) {  // 5.10
    SteamiMemoryBackend mem(128, 128, false);
    board.setDisplayBackend(mem);
    SteamiDisplay d(board);
    d.clear(STEAMI_DARK);
    d.pixel(1, 1, STEAMI_BLUE);
    TEST_ASSERT_EQUAL_HEX16(STEAMI_DARK.rgb565(), mem.at(1, 1));
    TEST_ASSERT_EQUAL_HEX16(STEAMI_DARK.rgb565(), mem.at(2, 2));
}

static void test_clear_show_and_controls_forward(void) {
    SteamiMemoryBackend mem(128, 128);
    board.setDisplayBackend(mem);
    SteamiDisplay d(board);
    d.clear(STEAMI_DARK);
    const std::vector<uint16_t> snap = snapshot(mem);
    for (uint16_t p : snap) {
        TEST_ASSERT_EQUAL_HEX16(STEAMI_DARK.rgb565(), p);
    }
    TEST_ASSERT_EQUAL_UINT32(1, mem.fillCalls());
    d.clear();
    TEST_ASSERT_EQUAL_size_t(0, mem.litCount());
    d.show();
    TEST_ASSERT_EQUAL_UINT32(1, mem.showCalls());
    d.rotate(true);
    TEST_ASSERT_TRUE(mem.upsideDown());
    d.invert(true);
    TEST_ASSERT_TRUE(mem.inverted());
    d.powerOff();
    TEST_ASSERT_FALSE(mem.poweredOn());
    d.powerOn();
    TEST_ASSERT_TRUE(mem.poweredOn());
}

static void test_two_objects_share_one_backend(void) {  // Review Focus 3
    SteamiMemoryBackend mem(128, 128);
    board.setDisplayBackend(mem);
    SteamiDisplay a(board);
    TestDisplay b(board);
    a.pixel(1, 1);
    b.pixel(2, 2);
    TEST_ASSERT_TRUE(mem.lit(1, 1));
    TEST_ASSERT_TRUE(mem.lit(2, 2));
    TEST_ASSERT_EQUAL_UINT32(1, mem.beginCalls());
}

static void test_drawing_after_board_end_does_nothing(void) {  // Review Focus 3
    SteamiMemoryBackend a(128, 128);
    board.setDisplayBackend(a);
    SteamiDisplay d(board);
    TEST_ASSERT_TRUE(d.begin());
    board.end();
    board.setLogger(&capture);
    d.pixel(1, 1);
    d.clear(STEAMI_WHITE);
    d.show();
    TEST_ASSERT_EQUAL_INT16(0, d.width());
    TEST_ASSERT_NULL(d.backend());
    TEST_ASSERT_EQUAL_UINT32(0, a.fillCalls());
    TEST_ASSERT_EQUAL_UINT32(0, a.pixelCalls());
    TEST_ASSERT_EQUAL_UINT32(0, a.fillRectCalls());
    TEST_ASSERT_EQUAL_UINT32(0, a.showCalls());
    // Those draws ran beginDisplay() with no backend, which cached UNKNOWN_BOARD and locked
    // injection: end again, inject a new backend, and the same object draws into it.
    board.end();
    board.setLogger(&capture);
    SteamiMemoryBackend b(128, 128);
    board.setDisplayBackend(b);
    d.pixel(1, 1);
    TEST_ASSERT_EQUAL_UINT32(1, b.beginCalls());
    TEST_ASSERT_TRUE(b.lit(1, 1));
}

static const SteamiRgb kInk = STEAMI_WHITE;

static void hlineVlineRectCheck(SteamiMemoryBackend& mem, TestDisplay& d) {
    d.clear();
    mem.clearCounters();
    d.hline(10, 5, 4, kInk);
    TEST_ASSERT_EQUAL_size_t(4, mem.litCount());
    for (int16_t x = 10; x < 14; x++) {
        TEST_ASSERT_TRUE(mem.lit(x, 5));
    }
    TEST_ASSERT_EQUAL_UINT32(1, mem.fillRectCalls());
    TEST_ASSERT_EQUAL_UINT32(0, mem.pixelCalls());
    d.clear();
    mem.clearCounters();
    d.vline(7, 20, 3, kInk);
    TEST_ASSERT_EQUAL_size_t(3, mem.litCount());
    for (int16_t y = 20; y < 23; y++) {
        TEST_ASSERT_TRUE(mem.lit(7, y));
    }
    TEST_ASSERT_EQUAL_UINT32(1, mem.fillRectCalls());
    TEST_ASSERT_EQUAL_UINT32(0, mem.pixelCalls());

    d.clear();
    mem.clearCounters();
    d.rect(10, 10, 5, 4, kInk);
    TEST_ASSERT_EQUAL_size_t(14, mem.litCount());
    TEST_ASSERT_EQUAL_UINT64(14, mem.areaDrawn());
    TEST_ASSERT_TRUE(mem.fillRectCalls() <= 4);
    TEST_ASSERT_FALSE(mem.lit(11, 11));
    TEST_ASSERT_TRUE(mem.lit(10, 11));
    TEST_ASSERT_TRUE(mem.lit(14, 12));
    d.clear();
    mem.clearCounters();
    d.rect(10, 10, 5, 4, kInk, true);
    TEST_ASSERT_EQUAL_size_t(20, mem.litCount());
    TEST_ASSERT_EQUAL_UINT32(1, mem.fillRectCalls());
}
static void test_hline_vline_rect(void) {
    forEachPanel(hlineVlineRectCheck);
}

static void lineSymmetryCheck(SteamiMemoryBackend& mem, TestDisplay& d) {
    d.clear();
    d.line(0, 0, 10, 4, kInk);
    TEST_ASSERT_EQUAL_size_t(11, mem.litCount());
    TEST_ASSERT_TRUE(mem.lit(0, 0));
    TEST_ASSERT_TRUE(mem.lit(10, 4));
    const std::vector<uint16_t> forward = snapshot(mem);
    d.clear();
    d.line(10, 4, 0, 0, kInk);
    TEST_ASSERT_TRUE(forward == snapshot(mem));
    d.clear();
    d.line(5, 5, 5, 5, kInk);
    TEST_ASSERT_EQUAL_size_t(1, mem.litCount());
    TEST_ASSERT_TRUE(mem.lit(5, 5));
    // Same x, reversed y: also the same pixels.
    d.clear();
    d.line(20, 30, 20, 10, kInk);
    const std::vector<uint16_t> up = snapshot(mem);
    d.clear();
    d.line(20, 10, 20, 30, kInk);
    TEST_ASSERT_TRUE(up == snapshot(mem));
    TEST_ASSERT_EQUAL_size_t(21, mem.litCount());
}
static void test_line_endpoints_and_symmetry(void) {
    forEachPanel(lineSymmetryCheck);
}

static void lineCrossingCheck(SteamiMemoryBackend& mem, TestDisplay& d) {
    d.clear();
    d.line(-50, -50, 300, 300, kInk);
    const int16_t n = d.width();
    TEST_ASSERT_EQUAL_size_t(static_cast<size_t>(n), mem.litCount());
    for (int16_t i = 0; i < n; i++) {
        TEST_ASSERT_TRUE(mem.lit(i, i));
    }
}
static void test_lines_crossing_the_panel(void) {
    forEachPanel(lineCrossingCheck);
}

static void strokeWidthsCheck(SteamiMemoryBackend& mem, TestDisplay& d) {
    d.clear();
    d.line(10, 10, 60, 33, kInk);
    const std::vector<uint16_t> thin = snapshot(mem);
    d.clear();
    d.strokeLine(10, 10, 60, 33, 1, kInk);
    TEST_ASSERT_TRUE(thin == snapshot(mem));
    d.clear();
    d.strokeLine(10, 10, 60, 33, 2, kInk);
    const int16_t w = d.width();
    for (size_t i = 0; i < thin.size(); i++) {
        if (thin[i] == 0) {
            continue;
        }
        const int16_t x = static_cast<int16_t>(i % static_cast<size_t>(w));
        const int16_t y = static_cast<int16_t>(i / static_cast<size_t>(w));
        TEST_ASSERT_TRUE(mem.lit(x, y));
        TEST_ASSERT_TRUE(mem.lit(static_cast<int16_t>(x + 1), y));
        TEST_ASSERT_TRUE(mem.lit(x, static_cast<int16_t>(y + 1)));
        TEST_ASSERT_TRUE(mem.lit(static_cast<int16_t>(x + 1), static_cast<int16_t>(y + 1)));
    }
    d.clear();
    d.strokeLine(10, 10, 60, 33, 3, kInk);
    const std::vector<uint16_t> wide = snapshot(mem);
    const int16_t h = d.height();
    for (int16_t y = 0; y < h; y++) {
        for (int16_t x = 0; x < w; x++) {
            bool near = false;
            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    const int nx = x + dx;
                    const int ny = y + dy;
                    if (nx >= 0 && nx < w && ny >= 0 && ny < h &&
                        thin[static_cast<size_t>(ny) * static_cast<size_t>(w) +
                             static_cast<size_t>(nx)] != 0) {
                        near = true;
                    }
                }
            }
            // Lit exactly when within one pixel of a width-1 pixel: centred, nothing beyond.
            TEST_ASSERT_EQUAL(
                near,
                wide[static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)] !=
                    0);
        }
    }
    d.clear();
    mem.clearCounters();
    d.strokeLine(10, 10, 60, 33, 0, kInk);
    TEST_ASSERT_EQUAL_size_t(0, mem.litCount());
    TEST_ASSERT_EQUAL_UINT32(0, mem.pixelCalls() + mem.fillRectCalls());
}
static void test_stroke_widths(void) {
    forEachPanel(strokeWidthsCheck);
}

static void degenerateSpansCheck(SteamiMemoryBackend& mem, TestDisplay& d) {
    d.clear();
    mem.clearCounters();
    d.hline(10, 5, 0, kInk);
    d.hline(10, 5, -3, kInk);
    d.vline(10, 5, 0, kInk);
    d.rect(10, 10, 0, 5, kInk);
    d.rect(10, 10, 5, -1, kInk);
    d.rect(10, 10, 0, 5, kInk, true);
    TEST_ASSERT_EQUAL_size_t(0, mem.litCount());
    TEST_ASSERT_EQUAL_UINT32(0, mem.fillRectCalls());
    TEST_ASSERT_EQUAL_UINT32(0, mem.pixelCalls());

    d.rect(10, 10, 1, 1, kInk);
    TEST_ASSERT_EQUAL_size_t(1, mem.litCount());
    TEST_ASSERT_EQUAL_UINT64(1, mem.areaDrawn());
    d.clear();
    mem.clearCounters();
    d.rect(10, 10, 2, 2, kInk);
    TEST_ASSERT_EQUAL_size_t(4, mem.litCount());
    TEST_ASSERT_EQUAL_UINT64(4, mem.areaDrawn());
    d.clear();
    mem.clearCounters();
    d.rect(10, 10, 1, 5, kInk);
    TEST_ASSERT_EQUAL_size_t(5, mem.litCount());
    TEST_ASSERT_EQUAL_UINT64(5, mem.areaDrawn());
    d.clear();
    mem.clearCounters();
    d.rect(10, 10, 5, 1, kInk);
    TEST_ASSERT_EQUAL_size_t(5, mem.litCount());
    TEST_ASSERT_EQUAL_UINT64(5, mem.areaDrawn());
}
static void test_degenerate_spans(void) {  // Review Focus 5
    forEachPanel(degenerateSpansCheck);
}

static void extremeSpansCheck(SteamiMemoryBackend& mem, TestDisplay& d) {
    const int16_t W = d.width();
    const int16_t H = d.height();
    d.clear();
    mem.clearCounters();
    d.hline(100, 5, 32767, kInk);
    TEST_ASSERT_EQUAL_size_t(static_cast<size_t>(W - 100), mem.litCount());
    for (int16_t x = 100; x < W; x++) {
        TEST_ASSERT_TRUE(mem.lit(x, 5));
    }
    TEST_ASSERT_EQUAL_UINT32(1, mem.fillRectCalls());

    d.clear();
    d.vline(5, 100, 32767, kInk);
    TEST_ASSERT_EQUAL_size_t(static_cast<size_t>(H - 100), mem.litCount());
    for (int16_t y = 100; y < H; y++) {
        TEST_ASSERT_TRUE(mem.lit(5, y));
    }

    d.clear();
    d.hline(-32700, 5, 32767, kInk);
    TEST_ASSERT_EQUAL_size_t(67, mem.litCount());
    for (int16_t x = 0; x <= 66; x++) {
        TEST_ASSERT_TRUE(mem.lit(x, 5));
    }

    d.clear();
    d.hline(32000, 5, 32767, kInk);
    TEST_ASSERT_EQUAL_size_t(0, mem.litCount());

    d.clear();
    mem.clearCounters();
    d.rect(-100, -100, 32767, 32767, kInk, true);
    TEST_ASSERT_EQUAL_size_t(static_cast<size_t>(W) * static_cast<size_t>(H), mem.litCount());
    TEST_ASSERT_EQUAL_UINT32(1, mem.fillRectCalls());

    d.clear();
    d.rect(10, 10, 32767, 32767, kInk);
    TEST_ASSERT_EQUAL_size_t(static_cast<size_t>((W - 10) + (H - 11)), mem.litCount());
    TEST_ASSERT_TRUE(mem.lit(W - 1, 10));
    TEST_ASSERT_TRUE(mem.lit(10, H - 1));
    TEST_ASSERT_FALSE(mem.lit(W - 1, H - 1));

    d.clear();
    d.hline(-32768, 5, 32767, kInk);
    d.vline(5, 32767, 32767, kInk);
    d.rect(30000, 30000, 10000, 10000, kInk);
    d.rect(-32768, -32768, 32767, 32767, kInk, true);
    TEST_ASSERT_EQUAL_size_t(0, mem.litCount());

    d.clear();
    d.line(-32768, -32768, 32767, 32767, kInk);
    TEST_ASSERT_EQUAL_size_t(static_cast<size_t>(W), mem.litCount());
    for (int16_t i = 0; i < W; i++) {
        TEST_ASSERT_TRUE(mem.lit(i, i));
    }
}
static void test_extreme_spans(void) {  // Review Focus 1; UBSan runs under native_asan
    forEachPanel(extremeSpansCheck);
}

// ---- Circles and triangles ----

/// A fresh board state with `mem` injected, as forEachPanel prepares each panel.
static void injectFresh(SteamiMemoryBackend& mem) {
    board.end();
    board.setLogger(&capture);
    board.setDisplayBackend(mem);
}

/// The leftmost and rightmost lit x of row y; false when the row is dark.
static bool rowExtent(const SteamiMemoryBackend& mem, int16_t w, int16_t y, int16_t& lo,
                      int16_t& hi) {
    lo = -1;
    hi = -1;
    for (int16_t x = 0; x < w; x++) {
        if (mem.lit(x, y)) {
            if (lo < 0) {
                lo = x;
            }
            hi = x;
        }
    }
    return lo >= 0;
}

static void test_circle_is_8_way_symmetric(void) {
    SteamiMemoryBackend mem(240, 240);
    injectFresh(mem);
    SteamiDisplay d(board);
    const int16_t radii[] = {0, 1, 2, 5, 37, 100};
    for (int16_t r : radii) {
        d.clear();
        d.circle(120, 120, r, kInk);
        TEST_ASSERT_TRUE(mem.lit(static_cast<int16_t>(120 + r), 120));
        TEST_ASSERT_TRUE(mem.lit(120, static_cast<int16_t>(120 - r)));
        if (r == 0) {
            TEST_ASSERT_EQUAL_size_t(1, mem.litCount());
        }
        for (int16_t y = 0; y < 240; y++) {
            for (int16_t x = 0; x < 240; x++) {
                if (!mem.lit(x, y)) {
                    continue;
                }
                const int16_t a = static_cast<int16_t>(x - 120);
                const int16_t b = static_cast<int16_t>(y - 120);
                for (int sa : {-1, 1}) {
                    for (int sb : {-1, 1}) {
                        TEST_ASSERT_TRUE(mem.lit(static_cast<int16_t>(120 + sa * a),
                                                 static_cast<int16_t>(120 + sb * b)));
                        TEST_ASSERT_TRUE(mem.lit(static_cast<int16_t>(120 + sa * b),
                                                 static_cast<int16_t>(120 + sb * a)));
                    }
                }
            }
        }
    }
}

static void test_filled_circle_covers_outline_with_spans(void) {
    const int16_t radii[] = {3, 20, 64};
    for (int16_t r : radii) {
        SteamiMemoryBackend outline(240, 240);
        injectFresh(outline);
        SteamiDisplay a(board);
        a.circle(120, 120, r, kInk);

        SteamiMemoryBackend filled(240, 240);
        injectFresh(filled);
        SteamiDisplay b(board);
        b.circle(120, 120, r, kInk, true);

        for (int16_t y = 0; y < 240; y++) {
            for (int16_t x = 0; x < 240; x++) {
                if (outline.lit(x, y)) {
                    TEST_ASSERT_TRUE(filled.lit(x, y));
                }
            }
            // Each filled row spans exactly the outline's extent on that row.
            int16_t lo = 0, hi = 0, flo = 0, fhi = 0;
            const bool inOutline = rowExtent(outline, 240, y, lo, hi);
            TEST_ASSERT_EQUAL(inOutline, rowExtent(filled, 240, y, flo, fhi));
            TEST_ASSERT_EQUAL_INT16(lo, flo);
            TEST_ASSERT_EQUAL_INT16(hi, fhi);
        }
        TEST_ASSERT_EQUAL_UINT32(0, filled.pixelCalls());
        TEST_ASSERT_EQUAL_UINT32(static_cast<uint32_t>(2 * r + 1), filled.fillRectCalls());
        TEST_ASSERT_EQUAL_UINT64(filled.litCount(), filled.areaDrawn());
        TEST_ASSERT_EQUAL_UINT32(0, filled.outOfBounds());
    }

    SteamiMemoryBackend clipped(128, 128);
    injectFresh(clipped);
    SteamiDisplay d(board);
    d.circle(64, 64, 64, kInk, true);
    TEST_ASSERT_EQUAL_UINT32(128, clipped.fillRectCalls());  // visible rows only
    TEST_ASSERT_EQUAL_UINT32(0, clipped.pixelCalls());
    TEST_ASSERT_EQUAL_UINT64(clipped.litCount(), clipped.areaDrawn());
    TEST_ASSERT_EQUAL_UINT32(0, clipped.outOfBounds());
    board.end();
}

/// Every panel pixel of `mem` is lit exactly when `ref` (same panel) is lit at the pixel moved by
/// (-dx, -dy); a reference pixel that lands off the panel counts as dark.
static void expectShifted(const SteamiMemoryBackend& mem, const std::vector<uint16_t>& ref,
                          int16_t w, int16_t h, int32_t dx, int32_t dy) {
    for (int16_t y = 0; y < h; y++) {
        for (int16_t x = 0; x < w; x++) {
            const int32_t rx = x - dx;
            const int32_t ry = y - dy;
            const bool refLit =
                rx >= 0 && ry >= 0 && rx < w && ry < h &&
                ref[static_cast<size_t>(ry) * static_cast<size_t>(w) + static_cast<size_t>(rx)] !=
                    0;
            TEST_ASSERT_EQUAL(refLit, mem.lit(x, y));
        }
    }
}

static void circlesCrossingCheck(SteamiMemoryBackend& mem, TestDisplay& d) {
    const int16_t W = d.width();
    const int16_t H = d.height();
    const int16_t cx = d.centerX();
    const int16_t cy = d.centerY();
    const int16_t centres[4][2] = {
        {0, cy}, {static_cast<int16_t>(W - 1), cy}, {cx, 0}, {cx, static_cast<int16_t>(H - 1)}};
    for (bool fill : {false, true}) {
        d.clear();
        d.circle(cx, cy, 30, kInk, fill);  // wholly visible: the reference
        const std::vector<uint16_t> ref = snapshot(mem);
        for (const auto& c : centres) {
            d.clear();
            mem.clearCounters();
            d.circle(c[0], c[1], 30, kInk, fill);
            TEST_ASSERT_EQUAL_UINT32(0, mem.outOfBounds());
            expectShifted(mem, ref, W, H, c[0] - cx, c[1] - cy);
            if (fill) {
                TEST_ASSERT_EQUAL_UINT32(0, mem.pixelCalls());
                TEST_ASSERT_EQUAL_UINT64(mem.litCount(), mem.areaDrawn());
                const uint32_t rows = (c[1] == cy) ? 61 : 31;  // rows of the disc on the panel
                TEST_ASSERT_EQUAL_UINT32(rows, mem.fillRectCalls());
            }
        }
    }
    if (W == 128) {
        // Radius 100 around the centre: the outline misses the panel (corners at 90.5), the disc
        // covers it.
        d.clear();
        d.circle(cx, cy, 100, kInk);
        TEST_ASSERT_EQUAL_size_t(0, mem.litCount());
        d.clear();
        mem.clearCounters();
        d.circle(cx, cy, 100, kInk, true);
        TEST_ASSERT_EQUAL_size_t(static_cast<size_t>(W) * static_cast<size_t>(H), mem.litCount());
        TEST_ASSERT_EQUAL_UINT32(static_cast<uint32_t>(H), mem.fillRectCalls());
    }
}
static void test_circles_crossing_every_edge(void) {
    forEachPanel(circlesCrossingCheck);
}

/// Filled-triangle rows recomputed in double: each edge oriented from its lower-y end (ties: lower
/// x), x rounded to nearest with halves away from zero (lround); one run per row, min to max.
static void expectTriangleRows(const SteamiMemoryBackend& mem, int16_t w, int16_t h,
                               const int32_t v[3][2]) {
    for (int16_t y = 0; y < h; y++) {
        bool any = false;
        long lo = 0, hi = 0;
        for (int e = 0; e < 3; e++) {
            const int32_t* p = v[e];
            const int32_t* q = v[(e + 1) % 3];
            if (q[1] < p[1] || (q[1] == p[1] && q[0] < p[0])) {
                const int32_t* t = p;
                p = q;
                q = t;
            }
            if (y < p[1] || y > q[1]) {
                continue;
            }
            long xs[2];
            int n = 0;
            if (p[1] == q[1]) {
                xs[n++] = p[0];
                xs[n++] = q[0];
            } else {
                xs[n++] = p[0] + lround(static_cast<double>(q[0] - p[0]) * (y - p[1]) /
                                        static_cast<double>(q[1] - p[1]));
            }
            for (int i = 0; i < n; i++) {
                lo = any ? (xs[i] < lo ? xs[i] : lo) : xs[i];
                hi = any ? (xs[i] > hi ? xs[i] : hi) : xs[i];
                any = true;
            }
        }
        for (int16_t x = 0; x < w; x++) {
            TEST_ASSERT_EQUAL(any && x >= lo && x <= hi, mem.lit(x, y));
        }
    }
}

static void expectSameForSixOrders(int16_t size, const int32_t v[3][2]) {
    SteamiMemoryBackend mem(size, size);
    injectFresh(mem);
    SteamiDisplay d(board);
    const int order[6][3] = {{0, 1, 2}, {0, 2, 1}, {1, 0, 2}, {1, 2, 0}, {2, 0, 1}, {2, 1, 0}};
    for (bool fill : {true, false}) {
        std::vector<uint16_t> first;
        for (const auto& o : order) {
            d.clear();
            mem.clearCounters();
            d.triangle(v[o[0]][0], v[o[0]][1], v[o[1]][0], v[o[1]][1], v[o[2]][0], v[o[2]][1], kInk,
                       fill);
            if (fill) {
                TEST_ASSERT_EQUAL_UINT32(0, mem.pixelCalls());
                TEST_ASSERT_EQUAL_UINT64(mem.litCount(), mem.areaDrawn());
            }
            TEST_ASSERT_EQUAL_UINT32(0, mem.outOfBounds());
            if (first.empty()) {
                first = snapshot(mem);
                TEST_ASSERT_TRUE(mem.litCount() > 0);
            } else {
                TEST_ASSERT_TRUE(first == snapshot(mem));
            }
        }
        if (fill) {
            expectTriangleRows(mem, size, size, v);
        }
    }
    board.end();
}

static void test_filled_triangle_same_for_six_orders(void) {
    const int32_t small[3][2] = {{10, 5}, {60, 40}, {25, 90}};
    expectSameForSixOrders(128, small);
    const int32_t large[3][2] = {{-20, 10}, {200, 120}, {100, 300}};
    expectSameForSixOrders(240, large);

    // Spot values of the first one: row 40 runs from 10 + round(15 * 35 / 85) = 16 to 60.
    SteamiMemoryBackend mem(128, 128);
    injectFresh(mem);
    SteamiDisplay d(board);
    d.triangle(10, 5, 60, 40, 25, 90, kInk, true);
    int16_t lo = 0, hi = 0;
    TEST_ASSERT_TRUE(rowExtent(mem, 128, 40, lo, hi));
    TEST_ASSERT_EQUAL_INT16(16, lo);
    TEST_ASSERT_EQUAL_INT16(60, hi);
    TEST_ASSERT_TRUE(rowExtent(mem, 128, 5, lo, hi));
    TEST_ASSERT_EQUAL_INT16(10, lo);
    TEST_ASSERT_EQUAL_INT16(10, hi);
    TEST_ASSERT_FALSE(rowExtent(mem, 128, 4, lo, hi));
    TEST_ASSERT_FALSE(rowExtent(mem, 128, 91, lo, hi));
    board.end();
}

static void degenerateShapesCheck(SteamiMemoryBackend& mem, TestDisplay& d) {
    d.clear();
    mem.clearCounters();
    d.circle(20, 20, -1, kInk);
    d.circle(20, 20, -1, kInk, true);
    d.circle(20, 20, -32768, kInk, true);
    TEST_ASSERT_EQUAL_size_t(0, mem.litCount());
    TEST_ASSERT_EQUAL_UINT32(0, mem.pixelCalls() + mem.fillRectCalls());

    d.circle(20, 20, 0, kInk);
    TEST_ASSERT_EQUAL_size_t(1, mem.litCount());
    TEST_ASSERT_TRUE(mem.lit(20, 20));
    d.clear();
    mem.clearCounters();
    d.circle(20, 20, 0, kInk, true);
    TEST_ASSERT_EQUAL_size_t(1, mem.litCount());
    TEST_ASSERT_TRUE(mem.lit(20, 20));
    TEST_ASSERT_EQUAL_UINT32(0, mem.pixelCalls());
    TEST_ASSERT_EQUAL_UINT32(1, mem.fillRectCalls());

    for (bool fill : {false, true}) {
        d.clear();
        d.triangle(7, 9, 7, 9, 7, 9, kInk, fill);
        TEST_ASSERT_EQUAL_size_t(1, mem.litCount());
        TEST_ASSERT_TRUE(mem.lit(7, 9));
    }

    d.clear();
    mem.clearCounters();
    d.triangle(0, 0, 5, 5, 10, 10, kInk, true);
    TEST_ASSERT_EQUAL_UINT32(11, mem.fillRectCalls());  // one span per row, rows 0 to 10
    TEST_ASSERT_EQUAL_UINT32(0, mem.pixelCalls());
    TEST_ASSERT_EQUAL_size_t(11, mem.litCount());
    TEST_ASSERT_EQUAL_UINT64(11, mem.areaDrawn());
    for (int16_t i = 0; i <= 10; i++) {
        TEST_ASSERT_TRUE(mem.lit(i, i));
    }

    d.clear();
    mem.clearCounters();
    d.triangle(20, 5, 0, 5, 10, 5, kInk, true);  // flat: one span, both ends of every edge
    TEST_ASSERT_EQUAL_UINT32(1, mem.fillRectCalls());
    TEST_ASSERT_EQUAL_size_t(21, mem.litCount());
    TEST_ASSERT_TRUE(mem.lit(0, 5));
    TEST_ASSERT_TRUE(mem.lit(20, 5));
    TEST_ASSERT_EQUAL_UINT32(0, mem.outOfBounds());
}
static void test_degenerate_circles_and_triangles(void) {  // Review Focus 5
    forEachPanel(degenerateShapesCheck);
}

static void extremeShapesCheck(SteamiMemoryBackend& mem, TestDisplay& d) {
    const int16_t W = d.width();
    const int16_t H = d.height();
    const size_t all = static_cast<size_t>(W) * static_cast<size_t>(H);

    d.clear();
    mem.clearCounters();
    d.circle(d.centerX(), d.centerY(), 32767, kInk, true);
    TEST_ASSERT_EQUAL_size_t(all, mem.litCount());
    TEST_ASSERT_EQUAL_UINT32(0, mem.pixelCalls());
    TEST_ASSERT_EQUAL_UINT32(static_cast<uint32_t>(H), mem.fillRectCalls());

    d.clear();
    d.circle(d.centerX(), d.centerY(), 32767, kInk);
    TEST_ASSERT_EQUAL_size_t(0, mem.litCount());

    d.clear();
    d.circle(0, 0, 32767, kInk);
    TEST_ASSERT_EQUAL_size_t(0, mem.litCount());

    d.clear();
    mem.clearCounters();
    d.circle(-32768, 64, 32767, kInk, true);  // a wrapped cx - r would land at x 1
    TEST_ASSERT_EQUAL_size_t(0, mem.litCount());
    TEST_ASSERT_EQUAL_UINT32(0, mem.fillRectCalls());

    d.clear();
    d.circle(32767, 32767, 32767, kInk, true);
    d.circle(-32768, -32768, 32767, kInk);
    TEST_ASSERT_EQUAL_size_t(0, mem.litCount());

    d.clear();
    d.triangle(-32768, -32768, 32767, 0, -32768, 32767, kInk, true);
    TEST_ASSERT_EQUAL_size_t(all, mem.litCount());

    d.clear();
    d.triangle(-32768, -32768, 32767, 0, -32768, 32767, kInk);  // edges at x 0: y -16384, 16383
    TEST_ASSERT_EQUAL_size_t(0, mem.litCount());

    d.clear();
    d.triangle(32767, 32767, -32768, 32767, 32767, -32768, kInk, true);
    TEST_ASSERT_EQUAL_size_t(all, mem.litCount());  // the hypotenuse passes near (-1, -1)

    d.clear();
    d.triangle(-32768, -32768, -32768, 32767, -32000, 0, kInk, true);  // left of the panel
    TEST_ASSERT_EQUAL_size_t(0, mem.litCount());
}
static void test_extreme_circles_and_triangles(void) {  // Review Focus 1; UBSan under native_asan
    forEachPanel(extremeShapesCheck);
}

// ---- Arcs ----

/// The distance test of 5.6: outer radius r + 1/2, inner radius ri - 1/2 with ri = max(0, r - w +
/// 1).
static bool inRing(int32_t dx, int32_t dy, int32_t r, int32_t w) {
    const int64_t d4 = 4 * (static_cast<int64_t>(dx) * dx + static_cast<int64_t>(dy) * dy);
    const int32_t ri = r - w + 1 > 0 ? r - w + 1 : 0;
    return d4 <= static_cast<int64_t>(2 * r + 1) * (2 * r + 1) &&
           (ri == 0 || d4 >= static_cast<int64_t>(2 * ri - 1) * (2 * ri - 1));
}

/// Every panel pixel is lit exactly when inRing holds around (cx, cy).
static void expectRing(const SteamiMemoryBackend& mem, int16_t w, int16_t h, int32_t cx, int32_t cy,
                       int32_t r, int32_t width) {
    for (int16_t y = 0; y < h; y++) {
        for (int16_t x = 0; x < w; x++) {
            TEST_ASSERT_EQUAL(inRing(x - cx, y - cy, r, width), mem.lit(x, y));
        }
    }
}

static void fullArcCheck(SteamiMemoryBackend& mem, TestDisplay& d) {
    const int16_t W = d.width();
    const int16_t H = d.height();
    const int16_t cx = d.centerX();
    const int16_t cy = d.centerY();
    const int16_t centres[5][2] = {{cx, cy},
                                   {0, 0},
                                   {static_cast<int16_t>(W - 1), cy},
                                   {cx, 0},
                                   {cx, static_cast<int16_t>(H - 1)}};
    const int16_t rings[4][2] = {{50, 1}, {50, 7}, {5, 10}, {60, 7}};
    for (const auto& ring : rings) {
        for (const auto& c : centres) {
            d.clear();
            mem.clearCounters();
            d.arc(c[0], c[1], ring[0], 0, 360, kInk, static_cast<uint8_t>(ring[1]));
            expectRing(mem, W, H, c[0], c[1], ring[0], ring[1]);
            TEST_ASSERT_EQUAL_UINT32(0, mem.pixelCalls());
            TEST_ASSERT_EQUAL_UINT64(mem.litCount(), mem.areaDrawn());  // each pixel once
        }
    }
}
static void test_full_arc_equals_distance_ring(void) {
    forEachPanel(fullArcCheck);
}

static void test_quarter_arc_is_noon_to_three(void) {
    SteamiMemoryBackend mem(240, 240);
    injectFresh(mem);
    SteamiDisplay d(board);
    d.arc(120, 120, 50, 0, 90, kInk, 7);
    TEST_ASSERT_TRUE(mem.litCount() > 0);
    for (int16_t y = 0; y < 240; y++) {
        for (int16_t x = 0; x < 240; x++) {
            const int32_t dx = x - 120;
            const int32_t dy = y - 120;
            if (mem.lit(x, y)) {
                TEST_ASSERT_TRUE(dx >= 0 && dy <= 0);
            }
            if (dx > 0 && dy < 0 && inRing(dx, dy, 50, 7)) {
                TEST_ASSERT_TRUE(mem.lit(x, y));
            }
        }
    }
    TEST_ASSERT_EQUAL_UINT32(0, mem.pixelCalls());
    board.end();
}

/// The pixels of the arcs {start, sweep} drawn in turn on a cleared panel, centre 120, r 60, w 7.
static std::vector<uint16_t> arcsSnapshot(SteamiMemoryBackend& mem, SteamiDisplay& d,
                                          std::initializer_list<std::pair<int16_t, int16_t>> arcs,
                                          int16_t r = 60, uint8_t w = 7) {
    d.clear();
    for (const auto& a : arcs) {
        d.arc(120, 120, r, a.first, a.second, kInk, w);
    }
    return snapshot(mem);
}

static void test_adjacent_arcs_leave_no_gap(void) {
    SteamiMemoryBackend mem(240, 240);
    injectFresh(mem);
    SteamiDisplay d(board);
    TEST_ASSERT_TRUE(arcsSnapshot(mem, d, {{0, 90}, {90, 90}}) == arcsSnapshot(mem, d, {{0, 180}}));
    // The gauge: track then fill, against one 270 degree arc (two sectors inside arc()).
    TEST_ASSERT_TRUE(arcsSnapshot(mem, d, {{225, 135}, {0, 135}}) ==
                     arcsSnapshot(mem, d, {{225, 270}}));
    TEST_ASSERT_TRUE(mem.litCount() > 0);
    board.end();
}

static void test_arc_angles_normalised(void) {  // Review Focus 4
    SteamiMemoryBackend mem(240, 240);
    injectFresh(mem);
    SteamiDisplay d(board);
    TEST_ASSERT_TRUE(arcsSnapshot(mem, d, {{-90, 90}}) == arcsSnapshot(mem, d, {{270, 90}}));
    TEST_ASSERT_TRUE(arcsSnapshot(mem, d, {{450, 90}}) == arcsSnapshot(mem, d, {{90, 90}}));
    TEST_ASSERT_TRUE(arcsSnapshot(mem, d, {{0, 400}}) == arcsSnapshot(mem, d, {{0, 360}}));

    arcsSnapshot(mem, d, {{0, 0}, {0, -10}});
    TEST_ASSERT_EQUAL_size_t(0, mem.litCount());
    arcsSnapshot(mem, d, {{0, 360}}, 60, 0);
    TEST_ASSERT_EQUAL_size_t(0, mem.litCount());
    arcsSnapshot(mem, d, {{0, 360}}, -1, 1);
    TEST_ASSERT_EQUAL_size_t(0, mem.litCount());
    arcsSnapshot(mem, d, {{0, 360}}, 0, 1);
    TEST_ASSERT_EQUAL_size_t(1, mem.litCount());
    TEST_ASSERT_TRUE(mem.lit(120, 120));
    board.end();
}

static void test_stroke_circle_is_full_arc(void) {
    SteamiMemoryBackend mem(240, 240);
    injectFresh(mem);
    TestDisplay d(board);
    const int16_t centres[3][2] = {{120, 120}, {10, 200}, {239, 0}};
    for (const auto& c : centres) {
        d.clear();
        d.strokeCircle(c[0], c[1], 40, 2, kInk);
        const std::vector<uint16_t> stroke = snapshot(mem);
        d.clear();
        d.arc(c[0], c[1], 40, 0, 360, kInk, 2);
        TEST_ASSERT_TRUE(stroke == snapshot(mem));
        TEST_ASSERT_TRUE(mem.litCount() > 0);
    }
    board.end();
}

static void extremeArcsCheck(SteamiMemoryBackend& mem, TestDisplay& d) {
    const int16_t W = d.width();
    const int16_t H = d.height();
    const int16_t cx = d.centerX();
    const int16_t cy = d.centerY();

    d.clear();
    d.arc(cx, cy, 32767, 0, 360, kInk, 255);
    TEST_ASSERT_EQUAL_size_t(0, mem.litCount());
    expectRing(mem, W, H, cx, cy, 32767, 255);  // no panel pixel is in that ring

    d.clear();
    d.arc(-32768, -32768, 32767, 45, 90, kInk, 3);
    TEST_ASSERT_EQUAL_size_t(0, mem.litCount());

    d.clear();
    mem.clearCounters();
    d.arc(cx, cy, 300, 0, 360, kInk, 255);  // the panel inside the ring, around a hole of 46
    expectRing(mem, W, H, cx, cy, 300, 255);
    TEST_ASSERT_EQUAL_UINT32(0, mem.pixelCalls());
    TEST_ASSERT_EQUAL_UINT64(mem.litCount(), mem.areaDrawn());
}
static void test_extreme_arcs(void) {  // Review Focus 1; UBSan under native_asan
    forEachPanel(extremeArcsCheck);
}

// ---- Text ----

/// The bounding box of the lit pixels; false when nothing is lit.
struct Box {
    int16_t x0, y0, x1, y1;
};
static bool litBox(const SteamiMemoryBackend& mem, int16_t w, int16_t h, Box& box) {
    box = {w, h, -1, -1};
    for (int16_t y = 0; y < h; y++) {
        for (int16_t x = 0; x < w; x++) {
            if (mem.lit(x, y)) {
                box.x0 = x < box.x0 ? x : box.x0;
                box.y0 = y < box.y0 ? y : box.y0;
                box.x1 = x > box.x1 ? x : box.x1;
                box.y1 = y > box.y1 ? y : box.y1;
            }
        }
    }
    return box.x1 >= 0;
}

static void glyphSizeCheck(SteamiMemoryBackend& mem, TestDisplay& d) {
    for (uint8_t s = 1; s <= 4; s++) {
        d.clear();
        mem.clearCounters();
        d.text("A", 10, 10, kInk, s);
        Box box;
        TEST_ASSERT_TRUE(litBox(mem, mem.width(), mem.height(), box));
        TEST_ASSERT_EQUAL_INT16(10 + s, box.x0);
        TEST_ASSERT_EQUAL_INT16(10 + 7 * s - 1, box.x1);
        TEST_ASSERT_EQUAL_INT16(10, box.y0);
        TEST_ASSERT_EQUAL_INT16(10 + 7 * s - 1, box.y1);
        TEST_ASSERT_EQUAL_size_t(28u * s * s, mem.litCount());
        TEST_ASSERT_EQUAL_UINT32(0, mem.pixelCalls());
    }
}
static void test_glyph_size_at_scales_1_to_4(void) {
    forEachPanel(glyphSizeCheck);
}

static void test_text_width_counts_glyphs(void) {
    TestDisplay d(board);
    TEST_ASSERT_EQUAL_INT16(16, d.textWidth("\xC2\xB0" "C"));
    TEST_ASSERT_EQUAL_INT16(32, d.textWidth("\xC2\xB0" "C", 2));
    TEST_ASSERT_EQUAL_INT16(88, d.textWidth("Temp\xC3\xA9rature"));
    TEST_ASSERT_EQUAL_INT16(0, d.textWidth(""));
    TEST_ASSERT_EQUAL_INT16(0, d.textWidth(nullptr));
    const std::string many(5000, 'W');
    TEST_ASSERT_EQUAL_INT16(32767, d.textWidth(many.c_str(), 255));
}

static void expectSameText(SteamiMemoryBackend& mem, TestDisplay& d, const char* a, const char* b) {
    d.clear();
    d.text(a, 10, 10, kInk);
    const std::vector<uint16_t> first = snapshot(mem);
    d.clear();
    d.text(b, 10, 10, kInk);
    TEST_ASSERT_TRUE(first == snapshot(mem));
}

static void accentsCheck(SteamiMemoryBackend& mem, TestDisplay& d) {
    expectSameText(mem, d, "\xC3\xA9", "e");
    expectSameText(mem, d, "\xC3\x89", "E");
    expectSameText(mem, d, "\xC5\xB8", "Y");
    d.clear();
    d.text("\xC2\xB0", 10, 10, kInk);
    Box box;
    TEST_ASSERT_TRUE(litBox(mem, mem.width(), mem.height(), box));
    TEST_ASSERT_EQUAL_INT16(11, box.x0);
    TEST_ASSERT_EQUAL_INT16(14, box.x1);
    TEST_ASSERT_EQUAL_INT16(10, box.y0);
    TEST_ASSERT_EQUAL_INT16(13, box.y1);
}
static void test_accents_fold_when_drawn(void) {
    forEachPanel(accentsCheck);
}

static void invalidUtf8Check(SteamiMemoryBackend& mem, TestDisplay& d) {
    expectSameText(mem, d, "\xFF", "?");
    expectSameText(mem, d, "A\xC3", "A?");
    expectSameText(mem, d, "\xE2\x82\xAC", "?");
    TEST_ASSERT_EQUAL_INT16(16, d.textWidth("A\xC3"));
}
static void test_text_invalid_utf8_draws_question_marks(void) {  // Review Focus 2
    forEachPanel(invalidUtf8Check);
}

static void textClippingCheck(SteamiMemoryBackend& mem, TestDisplay& d) {
    const int16_t h = mem.height();
    d.clear();
    d.text("Hello", 10, h - 3, kInk);
    Box box;
    TEST_ASSERT_TRUE(litBox(mem, mem.width(), h, box));
    TEST_ASSERT_EQUAL_INT16(h - 3, box.y0);
    TEST_ASSERT_EQUAL_INT16(h - 1, box.y1);
    d.clear();
    d.text("Hello", -20, 10, kInk);
    TEST_ASSERT_TRUE(mem.litCount() > 0);
    d.clear();
    d.text("WWWW", -100, -100, kInk, 255);
    d.text("X", 32767, 32767, kInk);
    d.text("X", -32768, -32768, kInk, 255);
    d.text("Hello", 32000, 10, kInk, 255);
}
static void test_text_clipping(void) {
    forEachPanel(textClippingCheck);
}

static void nothingCheck(SteamiMemoryBackend& mem, TestDisplay& d) {
    d.clear();
    mem.clearCounters();
    d.text(nullptr, 10, 10);
    d.text("", 10, 10);
    TEST_ASSERT_EQUAL_size_t(0, mem.litCount());
    TEST_ASSERT_EQUAL_UINT32(0, mem.fillRectCalls());
    TEST_ASSERT_EQUAL_UINT32(0, mem.pixelCalls());
}
static void test_text_nullptr_and_empty_draw_nothing(void) {
    forEachPanel(nothingCheck);
}

static void scaleZeroCheck(SteamiMemoryBackend& mem, TestDisplay& d) {
    d.clear();
    d.text("AB", 10, 10, kInk, 1);
    const std::vector<uint16_t> one = snapshot(mem);
    d.clear();
    d.text("AB", 10, 10, kInk, 0);
    TEST_ASSERT_TRUE(one == snapshot(mem));
    TEST_ASSERT_EQUAL_INT16(16, d.textWidth("AB", 0));
    d.clear();
    d.textSmall("AB", 10, 10, kInk, 1);
    const std::vector<uint16_t> small = snapshot(mem);
    d.clear();
    d.textSmall("AB", 10, 10, kInk, 0);
    TEST_ASSERT_TRUE(small == snapshot(mem));
    TEST_ASSERT_EQUAL_INT16(12, d.textSmallWidth("AB", 0));
}
static void test_text_scale_zero_is_one(void) {  // Review Focus 5
    forEachPanel(scaleZeroCheck);
}

static void smallTextCheck(SteamiMemoryBackend& mem, TestDisplay& d) {
    d.clear();
    d.textSmall("A", 10, 10, kInk);
    Box box;
    TEST_ASSERT_TRUE(litBox(mem, mem.width(), mem.height(), box));
    TEST_ASSERT_EQUAL_INT16(10, box.x0);
    TEST_ASSERT_EQUAL_INT16(14, box.x1);
    TEST_ASSERT_EQUAL_INT16(10, box.y0);
    TEST_ASSERT_EQUAL_INT16(16, box.y1);
    TEST_ASSERT_EQUAL_INT16(18, d.textSmallWidth("950"));
    TEST_ASSERT_EQUAL_INT16(36, d.textSmallWidth("950", 2));
    TEST_ASSERT_EQUAL_INT16(0, d.textSmallWidth(nullptr));
    d.clear();
    mem.clearCounters();
    d.textSmall(nullptr, 10, 10, kInk);
    TEST_ASSERT_EQUAL_size_t(0, mem.litCount());
    TEST_ASSERT_EQUAL_UINT32(0, mem.fillRectCalls());
}
static void test_small_text(void) {
    forEachPanel(smallTextCheck);
}

static void glyphWindowCheck(SteamiMemoryBackend& mem, TestDisplay& d) {
    d.clear();
    d.glyph(steamiGlyph8x8('.'), 2, 4, 8, 10, 10, kInk, 1);
    TEST_ASSERT_EQUAL_size_t(4, mem.litCount());
    for (int16_t x = 11; x <= 12; x++) {
        for (int16_t y = 15; y <= 16; y++) {
            TEST_ASSERT_TRUE(mem.lit(x, y));
        }
    }
}
static void test_glyph_window(void) {
    forEachPanel(glyphWindowCheck);
}

static void test_protected_name_in_messages(void) {
    TestDisplay d(board);
    TEST_ASSERT_FALSE(d.begin());
    TEST_ASSERT_EQUAL_size_t(1, g_lines.size());
    TEST_ASSERT_EQUAL_UINT32(0, g_lines[0].rfind("[STeaMi] SteamiTestDisplay: ", 0));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_rgb565_of_every_colour);
    RUN_TEST(test_gray4_of_every_colour);
    RUN_TEST(test_gray_levels_round_trip);
    RUN_TEST(test_from_rgb565_and_equality);
    RUN_TEST(test_color_rgb_scales_largest_channel);
    RUN_TEST(test_color_rgb_rounds_to_nearest);
    RUN_TEST(test_color_rgb_zero_is_black);
    RUN_TEST(test_color_rgb_unavailable_is_black);
    RUN_TEST(test_sin_table_matches_rounded_sine);
    RUN_TEST(test_sin_symmetries_and_normalisation);
    RUN_TEST(test_round_q14);
    RUN_TEST(test_isqrt);
    RUN_TEST(test_8x8_table_is_micropython_font);
    RUN_TEST(test_5x7_table_is_driver_font);
    RUN_TEST(test_out_of_range_codes_give_question_mark);
    RUN_TEST(test_degree_glyphs);
    RUN_TEST(test_utf8_degree_and_folding);
    RUN_TEST(test_utf8_invalid_bytes);
    RUN_TEST(test_utf8_truncated_sequences);
    RUN_TEST(test_glyph_count_nullptr_and_empty);
    RUN_TEST(test_memory_backend_counts_out_of_bounds);
    RUN_TEST(test_memory_backend_grey_mode_stores_oled_levels);
    RUN_TEST(test_memory_backend_writes_ppm);
    RUN_TEST(test_board_without_backend_is_unknown_board);
    RUN_TEST(test_board_begins_injected_backend_once);
    RUN_TEST(test_board_failed_begin_is_no_memory);
    RUN_TEST(test_set_backend_after_begin_display_is_ignored);
    RUN_TEST(test_unknown_board_locks_injection_until_end);
    RUN_TEST(test_board_end_forgets_backend_and_status);
    RUN_TEST(test_geometry_before_begin);
    RUN_TEST(test_no_backend_reports_e30_once);
    RUN_TEST(test_failed_begin_reports_e31_and_draws_nothing);
    RUN_TEST(test_auto_begin_on_first_draw);
    RUN_TEST(test_backend_accessor);
    RUN_TEST(test_default_board_is_steami);
    RUN_TEST(test_pixel_clipping);
    RUN_TEST(test_primitives_never_lift_colours);
    RUN_TEST(test_clear_show_and_controls_forward);
    RUN_TEST(test_two_objects_share_one_backend);
    RUN_TEST(test_drawing_after_board_end_does_nothing);
    RUN_TEST(test_hline_vline_rect);
    RUN_TEST(test_line_endpoints_and_symmetry);
    RUN_TEST(test_lines_crossing_the_panel);
    RUN_TEST(test_stroke_widths);
    RUN_TEST(test_degenerate_spans);
    RUN_TEST(test_extreme_spans);
    RUN_TEST(test_circle_is_8_way_symmetric);
    RUN_TEST(test_filled_circle_covers_outline_with_spans);
    RUN_TEST(test_circles_crossing_every_edge);
    RUN_TEST(test_filled_triangle_same_for_six_orders);
    RUN_TEST(test_degenerate_circles_and_triangles);
    RUN_TEST(test_extreme_circles_and_triangles);
    RUN_TEST(test_full_arc_equals_distance_ring);
    RUN_TEST(test_quarter_arc_is_noon_to_three);
    RUN_TEST(test_adjacent_arcs_leave_no_gap);
    RUN_TEST(test_arc_angles_normalised);
    RUN_TEST(test_stroke_circle_is_full_arc);
    RUN_TEST(test_extreme_arcs);
    RUN_TEST(test_glyph_size_at_scales_1_to_4);
    RUN_TEST(test_text_width_counts_glyphs);
    RUN_TEST(test_accents_fold_when_drawn);
    RUN_TEST(test_text_invalid_utf8_draws_question_marks);
    RUN_TEST(test_text_clipping);
    RUN_TEST(test_text_nullptr_and_empty_draw_nothing);
    RUN_TEST(test_text_scale_zero_is_one);
    RUN_TEST(test_small_text);
    RUN_TEST(test_glyph_window);
    RUN_TEST(test_protected_name_in_messages);
    return UNITY_END();
}
