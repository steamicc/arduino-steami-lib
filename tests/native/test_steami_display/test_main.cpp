// SPDX-License-Identifier: GPL-3.0-or-later

#include <Arduino.h>
#include <Wire.h>
#include <math.h>
#include <stdint.h>
#include <string.h>
#include <unity.h>

#include <initializer_list>
#include <string>
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
    RUN_TEST(test_protected_name_in_messages);
    return UNITY_END();
}
