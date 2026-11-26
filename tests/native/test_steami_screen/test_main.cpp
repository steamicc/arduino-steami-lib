// SPDX-License-Identifier: GPL-3.0-or-later

#include <Arduino.h>
#include <Wire.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <string.h>
#include <unity.h>

#include <string>
#include <vector>

#include "SteamiBoard.h"
#include "SteamiDisplay.h"
#include "SteamiFont.h"
#include "SteamiMemoryBackend.h"
#include "SteamiRgb.h"
#include "SteamiScreen.h"
#include "SteamiScreenCheck.h"
#include "SteamiValue.h"

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

static bool endsWith(const std::string& line, const char* suffix) {
    const size_t n = strlen(suffix);
    return line.size() >= n && line.compare(line.size() - n, n, suffix) == 0;
}

static size_t countLogged(const char* suffix) {
    size_t count = 0;
    for (const std::string& line : g_lines) {
        if (endsWith(line, suffix)) {
            count++;
        }
    }
    return count;
}

/// The first logged line ending with suffix, "" if none.
static std::string loggedLine(const char* suffix) {
    for (const std::string& line : g_lines) {
        if (endsWith(line, suffix)) {
            return line;
        }
    }
    return "";
}

/// What value() would show: v formatted into the 16 B buffer value() uses.
static std::string fmt(SteamiValue v, uint8_t d = 1) {
    char b[16];
    v.format(b, sizeof b, d);
    return b;
}

// The target's analogRead() returns int (STM32duino 3.1.0); the native mock has none.
static int analogRead(uint32_t) {
    return 512;
}

static void test_format_integers(void) {
    TEST_ASSERT_EQUAL_STRING("0", fmt(0).c_str());
    TEST_ASSERT_EQUAL_STRING("-5", fmt(-5).c_str());
    TEST_ASSERT_EQUAL_STRING("4000000000", fmt(4000000000u).c_str());
    TEST_ASSERT_EQUAL_STRING("-2147483648", fmt(-2147483647L - 1).c_str());
    TEST_ASSERT_EQUAL_STRING("3000000000", fmt(3000000000UL).c_str());
}

static void test_format_floats(void) {
    TEST_ASSERT_EQUAL_STRING("23.5", fmt(23.5f).c_str());
    TEST_ASSERT_EQUAL_STRING("23.5", fmt(23.5).c_str());
    TEST_ASSERT_EQUAL_STRING("45.0", fmt(45.0f).c_str());
    TEST_ASSERT_EQUAL_STRING("24", fmt(23.5f, 0).c_str());
    TEST_ASSERT_EQUAL_STRING("3", fmt(2.5f, 0).c_str());
    TEST_ASSERT_EQUAL_STRING("-3", fmt(-2.5f, 0).c_str());
    TEST_ASSERT_EQUAL_STRING("0.3", fmt(0.25f).c_str());
    TEST_ASSERT_EQUAL_STRING("-0.3", fmt(-0.25f).c_str());
    TEST_ASSERT_EQUAL_STRING("0.13", fmt(0.125f, 2).c_str());
    TEST_ASSERT_EQUAL_STRING("10.0", fmt(9.96f).c_str());
    TEST_ASSERT_EQUAL_STRING("1.5000", fmt(1.5f, 4).c_str());
    TEST_ASSERT_EQUAL_STRING("1.5000", fmt(1.5f, 9).c_str());  // clamped to 4 decimals
}

static void test_format_negative_zero(void) {
    TEST_ASSERT_EQUAL_STRING("0.0", fmt(-0.0f).c_str());
    TEST_ASSERT_EQUAL_STRING("0.0", fmt(-0.04f).c_str());
    TEST_ASSERT_EQUAL_STRING("0", fmt(-0.4f, 0).c_str());
}

static void test_format_dashes(void) {
    TEST_ASSERT_EQUAL_STRING("--", fmt(NAN).c_str());
    TEST_ASSERT_EQUAL_STRING("--", fmt(INFINITY).c_str());
    TEST_ASSERT_EQUAL_STRING("--", fmt(-INFINITY).c_str());
    TEST_ASSERT_EQUAL_STRING("--", fmt(1e9f).c_str());
    TEST_ASSERT_EQUAL_STRING("--", fmt(-1e9f).c_str());
    TEST_ASSERT_EQUAL_STRING("--", fmt(static_cast<const char*>(nullptr)).c_str());
    // The largest float below 1e9 still prints whole, 15 characters with its sign.
    TEST_ASSERT_EQUAL_STRING("999999936.0000", fmt(999999936.0f, 4).c_str());
    TEST_ASSERT_EQUAL_STRING("-999999936.0000", fmt(-999999936.0f, 4).c_str());
}

/// Review Focus 5: the exact text when it fits the 16 B buffer, else "--", never a cut number.
static std::string expectedExtreme(const std::string& text) {
    return text.size() <= 15 ? text : "--";
}

static void test_format_extremes(void) {
    TEST_ASSERT_EQUAL_STRING(expectedExtreme(std::to_string(LONG_MIN)).c_str(),
                             fmt(LONG_MIN).c_str());
    TEST_ASSERT_EQUAL_STRING(expectedExtreme(std::to_string(LONG_MAX)).c_str(),
                             fmt(LONG_MAX).c_str());
    TEST_ASSERT_EQUAL_STRING(expectedExtreme(std::to_string(ULONG_MAX)).c_str(),
                             fmt(ULONG_MAX).c_str());
}

static void test_format_respects_size(void) {
    char b[4];
    TEST_ASSERT_EQUAL_size_t(2, SteamiValue(12345).format(b, 4, 1));
    TEST_ASSERT_EQUAL_STRING("--", b);
    TEST_ASSERT_EQUAL_size_t(3, SteamiValue("abcdef").format(b, 4, 1));
    TEST_ASSERT_EQUAL_STRING("abc", b);
    TEST_ASSERT_EQUAL_size_t(0, SteamiValue("abcdef").format(b, 0, 1));
    TEST_ASSERT_EQUAL_STRING("abc", b);  // untouched
    char c[2];
    TEST_ASSERT_EQUAL_size_t(1, SteamiValue(NAN).format(c, 2, 1));
    TEST_ASSERT_EQUAL_STRING("-", c);
}

