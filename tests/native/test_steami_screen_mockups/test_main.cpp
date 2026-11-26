// SPDX-License-Identifier: GPL-3.0-or-later

#include <Arduino.h>
#include <Wire.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <unity.h>

#include <string>
#include <vector>

#include "SteamiBoard.h"
#include "SteamiMemoryBackend.h"
#include "SteamiMockups.h"
#include "SteamiRgb.h"
#include "SteamiScreen.h"
#include "SteamiScreenCheck.h"

// Suite 4 of the spec's section 7: mockups 01 to 03 drawn with the examples' calls, on the 128 px
// grey panel of V1a/V1b and the 240 px colour panel of V1c. The boxes are those that
// golden_layout.py's title_row/text_box, value_layout, bar_boxes and subtitle_rows give for the
// same calls.

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

/// Runs `check` on a fresh board state with a size x size memory panel injected and cleared, then
/// asserts that nothing reached the backend off the panel or outside the circle.
template <typename Check>
static void onPanel(int16_t size, bool color, Check check) {
    board.end();
    board.setLogger(&capture);
    SteamiMemoryBackend mem(size, size, color);
    board.setDisplayBackend(mem);
    SteamiScreen s(board);
    s.clear();
    mem.clearCounters();
    check(mem, s);
    TEST_ASSERT_EQUAL_UINT32(0, mem.outOfBounds());
    TEST_ASSERT_EQUAL_UINT32(0, mem.outsideCircle());
    board.end();
}

/// The checks every mockup passes besides onPanel's: one show(), something lit.
static void expectShownOnce(const SteamiMemoryBackend& mem) {
    TEST_ASSERT_EQUAL_UINT32(1, mem.showCalls());
    TEST_ASSERT_TRUE(mem.litCount() > 0);
}

/// Draws a mockup with onPanel, checks it, then writes steami-mockup-<name>-<size>-<mode>.ppm to
/// $STEAMI_PPM_DIR when that is set and not empty.
template <typename Draw>
static void render(const char* name, int16_t size, bool color, Draw draw) {
    onPanel(size, color, [&](SteamiMemoryBackend& mem, SteamiScreen& s) {
        draw(mem, s);
        expectShownOnce(mem);
        const char* dir = getenv("STEAMI_PPM_DIR");
        if (dir != nullptr && dir[0] != '\0') {
            const std::string path = std::string(dir) + "/steami-mockup-" + name + "-" +
                                     std::to_string(size) + (color ? "-color" : "-gray") + ".ppm";
            TEST_ASSERT_TRUE(mem.writePpm(path.c_str()));
        }
    });
}

/// Which pixels of the panel hold colour c.
static std::vector<bool> maskOf(const SteamiMemoryBackend& mem, SteamiRgb c) {
    const uint16_t v = mem.stored(c);
    std::vector<bool> mask;
    mask.reserve(mem.pixels().size());
    for (uint16_t p : mem.pixels()) {
        mask.push_back(p == v);
    }
    return mask;
}

/// The pixels a drawing alone leaves in colour c on a fresh, cleared panel of that size and mode.
template <typename Draw>
static std::vector<bool> maskAlone(int16_t size, bool color, SteamiRgb c, Draw draw) {
    std::vector<bool> mask;
    onPanel(size, color, [&](SteamiMemoryBackend& mem, SteamiScreen& s) {
        draw(s);
        mask = maskOf(mem, c);
    });
    return mask;
}

static bool any(const std::vector<bool>& mask) {
    for (bool b : mask) {
        if (b) {
            return true;
        }
    }
    return false;
}

static void test_mockup_01_temperature(void) {
    render("01_temperature", 128, false, [](SteamiMemoryBackend& mem, SteamiScreen& s) {
        mockupTemperature(s, 23.5f, "WSEN-HIDS");
        expectBox(inkBox(mem, STEAMI_GRAY), 21, 20, 106, 27);   // title row 20
        expectBox(inkBox(mem, STEAMI_WHITE), 38, 50, 89, 63);   // "23.5" at f 2, rows 50..63
        expectBox(inkBox(mem, STEAMI_LIGHT), 57, 66, 70, 72);   // "°C" rows 66..72
        expectBox(inkBox(mem, STEAMI_DARK), 29, 108, 98, 114);  // "WSEN-HIDS"
    });
    render("01_temperature", 240, true, [](SteamiMemoryBackend& mem, SteamiScreen& s) {
        mockupTemperature(s, 23.5f, "BME280");
        expectBox(inkBox(mem, STEAMI_GRAY), 34, 41, 205, 56);  // title row 41
        expectBox(inkBox(mem, STEAMI_WHITE), 68, 91, 171, 118);
        expectBox(inkBox(mem, STEAMI_LIGHT), 106, 123, 133, 136);
        expectBox(inkBox(mem, STEAMI_DARK), 74, 202, 165, 215);  // "BME280"
    });
}

