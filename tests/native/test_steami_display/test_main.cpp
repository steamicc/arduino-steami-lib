// SPDX-License-Identifier: GPL-3.0-or-later

#include <Arduino.h>
#include <Wire.h>
#include <string.h>
#include <unity.h>

#include <string>
#include <vector>

#include "SteamiBoard.h"
#include "SteamiColor.h"
#include "SteamiRgb.h"
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
    return UNITY_END();
}