static void test_format_text_keeps_whole_code_points(void) {
    char b[3];
    TEST_ASSERT_EQUAL_size_t(1, SteamiValue("T\xC3\xA9").format(b, 3, 1));
    TEST_ASSERT_EQUAL_STRING("T", b);
    char c[4];
    TEST_ASSERT_EQUAL_size_t(3, SteamiValue("T\xC3\xA9").format(c, 4, 1));
    TEST_ASSERT_EQUAL_STRING("T\xC3\xA9", c);
    char d[2];
    TEST_ASSERT_EQUAL_size_t(0, SteamiValue("\xC2\xB0"
                                            "C")
                                    .format(d, 2, 1));
    TEST_ASSERT_EQUAL_STRING("", d);
}

// Spec 7: every type a student passes to value() picks one constructor and formats.
static void test_value_overloads_compile_and_format(void) {
    uint8_t u8 = 200;
    int16_t i16 = -300;
    size_t n = 7;
    millisClock() = 4000000000u;
    TEST_ASSERT_EQUAL_STRING("0", fmt(0).c_str());
    TEST_ASSERT_EQUAL_STRING("23", fmt(23).c_str());
    TEST_ASSERT_EQUAL_STRING("23.5", fmt(23.5).c_str());
    TEST_ASSERT_EQUAL_STRING("23.5", fmt(23.5f).c_str());
    TEST_ASSERT_EQUAL_STRING("4000000000", fmt(millis()).c_str());
    TEST_ASSERT_EQUAL_STRING("512", fmt(analogRead(0)).c_str());
    TEST_ASSERT_EQUAL_STRING("200", fmt(u8).c_str());
    TEST_ASSERT_EQUAL_STRING("-300", fmt(i16).c_str());
    TEST_ASSERT_EQUAL_STRING("7", fmt(n).c_str());
    TEST_ASSERT_EQUAL_STRING("text", fmt("text").c_str());
    TEST_ASSERT_EQUAL_STRING("--", fmt(NAN).c_str());
    TEST_ASSERT_EQUAL_STRING("1", fmt(true).c_str());
    millisClock() = 0;
}

// ---- Memory backend: drawing outside the round panel ----

static void test_memory_backend_counts_outside_circle(void) {
    // Outside: (x - 64)^2 + (y - 64)^2 > 65^2 at a pixel or at a corner of a rectangle.
    SteamiMemoryBackend m(128, 128);
    m.pixel(64, 64, 0xFFFF);
    TEST_ASSERT_EQUAL_UINT32(0, m.outsideCircle());
    m.pixel(0, 0, 0xFFFF);
    TEST_ASSERT_EQUAL_UINT32(1, m.outsideCircle());
    m.fillRect(0, 63, 128, 1, 0xFFFF);  // corners at 4097 and 3970: inside
    TEST_ASSERT_EQUAL_UINT32(1, m.outsideCircle());
    m.fillRect(60, 60, 8, 8, 0xFFFF);
    TEST_ASSERT_EQUAL_UINT32(1, m.outsideCircle());
    m.fillRect(120, 0, 4, 4, 0xFFFF);
    TEST_ASSERT_EQUAL_UINT32(2, m.outsideCircle());
    m.clearCounters();
    TEST_ASSERT_EQUAL_UINT32(0, m.outsideCircle());
    m.pixel(-1, 64, 0xFFFF);  // an off-panel call is counted by outOfBounds only
    TEST_ASSERT_EQUAL_UINT32(1, m.outOfBounds());
    TEST_ASSERT_EQUAL_UINT32(0, m.outsideCircle());

    SteamiMemoryBackend grey(128, 128, false);
    TEST_ASSERT_EQUAL_HEX16(STEAMI_BLUE.rgb565(), m.stored(STEAMI_BLUE));
    TEST_ASSERT_EQUAL_HEX16(SteamiRgb::gray(6).rgb565(), grey.stored(STEAMI_BLUE));
}

// ---- SteamiScreen layout kernel ----

[[maybe_unused]] static constexpr SteamiScreen kConstInitScreen(board);  // constant-initialised
static_assert(sizeof(SteamiScreen) <= 3 * sizeof(void*), "12 B on the 32-bit target, under 16 B");

/// The protected kernel of SteamiScreen, as the widgets use it.
class TestScreen : public SteamiScreen {
   public:
    explicit TestScreen(SteamiBoard& b) : SteamiScreen(b) {}
    using SteamiDisplay::textSmall;
    using SteamiScreen::cellWidth;
    using SteamiScreen::chord;
    using SteamiScreen::drawGlyphs;
    using SteamiScreen::fitGlyphs;
    using SteamiScreen::glyphCount;
    using SteamiScreen::Ink;
    using SteamiScreen::inkChord;
    using SteamiScreen::inkRows;
    using SteamiScreen::k;
    using SteamiScreen::lifted;
    using SteamiScreen::marginEw;
    using SteamiScreen::marginNs;
    using SteamiScreen::Point;
    using SteamiScreen::Region;
    using SteamiScreen::resolve;
    using SteamiScreen::u;
    using SteamiScreen::valueWidth;
};

using Region = TestScreen::Region;
using Ink = TestScreen::Ink;

/// Runs `check` on a fresh board state with a size x size memory panel injected and cleared, then
/// asserts that nothing reached the backend off the panel or outside the circle.
template <typename Check>
static void onPanel(int16_t size, bool color, Check check) {
    board.end();
    board.setLogger(&capture);
    SteamiMemoryBackend mem(size, size, color);
    board.setDisplayBackend(mem);
    TestScreen s(board);
    s.clear();
    mem.clearCounters();
    check(mem, s);
    TEST_ASSERT_EQUAL_UINT32(0, mem.outOfBounds());
    TEST_ASSERT_EQUAL_UINT32(0, mem.outsideCircle());
    board.end();
}

/// onPanel on a 128 grey, a 128 colour, a 240 colour and a 240 grey panel.
template <typename Check>
static void forEachPanel(Check check) {
    onPanel(128, false, check);
    onPanel(128, true, check);
    onPanel(240, true, check);
    onPanel(240, false, check);
}

static void expectRegion(const Region& r, int32_t left, int32_t right) {
    TEST_ASSERT_EQUAL_INT32_MESSAGE(left, r.left, "left");
    TEST_ASSERT_EQUAL_INT32_MESSAGE(right, r.right, "right");
}

static void expectPoint(const TestScreen::Point& p, int32_t x, int32_t y, const char* at) {
    TEST_ASSERT_EQUAL_INT32_MESSAGE(x, p.x, at);
    TEST_ASSERT_EQUAL_INT32_MESSAGE(y, p.y, at);
}