static void test_mockup_02_battery(void) {
    render("02_battery", 128, false, [](SteamiMemoryBackend& mem, SteamiScreen& s) {
        mockupBattery(s, 72, 3.842f);
        expectBox(inkBox(mem, STEAMI_WHITE), 50, 38, 77, 51);  // "72", yOffset -12
        expectBox(inkBox(mem, STEAMI_LIGHT), 61, 54, 66, 60);  // "%"
        // Spec: "rows 72..79 on 128, as mockup 02": the unfilled track, then the 63 px GREEN fill.
        expectBox(inkBox(mem, STEAMI_GRID_DARK), 83, 72, 107, 79);
        expectBox(inkBox(mem, STEAMI_GREEN), 20, 72, 82, 79);
        expectBox(inkBox(mem, STEAMI_DARK, 95, 105), 37, 95, 90, 101);    // "3842 mV"
        expectBox(inkBox(mem, STEAMI_DARK, 106, 127), 37, 106, 90, 112);  // "BQ27441"
    });
    render("02_battery", 240, true, [](SteamiMemoryBackend& mem, SteamiScreen& s) {
        mockupBattery(s, 72, 3.842f);
        expectBox(inkBox(mem, STEAMI_WHITE), 92, 69, 147, 96);
        expectBox(inkBox(mem, STEAMI_LIGHT), 114, 101, 125, 114);
        expectBox(inkBox(mem, STEAMI_GRID_DARK), 156, 135, 202, 149);
        expectBox(inkBox(mem, STEAMI_GREEN), 38, 135, 155, 149);           // 118 px of 165
        expectBox(inkBox(mem, STEAMI_DARK, 177, 196), 66, 177, 173, 190);  // "3842 mV"
        expectBox(inkBox(mem, STEAMI_DARK, 197, 239), 66, 197, 173, 210);  // "BQ27441"
    });
}

static void test_mockup_03_comfort(void) {
    render("03_comfort_dual", 128, false, [](SteamiMemoryBackend& mem, SteamiScreen& s) {
        mockupComfort(s, 23.5f, 45.3f, "WSEN-HIDS");
        expectBox(inkBox(mem, STEAMI_DARK), 64, 32, 64, 95);  // the vline at centerX()
        // "23.5" at W and "45.3" at E, both at f 2: 7 inked rows x 2.
        expectBox(inkBox(mem, STEAMI_WHITE), 6, 50, 122, 63);
        expectBox(inkBox(mem, STEAMI_LIGHT), 24, 66, 99, 72);              // "°C" and "%"
        expectBox(inkBox(mem, STEAMI_GRAY, 0, 30), 37, 20, 90, 26);        // title
        expectBox(inkBox(mem, STEAMI_GRAY, 31, 127), 22, 39, 102, 45);     // "TEMP" and "HUM"
        expectBox(inkBox(mem, STEAMI_GREEN, 95, 105), 21, 95, 106, 101);   // "Comfortable"
        expectBox(inkBox(mem, STEAMI_GREEN, 106, 127), 29, 106, 98, 112);  // "WSEN-HIDS"
    });
    render("03_comfort_dual", 240, true, [](SteamiMemoryBackend& mem, SteamiScreen& s) {
        mockupComfort(s, 23.5f, 45.3f, "BME280");
        expectBox(inkBox(mem, STEAMI_DARK), 120, 60, 120, 179);
        // Both values at f 3 (W and E share the size): 7 inked rows x 3.
        expectBox(inkBox(mem, STEAMI_WHITE), 21, 96, 219, 116);
        expectBox(inkBox(mem, STEAMI_LIGHT), 45, 120, 187, 133);
        expectBox(inkBox(mem, STEAMI_GRAY, 0, 60), 66, 37, 173, 50);        // title
        expectBox(inkBox(mem, STEAMI_GRAY, 61, 239), 39, 75, 193, 88);      // "TEMP" and "HUM"
        expectBox(inkBox(mem, STEAMI_GREEN, 173, 192), 34, 173, 205, 186);  // "Comfortable"
        expectBox(inkBox(mem, STEAMI_GREEN, 193, 239), 74, 193, 165, 206);  // "BME280"
    });
}

