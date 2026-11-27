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
    using SteamiScreen::blockHeight;
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
    using SteamiScreen::polarX;
    using SteamiScreen::polarY;
    using SteamiScreen::Region;
    using SteamiScreen::resolve;
    using SteamiScreen::resolveX;
    using SteamiScreen::resolveY;
    using SteamiScreen::Run;
    using SteamiScreen::u;
    using SteamiScreen::valueWidth;
};

using Region = TestScreen::Region;
using Ink = TestScreen::Ink;
using Run = TestScreen::Run;

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

/// fitGlyphs() over the whole of s, in the plan's argument order: the run of s at row y, the room
/// `limit`, `shrink` columns kept free at each end of the ink chord, and `pad`.
static size_t fit(TestScreen& screen, const char* s, Ink ink, uint8_t f, int32_t y, Region limit,
                  uint8_t shrink, int32_t pad) {
    Run t{s, TestScreen::glyphCount(s), y, ink, f};
    t.shrink = shrink;
    return screen.fitGlyphs(t, &limit, pad);
}

static void test_fit_glyphs_longest_prefix(void) {
    onPanel(128, true, [](SteamiMemoryBackend&, TestScreen& s) {
        const Region all{0, 127};
        TEST_ASSERT_EQUAL_size_t(13, fit(s, "ABCDEFGHIJKLMNOPQRST", Ink::TEXT, 1, 29, all, 0, 0));
        TEST_ASSERT_EQUAL_size_t(14, fit(s, "______________T", Ink::TEXT, 1, 29, all, 0, 0));
        TEST_ASSERT_EQUAL_size_t(14, fit(s, "              ", Ink::TEXT, 1, 29, all, 0, 0));
        // region 33..95; with the limit {0, 50}, 33..50: 18 px, "TEM"
        TEST_ASSERT_EQUAL_size_t(4, fit(s, "TEMP", Ink::LABEL, 1, 9, all, 1, 0));
        TEST_ASSERT_EQUAL_size_t(3, fit(s, "TEMP", Ink::LABEL, 1, 9, Region{0, 50}, 1, 0));
    });
    onPanel(240, true, [](SteamiMemoryBackend&, TestScreen& s) {
        const Region all{0, 239};
        TEST_ASSERT_EQUAL_size_t(13, fit(s, "ABCDEFGHIJKLMNOPQRST", Ink::TEXT, 2, 63, all, 0, 0));
        TEST_ASSERT_EQUAL_size_t(13, fit(s, "______________T", Ink::TEXT, 2, 63, all, 0, 0));
        TEST_ASSERT_EQUAL_size_t(14, fit(s, "              ", Ink::TEXT, 2, 63, all, 0, 0));
    });
}