// Values printed by golden_layout.py.
static void test_scale_quantities(void) {
    onPanel(128, true, [](SteamiMemoryBackend&, TestScreen& s) {
        TEST_ASSERT_EQUAL_INT32(8, s.u(8));
        TEST_ASSERT_EQUAL_INT32(-12, s.u(-12));
        TEST_ASSERT_EQUAL_UINT8(1, s.k());
        TEST_ASSERT_EQUAL_UINT8(16, s.maxChars());
        TEST_ASSERT_EQUAL_INT32(12, s.marginEw(1));
        TEST_ASSERT_EQUAL_INT32(20, s.marginNs(8, 1));
        TEST_ASSERT_EQUAL_INT32(29, s.marginNs(104, 1));
        TEST_ASSERT_EQUAL_INT32(29, s.marginNs(112, 1));
        TEST_ASSERT_EQUAL_INT32(29, s.marginNs(160, 1));  // tw >= 2r
        TEST_ASSERT_EQUAL_INT32(36, s.marginNs(8, 2));
    });
    onPanel(240, true, [](SteamiMemoryBackend&, TestScreen& s) {
        TEST_ASSERT_EQUAL_INT32(15, s.u(8));
        TEST_ASSERT_EQUAL_INT32(-22, s.u(-12));
        TEST_ASSERT_EQUAL_INT32(75, s.u(40));
        TEST_ASSERT_EQUAL_UINT8(2, s.k());
        TEST_ASSERT_EQUAL_UINT8(15, s.maxChars());
        TEST_ASSERT_EQUAL_INT32(22, s.marginEw(2));
        TEST_ASSERT_EQUAL_INT32(30, s.marginEw(3));
        TEST_ASSERT_EQUAL_INT32(37, s.marginNs(16, 2));
        TEST_ASSERT_EQUAL_INT32(41, s.marginNs(176, 2));
        TEST_ASSERT_EQUAL_INT32(63, s.marginNs(208, 2));
        TEST_ASSERT_EQUAL_INT32(67, s.marginNs(16, 4));
        TEST_ASSERT_EQUAL_INT32(52, s.marginNs(16, 3));
    });
    // An injected panel narrower than 104 px: the clamped cap r + 2, no isqrt of a negative number.
    onPanel(96, false, [](SteamiMemoryBackend&, TestScreen& s) {
        TEST_ASSERT_EQUAL_UINT8(1, s.k());
        TEST_ASSERT_EQUAL_INT32(50, s.marginNs(200, 1));
        TEST_ASSERT_EQUAL_INT32(50, s.marginNs(160, 1));
    });
}

static void test_chord_convention(void) {
    onPanel(128, true, [](SteamiMemoryBackend&, TestScreen& s) {
        expectRegion(s.chord(20, 20), 18, 110);
        expectRegion(s.chord(9, 15), 32, 96);
        TEST_ASSERT_EQUAL_INT32(107, s.chord(29, 29).width());
        TEST_ASSERT_EQUAL_INT32(0, s.chord(-1, 0).width());
        expectRegion(s.chord(64, 64), 0, 128);  // row of the centre: 2r + 1 px
    });
    onPanel(240, true, [](SteamiMemoryBackend&, TestScreen& s) {
        TEST_ASSERT_EQUAL_INT32(173, s.chord(37, 37).width());
        expectRegion(s.chord(41, 41), 30, 210);
        TEST_ASSERT_EQUAL_INT32(211, s.chord(63, 63).width());
        expectRegion(s.chord(1, 14), 105, 135);
        TEST_ASSERT_EQUAL_INT32(0, s.chord(239, 241).width());
    });
}

static void test_ink_rows_and_widths(void) {
    TEST_ASSERT_EQUAL_HEX8(0xC0, TestScreen::inkRows("_", 1, Ink::TEXT));  // rows 6 and 7
    TEST_ASSERT_EQUAL_HEX8(0x7F, TestScreen::inkRows("8.", 2, Ink::VALUE));
    TEST_ASSERT_EQUAL_HEX8(0x00, TestScreen::inkRows("   ", 3, Ink::TEXT));
    TEST_ASSERT_EQUAL_HEX8(0x7F, TestScreen::inkRows("TEMP", 4, Ink::LABEL));
    TestScreen s(board);
    TEST_ASSERT_EQUAL_INT32(32, TestScreen::cellWidth("23.5", 4, Ink::TEXT, 1));
    TEST_ASSERT_EQUAL_INT32(s.textWidth("23.5"), TestScreen::cellWidth("23.5", 4, Ink::TEXT, 1));
    TEST_ASSERT_EQUAL_INT32(28, TestScreen::valueWidth("23.5", 1));
    TEST_ASSERT_EQUAL_INT32(56, TestScreen::valueWidth("23.5", 2));
    TEST_ASSERT_EQUAL_INT32(56, TestScreen::valueWidth("12:34:56", 1));
    TEST_ASSERT_EQUAL_INT32(48, TestScreen::cellWidth("TEMP", 4, Ink::LABEL, 2));
    // glyphs, not bytes
    TEST_ASSERT_EQUAL_INT32(88, TestScreen::cellWidth("Temp\xC3\xA9rature", 11, Ink::TEXT, 1));
    TEST_ASSERT_EQUAL_size_t(11, TestScreen::glyphCount("Temp\xC3\xA9rature"));
    TEST_ASSERT_EQUAL_size_t(0, TestScreen::glyphCount(nullptr));
}

static void test_fit_glyphs_longest_prefix(void) {
    onPanel(128, true, [](SteamiMemoryBackend&, TestScreen& s) {
        const Region all{0, 127};
        TEST_ASSERT_EQUAL_size_t(13,
                                 s.fitGlyphs("ABCDEFGHIJKLMNOPQRST", Ink::TEXT, 1, 29, all, 0, 0));
        TEST_ASSERT_EQUAL_size_t(14, s.fitGlyphs("______________T", Ink::TEXT, 1, 29, all, 0, 0));
        TEST_ASSERT_EQUAL_size_t(14, s.fitGlyphs("              ", Ink::TEXT, 1, 29, all, 0, 0));
        // region 33..95; with the limit {0, 50}, 33..50: 18 px, "TEM"
        TEST_ASSERT_EQUAL_size_t(4, s.fitGlyphs("TEMP", Ink::LABEL, 1, 9, all, 1, 0));
        TEST_ASSERT_EQUAL_size_t(3, s.fitGlyphs("TEMP", Ink::LABEL, 1, 9, Region{0, 50}, 1, 0));
    });
    onPanel(240, true, [](SteamiMemoryBackend&, TestScreen& s) {
        const Region all{0, 239};
        TEST_ASSERT_EQUAL_size_t(13,
                                 s.fitGlyphs("ABCDEFGHIJKLMNOPQRST", Ink::TEXT, 2, 63, all, 0, 0));
        TEST_ASSERT_EQUAL_size_t(13, s.fitGlyphs("______________T", Ink::TEXT, 2, 63, all, 0, 0));
        TEST_ASSERT_EQUAL_size_t(14, s.fitGlyphs("              ", Ink::TEXT, 2, 63, all, 0, 0));
    });
}