// Review Focus 1: a sensor is missing in each example screen.
static void test_mockups_without_sensors(void) {
    struct Panel {
        int16_t size;
        bool color;
    };
    for (const Panel p : {Panel{128, false}, Panel{240, true}}) {
        const bool small = p.size == 128;

        // 01: "--" in place of the value, no subtitle text.
        std::vector<bool> white;
        onPanel(p.size, p.color, [&](SteamiMemoryBackend& mem, SteamiScreen& s) {
            mockupTemperature(s, NAN, "");
            expectShownOnce(mem);
            white = maskOf(mem, STEAMI_WHITE);
            TEST_ASSERT_EQUAL_size_t(0, inkBox(mem, STEAMI_DARK).count);
        });
        const std::vector<bool> dashes01 =
            maskAlone(p.size, p.color, STEAMI_WHITE, [](SteamiScreen& s) { s.value("--", "°C"); });
        TEST_ASSERT_TRUE(any(dashes01));
        TEST_ASSERT_TRUE(white == dashes01);

        // 02: "0", the track with no fill, "--" over "BQ27441".
        onPanel(p.size, p.color, [&](SteamiMemoryBackend& mem, SteamiScreen& s) {
            mockupBattery(s, 0, NAN);
            expectShownOnce(mem);
            white = maskOf(mem, STEAMI_WHITE);
            TEST_ASSERT_EQUAL_size_t(0, inkBox(mem, STEAMI_GREEN).count);
            if (small) {
                expectBox(inkBox(mem, STEAMI_GRID_DARK), 20, 72, 107, 79);
                expectBox(inkBox(mem, STEAMI_DARK, 95, 105), 57, 98, 70, 98);  // "--"
                expectBox(inkBox(mem, STEAMI_DARK, 106, 127), 37, 106, 90, 112);
            } else {
                expectBox(inkBox(mem, STEAMI_GRID_DARK), 38, 135, 202, 149);
                expectBox(inkBox(mem, STEAMI_DARK, 177, 196), 106, 183, 133, 184);  // "--"
                expectBox(inkBox(mem, STEAMI_DARK, 197, 239), 66, 197, 173, 210);
            }
        });
        const std::vector<bool> zero =
            maskAlone(p.size, p.color, STEAMI_WHITE, [](SteamiScreen& s) {
                s.value("0", "%", SteamiAt::CENTER, nullptr, STEAMI_WHITE, 2, -12);
            });
        TEST_ASSERT_TRUE(any(zero));
        TEST_ASSERT_TRUE(white == zero);

        // 03: "--" at W and E; "Uncomfortable" over an empty line, the block keeping two lines.
        onPanel(p.size, p.color, [&](SteamiMemoryBackend& mem, SteamiScreen& s) {
            mockupComfort(s, NAN, NAN, "");
            expectShownOnce(mem);
            white = maskOf(mem, STEAMI_WHITE);
            if (small) {
                expectBox(inkBox(mem, STEAMI_GREEN), 13, 86, 114, 92);  // lines at 86 and 97
            } else {
                expectBox(inkBox(mem, STEAMI_GREEN), 18, 151, 221, 164);  // lines at 151 and 171
            }
        });
        const std::vector<bool> dashes03 =
            maskAlone(p.size, p.color, STEAMI_WHITE, [](SteamiScreen& s) {
                s.value("--", "°C", SteamiAt::W, "TEMP");
                s.value("--", "%", SteamiAt::E, "HUM");
            });
        TEST_ASSERT_TRUE(any(dashes03));
        TEST_ASSERT_TRUE(white == dashes03);
    }
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_mockup_01_temperature);
    RUN_TEST(test_mockup_02_battery);
    RUN_TEST(test_mockup_03_comfort);
    RUN_TEST(test_mockups_without_sensors);
    return UNITY_END();
}