static void test_fit_glyphs_room_and_run(void) {
    onPanel(128, true, [](SteamiMemoryBackend&, TestScreen& s) {
        // At most t.glyphs are tried; nullptr is the whole panel.
        Run t{"ABCDEFGHIJKLMNOPQRST", 5, 29, Ink::TEXT, 1};
        TEST_ASSERT_EQUAL_size_t(5, s.fitGlyphs(t));
        t.glyphs = 20;
        TEST_ASSERT_EQUAL_size_t(13, s.fitGlyphs(t));
        // The room comes back as the kept prefix's ink chord (row 29: 11..117) shrunk by t.shrink
        // and intersected with the room given: "TEMP" label at row 9, chord 32..96.
        Run label{"TEMP", 4, 9, Ink::LABEL, 1};
        label.shrink = 1;
        Region room{0, 50};
        TEST_ASSERT_EQUAL_size_t(3, s.fitGlyphs(label, &room));
        TEST_ASSERT_EQUAL_INT32(33, room.left);
        TEST_ASSERT_EQUAL_INT32(50, room.right);
        room = {0, 127};
        TEST_ASSERT_EQUAL_size_t(13, s.fitGlyphs(t, &room));
        TEST_ASSERT_EQUAL_INT32(11, room.left);
        TEST_ASSERT_EQUAL_INT32(117, room.right);
        // pad: 13 cells (104 px) plus 3 still fit the 107 px chord, plus 4 do not.
        room = {0, 127};
        TEST_ASSERT_EQUAL_size_t(13, s.fitGlyphs(t, &room, 3));
        room = {0, 127};
        TEST_ASSERT_EQUAL_size_t(12, s.fitGlyphs(t, &room, 4));
        // A prefix without ink keeps the room it was given (the chord of no ink is the panel).
        Run blank{"   ", 3, 29, Ink::TEXT, 1};
        blank.shrink = 1;
        room = {5, 60};
        TEST_ASSERT_EQUAL_size_t(3, s.fitGlyphs(blank, &room));
        TEST_ASSERT_EQUAL_INT32(5, room.left);
        TEST_ASSERT_EQUAL_INT32(60, room.right);
        // Nothing kept: the room is that of no ink too, shrunk by t.shrink.
        Run outside{"A", 1, -40, Ink::TEXT, 1};
        outside.shrink = 1;
        room = {0, 127};
        TEST_ASSERT_EQUAL_size_t(0, s.fitGlyphs(outside, &room));
        TEST_ASSERT_EQUAL_INT32(1, room.left);
        TEST_ASSERT_EQUAL_INT32(126, room.right);
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
    screen.drawGlyphs(Run{s, glyphs, 10, ink, 2}, 10, STEAMI_WHITE);
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
        const int32_t lower = s.inkChord(Run{"acemnorsuvwxz", 13, capRow, Ink::TEXT, f}).width();
        const int32_t upper = s.inkChord(Run{"ABCDEFGHIJKLM", 13, capRow, Ink::TEXT, f}).width();
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

// ---- value() at CENTER, N, S, W and E ----

static const char kDegC[] =
    "\xC2\xB0"
    "C";

/// The pixels the first `glyphs` glyphs of s light with `ink` at factor f, read from the font
/// tables: 5x7 for LABEL, the column window (2, 4) of '.' and ':' for VALUE.
static size_t runInk(const char* s, size_t glyphs, Ink ink, uint8_t f) {
    size_t n = 0;
    size_t i = 0;
    for (uint8_t code = steamiNextGlyph(s); code != 0 && i < glyphs;
         code = steamiNextGlyph(s), i++) {
        const bool small = ink == Ink::LABEL;
        const uint8_t* g = small ? steamiGlyph5x7(code) : steamiGlyph8x8(code);
        const bool window = ink == Ink::VALUE && (code == '.' || code == ':');
        const uint8_t first = window ? 2 : 0;
        const uint8_t end = small ? 5 : (window ? 6 : 8);
        for (uint8_t c = first; c < end; c++) {
            for (uint8_t row = 0; row < (small ? 7 : 8); row++) {
                n += (g[c] >> row) & 1;
            }
        }
    }
    return n * f * f;
}

static size_t valueInk(const char* s, uint8_t f) {
    return runInk(s, TestScreen::glyphCount(s), Ink::VALUE, f);
}

struct ValueCase {
    const char* text;
    SteamiAt at;
    const char* unit;
    const char* label;
    int16_t yOffset;
    int16_t value128[4];
    int16_t label128[4];  // all -1: no label
    int16_t unit128[4];   // all -1: no unit
    int16_t value240[4];
    int16_t label240[4];
    int16_t unit240[4];
};

#define NO_BOX {-1, -1, -1, -1}

// Values printed by golden_layout.py: the WHITE (value), GRAY (label) and LIGHT (unit) ink boxes of
// value(text, unit, at, label, STEAMI_WHITE, 2, yOffset) drawn alone, on 128 and on 240. 01's
// "23.5" is 56 px of cells from x 36 on 128 (the 8x8 '2' leaves columns 0 and 1 empty).
static const ValueCase kValues[] = {
    {"23.5",
     SteamiAt::CENTER,
     kDegC,
     nullptr,
     0,
     {38, 50, 89, 63},
     NO_BOX,
     {57, 66, 70, 72},
     {68, 91, 171, 118},
     NO_BOX,
     {106, 123, 133, 136}},
    {"72",
     SteamiAt::CENTER,
     "%",
     nullptr,
     -12,
     {50, 38, 77, 51},
     NO_BOX,
     {61, 54, 66, 60},
     {92, 69, 147, 96},
     NO_BOX,
     {114, 101, 125, 114}},
    {"23.5",
     SteamiAt::W,
     kDegC,
     "TEMP",
     0,
     {6, 50, 57, 63},
     {22, 39, 44, 45},
     {24, 66, 37, 72},
     {21, 96, 98, 116},
     {39, 75, 84, 88},
     {45, 120, 72, 133}},
    {"45.3",
     SteamiAt::E,
     "%",
     "HUM",
     0,
     {71, 50, 122, 63},
     {86, 39, 102, 45},
     {94, 66, 99, 72},
     {142, 96, 219, 116},
     {160, 75, 193, 88},
     {176, 120, 187, 133}},
    {"23.5",
     SteamiAt::N,
     kDegC,
     nullptr,
     0,
     {38, 36, 89, 49},
     NO_BOX,
     {57, 52, 70, 58},
     {68, 67, 171, 94},
     NO_BOX,
     {106, 99, 133, 112}},
    {"23.5",
     SteamiAt::N,
     nullptr,
     nullptr,
     0,
     {38, 36, 89, 49},
     NO_BOX,
     NO_BOX,
     {68, 67, 171, 94},
     NO_BOX,
     NO_BOX},
    {"23.5",
     SteamiAt::S,
     kDegC,
     nullptr,
     0,
     {38, 76, 89, 89},
     NO_BOX,
     {57, 92, 70, 98},
     {68, 141, 171, 168},
     NO_BOX,
     {106, 173, 133, 186}},
    {"23.5",
     SteamiAt::S,
     nullptr,
     nullptr,
     0,
     {38, 76, 89, 89},
     NO_BOX,
     NO_BOX,
     {68, 141, 171, 168},
     NO_BOX,
     NO_BOX},
};

#undef NO_BOX

/// The box b, or no pixel of colour c when b[0] is -1.
static void expectPart(const SteamiMemoryBackend& mem, SteamiRgb c, const int16_t* b) {
    const InkBox got = inkBox(mem, c);
    if (b[0] < 0) {
        TEST_ASSERT_EQUAL_size_t(0, got.count);
        return;
    }
    expectBox(got, b[0], b[1], b[2], b[3]);
}

static void test_value_center_golden(void) {
    for (int panel = 0; panel < 2; panel++) {
        // 128 grey and 240 colour.
        onPanel(
            panel == 0 ? 128 : 240, panel == 1, [panel](SteamiMemoryBackend& mem, TestScreen& s) {
                for (const ValueCase& v : kValues) {
                    s.clear();
                    s.value(v.text, v.unit, v.at, v.label, STEAMI_WHITE, 2, v.yOffset);
                    expectPart(mem, STEAMI_WHITE, panel == 0 ? v.value128 : v.value240);
                    expectPart(mem, STEAMI_GRAY, panel == 0 ? v.label128 : v.label240);
                    expectPart(mem, STEAMI_LIGHT, panel == 0 ? v.unit128 : v.unit240);
                    // Every part is whole here, and nothing else is drawn.
                    const uint8_t f = static_cast<uint8_t>(
                        v.at == SteamiAt::W || v.at == SteamiAt::E ? (panel == 0 ? 2 : 3)
                                                                   : (panel == 0 ? 2 : 4));
                    const uint8_t k = s.k();
                    const size_t total = valueInk(v.text, f) +
                                         (v.label ? runInk(v.label, 99, Ink::LABEL, k) : 0) +
                                         (v.unit ? runInk(v.unit, 99, Ink::TEXT, k) : 0);
                    TEST_ASSERT_EQUAL_size_t(valueInk(v.text, f), inkBox(mem, STEAMI_WHITE).count);
                    TEST_ASSERT_EQUAL_size_t(total, inkBoxAll(mem).count);
                }
            });
    }
    // The block: 8f + 8f/3 + 8k with a unit, 8f without (29 on 128 at f 2: top row 50).
    onPanel(128, false, [](SteamiMemoryBackend&, TestScreen& s) {
        TEST_ASSERT_EQUAL_INT32(29, s.blockHeight(2, true));
        TEST_ASSERT_EQUAL_INT32(16, s.blockHeight(2, false));
        TEST_ASSERT_EQUAL_INT32(18, s.blockHeight(1, true));
    });
    onPanel(240, true, [](SteamiMemoryBackend&, TestScreen& s) {
        TEST_ASSERT_EQUAL_INT32(58, s.blockHeight(4, true));
        TEST_ASSERT_EQUAL_INT32(48, s.blockHeight(3, true));
        TEST_ASSERT_EQUAL_INT32(24, s.blockHeight(3, false));
    });
}

struct SideCase {
    const char* text;
    uint8_t f128;
    uint8_t f240;
    int16_t w128[4];  // at W; E is the mirror image of the half region
    int16_t e128[4];
    int16_t w240[4];
    int16_t e240[4];
};

// Values printed by golden_layout.py: value(text, "°C", W or E) alone; every text is whole.
static const SideCase kSides[] = {
    {"1", 2, 3, {26, 50, 37, 63}, {91, 50, 102, 63}, {51, 96, 68, 116}, {172, 96, 189, 116}},
    {"8888", 1, 3, {16, 55, 45, 61}, {82, 55, 111, 61}, {15, 96, 104, 116}, {136, 96, 225, 116}},
    {"123456", 1, 2, {8, 55, 53, 61}, {74, 55, 119, 61}, {13, 102, 104, 115}, {135, 102, 226, 115}},
    {"12:34:56",
     1,
     1,
     {4, 55, 57, 61},
     {70, 55, 123, 61},
     {32, 107, 85, 113},
     {155, 107, 208, 113}},
};

static void test_value_glyph_scale_side_by_side(void) {
    forEachPanel([](SteamiMemoryBackend& mem, TestScreen& s) {
        const bool big = s.width() == 240;
        const int32_t f = big ? 3 : 2;
        // "23.5" and "45" side by side get the same f: ink height 7f.
        s.value(23.5f, kDegC, SteamiAt::W, "TEMP");
        InkBox west = inkBox(mem, STEAMI_WHITE);
        s.value(45, "%", SteamiAt::E, "HUM");
        const InkBox both = inkBox(mem, STEAMI_WHITE);
        TEST_ASSERT_EQUAL_INT(7 * f, west.y1 - west.y0 + 1);
        TEST_ASSERT_EQUAL_INT(7 * f, both.y1 - both.y0 + 1);
        TEST_ASSERT_EQUAL_size_t(valueInk("23.5", f) + valueInk("45", f), both.count);
        const std::vector<uint16_t> westThenEast = mem.pixels();
        s.clear();
        s.value(45, "%", SteamiAt::E, "HUM");
        s.value(23.5f, kDegC, SteamiAt::W, "TEMP");
        TEST_ASSERT_TRUE(westThenEast == mem.pixels());
        // A value no wider than "88.8" gets the f of "88.8".
        s.clear();
        s.value("88.8", kDegC, SteamiAt::W);
        west = inkBox(mem, STEAMI_WHITE);
        TEST_ASSERT_EQUAL_INT(7 * f, west.y1 - west.y0 + 1);
        for (const SideCase& v : kSides) {
            for (SteamiAt at : {SteamiAt::W, SteamiAt::E}) {
                s.clear();
                s.value(v.text, kDegC, at);
                const int16_t* b =
                    at == SteamiAt::W ? (big ? v.w240 : v.w128) : (big ? v.e240 : v.e128);
                const InkBox got = inkBox(mem, STEAMI_WHITE);
                expectBox(got, b[0], b[1], b[2], b[3]);
                const uint8_t vf = big ? v.f240 : v.f128;
                TEST_ASSERT_EQUAL_size_t_MESSAGE(valueInk(v.text, vf), got.count, v.text);
                // Inside the half region: W ends before centerX() - u(3), E starts after it.
                if (at == SteamiAt::W) {
                    TEST_ASSERT_TRUE(got.x1 <= s.centerX() - s.u(3) - 1);
                } else {
                    TEST_ASSERT_TRUE(got.x0 >= s.centerX() + s.u(3) + 1);
                }
            }
        }
    });
}

/// True when value(text, unit, CENTER, nullptr, WHITE, 2, yOffset) draws fewer value pixels than
/// the whole text at f 1 (the f every 15-character value falls to).
static bool centerCut(TestScreen& s, SteamiMemoryBackend& mem, const char* text, const char* unit,
                      int16_t yOffset) {
    s.clear();
    s.value(text, unit, SteamiAt::CENTER, nullptr, STEAMI_WHITE, 2, yOffset);
    return inkBox(mem, STEAMI_WHITE).count < valueInk(text, 1);
}

static void test_value_truncation(void) {
    static const char kFifteen[] = "888888888888888";  // 120 px at f 1
    static const char kNumeric[] = "-999999936.0000";  // 116 px at f 1
    forEachPanel([](SteamiMemoryBackend& mem, TestScreen& s) {
        const bool big = s.width() == 240;
        // W/E texts wider than the x1 half region (59 px on 128) are cut to their longest fitting
        // prefix.
        for (const char* t : {"12345678", "-1234.56", "Uncomfortable", "ABCDEFGHIJKLMNO"}) {
            for (SteamiAt at : {SteamiAt::W, SteamiAt::E}) {
                s.clear();
                s.value(t, kDegC, at);
                const InkBox got = inkBox(mem, STEAMI_WHITE);
                TEST_ASSERT_TRUE_MESSAGE(got.count > 0, t);
                const Region half =
                    at == SteamiAt::W
                        ? Region{s.chord(got.y0, got.y1).left + s.k(), s.centerX() - s.u(3) - 1}
                        : Region{s.centerX() + s.u(3) + 1, s.chord(got.y0, got.y1).right - s.k()};
                TEST_ASSERT_TRUE_MESSAGE(got.x0 >= half.left && got.x1 <= half.right, t);
                const bool whole = got.count == valueInk(t, 1);
                // On 240 the x1 half region is 112 px: only the 15-glyph text (120 px) is cut.
                const bool expectWhole = big && strcmp(t, "ABCDEFGHIJKLMNO") != 0;
                TEST_ASSERT_EQUAL_MESSAGE(expectWhole, whole, t);
            }
        }
        // Values printed by golden_layout.py: "Uncomfortable" keeps 7 glyphs at W on 128 and is
        // whole on 240; "ABCDEFGHIJKLMNO" keeps 14 on 240.
        s.clear();
        s.value("Uncomfortable", kDegC, SteamiAt::W);
        if (big) {
            expectBox(inkBox(mem, STEAMI_WHITE), 8, 107, 109, 113);
        } else {
            expectBox(inkBox(mem, STEAMI_WHITE), 4, 55, 57, 61);
        }
        s.clear();
        s.value("ABCDEFGHIJKLMNO", kDegC, SteamiAt::W);
        if (big) {
            expectBox(inkBox(mem, STEAMI_WHITE), 4, 107, 113, 113);
            TEST_ASSERT_EQUAL_size_t(runInk("ABCDEFGHIJKLMN", 14, Ink::VALUE, 1),
                                     inkBox(mem, STEAMI_WHITE).count);
        }
        // A 15-character CENTER value is whole at the default rows and cut from a yOffset on.
        TEST_ASSERT_FALSE(centerCut(s, mem, kFifteen, kDegC, 0));
        TEST_ASSERT_TRUE(centerCut(s, mem, kFifteen, kDegC, big ? -50 : -12));
        s.clear();
        s.value(kFifteen, kDegC, SteamiAt::CENTER, nullptr, STEAMI_WHITE, 2, big ? -50 : -12);
        if (big) {
            expectBox(inkBox(mem, STEAMI_WHITE), 69, 14, 170, 20);  // 13 glyphs kept
        } else {
            expectBox(inkBox(mem, STEAMI_WHITE), 9, 43, 118, 49);  // 14 glyphs kept
        }
        // The yOffset thresholds of 5.9: cut from the first, whole one step closer to 0.
        struct Threshold {
            const char* text;
            const char* unit;
            int16_t up;
            int16_t down;
        };
        const Threshold small[] = {
            {kFifteen, kDegC, -7, 19}, {kFifteen, nullptr, -12, 14}, {kNumeric, kDegC, -14, 26}};
        const Threshold large[] = {
            {kFifteen, kDegC, -48, 59}, {kFifteen, nullptr, -53, 54}, {kNumeric, kDegC, -49, 60}};
        for (const Threshold& t : big ? large : small) {
            TEST_ASSERT_TRUE(centerCut(s, mem, t.text, t.unit, t.up));
            TEST_ASSERT_FALSE(centerCut(s, mem, t.text, t.unit, static_cast<int16_t>(t.up + 1)));
            TEST_ASSERT_TRUE(centerCut(s, mem, t.text, t.unit, t.down));
            TEST_ASSERT_FALSE(centerCut(s, mem, t.text, t.unit, static_cast<int16_t>(t.down - 1)));
        }
    });
}

static void test_value_labels_and_units(void) {
    forEachPanel([](SteamiMemoryBackend& mem, TestScreen& s) {
        const bool big = s.width() == 240;
        const uint8_t k = s.k();
        // Values printed by golden_layout.py: "TEMPERATURE" keeps 9 cells on 128, 8 on 240.
        const size_t kept = big ? 8 : 9;
        for (SteamiAt at : {SteamiAt::W, SteamiAt::E}) {
            s.clear();
            s.value(23.5f, kDegC, at, "TEMPERATURE");
            const InkBox label = inkBox(mem, STEAMI_GRAY);
            TEST_ASSERT_EQUAL_size_t(runInk("TEMPERATURE", kept, Ink::LABEL, k), label.count);
            if (at == SteamiAt::W) {
                if (big) {
                    expectBox(label, 15, 75, 108, 88);
                } else {
                    expectBox(label, 7, 39, 59, 45);
                }
            } else {
                if (big) {
                    expectBox(label, 130, 75, 223, 88);
                } else {
                    expectBox(label, 68, 39, 120, 45);
                }
            }
            // Up to 9 (128) or 8 (240) characters stay whole.
            const char* whole = big ? "TEMPERAT" : "TEMPERATU";
            s.clear();
            s.value(23.5f, kDegC, at, whole);
            TEST_ASSERT_EQUAL_size_t(runInk(whole, 99, Ink::LABEL, k),
                                     inkBox(mem, STEAMI_GRAY).count);
            // A long unit is cut inside the half region at its own rows.
            s.clear();
            s.value(23.5f, "ABCDEFGHIJ", at);
            const InkBox unit = inkBox(mem, STEAMI_LIGHT);
            TEST_ASSERT_TRUE(unit.count > 0);
            TEST_ASSERT_TRUE(unit.count < runInk("ABCDEFGHIJ", 99, Ink::TEXT, k));
            if (at == SteamiAt::W) {
                TEST_ASSERT_TRUE(unit.x0 >= s.chord(unit.y0, unit.y1).left + k);
                TEST_ASSERT_TRUE(unit.x1 <= s.centerX() - s.u(3) - 1);
            } else {
                TEST_ASSERT_TRUE(unit.x0 >= s.centerX() + s.u(3) + 1);
                TEST_ASSERT_TRUE(unit.x1 <= s.chord(unit.y0, unit.y1).right - k);
            }
        }
        // At CENTER a long label and unit are cut to the chord at their rows, centred.
        s.clear();
        s.value(23.5f, "ABCDEFGHIJKLMNOPQRST", SteamiAt::CENTER, "ABCDEFGHIJKLMNOPQRSTUVWXYZ");
        TEST_ASSERT_TRUE(inkBox(mem, STEAMI_GRAY).count > 0);
        TEST_ASSERT_TRUE(inkBox(mem, STEAMI_LIGHT).count > 0);
    });
}

/// value(v, unit) alone on a fresh panel after setDecimals(d).
static std::vector<uint16_t> valueAlone(int16_t size, SteamiValue v, const char* unit, uint8_t d) {
    return rendered(size, size == 240, [v, unit, d](TestScreen& s) {
        s.setDecimals(d);
        s.value(v, unit);
    });
}

static void test_value_nan_and_decimals(void) {
    for (int16_t size : {128, 240}) {
        TEST_ASSERT_TRUE(valueAlone(size, NAN, kDegC, 1) == valueAlone(size, "--", kDegC, 1));
        TEST_ASSERT_TRUE(valueAlone(size, 23.456f, kDegC, 2) ==
                         valueAlone(size, "23.46", kDegC, 1));
        TEST_ASSERT_TRUE(valueAlone(size, 23.456f, kDegC, 0) == valueAlone(size, "23", kDegC, 1));
        TEST_ASSERT_TRUE(valueAlone(size, 23.456f, kDegC, 9) ==
                         valueAlone(size, "23.4560", kDegC, 1));
        TEST_ASSERT_TRUE(valueAlone(size, 23.456f, kDegC, 1) == valueAlone(size, "23.5", kDegC, 1));
        TEST_ASSERT_FALSE(valueAlone(size, NAN, kDegC, 1) == valueAlone(size, "", kDegC, 1));
    }
}

static void test_value_edge_cases(void) {
    for (int16_t size : {128, 240}) {
        const bool color = size == 240;
        // A scale of 0 counts as 1.
        TEST_ASSERT_TRUE(rendered(size, color, [](TestScreen& s) {
                             s.value(23.5f, kDegC, SteamiAt::CENTER, nullptr, STEAMI_WHITE, 0);
                         }) == rendered(size, color, [](TestScreen& s) {
                             s.value(23.5f, kDegC, SteamiAt::CENTER, nullptr, STEAMI_WHITE, 1);
                         }));
        // An empty unit is no unit.
        TEST_ASSERT_TRUE(rendered(size, color, [](TestScreen& s) { s.value(0, ""); }) ==
                         rendered(size, color, [](TestScreen& s) { s.value(0, nullptr); }));
    }
    forEachPanel([](SteamiMemoryBackend& mem, TestScreen& s) {
        // Rows out of the disc: nothing drawn (onPanel checks the circle).
        s.value(23.5f, kDegC, SteamiAt::W, "TEMP", STEAMI_WHITE, 2, 80);
        TEST_ASSERT_EQUAL_size_t(0, mem.litCount());
        // An empty value draws its label and unit only.
        s.clear();
        s.value("", kDegC, SteamiAt::CENTER, "TEMP");
        TEST_ASSERT_EQUAL_size_t(0, inkBox(mem, STEAMI_WHITE).count);
        TEST_ASSERT_TRUE(inkBox(mem, STEAMI_GRAY).count > 0);
        TEST_ASSERT_TRUE(inkBox(mem, STEAMI_LIGHT).count > 0);
        TEST_ASSERT_EQUAL_size_t(inkBox(mem, STEAMI_GRAY).count + inkBox(mem, STEAMI_LIGHT).count,
                                 inkBoxAll(mem).count);
        // Empty label and unit: the value only.
        s.clear();
        s.value(0, "", SteamiAt::CENTER, "");
        TEST_ASSERT_EQUAL_size_t(valueInk("0", static_cast<uint8_t>(2 * s.k())),
                                 inkBox(mem, STEAMI_WHITE).count);
        TEST_ASSERT_EQUAL_size_t(inkBox(mem, STEAMI_WHITE).count, inkBoxAll(mem).count);
        // A huge scale is clamped: f never wraps.
        s.clear();
        s.value(8, nullptr, SteamiAt::CENTER, nullptr, STEAMI_WHITE, 255);
        TEST_ASSERT_TRUE(inkBox(mem, STEAMI_WHITE).count > 0);
    });
}

// Review Focus 4: no int16_t wrap draws a value elsewhere.
static void test_value_offsets_at_int16_limits(void) {
    forEachPanel([](SteamiMemoryBackend& mem, TestScreen& s) {
        for (int16_t y : {static_cast<int16_t>(32767), static_cast<int16_t>(-32767),
                          static_cast<int16_t>(-32768), static_cast<int16_t>(20000),
                          static_cast<int16_t>(-20000)}) {
            for (SteamiAt at : {SteamiAt::CENTER, SteamiAt::W, SteamiAt::E}) {
                s.value(23.5f, kDegC, at, "TEMP", STEAMI_WHITE, 2, y);
                s.value("888888888888888", kDegC, at, "TEMPERATURE", STEAMI_WHITE, 1, y);
            }
        }
        TEST_ASSERT_EQUAL_size_t(0, mem.litCount());
        TEST_ASSERT_EQUAL_UINT32(0, mem.fillRectCalls());
        TEST_ASSERT_EQUAL_UINT32(0, mem.pixelCalls());
    });
}

static void test_value_colour_shortcut(void) {
    for (int16_t size : {128, 240}) {
        const bool color = size == 240;
        const std::vector<uint16_t> shortcut =
            rendered(size, color, [](TestScreen& s) { s.value(23.5f, kDegC, STEAMI_GREEN); });
        TEST_ASSERT_TRUE(shortcut == rendered(size, color, [](TestScreen& s) {
                             s.value(23.5f, kDegC, SteamiAt::CENTER, nullptr, STEAMI_GREEN);
                         }));
        TEST_ASSERT_FALSE(shortcut ==
                          rendered(size, color, [](TestScreen& s) { s.value(23.5f, kDegC); }));
    }
}

// ---- value() at the corners ----

/// Only the pixels of colour c in a copy of mem's panel, every other pixel 0.
static std::vector<uint16_t> pixelsOf(const SteamiMemoryBackend& mem, SteamiRgb c) {
    std::vector<uint16_t> out = mem.pixels();
    const uint16_t stored = mem.stored(c);
    for (uint16_t& p : out) {
        p = p == stored ? stored : 0;
    }
    return out;
}

static void test_value_corner_worked_examples(void) {
    // 128 grey, value(8, "C", NE, "TEMP", WHITE, 1): the value cell at x 102, row 20 (the 8x8 '8'
    // leaves column 0 empty, so its ink starts at 103); the label's rows 9..15 leave the region
    // [33, 95], so "TEMP" (24 px) moves from x 94 to x 72 (the 5x7 'T' inks column 0).
    onPanel(128, false, [](SteamiMemoryBackend& mem, TestScreen& s) {
        s.value(8, "C", SteamiAt::NE, "TEMP", STEAMI_WHITE, 1);
        const InkBox value = inkBox(mem, STEAMI_WHITE);
        TEST_ASSERT_EQUAL_INT(103, value.x0);
        TEST_ASSERT_EQUAL_INT(20, value.y0);
        const InkBox label = inkBox(mem, STEAMI_GRAY);
        TEST_ASSERT_EQUAL_INT(72, label.x0);
        TEST_ASSERT_TRUE(label.x1 <= 95);
        TEST_ASSERT_EQUAL_INT(9, label.y0);
        TEST_ASSERT_EQUAL_INT(15, label.y1);
        TEST_ASSERT_EQUAL_size_t(runInk("TEMP", 4, Ink::LABEL, 1), label.count);
        // At SE the unit "C" (rows 108..114, region [26, 102]) moves from x 103 to x 95 ('C'
        // leaves column 0 empty).
        s.clear();
        s.value(8, "C", SteamiAt::SE, "TEMP", STEAMI_WHITE, 1);
        const InkBox unit = inkBox(mem, STEAMI_LIGHT);
        TEST_ASSERT_EQUAL_INT(96, unit.x0);
        TEST_ASSERT_EQUAL_INT(108, unit.y0);
        TEST_ASSERT_EQUAL_INT(114, unit.y1);
    });
    // 240 colour, the NE call: the value falls to f 1 at x 180, row 22; the label's rows 1..14
    // leave the region [107, 133] (27 px), so "TEMP" (48 px) is cut to "TE" (24 px) at x 110.
    std::vector<uint16_t> label;
    onPanel(240, true, [&label](SteamiMemoryBackend& mem, TestScreen& s) {
        s.value(8, "C", SteamiAt::NE, "TEMP", STEAMI_WHITE, 1);
        const InkBox value = inkBox(mem, STEAMI_WHITE);
        TEST_ASSERT_EQUAL_INT(181, value.x0);
        TEST_ASSERT_EQUAL_INT(22, value.y0);
        TEST_ASSERT_EQUAL_size_t(valueInk("8", 1), value.count);
        const InkBox box = inkBox(mem, STEAMI_GRAY);
        TEST_ASSERT_EQUAL_INT(110, box.x0);
        TEST_ASSERT_TRUE(box.x1 <= 133);
        TEST_ASSERT_EQUAL_INT(1, box.y0);
        TEST_ASSERT_EQUAL_INT(14, box.y1);
        label = pixelsOf(mem, STEAMI_GRAY);
    });
    // LABEL stays 5x7 x k here (4b-font Task 5 moves it to 6x12), while textSmall(..., 2) is now
    // 6x12 x 2 on 240: the reference is the kernel's own LABEL run.
    TEST_ASSERT_TRUE(label == rendered(240, true, [](TestScreen& s) {
                         s.drawGlyphs(Run{"TE", 2, 1, Ink::LABEL, 2}, 110, STEAMI_GRAY);
                     }));
}

/// value(text, unit, at, label, c, scale) on a size x size panel.
struct CornerCall {
    int16_t size;
    const char* text;
    const char* unit;
    SteamiAt at;
    const char* label;
    uint8_t scale;
};

/// What golden_layout.py prints for a CornerCall.
struct CornerGolden {
    uint8_t f;          // the value's glyph factor
    uint8_t kept;       // value glyphs drawn
    uint8_t labelKept;  // label glyphs drawn; the unit is whole in every case
    int16_t value[4];
    int16_t label[4];  // all -1: no label
    int16_t unit[4];   // all -1: no unit
};

struct CornerCase {
    CornerCall call;
    CornerGolden golden;
};

#define NO_BOX {-1, -1, -1, -1}

// Values printed by golden_layout.py: the WHITE (value), GRAY (label) and LIGHT (unit) ink boxes of
// value(text, unit, at, label, STEAMI_WHITE, scale) drawn alone. "-12345.7" is -12345.678f with
// one decimal; "ABCDEFGHIJKLMNO" is cut to 13 glyphs on 128.
static const CornerCase kCorners[] = {
    {{128, "8", nullptr, SteamiAt::NE, nullptr, 1}, {1, 1, 0, {103, 20, 108, 26}, NO_BOX, NO_BOX}},
    {{128, "8", "C", SteamiAt::NE, "TEMP", 1},
     {1, 1, 4, {103, 20, 108, 26}, {72, 9, 94, 15}, {103, 28, 108, 34}}},
    {{128, "8", "C", SteamiAt::NE, "TEMPERATURE", 1},
     {1, 1, 10, {103, 20, 108, 26}, {36, 9, 94, 15}, {103, 28, 108, 34}}},
    {{128, "8", "C", SteamiAt::NE, "TEMPERATURES12", 1},
     {1, 1, 10, {103, 20, 108, 26}, {36, 9, 94, 15}, {103, 28, 108, 34}}},
    {{128, "8", "C", SteamiAt::NE, "TEMP", 2},
     {2, 1, 4, {94, 36, 105, 49}, {88, 25, 110, 31}, {97, 52, 102, 58}}},
    {{128, "8", "C", SteamiAt::NE, "TEMPERATURE", 2},
     {2, 1, 11, {94, 36, 105, 49}, {48, 25, 112, 31}, {97, 52, 102, 58}}},
    {{128, "8", "C", SteamiAt::NE, "TEMPERATURES12", 2},
     {2, 1, 14, {94, 36, 105, 49}, {30, 25, 112, 31}, {97, 52, 102, 58}}},
    {{128, "-12345.7", kDegC, SteamiAt::NE, "TEMPERATURES12", 2},
     {1, 8, 10, {51, 20, 108, 26}, {36, 9, 94, 15}, {73, 28, 86, 34}}},
    {{128, "Uncomfortable", kDegC, SteamiAt::NE, "TEMPERATURES12", 2},
     {1, 13, 14, {13, 29, 114, 35}, {22, 18, 104, 24}, {57, 37, 70, 43}}},
    {{128, "ABCDEFGHIJKLMNO", kDegC, SteamiAt::NE, "TEMPERATURES12", 2},
     {1, 13, 14, {13, 29, 115, 35}, {22, 18, 104, 24}, {57, 37, 70, 43}}},
    {{128, "8", "C", SteamiAt::NW, "TEMP", 1},
     {1, 1, 4, {20, 20, 25, 26}, {33, 9, 55, 15}, {20, 28, 25, 34}}},
    {{128, "8", "C", SteamiAt::NW, "TEMPERATURE", 1},
     {1, 1, 10, {20, 20, 25, 26}, {33, 9, 91, 15}, {20, 28, 25, 34}}},
    {{128, "8", "C", SteamiAt::NW, "TEMPERATURES12", 1},
     {1, 1, 10, {20, 20, 25, 26}, {33, 9, 91, 15}, {20, 28, 25, 34}}},
    {{128, "8", "C", SteamiAt::NW, "TEMP", 2},
     {2, 1, 4, {22, 36, 33, 49}, {16, 25, 38, 31}, {25, 52, 30, 58}}},
    {{128, "8", "C", SteamiAt::NW, "TEMPERATURE", 2},
     {2, 1, 11, {22, 36, 33, 49}, {15, 25, 79, 31}, {25, 52, 30, 58}}},
    {{128, "8", "C", SteamiAt::NW, "TEMPERATURES12", 2},
     {2, 1, 14, {22, 36, 33, 49}, {15, 25, 97, 31}, {25, 52, 30, 58}}},
    {{128, "-12345.7", kDegC, SteamiAt::NW, "TEMPERATURES12", 2},
     {1, 8, 10, {20, 20, 77, 26}, {33, 9, 91, 15}, {42, 28, 55, 34}}},
    {{128, "Uncomfortable", kDegC, SteamiAt::NW, "TEMPERATURES12", 2},
     {1, 13, 14, {13, 29, 114, 35}, {22, 18, 104, 24}, {57, 37, 70, 43}}},
    {{128, "ABCDEFGHIJKLMNO", kDegC, SteamiAt::NW, "TEMPERATURES12", 2},
     {1, 13, 14, {13, 29, 115, 35}, {22, 18, 104, 24}, {57, 37, 70, 43}}},
    {{128, "8", "C", SteamiAt::SE, "TEMP", 1},
     {1, 1, 4, {104, 100, 109, 106}, {95, 89, 117, 95}, {96, 108, 101, 114}}},
    {{128, "8", "C", SteamiAt::SE, "TEMPERATURE", 1},
     {1, 1, 11, {104, 100, 109, 106}, {53, 89, 117, 95}, {96, 108, 101, 114}}},
    {{128, "8", "C", SteamiAt::SE, "TEMPERATURES12", 1},
     {1, 1, 14, {104, 100, 109, 106}, {35, 89, 117, 95}, {96, 108, 101, 114}}},
    {{128, "8", "C", SteamiAt::SE, "TEMP", 2},
     {2, 1, 4, {94, 76, 105, 89}, {88, 65, 110, 71}, {97, 92, 102, 98}}},
    {{128, "8", "C", SteamiAt::SE, "TEMPERATURE", 2},
     {2, 1, 11, {94, 76, 105, 89}, {61, 65, 125, 71}, {97, 92, 102, 98}}},
    {{128, "8", "C", SteamiAt::SE, "TEMPERATURES12", 2},
     {2, 1, 14, {94, 76, 105, 89}, {43, 65, 125, 71}, {97, 92, 102, 98}}},
    {{128, "-12345.7", kDegC, SteamiAt::SE, "TEMPERATURES12", 2},
     {1, 8, 14, {52, 100, 109, 106}, {35, 89, 117, 95}, {74, 108, 87, 114}}},
    {{128, "Uncomfortable", kDegC, SteamiAt::SE, "TEMPERATURES12", 2},
     {1, 13, 14, {13, 91, 114, 97}, {22, 80, 104, 86}, {57, 99, 70, 105}}},
    {{128, "ABCDEFGHIJKLMNO", kDegC, SteamiAt::SE, "TEMPERATURES12", 2},
     {1, 13, 14, {13, 91, 115, 97}, {22, 80, 104, 86}, {57, 99, 70, 105}}},
    {{128, "8", "C", SteamiAt::SW, "TEMP", 1},
     {1, 1, 4, {19, 100, 24, 106}, {10, 89, 32, 95}, {27, 108, 32, 114}}},
    {{128, "8", "C", SteamiAt::SW, "TEMPERATURE", 1},
     {1, 1, 11, {19, 100, 24, 106}, {10, 89, 74, 95}, {27, 108, 32, 114}}},
    {{128, "8", "C", SteamiAt::SW, "TEMPERATURES12", 1},
     {1, 1, 14, {19, 100, 24, 106}, {10, 89, 92, 95}, {27, 108, 32, 114}}},
    {{128, "8", "C", SteamiAt::SW, "TEMP", 2},
     {2, 1, 4, {22, 76, 33, 89}, {16, 65, 38, 71}, {25, 92, 30, 98}}},
    {{128, "8", "C", SteamiAt::SW, "TEMPERATURE", 2},
     {2, 1, 11, {22, 76, 33, 89}, {2, 65, 66, 71}, {25, 92, 30, 98}}},
    {{128, "8", "C", SteamiAt::SW, "TEMPERATURES12", 2},
     {2, 1, 14, {22, 76, 33, 89}, {2, 65, 84, 71}, {25, 92, 30, 98}}},
    {{128, "-12345.7", kDegC, SteamiAt::SW, "TEMPERATURES12", 2},
     {1, 8, 14, {19, 100, 76, 106}, {10, 89, 92, 95}, {41, 108, 54, 114}}},
    {{128, "Uncomfortable", kDegC, SteamiAt::SW, "TEMPERATURES12", 2},
     {1, 13, 14, {13, 91, 114, 97}, {22, 80, 104, 86}, {57, 99, 70, 105}}},
    {{128, "ABCDEFGHIJKLMNO", kDegC, SteamiAt::SW, "TEMPERATURES12", 2},
     {1, 13, 14, {13, 91, 115, 97}, {22, 80, 104, 86}, {57, 99, 70, 105}}},
    {{240, "8", "C", SteamiAt::NE, "TEMP", 1},
     {1, 1, 2, {181, 22, 186, 28}, {110, 1, 131, 14}, {178, 30, 189, 43}}},
    {{240, "8", "C", SteamiAt::NE, "TEMPERATURE", 1},
     {1, 1, 2, {181, 22, 186, 28}, {110, 1, 131, 14}, {178, 30, 189, 43}}},
    {{240, "8", "C", SteamiAt::NE, "TEMPERATURES12", 1},
     {1, 1, 2, {181, 22, 186, 28}, {110, 1, 131, 14}, {178, 30, 189, 43}}},
    {{240, "8", "C", SteamiAt::NE, "TEMP", 2},
     {4, 1, 4, {175, 67, 198, 94}, {163, 46, 208, 59}, {181, 99, 192, 112}}},
    {{240, "8", "C", SteamiAt::NE, "TEMPERATURE", 2},
     {4, 1, 11, {175, 67, 198, 94}, {81, 46, 210, 59}, {181, 99, 192, 112}}},
    {{240, "8", "C", SteamiAt::NE, "TEMPERATURES12", 2},
     {4, 1, 14, {175, 67, 198, 94}, {45, 46, 210, 59}, {181, 99, 192, 112}}},
    {{240, "-12345.7", kDegC, SteamiAt::NE, "TEMPERATURES12", 2},
     {3, 8, 13, {33, 52, 206, 72}, {42, 31, 193, 44}, {106, 76, 133, 89}}},
    {{240, "Uncomfortable", kDegC, SteamiAt::NE, "TEMPERATURES12", 2},
     {1, 13, 2, {85, 22, 186, 28}, {110, 1, 131, 14}, {122, 30, 149, 43}}},
    {{240, "ABCDEFGHIJKLMNO", kDegC, SteamiAt::NE, "TEMPERATURES12", 2},
     {1, 15, 2, {69, 22, 186, 28}, {110, 1, 131, 14}, {114, 30, 141, 43}}},
    {{240, "8", "C", SteamiAt::NW, "TEMP", 1},
     {1, 1, 2, {54, 22, 59, 28}, {107, 1, 128, 14}, {51, 30, 62, 43}}},
    {{240, "8", "C", SteamiAt::NW, "TEMPERATURE", 1},
     {1, 1, 2, {54, 22, 59, 28}, {107, 1, 128, 14}, {51, 30, 62, 43}}},
    {{240, "8", "C", SteamiAt::NW, "TEMPERATURES12", 1},
     {1, 1, 2, {54, 22, 59, 28}, {107, 1, 128, 14}, {51, 30, 62, 43}}},
    {{240, "8", "C", SteamiAt::NW, "TEMP", 2},
     {4, 1, 4, {41, 67, 64, 94}, {29, 46, 74, 59}, {47, 99, 58, 112}}},
    {{240, "8", "C", SteamiAt::NW, "TEMPERATURE", 2},
     {4, 1, 11, {41, 67, 64, 94}, {28, 46, 157, 59}, {47, 99, 58, 112}}},
    {{240, "8", "C", SteamiAt::NW, "TEMPERATURES12", 2},
     {4, 1, 14, {41, 67, 64, 94}, {28, 46, 193, 59}, {47, 99, 58, 112}}},
    {{240, "-12345.7", kDegC, SteamiAt::NW, "TEMPERATURES12", 2},
     {3, 8, 13, {33, 52, 206, 72}, {42, 31, 193, 44}, {106, 76, 133, 89}}},
    {{240, "Uncomfortable", kDegC, SteamiAt::NW, "TEMPERATURES12", 2},
     {1, 13, 2, {54, 22, 155, 28}, {107, 1, 128, 14}, {91, 30, 118, 43}}},
    {{240, "ABCDEFGHIJKLMNO", kDegC, SteamiAt::NW, "TEMPERATURES12", 2},
     {1, 15, 2, {54, 22, 171, 28}, {107, 1, 128, 14}, {99, 30, 126, 43}}},
    {{240, "8", "C", SteamiAt::SE, "TEMP", 1},
     {1, 1, 4, {182, 210, 187, 216}, {158, 189, 203, 202}, {150, 218, 161, 231}}},
    {{240, "8", "C", SteamiAt::SE, "TEMPERATURE", 1},
     {1, 1, 11, {182, 210, 187, 216}, {74, 189, 203, 202}, {150, 218, 161, 231}}},
    {{240, "8", "C", SteamiAt::SE, "TEMPERATURES12", 1},
     {1, 1, 14, {182, 210, 187, 216}, {38, 189, 203, 202}, {150, 218, 161, 231}}},
    {{240, "8", "C", SteamiAt::SE, "TEMP", 2},
     {4, 1, 4, {175, 141, 198, 168}, {163, 120, 208, 133}, {181, 173, 192, 186}}},
    {{240, "8", "C", SteamiAt::SE, "TEMPERATURE", 2},
     {4, 1, 11, {175, 141, 198, 168}, {106, 120, 235, 133}, {181, 173, 192, 186}}},
    {{240, "8", "C", SteamiAt::SE, "TEMPERATURES12", 2},
     {4, 1, 14, {175, 141, 198, 168}, {70, 120, 235, 133}, {181, 173, 192, 186}}},
    {{240, "-12345.7", kDegC, SteamiAt::SE, "TEMPERATURES12", 2},
     {3, 8, 14, {33, 164, 206, 184}, {36, 143, 201, 156}, {106, 188, 133, 201}}},
    {{240, "Uncomfortable", kDegC, SteamiAt::SE, "TEMPERATURES12", 2},
     {1, 13, 14, {86, 210, 187, 216}, {38, 189, 203, 202}, {123, 218, 150, 231}}},
    {{240, "ABCDEFGHIJKLMNO", kDegC, SteamiAt::SE, "TEMPERATURES12", 2},
     {1, 15, 14, {70, 210, 187, 216}, {38, 189, 203, 202}, {115, 218, 142, 231}}},
    {{240, "8", "C", SteamiAt::SW, "TEMP", 1},
     {1, 1, 4, {53, 210, 58, 216}, {35, 189, 80, 202}, {79, 218, 90, 231}}},
    {{240, "8", "C", SteamiAt::SW, "TEMPERATURE", 1},
     {1, 1, 11, {53, 210, 58, 216}, {35, 189, 164, 202}, {79, 218, 90, 231}}},
    {{240, "8", "C", SteamiAt::SW, "TEMPERATURES12", 1},
     {1, 1, 14, {53, 210, 58, 216}, {35, 189, 200, 202}, {79, 218, 90, 231}}},
    {{240, "8", "C", SteamiAt::SW, "TEMP", 2},
     {4, 1, 4, {41, 141, 64, 168}, {29, 120, 74, 133}, {47, 173, 58, 186}}},
    {{240, "8", "C", SteamiAt::SW, "TEMPERATURE", 2},
     {4, 1, 11, {41, 141, 64, 168}, {3, 120, 132, 133}, {47, 173, 58, 186}}},
    {{240, "8", "C", SteamiAt::SW, "TEMPERATURES12", 2},
     {4, 1, 14, {41, 141, 64, 168}, {3, 120, 168, 133}, {47, 173, 58, 186}}},
    {{240, "-12345.7", kDegC, SteamiAt::SW, "TEMPERATURES12", 2},
     {3, 8, 14, {33, 164, 206, 184}, {36, 143, 201, 156}, {106, 188, 133, 201}}},
    {{240, "Uncomfortable", kDegC, SteamiAt::SW, "TEMPERATURES12", 2},
     {1, 13, 14, {53, 210, 154, 216}, {35, 189, 200, 202}, {90, 218, 117, 231}}},
    {{240, "ABCDEFGHIJKLMNO", kDegC, SteamiAt::SW, "TEMPERATURES12", 2},
     {1, 15, 14, {53, 210, 170, 216}, {35, 189, 200, 202}, {98, 218, 125, 231}}},
};

#undef NO_BOX

static void test_value_corner_golden(void) {
    forEachPanel([](SteamiMemoryBackend& mem, TestScreen& s) {
        const uint8_t k = s.k();
        for (const CornerCase& v : kCorners) {
            const CornerCall& a = v.call;
            const CornerGolden& g = v.golden;
            if (a.size != s.width()) {
                continue;
            }
            s.clear();
            s.value(a.text, a.unit, a.at, a.label, STEAMI_WHITE, a.scale);
            expectPart(mem, STEAMI_WHITE, g.value);
            expectPart(mem, STEAMI_GRAY, g.label);
            expectPart(mem, STEAMI_LIGHT, g.unit);
            const size_t value = runInk(a.text, g.kept, Ink::VALUE, g.f);
            const size_t label = a.label ? runInk(a.label, g.labelKept, Ink::LABEL, k) : 0;
            const size_t unit = a.unit ? runInk(a.unit, 99, Ink::TEXT, k) : 0;
            TEST_ASSERT_EQUAL_size_t_MESSAGE(value, inkBox(mem, STEAMI_WHITE).count, a.text);
            TEST_ASSERT_EQUAL_size_t_MESSAGE(label, inkBox(mem, STEAMI_GRAY).count, a.label);
            TEST_ASSERT_EQUAL_size_t(value + label + unit, inkBoxAll(mem).count);
        }
    });
    // A corner value takes resolve()'s row: yOffset does not move it.
    for (int16_t size : {128, 240}) {
        for (SteamiAt at : {SteamiAt::NE, SteamiAt::NW, SteamiAt::SE, SteamiAt::SW}) {
            TEST_ASSERT_TRUE(rendered(size, size == 240, [at](TestScreen& s) {
                                 s.value(23.5f, kDegC, at, "TEMP", STEAMI_WHITE, 2, 0);
                             }) == rendered(size, size == 240, [at](TestScreen& s) {
                                 s.value(23.5f, kDegC, at, "TEMP", STEAMI_WHITE, 2, 40);
                             }));
        }
    }
}

static void test_value_corners_stay_inside(void) {
    TEST_ASSERT_EQUAL_STRING("-12345.7", fmt(-12345.678f).c_str());
    // onPanel checks outsideCircle() == 0 and outOfBounds() == 0 after every panel.
    forEachPanel([](SteamiMemoryBackend& mem, TestScreen& s) {
        const uint8_t k = s.k();
        for (const CornerCase& v : kCorners) {
            s.clear();
            const CornerCall& a = v.call;
            s.value(a.text, a.unit, a.at, a.label, STEAMI_WHITE, a.scale);
            TEST_ASSERT_TRUE(inkBox(mem, STEAMI_WHITE).count > 0);
        }
        for (SteamiAt at : {SteamiAt::NE, SteamiAt::NW, SteamiAt::SE, SteamiAt::SW}) {
            for (uint8_t scale : {0, 1, 2, 3, 255}) {
                for (int text = 0; text < 2; text++) {
                    s.clear();
                    if (text == 0) {
                        s.value(-12345.678f, kDegC, at, "TEMPERATURES12", STEAMI_WHITE, scale);
                    } else {
                        s.value("Uncomfortable", kDegC, at, "TEMPERATURES12", STEAMI_WHITE, scale);
                    }
                    // The value lies inside the chord at its inked rows, shrunk by k.
                    const InkBox got = inkBox(mem, STEAMI_WHITE);
                    TEST_ASSERT_TRUE(got.count > 0);
                    const Region c = s.chord(got.y0, got.y1);
                    TEST_ASSERT_TRUE(got.x0 >= c.left + k && got.x1 <= c.right - k);
                }
            }
        }
    });
}

// ---- bar(), contrast, messages and two screens on one backend ----

/// The lit pixels of row y.
static size_t litOnRow(const SteamiMemoryBackend& mem, int16_t y) {
    return steami_screen_check::boxOf(mem, y, y, [](uint16_t p) { return p != 0; }).count;
}

// Values printed by golden_layout.py: bar(72, 100, -12, STEAMI_GREEN) on 128 grey and 240 colour.
// The fill is drawn over the track, so GRID_DARK keeps only the track's unfilled part.
static void test_bar_golden(void) {
    onPanel(128, false, [](SteamiMemoryBackend& mem, TestScreen& s) {
        s.bar(72, 100, -12, STEAMI_GREEN);
        expectBox(inkBox(mem, STEAMI_GRID_DARK), 83, 72, 107, 79);
        expectBox(inkBox(mem, STEAMI_GREEN), 20, 72, 82, 79);
        TEST_ASSERT_EQUAL_size_t(25 * 8, inkBox(mem, STEAMI_GRID_DARK).count);
        TEST_ASSERT_EQUAL_size_t(63 * 8, inkBox(mem, STEAMI_GREEN).count);
        TEST_ASSERT_EQUAL_size_t(88 * 8, inkBoxAll(mem).count);
        s.clear();
        s.bar(72, 100, 0, STEAMI_GREEN);
        expectBox(inkBoxAll(mem), 20, 84, 107, 91);
    });
    onPanel(240, true, [](SteamiMemoryBackend& mem, TestScreen& s) {
        s.bar(72, 100, -12, STEAMI_GREEN);
        expectBox(inkBox(mem, STEAMI_GRID_DARK), 156, 135, 202, 149);
        expectBox(inkBox(mem, STEAMI_GREEN), 38, 135, 155, 149);
        TEST_ASSERT_EQUAL_size_t(47 * 15, inkBox(mem, STEAMI_GRID_DARK).count);
        TEST_ASSERT_EQUAL_size_t(118 * 15, inkBox(mem, STEAMI_GREEN).count);
        TEST_ASSERT_EQUAL_size_t(165 * 15, inkBoxAll(mem).count);
    });
}

// Spec 7: "a BLUE bar fill at least 7" on the grey panel; the track stays level 3.
static void test_bar_contrast(void) {
    onPanel(128, false, [](SteamiMemoryBackend& mem, TestScreen& s) {
        s.bar(50, 100, 0, STEAMI_BLUE);
        expectBox(inkBoxAll(mem), 20, 84, 107, 91);
        size_t fill = 0;
        for (int16_t y = 84; y <= 91; y++) {
            for (int16_t x = 20; x <= 107; x++) {
                const uint8_t level = SteamiRgb::fromRgb565(mem.at(x, y)).gray4();
                if (x <= 63) {  // 88 * 50 / 100 = 44 columns of fill
                    TEST_ASSERT_TRUE(level >= 7);
                    fill++;
                } else {
                    TEST_ASSERT_EQUAL_UINT8(3, level);
                }
            }
        }
        TEST_ASSERT_EQUAL_size_t(44 * 8, fill);
    });
    onPanel(240, true, [](SteamiMemoryBackend& mem, TestScreen& s) {
        s.bar(50, 100, 0, STEAMI_BLUE);
        expectBox(inkBox(mem, STEAMI_BLUE), 38, 157, 119, 171);
        TEST_ASSERT_EQUAL_size_t(82 * 15, inkBox(mem, STEAMI_BLUE).count);
        expectBox(inkBox(mem, STEAMI_GRID_DARK), 120, 157, 202, 171);
    });
}

// Review Focus 5: an empty or a full bar, never a crash.
static void test_bar_extremes(void) {
    for (int16_t size : {128, 240}) {
        const bool color = size == 240;
        const std::vector<uint16_t> track = rendered(size, color, [](TestScreen& s) { s.bar(0); });
        const std::vector<uint16_t> full = rendered(size, color, [](TestScreen& s) { s.bar(100); });
        TEST_ASSERT_FALSE(track == full);
        TEST_ASSERT_TRUE(track == rendered(size, color, [](TestScreen& s) { s.bar(-5); }));
        TEST_ASSERT_TRUE(track == rendered(size, color, [](TestScreen& s) { s.bar(-INFINITY); }));
        TEST_ASSERT_TRUE(full == rendered(size, color, [](TestScreen& s) { s.bar(150); }));
        TEST_ASSERT_TRUE(full == rendered(size, color, [](TestScreen& s) { s.bar(INFINITY); }));
        TEST_ASSERT_TRUE(track == rendered(size, color, [](TestScreen& s) { s.bar(NAN); }));
        TEST_ASSERT_TRUE(track == rendered(size, color, [](TestScreen& s) { s.bar(50, 0); }));
        TEST_ASSERT_TRUE(track == rendered(size, color, [](TestScreen& s) { s.bar(50, -10); }));
        TEST_ASSERT_TRUE(track == rendered(size, color, [](TestScreen& s) { s.bar(50, NAN); }));
        TEST_ASSERT_TRUE(track ==
                         rendered(size, color, [](TestScreen& s) { s.bar(50, INFINITY); }));
        TEST_ASSERT_TRUE(full ==
                         rendered(size, color, [](TestScreen& s) { s.bar(INFINITY, INFINITY); }));
        // The colour shortcut is bar(v, maxV, 0, c); the default colour is LIGHT.
        const std::vector<uint16_t> green =
            rendered(size, color, [](TestScreen& s) { s.bar(50, 100, STEAMI_GREEN); });
        TEST_ASSERT_TRUE(
            green == rendered(size, color, [](TestScreen& s) { s.bar(50, 100, 0, STEAMI_GREEN); }));
        TEST_ASSERT_TRUE(
            rendered(size, color, [](TestScreen& s) { s.bar(50); }) ==
            rendered(size, color, [](TestScreen& s) { s.bar(50, 100, 0, STEAMI_LIGHT); }));
        TEST_ASSERT_FALSE(green == rendered(size, color, [](TestScreen& s) { s.bar(50); }));
    }
    // The empty bar is the track alone.
    onPanel(128, false, [](SteamiMemoryBackend& mem, TestScreen& s) {
        s.bar(NAN);
        TEST_ASSERT_EQUAL_size_t(88 * 8, inkBox(mem, STEAMI_GRID_DARK).count);
        TEST_ASSERT_EQUAL_size_t(88 * 8, inkBoxAll(mem).count);
    });
    // 88 * 3e38 overflows a float: the ratio is taken first, 88 * 0.882 = 77 columns.
    onPanel(128, true, [](SteamiMemoryBackend& mem, TestScreen& s) {
        s.bar(3e38f, 3.4e38f);
        expectBox(inkBox(mem, STEAMI_LIGHT), 20, 84, 96, 91);
    });
}

// Spec 5.12 rule 7 for any yOffset; at yOffset 0 the track is whole on every row.
static void test_bar_stays_in_the_circle(void) {
    // onPanel checks outsideCircle() == 0 and outOfBounds() == 0 after every panel.
    forEachPanel([](SteamiMemoryBackend& mem, TestScreen& s) {
        for (int16_t y = -60; y <= 60; y += 4) {
            s.bar(50, 100, y, STEAMI_GREEN);
            s.bar(100, 100, y);
        }
        TEST_ASSERT_TRUE(mem.litCount() > 0);
        s.clear();
        s.bar(0);
        const bool small = s.width() == 128;
        const int16_t top = small ? 84 : 157;
        const int16_t rows = small ? 8 : 15;
        for (int16_t y = top; y < top + rows; y++) {
            TEST_ASSERT_EQUAL_size_t(small ? 88 : 165, litOnRow(mem, y));
        }
        TEST_ASSERT_EQUAL_size_t(static_cast<size_t>((small ? 88 : 165) * rows), mem.litCount());
    });
}

// Review Focus 4: no int16_t wrap draws a bar elsewhere.
static void test_bar_offsets_at_int16_limits(void) {
    forEachPanel([](SteamiMemoryBackend& mem, TestScreen& s) {
        for (int16_t y : {static_cast<int16_t>(32767), static_cast<int16_t>(-32767),
                          static_cast<int16_t>(-32768), static_cast<int16_t>(20000),
                          static_cast<int16_t>(-20000)}) {
            s.bar(50, 100, y);
            s.bar(100, 100, y, STEAMI_GREEN);
        }
        TEST_ASSERT_EQUAL_size_t(0, mem.litCount());
        TEST_ASSERT_EQUAL_UINT32(0, mem.fillRectCalls());
        TEST_ASSERT_EQUAL_UINT32(0, mem.pixelCalls());
    });
}

// Review Focus 2: no screen at all (UNKNOWN release, nothing injected).
static void test_widgets_without_a_screen(void) {
    TestScreen a(board), b(board);
    TEST_ASSERT_EQUAL_INT16(0, a.width());
    TEST_ASSERT_EQUAL_UINT8(1, a.k());
    TEST_ASSERT_EQUAL_UINT8(0, a.maxChars());
    // The floor is 0 and the cap clamped to r + 2: no isqrt of a negative number, which UBSan
    // could not see through the uint32_t argument.
    TEST_ASSERT_EQUAL_INT32(2, a.marginNs(8, 1));
    TEST_ASSERT_EQUAL_INT32(2, a.marginNs(160, 1));
    a.title("T");
    a.subtitle("s");
    a.value(23.5f, kDegC, SteamiAt::W, "L");
    b.bar(50);  // b begins through its first widget
    b.text("x", SteamiAt::NE);
    a.show();
    TEST_ASSERT_TRUE(a.status() == SteamiStatus::UNKNOWN_BOARD);
    TEST_ASSERT_TRUE(b.status() == SteamiStatus::UNKNOWN_BOARD);
    TEST_ASSERT_EQUAL_size_t(1, countLogged("(E30)"));  // deduplicated by who and code
    TEST_ASSERT_EQUAL_STRING(
        "[STeaMi] SteamiScreen: cannot tell if this board is a V1a/V1b or a V1c. "
        "Add #define STEAMI_BOARD_V1C (or V1B) before #include <STeaMi.h>. (E30)",
        loggedLine("(E30)").c_str());
    TEST_ASSERT_EQUAL_STRING("unknown board", a.error());
    TEST_ASSERT_EQUAL_STRING("unknown board", b.error());
    // Each round widget begins the display on its own (4b): one fresh screen per widget.
    TestScreen c(board), d(board), e(board), f(board);
    c.gauge(50);
    TEST_ASSERT_TRUE(c.status() == SteamiStatus::UNKNOWN_BOARD);
    d.compass(0);
    TEST_ASSERT_TRUE(d.status() == SteamiStatus::UNKNOWN_BOARD);
    e.watch(1, 2, 3);
    TEST_ASSERT_TRUE(e.status() == SteamiStatus::UNKNOWN_BOARD);
    f.face(SteamiFace::HAPPY);
    TEST_ASSERT_TRUE(f.status() == SteamiStatus::UNKNOWN_BOARD);
    TEST_ASSERT_EQUAL_size_t(1, countLogged("(E30)"));
}

static void test_screen_e31_message(void) {
    SteamiMemoryBackend mem(240, 240, true, false);
    board.setDisplayBackend(mem);
    TestScreen s(board);
    s.title("Comfort");
    s.subtitle("a", "b");
    s.value(23.5f, kDegC, SteamiAt::W, "TEMP");
    s.value(8, "C", SteamiAt::NE, "TEMP");
    s.text("x", SteamiAt::E);
    s.bar(50);
    s.bar(50, 100, STEAMI_GREEN);
    s.gauge(50);
    s.compass(90);
    s.watch(10, 10, 30);
    s.face(SteamiFace::LOVE, true);
    s.face(nullptr);
    TEST_ASSERT_EQUAL_UINT32(0, mem.fillRectCalls());
    TEST_ASSERT_EQUAL_UINT32(0, mem.pixelCalls());
    TEST_ASSERT_EQUAL_size_t(0, mem.litCount());
    TEST_ASSERT_EQUAL_UINT32(1, mem.beginCalls());
    TEST_ASSERT_EQUAL_size_t(1, countLogged("(E31)"));
    TEST_ASSERT_EQUAL_STRING(
        "[STeaMi] SteamiScreen: not enough RAM for the colour screen (115200 bytes in one block). "
        "Call screen.begin() first in setup(). (E31)",
        loggedLine("(E31)").c_str());
    TEST_ASSERT_EQUAL_STRING("no memory", s.error());
}

// Values printed by golden_layout.py: title "Comfort" and value(45.3, "%", E, "HUM") on 128.
static void test_two_screens_share_a_memory_backend(void) {
    SteamiMemoryBackend mem(128, 128, false);
    board.setDisplayBackend(mem);
    SteamiScreen a(board), b(board);
    a.title("Comfort");
    b.value(45.3f, "%", SteamiAt::E, "HUM");
    a.show();
    TEST_ASSERT_EQUAL_UINT32(1, mem.beginCalls());
    TEST_ASSERT_EQUAL_UINT32(1, mem.showCalls());
    TEST_ASSERT_EQUAL_PTR(&mem, a.backend());
    TEST_ASSERT_EQUAL_PTR(&mem, b.backend());
    TEST_ASSERT_TRUE(a.status() == SteamiStatus::OK);
    TEST_ASSERT_TRUE(b.status() == SteamiStatus::OK);
    expectBox(inkBox(mem, STEAMI_GRAY, 20, 27), 37, 20, 90, 26);  // the title, above the label
    expectBox(inkBox(mem, STEAMI_WHITE), 71, 50, 122, 63);
    TEST_ASSERT_EQUAL_UINT32(0, mem.outsideCircle());
}

// ---- face() (4b) ----

// The FACES of micropython-steami-lib 927e96b const.py, rows top to bottom, MSB = left column.
static const uint8_t kConstPyFaces[6][8] = {
    {0x00, 0x24, 0x24, 0x00, 0x00, 0x42, 0x3C, 0x00},   // happy
    {0x00, 0x24, 0x24, 0x00, 0x00, 0x3C, 0x42, 0x00},   // sad
    {0x00, 0x24, 0x24, 0x00, 0x18, 0x24, 0x24, 0x18},   // surprised
    {0x00, 0x00, 0x66, 0x00, 0x00, 0x18, 0x18, 0x00},   // sleeping
    {0x00, 0x42, 0x24, 0x24, 0x00, 0x3C, 0x42, 0x00},   // angry
    {0x00, 0x66, 0xFF, 0xFF, 0x7E, 0x3C, 0x18, 0x00}};  // love
static_assert(sizeof(STEAMI_FACES) == 48, "spec 5.12: faces 48 B");

/// The cell and the top-left corner of face()'s 8x8 grid.
struct FaceGrid {
    int32_t cell;
    int32_t ox;
    int32_t oy;
};

// Values printed by golden_layout.py (face_layout).
static FaceGrid faceGrid(int16_t size, bool compact) {
    if (size == 128) {
        return compact ? FaceGrid{8, 32, 28} : FaceGrid{11, 20, 20};
    }
    return compact ? FaceGrid{15, 60, 53} : FaceGrid{20, 40, 40};
}

/// Every pixel of the panel holds c where `bitmap` laid out on `grid` has a set bit, 0 elsewhere.
static void expectFace(const SteamiMemoryBackend& mem, const uint8_t bitmap[8], FaceGrid grid,
                       SteamiRgb c) {
    const uint16_t ink = mem.stored(c);
    size_t lit = 0;
    for (int16_t y = 0; y < mem.height(); y++) {
        for (int16_t x = 0; x < mem.width(); x++) {
            const int32_t col = x - grid.ox;
            const int32_t row = y - grid.oy;
            const bool inGrid = col >= 0 && row >= 0 && col < 8 * grid.cell && row < 8 * grid.cell;
            const bool on = inGrid && (bitmap[row / grid.cell] & (0x80 >> (col / grid.cell))) != 0;
            if (mem.at(x, y) != (on ? ink : 0)) {
                TEST_FAIL_MESSAGE("a pixel differs from the face mask");
            }
            lit += on ? 1 : 0;
        }
    }
    TEST_ASSERT_TRUE_MESSAGE(lit > 0, "empty mask");
}

static void test_faces_equal_const_py(void) {
    TEST_ASSERT_EQUAL_MEMORY(kConstPyFaces, STEAMI_FACES, sizeof kConstPyFaces);
    forEachPanel([](SteamiMemoryBackend& mem, TestScreen& s) {
        for (uint8_t f = 0; f < 6; f++) {
            s.clear();
            s.face(static_cast<SteamiFace>(f));
            expectFace(mem, kConstPyFaces[f], faceGrid(s.width(), false), STEAMI_LIGHT);
        }
    });
}

static void test_face_compact(void) {
    forEachPanel([](SteamiMemoryBackend& mem, TestScreen& s) {
        s.face(SteamiFace::HAPPY, true);
        expectFace(mem, kConstPyFaces[0], faceGrid(s.width(), true), STEAMI_LIGHT);
    });
}

// Review Focus 5: the three overloads agree; a nullptr bitmap or an f past LOVE draws nothing.
static void test_face_overloads_and_bad_input(void) {
    for (int16_t size : {128, 240}) {
        const bool color = size == 240;
        const std::vector<uint16_t> green =
            rendered(size, color, [](TestScreen& s) { s.face(SteamiFace::SAD, STEAMI_GREEN); });
        TEST_ASSERT_TRUE(green == rendered(size, color, [](TestScreen& s) {
                             s.face(SteamiFace::SAD, false, STEAMI_GREEN);
                         }));
        TEST_ASSERT_FALSE(green ==
                          rendered(size, color, [](TestScreen& s) { s.face(SteamiFace::SAD); }));
        TEST_ASSERT_TRUE(rendered(size, color, [](TestScreen& s) { s.face(STEAMI_FACES[4]); }) ==
                         rendered(size, color, [](TestScreen& s) { s.face(SteamiFace::ANGRY); }));
        TEST_ASSERT_TRUE(
            rendered(size, color, [](TestScreen& s) { s.face(STEAMI_FACES[5], true); }) ==
            rendered(size, color, [](TestScreen& s) { s.face(SteamiFace::LOVE, true); }));
    }
    // The six faces are mirror images of themselves: an asymmetric bitmap pins MSB = left.
    static const uint8_t kSteps[8] = {0x80, 0xC0, 0xE0, 0xF0, 0x01, 0x03, 0x07, 0x0F};
    forEachPanel([](SteamiMemoryBackend& mem, TestScreen& s) {
        s.face(kSteps, true, STEAMI_GREEN);
        expectFace(mem, kSteps, faceGrid(s.width(), true), STEAMI_GREEN);
    });
    forEachPanel([](SteamiMemoryBackend& mem, TestScreen& s) {
        s.face(nullptr);
        s.face(nullptr, true, STEAMI_GREEN);
        s.face(static_cast<SteamiFace>(6));
        s.face(static_cast<SteamiFace>(255), true);
        s.face(static_cast<SteamiFace>(6), STEAMI_GREEN);
        TEST_ASSERT_EQUAL_UINT32(0, mem.fillRectCalls());
        TEST_ASSERT_EQUAL_UINT32(0, mem.pixelCalls());
        TEST_ASSERT_EQUAL_size_t(0, mem.litCount());
    });
}

// Review Focus 3: eight 0xFF rows, full and compact, stay inside the circle on every panel
// (onPanel checks outsideCircle() == 0); one filled square per bit.
static void test_faces_stay_inside(void) {
    static const uint8_t kFull[8] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    forEachPanel([](SteamiMemoryBackend& mem, TestScreen& s) {
        for (bool compact : {false, true}) {
            s.clear();
            mem.clearCounters();
            s.face(kFull, compact);
            TEST_ASSERT_EQUAL_UINT32(64, mem.fillRectCalls());
            TEST_ASSERT_EQUAL_UINT32(0, mem.outsideCircle());
            expectFace(mem, kFull, faceGrid(s.width(), compact), STEAMI_LIGHT);
        }
    });
}

// ---- polarX, polarY and watch() (4b) ----

// Values printed by golden_layout.py (polar).
static void test_polar_points(void) {
    onPanel(128, false, [](SteamiMemoryBackend&, TestScreen& s) {
        TEST_ASSERT_EQUAL_INT32(30, s.polarX(43, 233));
        TEST_ASSERT_EQUAL_INT32(90, s.polarY(43, 233));
        TEST_ASSERT_EQUAL_INT32(98, s.polarX(43, 127));
        TEST_ASSERT_EQUAL_INT32(90, s.polarY(43, 127));
        TEST_ASSERT_EQUAL_INT32(64, s.polarX(44, 0));
        TEST_ASSERT_EQUAL_INT32(20, s.polarY(44, 0));
        TEST_ASSERT_EQUAL_INT32(64, s.polarX(0, 77));
        TEST_ASSERT_EQUAL_INT32(64, s.polarY(0, 77));
        TEST_ASSERT_EQUAL_INT32(20, s.polarX(44, -90));
        TEST_ASSERT_EQUAL_INT32(64, s.polarY(44, -90));
        TEST_ASSERT_EQUAL_INT32(20, s.polarX(44, 270));
        TEST_ASSERT_EQUAL_INT32(64, s.polarY(44, 270));
    });
    onPanel(240, true, [](SteamiMemoryBackend&, TestScreen& s) {
        TEST_ASSERT_EQUAL_INT32(55, s.polarX(82, 233));
        TEST_ASSERT_EQUAL_INT32(169, s.polarY(82, 233));
        TEST_ASSERT_EQUAL_INT32(120, s.polarX(83, 0));
        TEST_ASSERT_EQUAL_INT32(37, s.polarY(83, 0));
    });
    // rnd is odd-symmetric: the point at deg + 180 is the mirror image about the centre.
    forEachPanel([](SteamiMemoryBackend&, TestScreen& s) {
        const int32_t cx = s.centerX();
        const int32_t cy = s.centerY();
        for (int32_t d : {3, 5, 44}) {
            for (int32_t deg = 0; deg < 360; deg++) {
                TEST_ASSERT_EQUAL_INT32(-(s.polarX(d, deg) - cx), s.polarX(d, deg + 180) - cx);
                TEST_ASSERT_EQUAL_INT32(-(s.polarY(d, deg) - cy), s.polarY(d, deg + 180) - cy);
            }
        }
    });
}

/// The pixel (x, y) holds c.
static void expectAt(const SteamiMemoryBackend& mem, int16_t x, int16_t y, SteamiRgb c,
                     const char* what) {
    TEST_ASSERT_EQUAL_HEX16_MESSAGE(mem.stored(c), mem.at(x, y), what);
}

/// A hand tip of watch() on the 128 and the 240 panel.
struct WatchTip {
    int16_t x128, y128, x240, y240;
};

static void expectTip(const SteamiMemoryBackend& mem, WatchTip t, SteamiRgb c, const char* what) {
    if (mem.width() == 128) {
        expectAt(mem, t.x128, t.y128, c, what);
    } else {
        expectAt(mem, t.x240, t.y240, c, what);
    }
}

// Spec 7's "watch(3, 0, 0) hand directions" (values printed by golden_layout.py). RED is level 8
// on the grey panel, a grey no other part of the watch uses.
static void test_watch_hands_at_three(void) {
    for (int16_t size : {128, 240}) {
        onPanel(size, size == 240, [](SteamiMemoryBackend& mem, TestScreen& s) {
            s.watch(3, 0, 0, STEAMI_RED);
            expectTip(mem, {92, 64, 172, 120}, STEAMI_RED, "hour tip");
            TEST_ASSERT_EQUAL_INT(s.width() == 128 ? 92 : 172, inkBox(mem, STEAMI_RED).x1);
            // The second hand, at the same 0 degrees and drawn after the minute hand (device.py's
            // order), covers the minute tip.
            expectTip(mem, {64, 22, 120, 42}, STEAMI_GRAY, "minute tip under the second hand");
            expectTip(mem, {64, 17, 120, 31}, STEAMI_GRAY, "second tip");
            expectAt(mem, s.centerX(), s.centerY(), STEAMI_GRAY, "pivot");
        });
        onPanel(size, size == 240, [](SteamiMemoryBackend& mem, TestScreen& s) {
            // The minute hand at 3 degrees, the second hand pointing down: the minute tip is the
            // topmost RED pixel (the hour hand's RED starts at row 61 / 115).
            s.watch(3, 0, 30, STEAMI_RED);
            expectTip(mem, {66, 22, 124, 42}, STEAMI_RED, "minute tip");
            TEST_ASSERT_EQUAL_INT(s.width() == 128 ? 22 : 42, inkBox(mem, STEAMI_RED).y0);
        });
    }
}

// Values printed by golden_layout.py: m / 2 and s / 10 are integer divisions, so 10:11:59 puts the
// hour at 305 degrees and the minute at 71.
static void test_watch_integer_angles(void) {
    forEachPanel([](SteamiMemoryBackend& mem, TestScreen& s) {
        s.watch(10, 10, 30);
        expectTip(mem, {41, 48, 77, 90}, STEAMI_LIGHT, "10:10:30 hour tip");
        expectTip(mem, {101, 45, 189, 85}, STEAMI_LIGHT, "10:10:30 minute tip");
        expectTip(mem, {64, 111, 120, 209}, STEAMI_GRAY, "10:10:30 second tip");
    });
    forEachPanel([](SteamiMemoryBackend& mem, TestScreen& s) {
        s.watch(10, 11, 59);
        expectTip(mem, {41, 48, 77, 90}, STEAMI_LIGHT, "10:11:59 hour tip");
        expectTip(mem, {104, 50, 194, 95}, STEAMI_LIGHT, "10:11:59 minute tip");
        expectTip(mem, {59, 17, 111, 31}, STEAMI_GRAY, "10:11:59 second tip");
    });
}

// Values printed by golden_layout.py: at 1:37:05 no hand crosses a number, so the WHITE box is the
// box of "12", "3", "6" and "9".
static void test_watch_numbers_golden(void) {
    onPanel(128, false, [](SteamiMemoryBackend& mem, TestScreen& s) {
        s.watch(1, 37, 5);
        expectBox(inkBox(mem, STEAMI_WHITE), 20, 19, 107, 107);
    });
    onPanel(240, true, [](SteamiMemoryBackend& mem, TestScreen& s) {
        s.watch(1, 37, 5);
        expectBox(inkBox(mem, STEAMI_WHITE), 39, 37, 199, 200);  // text() in 8x16 (4b-font)
    });
}

// Review Focus 4: a time counted by hand past its range still gives integer angles, inside the
// circle (onPanel checks outsideCircle() == 0).
static void test_watch_out_of_range(void) {
    for (int16_t size : {128, 240}) {
        const bool color = size == 240;
        TEST_ASSERT_TRUE(rendered(size, color, [](TestScreen& s) { s.watch(25, 61, 61); }) ==
                         rendered(size, color, [](TestScreen& s) { s.watch(1, 61, 61); }));
    }
    forEachPanel([](SteamiMemoryBackend& mem, TestScreen& s) {
        s.watch(255, 255, 255);
        TEST_ASSERT_TRUE(mem.litCount() > 0);
        s.clear();
        s.watch(0, 0, 0, STEAMI_BLUE);
        TEST_ASSERT_TRUE(inkBox(mem, STEAMI_BLUE).count > 0);
    });
}

// Review Focus 3: every hour, a spread of minutes and seconds, on the V1c's 2 px strokes too.
static void test_watch_stays_inside(void) {
    forEachPanel([](SteamiMemoryBackend& mem, TestScreen& s) {
        for (uint8_t h = 0; h < 12; h++) {
            for (uint8_t m = 0; m < 60; m += 7) {
                for (uint8_t sec = 0; sec < 60; sec += 13) {
                    s.watch(h, m, sec);
                    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, mem.outsideCircle(), "outside the circle");
                }
            }
        }
    });
}

// ---- compass() (4b) ----

/// compass(heading, STEAMI_RED) on one panel: the needle tip and the box of the N letter, the only
/// WHITE ink (values printed by golden_layout.py).
struct CompassCase {
    int16_t size;
    int16_t heading;
    int16_t tipX, tipY;
    int16_t nX0, nY0, nX1, nY1;
};

static const CompassCase kCompassCases[] = {
    {128, 0, 64, 20, 61, 3, 66, 9},           {128, 90, 20, 64, 4, 60, 9, 66},
    {128, 180, 64, 108, 61, 117, 66, 123},    {128, 270, 108, 64, 118, 60, 123, 66},
    {240, 0, 120, 37, 116, 7, 122, 16},       {240, 90, 37, 120, 9, 114, 15, 123},
    {240, 180, 120, 203, 116, 221, 122, 230}, {240, 270, 203, 120, 223, 114, 229, 123}};

// Spec 7's "compass() at 0, 90, 180, 270: needle tip and N letter positions", on the 128 grey and
// the 240 colour panel. RED is level 8 on the grey panel and WHITE level 15: no other part of the
// compass uses them.
static void test_compass_needle_and_north(void) {
    for (const CompassCase& c : kCompassCases) {
        onPanel(c.size, c.size == 240, [&c](SteamiMemoryBackend& mem, TestScreen& s) {
            s.compass(c.heading, STEAMI_RED);
            const InkBox north = inkBox(mem, STEAMI_WHITE);
            expectBox(north, c.nX0, c.nY0, c.nX1, c.nY1);
            expectAt(mem, c.tipX, c.tipY, STEAMI_RED, "needle tip");
            // The north half ends at the tip: the RED box reaches it on the tip's side.
            const InkBox red = inkBox(mem, STEAMI_RED);
            const int16_t side[] = {red.y0, red.x0, red.y1, red.x1};
            TEST_ASSERT_EQUAL_INT16(c.heading % 180 == 0 ? c.tipY : c.tipX, side[c.heading / 90]);
            if (c.heading == 90) {
                TEST_ASSERT_TRUE_MESSAGE(north.x1 < s.centerX(), "at 90 the N is on the left");
            }
            expectAt(mem, s.centerX(), s.centerY(), STEAMI_GRAY, "pivot");
        });
    }
}

/// The pixels of mem that hold c, in row order.
static std::vector<bool> colourMask(const SteamiMemoryBackend& mem, SteamiRgb c) {
    const uint16_t v = mem.stored(c);
    std::vector<bool> out;
    for (uint16_t p : mem.pixels()) {
        out.push_back(p == v);
    }
    return out;
}

// Design 5.3 (F16): a compass letter is text() at scale 1, its cell centred on the Q14 point by
// textWidth() / 2 and textHeight() / 2, on 128 (8x8) and on 240 (8x16).
static void test_letters_are_text_at_scale_1(void) {
    for (int16_t size : {128, 240}) {
        const bool color = size == 240;
        std::vector<uint16_t> north;
        onPanel(size, color, [&north](SteamiMemoryBackend& mem, TestScreen& s) {
            s.compass(0.0f);
            north = pixelsOf(mem, STEAMI_WHITE);
        });
        TEST_ASSERT_TRUE(
            north == rendered(size, color, [](TestScreen& s) {
                const int32_t l =
                    s.radius() - s.u(STEAMI_COMPASS_INSET) + s.u(STEAMI_COMPASS_LETTERS);
                s.text("N", static_cast<int16_t>(s.polarX(l, 0) - s.textWidth("N") / 2),
                       static_cast<int16_t>(s.polarY(l, 0) - s.textHeight() / 2), STEAMI_WHITE, 1);
            }));
    }
}

// Review Focus 2: a heading that points nowhere (NAN, which heading() returns without a sensor,
// and +-INF) draws the rose at 0 and the pivot, with no needle.
static void test_compass_without_a_heading(void) {
    forEachPanel([](SteamiMemoryBackend& mem, TestScreen& s) {
        s.compass(0.0f);
        const std::vector<bool> north = colourMask(mem, STEAMI_WHITE);
        for (float h : {NAN, INFINITY, -INFINITY}) {
            s.clear();
            s.compass(h, STEAMI_RED);
            TEST_ASSERT_EQUAL_size_t_MESSAGE(0, inkBox(mem, STEAMI_RED).count, "a needle");
            TEST_ASSERT_TRUE_MESSAGE(colourMask(mem, STEAMI_WHITE) == north, "N not at heading 0");
            expectAt(mem, s.centerX(), s.centerY(), STEAMI_GRAY, "pivot");
        }
    });
}

// Review Focus 2: the heading is rounded to a whole degree after fmodf, and wraps.
static void test_compass_heading_rounding(void) {
    for (int16_t size : {128, 240}) {
        const bool color = size == 240;
        const std::vector<uint16_t> north =
            rendered(size, color, [](TestScreen& s) { s.compass(0.0f); });
        TEST_ASSERT_TRUE(rendered(size, color, [](TestScreen& s) { s.compass(359.6f); }) == north);
        TEST_ASSERT_TRUE(rendered(size, color, [](TestScreen& s) { s.compass(0.4f); }) == north);
        TEST_ASSERT_TRUE(rendered(size, color, [](TestScreen& s) { s.compass(-90.4f); }) ==
                         rendered(size, color, [](TestScreen& s) { s.compass(270.0f); }));
        const std::vector<uint16_t> east =
            rendered(size, color, [](TestScreen& s) { s.compass(90.0f); });
        TEST_ASSERT_TRUE(rendered(size, color, [](TestScreen& s) { s.compass(450.0f); }) == east);
        TEST_ASSERT_FALSE(east == north);
    }
    // A finite huge heading still draws a needle: fmodf first, so lroundf never sees 1e30.
    forEachPanel([](SteamiMemoryBackend& mem, TestScreen& s) {
        s.compass(1e30f, STEAMI_RED);
        TEST_ASSERT_TRUE(inkBox(mem, STEAMI_RED).count > 0);
        s.clear();
        s.compass(-1e30f, STEAMI_RED);
        TEST_ASSERT_TRUE(inkBox(mem, STEAMI_RED).count > 0);
    });
}

// Decision 2, a real rose: at heading 45 the whole rose turns by -45. At the tick start R - u(6)
// (values printed by golden_layout.py), the N tick now lies at 315 and the NE diagonal at the top;
// the needle (44 / 83 long) reaches neither point.
static void test_compass_rose_turns(void) {
    for (int16_t size : {128, 240}) {
        onPanel(size, size == 240, [](SteamiMemoryBackend& mem, TestScreen& s) {
            s.compass(45.0f, STEAMI_RED);
            const int32_t r = s.radius() - s.u(STEAMI_COMPASS_INSET);
            const int32_t tick = r - s.u(STEAMI_COMPASS_TICK);
            TEST_ASSERT_TRUE(r * 85 / 100 < tick);
            TEST_ASSERT_EQUAL_INT32(s.width() == 128 ? 31 : 58, s.polarX(tick, 315));
            TEST_ASSERT_EQUAL_INT32(s.width() == 128 ? 18 : 33, s.polarY(tick, 0));
            expectAt(mem, static_cast<int16_t>(s.polarX(tick, 315)),
                     static_cast<int16_t>(s.polarY(tick, 315)), STEAMI_LIGHT, "N tick at 315");
            expectAt(mem, static_cast<int16_t>(s.polarX(tick, 0)),
                     static_cast<int16_t>(s.polarY(tick, 0)), STEAMI_DARK, "NE tick at the top");
        });
    }
}

// Review Focus 3: every whole heading, on the V1c's 2 px strokes too.
static void test_compass_stays_inside(void) {
    forEachPanel([](SteamiMemoryBackend& mem, TestScreen& s) {
        for (int16_t h = 0; h < 360; h++) {
            s.compass(static_cast<float>(h));
            TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, mem.outsideCircle(), "outside the circle");
        }
    });
}

// ---- gauge() (4b) ----

/// On row centerY(), left of the centre, the DARK pixels are exactly the columns from..to.
static void expectRingRow(const SteamiMemoryBackend& mem, TestScreen& s, int16_t from, int16_t to) {
    const uint16_t dark = mem.stored(STEAMI_DARK);
    for (int16_t x = 0; x < s.centerX(); x++) {
        const bool ring = x >= from && x <= to;
        TEST_ASSERT_TRUE_MESSAGE(ring == (mem.at(x, s.centerY()) == dark), "ring on row centerY()");
    }
}

static size_t countTrue(const std::vector<bool>& mask) {
    size_t n = 0;
    for (bool b : mask) {
        n += b ? 1 : 0;
    }
    return n;
}

// Spec 5.8 table (values printed by golden_layout.py): outer radius 60 and width 7 on 128, 113 and
// 13 on 240, so the ring is 54..60 / 101..113 from the centre; the gap from 135 to 225 leaves the
// bottom of the ring empty.
static void test_gauge_ring(void) {
    forEachPanel([](SteamiMemoryBackend& mem, TestScreen& s) {
        s.gauge(0);
        const bool small = s.width() == 128;
        expectRingRow(mem, s, small ? 4 : 7, small ? 10 : 19);
        const int32_t outer = s.radius() - s.u(STEAMI_GAUGE_INSET);
        TEST_ASSERT_EQUAL_INT32(small ? 60 : 113, outer);
        TEST_ASSERT_FALSE_MESSAGE(
            mem.at(s.centerX(), static_cast<int16_t>(s.centerY() + outer - 2)) ==
                mem.stored(STEAMI_DARK),
            "the bottom gap");
    });
}

// Spec 7: gauge(50, 0, 100) lights half of the track within 2 %, and track and fill have the same
// thickness: the LIGHT mask of gauge(100) is the DARK mask of gauge(0) (the labels are GRAY). The
// counts are printed by golden_layout.py.
static void test_gauge_half_and_equal_thickness(void) {
    forEachPanel([](SteamiMemoryBackend& mem, TestScreen& s) {
        const bool small = s.width() == 128;
        s.gauge(0);
        const std::vector<bool> track = colourMask(mem, STEAMI_DARK);
        s.clear();
        s.gauge(100);
        const std::vector<bool> full = colourMask(mem, STEAMI_LIGHT);
        TEST_ASSERT_TRUE_MESSAGE(full == track, "fill and track differ");
        s.clear();
        s.gauge(50, 0, 100);
        const size_t half = inkBox(mem, STEAMI_LIGHT).count;
        const size_t whole = countTrue(full);
        TEST_ASSERT_EQUAL_size_t(small ? 948 : 3278, half);
        TEST_ASSERT_EQUAL_size_t(small ? 1889 : 6543, whole);
        TEST_ASSERT_TRUE(half * 100 >= whole * 48 && half * 100 <= whole * 52);
    });
}

/// gauge(v, minV, maxV) on one panel: the box and count of its GRAY min and max labels (values
/// printed by golden_layout.py).
struct GaugeLabelCase {
    int16_t size;
    float v, minV, maxV;
    int16_t x0, y0, x1, y1;
    size_t count;
};

static void expectGaugeLabels(const GaugeLabelCase& c) {
    onPanel(c.size, c.size == 240, [&c](SteamiMemoryBackend& mem, TestScreen& s) {
        s.gauge(c.v, c.minV, c.maxV);
        const InkBox gray = inkBox(mem, STEAMI_GRAY);
        expectBox(gray, c.x0, c.y0, c.x1, c.y1);
        TEST_ASSERT_EQUAL_size_t(c.count, gray.count);
        // Decision 3: no value, no unit.
        TEST_ASSERT_EQUAL_size_t(0, inkBox(mem, STEAMI_WHITE).count);
    });
}

static const GaugeLabelCase kGaugeLabels[] = {
    {128, 0, 0, 100, 27, 90, 105, 96, 67},         {128, 342, 0, 500, 27, 90, 105, 96, 74},
    {128, 0, -0.5f, 99.9f, 27, 90, 102, 96, 49},  // "0" and "99": truncated toward zero
    {240, 0, 0, 100, 49, 169, 200, 182, 268},      {240, 342, 0, 500, 49, 169, 200, 182, 296},
    {240, 0, -0.5f, 99.9f, 49, 169, 194, 182, 196}};

// Spec 5.9: the labels are the integer part of minV and maxV in 5x7 x k GRAY, their top row on the
// point at 233 / 127 degrees and distance outer - width - u(10), centred on it.
static void test_gauge_labels_golden(void) {
    for (const GaugeLabelCase& c : kGaugeLabels) {
        expectGaugeLabels(c);
    }
}

// Two "--" labels (|v| >= 1e9), and two long labels slid inward, whole, inside the circle (on 128
// and 240 the two overlap, a known difference of the PR).
static const GaugeLabelCase kGaugeExtremeLabels[] = {
    {128, 0, 1e9f, -1e9f, 24, 93, 102, 93, 20},
    {128, 0, -100000000.0f, 100000000.0f, 10, 90, 117, 96, 325},
    {240, 0, 1e9f, -1e9f, 43, 175, 194, 176, 80},
    {240, 0, -100000000.0f, 100000000.0f, 20, 169, 218, 182, 1254}};

// Review Focus 2: an empty or a full fill for non-finite, equal or reversed bounds, never a crash.
static void test_gauge_extremes(void) {
    forEachPanel([](SteamiMemoryBackend& mem, TestScreen& s) {
        s.gauge(100);
        const std::vector<bool> full = colourMask(mem, STEAMI_LIGHT);
        const struct {
            float v, minV, maxV;
            const char* what;
        } empty[] = {{NAN, 0, 500, "v NAN"},   {-INFINITY, 0, 500, "v -INF"},
                     {-1, 0, 500, "v -1"},     {5, 7, 7, "minV == maxV"},
                     {5, NAN, 10, "minV NAN"}, {INFINITY, 0, INFINITY, "INF / INF"}};
        for (const auto& e : empty) {
            s.clear();
            s.gauge(e.v, e.minV, e.maxV);
            TEST_ASSERT_EQUAL_size_t_MESSAGE(0, inkBox(mem, STEAMI_LIGHT).count, e.what);
        }
        for (float v : {INFINITY, 600.0f}) {
            s.clear();
            s.gauge(v, 0, 500);
            TEST_ASSERT_TRUE_MESSAGE(colourMask(mem, STEAMI_LIGHT) == full, "not full");
        }
        // Ratio 0.75 either way round: 202.5 degrees round to 203.
        s.clear();
        s.gauge(2.5f, 10, 0);
        const std::vector<bool> reversed = colourMask(mem, STEAMI_LIGHT);
        s.clear();
        s.gauge(7.5f, 0, 10);
        TEST_ASSERT_TRUE(reversed == colourMask(mem, STEAMI_LIGHT));
        TEST_ASSERT_TRUE(countTrue(reversed) > 0 && reversed != full);
    });
    for (const GaugeLabelCase& c : kGaugeExtremeLabels) {
        expectGaugeLabels(c);
    }
}

// The ring width in physical pixels: 3 gives x 4..6 / 7..9; 64 and 255 fill the disc towards the
// centre without a crash and stay inside the circle (onPanel).
static void test_gauge_arc_width(void) {
    forEachPanel([](SteamiMemoryBackend& mem, TestScreen& s) {
        const bool small = s.width() == 128;
        s.gauge(0, 0, 100, STEAMI_LIGHT, 3);
        expectRingRow(mem, s, small ? 4 : 7, small ? 6 : 9);
        for (uint8_t w : {static_cast<uint8_t>(64), static_cast<uint8_t>(255)}) {
            s.clear();
            s.gauge(50, 0, 100, STEAMI_LIGHT, w);
            TEST_ASSERT_TRUE(inkBox(mem, STEAMI_LIGHT).count > 0);
            TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, mem.outsideCircle(), "outside the circle");
        }
    });
}