static void test_resolve_every_anchor(void) {
    onPanel(128, true, [](SteamiMemoryBackend&, TestScreen& s) {
        expectPoint(s.resolve(SteamiAt::N, 16, 1), 56, 20, "N");
        expectPoint(s.resolve(SteamiAt::NE, 16, 1), 100, 20, "NE");
        expectPoint(s.resolve(SteamiAt::E, 16, 1), 100, 60, "E");
        expectPoint(s.resolve(SteamiAt::SE, 16, 1), 100, 100, "SE");
        expectPoint(s.resolve(SteamiAt::S, 16, 1), 56, 100, "S");
        expectPoint(s.resolve(SteamiAt::SW, 16, 1), 12, 100, "SW");
        expectPoint(s.resolve(SteamiAt::W, 16, 1), 12, 60, "W");
        expectPoint(s.resolve(SteamiAt::NW, 16, 1), 12, 20, "NW");
        expectPoint(s.resolve(SteamiAt::CENTER, 16, 1), 56, 60, "CENTER");
    });
    onPanel(240, true, [](SteamiMemoryBackend&, TestScreen& s) {
        expectPoint(s.resolve(SteamiAt::N, 32, 2), 104, 37, "N");
        expectPoint(s.resolve(SteamiAt::NE, 32, 2), 186, 37, "NE");
        expectPoint(s.resolve(SteamiAt::E, 32, 2), 186, 112, "E");
        expectPoint(s.resolve(SteamiAt::SE, 32, 2), 186, 187, "SE");
        expectPoint(s.resolve(SteamiAt::S, 32, 2), 104, 187, "S");
        expectPoint(s.resolve(SteamiAt::SW, 32, 2), 22, 187, "SW");
        expectPoint(s.resolve(SteamiAt::W, 32, 2), 22, 112, "W");
        expectPoint(s.resolve(SteamiAt::NW, 32, 2), 22, 37, "NW");
        expectPoint(s.resolve(SteamiAt::CENTER, 32, 2), 104, 112, "CENTER");
        TEST_ASSERT_EQUAL_INT32(105, s.resolve(SteamiAt::CENTER, 32, 4).y);  // u(56)
    });
}

/// True when a pixel of column x is lit on the rows y0..y1.
static bool columnLit(const SteamiMemoryBackend& mem, int16_t x, int16_t y0, int16_t y1) {
    for (int16_t y = y0; y <= y1; y++) {
        if (mem.lit(x, y)) {
            return true;
        }
    }
    return false;
}

/// Draws s with drawGlyphs on a fresh 128 colour panel. (10, 10) is outside the disc, so only
/// outOfBounds() is checked here, not outsideCircle().
static void drawOnFreshPanel(SteamiMemoryBackend& mem, const char* s, size_t glyphs, Ink ink) {
    board.end();
    board.setLogger(&capture);
    board.setDisplayBackend(mem);
    TestScreen screen(board);
    screen.drawGlyphs(s, glyphs, ink, 10, 10, STEAMI_WHITE, 2);
    TEST_ASSERT_EQUAL_UINT32(0, mem.outOfBounds());
    board.end();
}

static void test_draw_glyphs_value_window(void) {
    SteamiMemoryBackend dot(128, 128);
    drawOnFreshPanel(dot, "8.8", 3, Ink::VALUE);
    // '.': window columns 3, 4 at f 2, ink x 28..31 on y 20..23 and nothing else in 26..33.
    for (int16_t x = 28; x <= 31; x++) {
        for (int16_t y = 10; y <= 25; y++) {
            TEST_ASSERT_EQUAL_MESSAGE(y >= 20 && y <= 23, dot.lit(x, y), "'.' ink");
        }
    }
    TEST_ASSERT_FALSE(columnLit(dot, 26, 10, 25));
    TEST_ASSERT_FALSE(columnLit(dot, 27, 10, 25));
    TEST_ASSERT_FALSE(dot.lit(27, 20));
    TEST_ASSERT_FALSE(dot.lit(32, 20));
    // The second '8' starts at x 34; its first lit column is 36.
    for (int16_t x = 32; x < 36; x++) {
        TEST_ASSERT_FALSE(columnLit(dot, x, 10, 25));
    }
    TEST_ASSERT_TRUE(columnLit(dot, 36, 10, 25));
    expectBox(inkBoxAll(dot), 12, 10, 34 + 16 - 1 - 2, 23);  // the 8x8 '8' leaves column 7 empty

    SteamiMemoryBackend colon(128, 128);
    drawOnFreshPanel(colon, "8:8", 3, Ink::VALUE);
    // ':': ink x 28..31 on y 14..15 and 20..21 (glyph rows 2 and 5).
    for (int16_t x = 28; x <= 31; x++) {
        for (int16_t y = 10; y <= 25; y++) {
            const bool ink = (y >= 14 && y <= 15) || (y >= 20 && y <= 21);
            TEST_ASSERT_EQUAL_MESSAGE(ink, colon.lit(x, y), "':' ink");
        }
    }
    TEST_ASSERT_FALSE(columnLit(colon, 26, 10, 25));
    TEST_ASSERT_FALSE(columnLit(colon, 27, 10, 25));
    TEST_ASSERT_FALSE(colon.lit(27, 14));
    TEST_ASSERT_FALSE(colon.lit(32, 14));
    for (int16_t x = 32; x < 36; x++) {
        TEST_ASSERT_FALSE(columnLit(colon, x, 10, 25));
    }
    TEST_ASSERT_TRUE(columnLit(colon, 36, 10, 25));

    // The glyph count stops the string: "ABC" with 2 glyphs is "AB".
    SteamiMemoryBackend two(128, 128);
    drawOnFreshPanel(two, "ABC", 2, Ink::TEXT);
    SteamiMemoryBackend ab(128, 128);
    board.setLogger(&capture);
    board.setDisplayBackend(ab);
    SteamiDisplay(board).text("AB", 10, 10, STEAMI_WHITE, 2);
    board.end();
    TEST_ASSERT_TRUE(ab.litCount() > 0);
    TEST_ASSERT_TRUE(two.pixels() == ab.pixels());
}

static void test_lifted_contrast(void) {
    onPanel(128, false, [](SteamiMemoryBackend&, TestScreen& s) {
        TEST_ASSERT_EQUAL_UINT8(7, s.lifted(STEAMI_BLUE, STEAMI_GRID_DARK).gray4());
        TEST_ASSERT_TRUE(s.lifted(STEAMI_GREEN, STEAMI_GRID_DARK) == STEAMI_GREEN);
        TEST_ASSERT_EQUAL_UINT8(15, s.lifted(STEAMI_BLACK, STEAMI_WHITE).gray4());  // capped
    });
    onPanel(240, true, [](SteamiMemoryBackend&, TestScreen& s) {
        TEST_ASSERT_TRUE(s.lifted(STEAMI_BLUE, STEAMI_GRID_DARK) == STEAMI_BLUE);
    });
}

// ---- text(s, at), title and subtitle ----

/// The pixels of a fresh, cleared size x size panel after `draw` (the "A == B" of the plan).
template <typename Draw>
static std::vector<uint16_t> rendered(int16_t size, bool color, Draw draw) {
    std::vector<uint16_t> out;
    onPanel(size, color, [&](SteamiMemoryBackend& mem, TestScreen& s) {
        draw(s);
        out = mem.pixels();
    });
    return out;
}

/// The pixels the whole of s lights in 8x8 glyphs at factor f, read from the font table.
static size_t wholeInk(const char* s, uint8_t f) {
    size_t n = 0;
    for (uint8_t code = steamiNextGlyph(s); code != 0; code = steamiNextGlyph(s)) {
        const uint8_t* g = steamiGlyph8x8(code);
        for (uint8_t c = 0; c < 8; c++) {
            for (uint8_t row = 0; row < 8; row++) {
                n += (g[c] >> row) & 1;
            }
        }
    }
    return n * f * f;
}

/// The first glyph row that the 8x8 glyphs of s ink (0 = top of the cell).
static int32_t firstInkRow(const char* s) {
    const uint8_t m = TestScreen::inkRows(s, TestScreen::glyphCount(s), Ink::TEXT);
    int32_t row = 0;
    while (row < 8 && ((m >> row) & 1) == 0) {
        row++;
    }
    return row;
}

static const SteamiAt kAnchors[] = {SteamiAt::N,  SteamiAt::NE, SteamiAt::E,
                                    SteamiAt::SE, SteamiAt::S,  SteamiAt::SW,
                                    SteamiAt::W,  SteamiAt::NW, SteamiAt::CENTER};

// Values printed by golden_layout.py: text("Hi", at, STEAMI_WHITE, scale), [panel][scale - 1][at].
static const int16_t kHiBoxes[2][2][9][4] = {{{{57, 20, 68, 26},
                                               {95, 20, 106, 26},
                                               {101, 60, 112, 66},
                                               {97, 100, 108, 106},
                                               {57, 100, 68, 106},
                                               {18, 100, 29, 106},
                                               {13, 60, 24, 66},
                                               {20, 20, 31, 26},
                                               {57, 60, 68, 66}},
                                              {{50, 36, 73, 49},
                                               {78, 36, 101, 49},
                                               {78, 56, 101, 69},
                                               {78, 76, 101, 89},
                                               {50, 76, 73, 89},
                                               {22, 76, 45, 89},
                                               {22, 56, 45, 69},
                                               {22, 36, 45, 49},
                                               {50, 56, 73, 69}}},
                                             {{{106, 37, 129, 50},
                                               {175, 37, 198, 50},
                                               {188, 112, 211, 125},
                                               {178, 187, 201, 200},
                                               {106, 187, 129, 200},
                                               {35, 187, 58, 200},
                                               {24, 112, 47, 125},
                                               {38, 37, 61, 50},
                                               {106, 112, 129, 125}},
                                              {{92, 67, 139, 94},
                                               {143, 67, 190, 94},
                                               {143, 105, 190, 132},
                                               {143, 141, 190, 168},
                                               {92, 141, 139, 168},
                                               {41, 141, 88, 168},
                                               {41, 105, 88, 132},
                                               {41, 67, 88, 94},
                                               {92, 105, 139, 132}}}};

static void test_text_every_anchor_golden(void) {
    for (int panel = 0; panel < 2; panel++) {
        const int16_t size = panel == 0 ? 128 : 240;
        onPanel(size, panel == 1, [panel](SteamiMemoryBackend& mem, TestScreen& s) {
            for (uint8_t scale = 1; scale <= 2; scale++) {
                for (SteamiAt at : kAnchors) {
                    s.clear();
                    s.text("Hi", at, STEAMI_WHITE, scale);
                    const int16_t* b = kHiBoxes[panel][scale - 1][static_cast<int>(at)];
                    expectBox(inkBox(mem, STEAMI_WHITE), b[0], b[1], b[2], b[3]);
                }
            }
        });
    }
}

struct TitleCase {
    const char* s;
    int16_t box128[4];
    int16_t box240[4];
};

// Values printed by golden_layout.py: the GRAY ink box of title(s) on 128 and on 240.
static const TitleCase kTitles[] = {
    {"Temperature", {21, 20, 106, 27}, {34, 41, 205, 56}},  // whole: cells 20..107 / 32..207
    {"Comfort", {37, 20, 90, 26}, {66, 37, 173, 50}},
    {"Battery", {37, 20, 90, 27}, {66, 37, 173, 52}},
    {"ABCDEFGHIJKLM", {13, 29, 115, 35}, {18, 63, 223, 76}},         // 13 cells, at the cap
    {"ABCDEFGHIJKLMNOPQRST", {13, 29, 115, 35}, {18, 63, 223, 76}},  // 13 cells kept, centred
    {"______________", {8, 35, 119, 36}, {16, 75, 223, 78}},         // 14 kept / 13 kept
    {"______________T", {8, 35, 119, 36}, {16, 75, 223, 78}},        // 14 kept / 13 kept
};