// Spec 5.10 and 7: on the grey panel a BLUE (6) or RED (8) fill over the DARK (6) track is lifted
// to level 10; on the TFT the fill keeps its colour.
static void test_gauge_contrast(void) {
    onPanel(128, false, [](SteamiMemoryBackend& mem, TestScreen& s) {
        s.gauge(50, 0, 100);
        const std::vector<bool> fill = colourMask(mem, STEAMI_LIGHT);
        for (SteamiRgb c : {STEAMI_BLUE, STEAMI_RED}) {
            s.clear();
            s.gauge(50, 0, 100, c);
            size_t lit = 0;
            for (int16_t y = 0; y < s.height(); y++) {
                for (int16_t x = 0; x < s.width(); x++) {
                    const uint8_t level = SteamiRgb::fromRgb565(mem.at(x, y)).gray4();
                    if (fill[static_cast<size_t>(y) * s.width() + x]) {
                        TEST_ASSERT_EQUAL_UINT8(10, level);
                        lit++;
                    } else {
                        TEST_ASSERT_TRUE(level < 10);  // track 6, labels 9, background 0
                    }
                }
            }
            TEST_ASSERT_EQUAL_size_t(948, lit);
        }
    });
    onPanel(240, true, [](SteamiMemoryBackend& mem, TestScreen& s) {
        s.gauge(50, 0, 100);
        const std::vector<bool> fill = colourMask(mem, STEAMI_LIGHT);
        s.clear();
        s.gauge(50, 0, 100, STEAMI_BLUE);
        TEST_ASSERT_TRUE(colourMask(mem, STEAMI_BLUE) == fill);
        TEST_ASSERT_EQUAL_size_t(3278, inkBox(mem, STEAMI_BLUE).count);
    });
}

// Review Focus 3: every fill from -10 to 110 with short, long and huge labels, on the V1c's even
// strokes too.
static void test_gauge_stays_inside(void) {
    forEachPanel([](SteamiMemoryBackend& mem, TestScreen& s) {
        const float bounds[][2] = {{0, 100}, {-999, 99999}, {-100000000.0f, 1e8f}};
        for (const auto& b : bounds) {
            for (int16_t v = -10; v <= 110; v += 5) {
                s.gauge(v, b[0], b[1]);
                TEST_ASSERT_EQUAL_UINT32_MESSAGE(0, mem.outsideCircle(), "outside the circle");
            }
        }
        TEST_ASSERT_TRUE(mem.litCount() > 0);
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
    RUN_TEST(test_fit_glyphs_room_and_run);
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
    RUN_TEST(test_value_center_golden);
    RUN_TEST(test_value_glyph_scale_side_by_side);
    RUN_TEST(test_value_truncation);
    RUN_TEST(test_value_labels_and_units);
    RUN_TEST(test_value_nan_and_decimals);
    RUN_TEST(test_value_edge_cases);
    RUN_TEST(test_value_offsets_at_int16_limits);
    RUN_TEST(test_value_colour_shortcut);
    RUN_TEST(test_value_corner_worked_examples);
    RUN_TEST(test_value_corner_golden);
    RUN_TEST(test_value_corners_stay_inside);
    RUN_TEST(test_bar_golden);
    RUN_TEST(test_bar_contrast);
    RUN_TEST(test_bar_extremes);
    RUN_TEST(test_bar_stays_in_the_circle);
    RUN_TEST(test_bar_offsets_at_int16_limits);
    RUN_TEST(test_widgets_without_a_screen);
    RUN_TEST(test_screen_e31_message);
    RUN_TEST(test_two_screens_share_a_memory_backend);
    RUN_TEST(test_faces_equal_const_py);
    RUN_TEST(test_face_compact);
    RUN_TEST(test_face_overloads_and_bad_input);
    RUN_TEST(test_faces_stay_inside);
    RUN_TEST(test_polar_points);
    RUN_TEST(test_watch_hands_at_three);
    RUN_TEST(test_watch_integer_angles);
    RUN_TEST(test_watch_numbers_golden);
    RUN_TEST(test_watch_out_of_range);
    RUN_TEST(test_watch_stays_inside);
    RUN_TEST(test_compass_needle_and_north);
    RUN_TEST(test_letters_are_text_at_scale_1);
    RUN_TEST(test_compass_without_a_heading);
    RUN_TEST(test_compass_heading_rounding);
    RUN_TEST(test_compass_rose_turns);
    RUN_TEST(test_compass_stays_inside);
    RUN_TEST(test_gauge_ring);
    RUN_TEST(test_gauge_half_and_equal_thickness);
    RUN_TEST(test_gauge_labels_golden);
    RUN_TEST(test_gauge_extremes);
    RUN_TEST(test_gauge_arc_width);
    RUN_TEST(test_gauge_contrast);
    RUN_TEST(test_gauge_stays_inside);
    return UNITY_END();
}