static void test_title_rows(void) {
    for (int panel = 0; panel < 2; panel++) {
        onPanel(
            panel == 0 ? 128 : 240, panel == 1, [panel](SteamiMemoryBackend& mem, TestScreen& s) {
                for (const TitleCase& t : kTitles) {
                    s.clear();
                    s.title(t.s);
                    const int16_t* b = panel == 0 ? t.box128 : t.box240;
                    expectBox(inkBox(mem, STEAMI_GRAY), b[0], b[1], b[2], b[3]);
                    TEST_ASSERT_EQUAL_size_t(inkBoxAll(mem).count, inkBox(mem, STEAMI_GRAY).count);
                }
                // 13 cells kept of 20, centred on centerX(): the same pixels as the 13 alone.
                s.clear();
                s.title("ABCDEFGHIJKLM");
                const std::vector<uint16_t> thirteen = mem.pixels();
                s.clear();
                s.title("ABCDEFGHIJKLMNOPQRST");
                TEST_ASSERT_TRUE(thirteen == mem.pixels());
                // Scale 2 sits at its floor and is cut there: 7 cells on 128, 6 on 240.
                s.clear();
                s.text("ABCDEFGH", SteamiAt::N, STEAMI_WHITE, 2);
                if (panel == 0) {
                    expectBox(inkBox(mem, STEAMI_WHITE), 10, 36, 117, 49);  // cells 8..119
                } else {
                    expectBox(inkBox(mem, STEAMI_WHITE), 28, 67, 211, 94);  // cells 24..215
                }
            });
    }
    // 14 '_' on 128: whole, cells 8..119, ink on every column.
    onPanel(128, false, [](SteamiMemoryBackend& mem, TestScreen& s) {
        s.title("______________");
        expectBox(inkBox(mem, STEAMI_GRAY), 8, 35, 119, 36);
        TEST_ASSERT_EQUAL_size_t(wholeInk("______________", 1), inkBox(mem, STEAMI_GRAY).count);
    });
}

static void test_title_band(void) {
    static const char* const kKinds[] = {"ABCDEFGHIJKLMNOPQRST", "acemnorsuvwxzacemnor",
                                         "gjpqygjpqygjpqygjpqy", "                    "};
    forEachPanel([](SteamiMemoryBackend& mem, TestScreen& s) {
        const bool big = s.width() == 240;
        const int32_t topMin = big ? 37 : 20;
        const int32_t topMax = big ? 63 : 29;
        const int32_t inkMax = big ? 78 : 36;
        char buf[21];
        for (const char* kind : kKinds) {
            for (size_t len = 1; len <= 20; len++) {
                memcpy(buf, kind, len);
                buf[len] = '\0';
                s.clear();
                s.title(buf);
                const int32_t tw = TestScreen::cellWidth(buf, len, Ink::TEXT, s.k());
                const int32_t top = s.resolve(SteamiAt::N, tw, s.k()).y;
                TEST_ASSERT_TRUE_MESSAGE(top >= topMin && top <= topMax, buf);
                const InkBox b = inkBox(mem, STEAMI_GRAY);
                if (kind[0] == ' ') {
                    TEST_ASSERT_EQUAL_size_t_MESSAGE(0, inkBoxAll(mem).count,
                                                     "spaces draw nothing");
                    continue;
                }
                TEST_ASSERT_TRUE_MESSAGE(b.count > 0, buf);
                TEST_ASSERT_TRUE_MESSAGE(b.y0 >= topMin && b.y1 <= inkMax, buf);
                if (kind[0] == 'A') {
                    TEST_ASSERT_EQUAL_INT_MESSAGE(top, b.y0, buf);  // capitals ink their top row
                }
            }
        }
        // A lowercase-only title takes its width from the rows it inks, not from its cell row.
        const int32_t capRow = topMax;
        const uint8_t f = s.k();
        const int32_t lower = s.inkChord("acemnorsuvwxz", 13, Ink::TEXT, capRow, f).width();
        const int32_t upper = s.inkChord("ABCDEFGHIJKLM", 13, Ink::TEXT, capRow, f).width();
        TEST_ASSERT_EQUAL_INT32(big ? 215 : 109, lower);
        TEST_ASSERT_EQUAL_INT32(big ? 211 : 107, upper);
        // Both still keep 13 cells at the cap.
        s.clear();
        s.title("acemnorsuvwxz");
        const std::vector<uint16_t> thirteen = mem.pixels();
        s.clear();
        s.title("acemnorsuvwxzacemnor");
        TEST_ASSERT_TRUE(thirteen == mem.pixels());
    });
}

// Review Focus 3: a cut counts glyphs and never splits a code point; no '?' is drawn.
static void test_title_cut_keeps_whole_code_points(void) {
    static const char kAccented[] = "Temp\xC3\xA9rature\xC3\xA9\xC3\xA9\xC3\xA9\xC3\xA9";
    TEST_ASSERT_EQUAL_size_t(20, strlen(kAccented));
    TEST_ASSERT_EQUAL_size_t(15, TestScreen::glyphCount(kAccented));
    // Values printed by golden_layout.py: 13 glyphs kept, cells 12..115 (104 px); the ink is
    // 102 px wide, since the 8x8 'T' leaves column 0 empty and the 13th glyph, 'é' folded to 'e',
    // leaves column 7 empty.
    onPanel(128, false, [](SteamiMemoryBackend& mem, TestScreen& s) {
        s.title(kAccented);
        expectBox(inkBox(mem, STEAMI_GRAY), 13, 29, 114, 36);
    });
    onPanel(240, true, [](SteamiMemoryBackend& mem, TestScreen& s) {
        s.title(kAccented);
        expectBox(inkBox(mem, STEAMI_GRAY), 18, 63, 221, 78);
    });
    for (int16_t size : {128, 240}) {
        const std::vector<uint16_t> accented =
            rendered(size, size == 240, [](TestScreen& s) { s.title(kAccented); });
        const std::vector<uint16_t> ascii = rendered(size, size == 240, [](TestScreen& s) {
            s.text("Temperatureeee", SteamiAt::N, STEAMI_GRAY);
        });
        TEST_ASSERT_TRUE(accented == ascii);
    }
}

static void test_text_cut_at_every_anchor(void) {
    forEachPanel([](SteamiMemoryBackend& mem, TestScreen& s) {
        for (SteamiAt at : {SteamiAt::NE, SteamiAt::NW, SteamiAt::SE, SteamiAt::SW}) {
            s.clear();
            s.text("Hi", at);
            TEST_ASSERT_TRUE(inkBox(mem, STEAMI_WHITE).count > 0);
        }
        const int32_t k = s.k();
        // W: the text ends at or before the chord's right edge - k at its rows.
        s.clear();
        s.text("ABCDEFGHIJKLMNO", SteamiAt::W);
        InkBox b = inkBox(mem, STEAMI_WHITE);
        TEST_ASSERT_TRUE(b.count > 0);
        TEST_ASSERT_TRUE(b.x1 <= s.chord(b.y0, b.y1).right - k);
        TEST_ASSERT_TRUE(b.x0 >= s.marginEw(s.k()));  // anchored at the W margin
        // E: the text starts at or after the chord's left edge + k at its rows.
        s.clear();
        s.text("ABCDEFGHIJKLMNO", SteamiAt::E);
        b = inkBox(mem, STEAMI_WHITE);
        TEST_ASSERT_TRUE(b.count > 0);
        TEST_ASSERT_TRUE(b.x0 >= s.chord(b.y0, b.y1).left + k);
        TEST_ASSERT_TRUE(b.x1 < s.width() - s.marginEw(s.k()));  // right-aligned at the anchor
        // A huge scale is clamped, f never wraps: nothing outside the circle (onPanel).
        s.clear();
        s.text("x", SteamiAt::N, STEAMI_WHITE, 200);
        s.text("x", SteamiAt::CENTER, STEAMI_WHITE, 255);
    });
}

struct SubtitleCase {
    const char* l1;
    const char* l2;
    int16_t box128[2][4];
    int16_t box240[2][4];
};

// Values printed by golden_layout.py: the DARK ink box of each line on 128 and on 240. The script
// asserts the spec's cell rows and that every line is whole.
static const SubtitleCase kSubtitles[] = {
    {"WSEN-HIDS", nullptr, {{29, 108, 98, 114}}, {{50, 202, 189, 215}}},
    {"BME280", nullptr, {{41, 108, 86, 114}}, {{74, 202, 165, 215}}},
    {"BQ27441", nullptr, {{37, 108, 90, 114}}, {{66, 202, 173, 215}}},
    {"VL53L1X", nullptr, {{37, 108, 90, 114}}, {{66, 202, 173, 215}}},
    {"SURPRISED", nullptr, {{29, 108, 98, 114}}, {{50, 202, 189, 215}}},
    {"Comfortable", nullptr, {{21, 104, 106, 110}}, {{34, 188, 205, 201}}},
    {"VL53L1X ToF", nullptr, {{21, 104, 106, 110}}, {{34, 188, 205, 201}}},
    {"Uncomfortable", nullptr, {{13, 95, 114, 101}}, {{18, 166, 221, 179}}},
    {"20s window", nullptr, {{25, 107, 103, 113}}, {{42, 196, 199, 209}}},
    {"3842 mV",
     "BQ27441",
     {{37, 95, 90, 101}, {37, 106, 90, 112}},
     {{66, 177, 173, 190}, {66, 197, 173, 210}}},
    {"Comfortable",
     "WSEN-HIDS",
     {{21, 95, 106, 101}, {29, 106, 98, 112}},
     {{34, 173, 205, 186}, {50, 193, 189, 206}}},
    {"Uncomfortable",
     "WSEN-HIDS",
     {{13, 86, 114, 92}, {29, 97, 98, 103}},
     {{18, 151, 221, 164}, {50, 171, 189, 184}}},
    {"WSEN-HIDS",
     "20s window",
     {{29, 95, 98, 101}, {25, 106, 103, 112}},
     {{50, 176, 189, 189}, {42, 196, 199, 209}}},
    {"Comfortable",
     "BME280",
     {{21, 95, 106, 101}, {41, 106, 86, 112}},
     {{34, 173, 205, 186}, {74, 193, 165, 206}}},
    {"Uncomfortable",
     "BME280",
     {{13, 86, 114, 92}, {41, 97, 86, 103}},
     {{18, 151, 221, 164}, {74, 171, 165, 184}}},
    {"BME280",
     "20s window",
     {{41, 95, 86, 101}, {25, 106, 103, 112}},
     {{74, 176, 165, 189}, {42, 196, 199, 209}}},
    {"Comfortable",
     "APDS-9960",
     {{21, 95, 106, 101}, {29, 106, 98, 112}},
     {{34, 173, 205, 186}, {50, 193, 189, 206}}},
    {"Uncomfortable",
     "APDS-9960",
     {{13, 86, 114, 92}, {29, 97, 98, 103}},
     {{18, 151, 221, 164}, {50, 171, 189, 184}}},
    {"APDS-9960",
     "20s window",
     {{29, 95, 98, 101}, {25, 106, 103, 112}},
     {{50, 176, 189, 189}, {42, 196, 199, 209}}},
};

static void test_subtitle_rows(void) {
    for (int panel = 0; panel < 2; panel++) {
        onPanel(panel == 0 ? 128 : 240, panel == 1,
                [panel](SteamiMemoryBackend& mem, TestScreen& s) {
                    const int16_t cell = static_cast<int16_t>(8 * s.k());
                    for (const SubtitleCase& t : kSubtitles) {
                        s.clear();
                        s.subtitle(t.l1, t.l2);
                        const int16_t (*boxes)[4] = panel == 0 ? t.box128 : t.box240;
                        const char* lines[] = {t.l1, t.l2};
                        size_t total = 0;
                        for (int i = 0; i < 2 && lines[i] != nullptr; i++) {
                            const int16_t* b = boxes[i];
                            // The band of the line's cell rows; capitals ink the top row of the
                            // cell.
                            const InkBox got = inkBox(mem, STEAMI_DARK, b[1], b[1] + cell - 1);
                            expectBox(got, b[0], b[1], b[2], b[3]);
                            TEST_ASSERT_EQUAL_size_t_MESSAGE(wholeInk(lines[i], s.k()), got.count,
                                                             lines[i]);
                            total += got.count;
                        }
                        TEST_ASSERT_EQUAL_size_t(total, inkBoxAll(mem).count);
                    }
                });
    }
}

static void test_subtitle_block_limits(void) {
    static const char kCaps[] = "ABCDEFGHIJKLMNOPQRST";  // every line starts with 'A': ink on row 0
    forEachPanel([](SteamiMemoryBackend& mem, TestScreen& s) {
        const uint8_t k = s.k();
        const int32_t pitch = s.u(STEAMI_SUBTITLE_PITCH);
        const int32_t cell = 8 * k;
        char a[21];
        char b[21];
        char c[21];
        for (size_t n = 1; n <= 3; n++) {
            for (size_t len = 3; len <= 20; len++) {
                const size_t lens[3] = {len, 23 - len, 10};
                char* lines[3] = {a, b, c};
                int32_t widest = 0;
                for (size_t i = 0; i < 3; i++) {
                    memcpy(lines[i], kCaps, lens[i]);
                    lines[i][lens[i]] = '\0';
                    if (i < n) {
                        const int32_t w = TestScreen::cellWidth(lines[i], lens[i], Ink::TEXT, k);
                        widest = w > widest ? w : widest;
                    }
                }
                s.clear();
                if (n == 1) {
                    s.subtitle(a);
                } else if (n == 2) {
                    s.subtitle(a, b);
                } else {
                    s.subtitle(a, b, c);
                }
                const int32_t S = s.resolve(SteamiAt::S, widest, k).y;
                const int32_t start = n == 1 ? S + s.u(STEAMI_SUBTITLE_DROP)
                                             : S - pitch * static_cast<int32_t>(n - 1) / 2;
                const int32_t first = inkBoxAll(mem).y0;
                const int32_t last = first + pitch * static_cast<int32_t>(n - 1);
                TEST_ASSERT_TRUE_MESSAGE(last >= S, a);
                TEST_ASSERT_TRUE_MESSAGE(last <= start + pitch * static_cast<int32_t>(n - 1), a);
                bool cut = false;
                size_t total = 0;
                for (size_t i = 0; i < n; i++) {
                    const int32_t top = first + pitch * static_cast<int32_t>(i);
                    const InkBox line = inkBox(mem, STEAMI_DARK, static_cast<int16_t>(top),
                                               static_cast<int16_t>(top + cell - 1));
                    TEST_ASSERT_EQUAL_INT_MESSAGE(top, line.y0, "pitch u(11)");
                    cut = cut || line.count < wholeInk(lines[i], k);
                    total += line.count;
                }
                TEST_ASSERT_EQUAL_size_t(total, inkBoxAll(mem).count);
                if (cut) {
                    TEST_ASSERT_EQUAL_INT_MESSAGE(S, last, "a line is cut only at the S row");
                }
            }
        }
    });
}

static void test_subtitle_colour_and_shortcuts(void) {
    forEachPanel([](SteamiMemoryBackend& mem, TestScreen& s) {
        s.subtitle("a", STEAMI_GREEN);
        TEST_ASSERT_TRUE(inkBoxAll(mem).count > 0);
        TEST_ASSERT_EQUAL_size_t(inkBoxAll(mem).count, inkBox(mem, STEAMI_GREEN).count);
        s.clear();
        s.subtitle("a", "b", STEAMI_GREEN);
        TEST_ASSERT_TRUE(inkBoxAll(mem).count > 0);
        TEST_ASSERT_EQUAL_size_t(inkBoxAll(mem).count, inkBox(mem, STEAMI_GREEN).count);
        // "" is a line: the block keeps three lines, and the empty one draws nothing.
        s.clear();
        s.subtitle("a", "", "c");
        const uint8_t k = s.k();
        const int32_t pitch = s.u(STEAMI_SUBTITLE_PITCH);
        const int32_t S = s.resolve(SteamiAt::S, 8 * k, k).y;
        const int32_t top = S - pitch;  // first line of a three-line block; "a" and "c" fit there
        const int32_t aRow = top + firstInkRow("a") * k;
        const int32_t cRow = top + 2 * pitch + firstInkRow("c") * k;
        const InkBox first = inkBox(mem, STEAMI_DARK, static_cast<int16_t>(top),
                                    static_cast<int16_t>(top + 8 * k - 1));
        const InkBox middle = inkBox(mem, STEAMI_DARK, static_cast<int16_t>(top + pitch),
                                     static_cast<int16_t>(top + pitch + 8 * k - 1));
        const InkBox last = inkBox(mem, STEAMI_DARK, static_cast<int16_t>(top + 2 * pitch),
                                   static_cast<int16_t>(top + 2 * pitch + 8 * k - 1));
        TEST_ASSERT_EQUAL_INT(aRow, first.y0);
        TEST_ASSERT_EQUAL_size_t(0, middle.count);
        TEST_ASSERT_EQUAL_INT(cRow, last.y0);
        TEST_ASSERT_EQUAL_size_t(first.count + last.count, inkBoxAll(mem).count);
        // Empty and nullptr strings draw nothing and reach no backend call.
        s.clear();
        mem.clearCounters();
        s.subtitle(nullptr);
        s.subtitle(nullptr, "b");
        s.subtitle("");
        s.title(nullptr);
        s.title("");
        s.text(nullptr, SteamiAt::N);
        s.text("", SteamiAt::NE);
        TEST_ASSERT_EQUAL_size_t(0, inkBoxAll(mem).count);
        TEST_ASSERT_EQUAL_UINT32(0, mem.fillRectCalls());
        TEST_ASSERT_EQUAL_UINT32(0, mem.pixelCalls());
    });
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_format_integers);
    RUN_TEST(test_format_floats);
    RUN_TEST(test_format_negative_zero);
    RUN_TEST(test_format_dashes);
    RUN_TEST(test_format_extremes);
    RUN_TEST(test_format_respects_size);
    RUN_TEST(test_format_text_keeps_whole_code_points);
    RUN_TEST(test_value_overloads_compile_and_format);
    RUN_TEST(test_memory_backend_counts_outside_circle);
    RUN_TEST(test_scale_quantities);
    RUN_TEST(test_chord_convention);
    RUN_TEST(test_ink_rows_and_widths);
    RUN_TEST(test_fit_glyphs_longest_prefix);
    RUN_TEST(test_resolve_every_anchor);
    RUN_TEST(test_draw_glyphs_value_window);
    RUN_TEST(test_lifted_contrast);
    RUN_TEST(test_text_every_anchor_golden);
    RUN_TEST(test_title_rows);
    RUN_TEST(test_title_band);
    RUN_TEST(test_title_cut_keeps_whole_code_points);
    RUN_TEST(test_text_cut_at_every_anchor);
    RUN_TEST(test_subtitle_rows);
    RUN_TEST(test_subtitle_block_limits);
    RUN_TEST(test_subtitle_colour_and_shortcuts);
    return UNITY_END();
}
