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

// Suite 4 of the spec's section 7: mockups 01 to 09 drawn with the examples' calls, on the 128 px
// grey panel of V1a/V1b and the 240 px colour panel of V1c. The boxes and points are those that
// golden_layout.py prints under "Values pinned by test_steami_screen_mockups (4b Task 5)" and
// "(4c Task 5)", composed from its widget goldens for the same calls.

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

/// The pixel (x, y) holds colour c.
static void expectPixel(const SteamiMemoryBackend& mem, int16_t x, int16_t y, SteamiRgb c) {
    TEST_ASSERT_EQUAL_HEX16(mem.stored(c), mem.at(x, y));
}

/// The bounds of the pixels holding colour c on the rows rowFrom..rowTo, from column xFrom on:
/// golden_layout.py's box_from(), the graph labels right of the left axis.
static InkBox inkBoxFrom(const SteamiMemoryBackend& mem, SteamiRgb c, int16_t xFrom,
                         int16_t rowFrom, int16_t rowTo) {
    const uint16_t v = mem.stored(c);
    InkBox b{32767, 32767, -1, -1, 0};
    for (int16_t y = rowFrom; y <= rowTo; y++) {
        for (int16_t x = xFrom; x < mem.width(); x++) {
            if (mem.at(x, y) != v) {
                continue;
            }
            b.x0 = x < b.x0 ? x : b.x0;
            b.y0 = y < b.y0 ? y : b.y0;
            b.x1 = x > b.x1 ? x : b.x1;
            b.y1 = y > b.y1 ? y : b.y1;
            b.count++;
        }
    }
    if (b.count == 0) {
        b.x0 = -1;
        b.y0 = -1;
    }
    return b;
}

/// How many pixels hold colour c at a distance d from the panel's centre with from <= d <= to.
static size_t countAtDistance(const SteamiMemoryBackend& mem, SteamiRgb c, int32_t from,
                              int32_t to) {
    const uint16_t v = mem.stored(c);
    const int32_t cx = mem.width() / 2;
    const int32_t cy = mem.height() / 2;
    size_t n = 0;
    for (int16_t y = 0; y < mem.height(); y++) {
        for (int16_t x = 0; x < mem.width(); x++) {
            const int32_t d2 = (x - cx) * (x - cx) + (y - cy) * (y - cy);
            if (mem.at(x, y) == v && d2 >= from * from && d2 <= to * to) {
                n++;
            }
        }
    }
    return n;
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
        expectBox(inkBox(mem, STEAMI_GRAY), 76, 39, 162, 51);    // title row 37, in 8x16
        expectBox(inkBox(mem, STEAMI_WHITE), 94, 97, 146, 116);  // "23.5" in bold 16x32
        expectBox(inkBox(mem, STEAMI_LIGHT), 113, 124, 126, 134);
        expectBox(inkBox(mem, STEAMI_DARK), 96, 204, 142, 213);  // "BME280", row 202
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
        expectBox(inkBox(mem, STEAMI_WHITE), 106, 75, 134, 94);  // "72" in 16x32, top 69
        expectBox(inkBox(mem, STEAMI_LIGHT), 117, 103, 122, 112);
        expectBox(inkBox(mem, STEAMI_GRID_DARK), 156, 135, 202, 149);
        expectBox(inkBox(mem, STEAMI_GREEN), 38, 135, 155, 149);           // 118 px of 165
        expectBox(inkBox(mem, STEAMI_DARK, 177, 196), 92, 179, 146, 188);  // "3842 mV"
        expectBox(inkBox(mem, STEAMI_DARK, 197, 239), 92, 199, 146, 210);  // "BQ27441"
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
        // Both values at f 4 (W and E share the size), "23.5" at x 32 and "45.3" at x 152: the
        // 16x32 digits ink rows 6..25.
        expectBox(inkBox(mem, STEAMI_WHITE), 34, 97, 206, 116);
        expectBox(inkBox(mem, STEAMI_LIGHT), 52, 124, 184, 134);
        expectBox(inkBox(mem, STEAMI_GRAY, 0, 60), 92, 39, 146, 48);  // title
        // "TEMP" and "HUM": one 6x12 cell above the values, cell top 91 - 7 - 12 = 72.
        expectBox(inkBox(mem, STEAMI_GRAY, 61, 239), 51, 73, 184, 80);
        expectBox(inkBox(mem, STEAMI_GREEN, 177, 196), 76, 179, 162, 188);  // "Comfortable"
        expectBox(inkBox(mem, STEAMI_GREEN, 197, 239), 96, 199, 142, 208);  // "BME280"
    });
}

static void test_mockup_04_gauge(void) {
    // gauge(342, 0, 500) fills 185 of 270 degrees: the fill ends at 50 degrees, between the
    // golden's polar(57, 45) and polar(57, 55) on 128 (radius 107 on 240), mid-ring.
    render("04_circular_gauge", 128, false, [](SteamiMemoryBackend& mem, SteamiScreen& s) {
        mockupGauge(s, 342);
        expectBox(inkBox(mem, STEAMI_WHITE), 42, 50, 85, 63);           // "342"
        expectBox(inkBox(mem, STEAMI_GRAY, 85, 100), 27, 90, 105, 96);  // labels "0" and "500"
        expectPixel(mem, 104, 24, STEAMI_LIGHT);
        expectPixel(mem, 111, 31, STEAMI_DARK);
    });
    render("04_circular_gauge", 240, true, [](SteamiMemoryBackend& mem, SteamiScreen& s) {
        mockupGauge(s, 342);
        expectBox(inkBox(mem, STEAMI_WHITE), 98, 97, 142, 116);  // "342" in bold 16x32 at x 96
        expectBox(inkBox(mem, STEAMI_GRAY, 160, 190), 52, 170, 192, 177);  // 6x12 labels
        expectPixel(mem, 196, 44, STEAMI_LIGHT);
        expectPixel(mem, 208, 59, STEAMI_DARK);
    });
}

/// Some pixel is set in both masks.
static bool overlaps(const std::vector<bool>& a, const std::vector<bool>& b) {
    for (size_t i = 0; i < a.size() && i < b.size(); i++) {
        if (a[i] && b[i]) {
            return true;
        }
    }
    return false;
}

// Design 2 and 7: program 04 draws no text on the V1c ring. The track is gauge()'s DARK arc drawn
// alone (ruling 4: in a gauge render the GRAY labels paint over the track and would hide the
// overlap); the DARK pixels of gauge(0, 0, 500) lie on it (the positive control). The title, the
// subtitle and the GRAY labels of gauge(342, 0, 500), each drawn alone, share no pixel with it.
// golden_layout.py "Program 04 on 240" prints their distances to the ring: 13.6, 30.5, 12.7, 9.4.
static void test_mockup_04_text_clears_the_ring(void) {
    const std::vector<bool> track = maskAlone(240, true, STEAMI_DARK, [](SteamiScreen& s) {
        const int16_t w = static_cast<int16_t>(s.radius() / STEAMI_GAUGE_WIDTH_DIVISOR);
        s.arc(s.centerX(), s.centerY(), static_cast<int16_t>(s.radius() - 4 * s.width() / 128),
              STEAMI_GAUGE_START, STEAMI_GAUGE_SWEEP, STEAMI_DARK,
              static_cast<uint8_t>(w > STEAMI_GAUGE_MIN_WIDTH ? w : STEAMI_GAUGE_MIN_WIDTH));
    });
    const std::vector<bool> dark =
        maskAlone(240, true, STEAMI_DARK, [](SteamiScreen& s) { s.gauge(0, 0, 500); });
    TEST_ASSERT_TRUE(any(dark));
    for (size_t i = 0; i < dark.size(); i++) {
        TEST_ASSERT_TRUE_MESSAGE(!dark[i] || track[i], "gauge DARK pixel off the track");
    }
    const std::vector<bool> title =
        maskAlone(240, true, STEAMI_GRAY, [](SteamiScreen& s) { s.title("Distance"); });
    const std::vector<bool> subtitle =
        maskAlone(240, true, STEAMI_DARK, [](SteamiScreen& s) { s.subtitle("VL53L1X ToF"); });
    const std::vector<bool> labels =
        maskAlone(240, true, STEAMI_GRAY, [](SteamiScreen& s) { s.gauge(342, 0, 500); });
    TEST_ASSERT_TRUE(any(title) && any(subtitle) && any(labels));
    TEST_ASSERT_FALSE_MESSAGE(overlaps(title, track), "title on the ring");
    TEST_ASSERT_FALSE_MESSAGE(overlaps(subtitle, track), "subtitle on the ring");
    TEST_ASSERT_FALSE_MESSAGE(overlaps(labels, track), "gauge labels on the ring");
}

// D13: the SVG polyline of mockup 05 as 1000 (141 - y) / 82, rounded, the last set to its "847".
static const float kValues05[20] = {354, 378, 415, 390, 451, 524, 561, 537, 573, 610,
                                    585, 622, 598, 634, 610, 622, 585, 610, 646, 847};

static void test_mockup_05_graph(void) {
    SteamiHistory<20> history;
    for (float v : kValues05) {
        history.add(v);
    }
    render("05_scrolling_graph", 128, false, [&](SteamiMemoryBackend& mem, SteamiScreen& s) {
        mockupGraph(s, history, true, "APDS-9960");
        expectBox(inkBox(mem, STEAMI_GRAY), 45, 20, 82, 27);    // "Light", row 20
        expectBox(inkBox(mem, STEAMI_WHITE), 53, 31, 74, 37);   // "847" at row 31
        expectBox(inkBox(mem, STEAMI_LIGHT), 21, 45, 113, 71);  // the curve
        // "1000" and "0", DARK inside the box rows 38..89, right of the axis (x 21)
        expectBox(inkBoxFrom(mem, STEAMI_DARK, 22, 38, 89), 24, 40, 46, 87);
        expectBox(inkBox(mem, STEAMI_DARK, 95, 113), 25, 95, 103, 112);  // lines at 95 and 106
        expectPixel(mem, 113, 45, STEAMI_LIGHT);  // the last point, at gx + gw
    });
    render("05_scrolling_graph", 240, true, [&](SteamiMemoryBackend& mem, SteamiScreen& s) {
        mockupGraph(s, history, true, "APDS-9960");
        expectBox(inkBox(mem, STEAMI_GRAY), 100, 39, 138, 51);   // "Light", row 37
        expectBox(inkBox(mem, STEAMI_WHITE), 108, 60, 130, 69);  // "847" in 8x16 at row 58
        expectBox(inkBox(mem, STEAMI_LIGHT), 39, 85, 213, 134);
        // "1000" and "0" in 6x12, inside the box rows 71..167, right of the axis (x 39..40)
        expectBox(inkBoxFrom(mem, STEAMI_DARK, 41, 71, 167), 44, 74, 66, 163);
        expectBox(inkBox(mem, STEAMI_DARK, 177, 213), 80, 179, 158, 208);  // lines at 177, 197
        expectPixel(mem, 212, 85, STEAMI_LIGHT);
    });
}

// The pressure branch of program 05 on the V1c, the reference of its photo: the axis labels
// "1050" and "950" (24 and 18 px of 6x12 cells, font design 6) lie inside the box (x 41..211,
// rows 71..167) and, by onPanel, inside the circle.
static void test_mockup_05_pressure_v1c(void) {
    SteamiHistory<20> history;
    for (int i = 0; i < 20; i++) {
        history.add(1013.0f);
    }
    render("05_pressure_v1c", 240, true, [&](SteamiMemoryBackend& mem, SteamiScreen& s) {
        mockupGraph(s, history, false, "BME280");
        expectBox(inkBox(mem, STEAMI_GRAY), 88, 39, 150, 48);                 // "Pressure"
        expectBox(inkBox(mem, STEAMI_WHITE), 105, 60, 134, 69);               // "1013"
        expectBox(inkBox(mem, STEAMI_LIGHT), 39, 106, 213, 107);              // flat, k rows thick
        expectBox(inkBoxFrom(mem, STEAMI_DARK, 41, 73, 84), 45, 74, 66, 81);  // "1050"
        expectBox(inkBoxFrom(mem, STEAMI_DARK, 41, 155, 166), 44, 156, 60, 163);  // "950"
    });
}

/// How many of the item cells at tops[0..n) hold GRAY or WHITE ink, each `cell` rows high.
static size_t inkedItemRows(const SteamiMemoryBackend& mem, const int16_t* tops, size_t n,
                            int16_t cell) {
    size_t rows = 0;
    for (size_t i = 0; i < n; i++) {
        const int16_t last = static_cast<int16_t>(tops[i] + cell - 1);
        if (inkBox(mem, STEAMI_GRAY, tops[i], last).count > 0 ||
            inkBox(mem, STEAMI_WHITE, tops[i], last).count > 0) {
            rows++;
        }
    }
    return rows;
}

static void test_mockup_06_menu(void) {
    // "Distance" (index 2) is selected on row 3 of both panels; the cell tops run one row past
    // the window, whose cell stays empty.
    render("06_dpad_menu", 128, false, [](SteamiMemoryBackend& mem, SteamiScreen& s) {
        int selected = 2;
        mockupMenu(s, selected);
        TEST_ASSERT_EQUAL_INT(2, selected);
        expectBox(inkBox(mem, STEAMI_GRAY, 0, 30), 49, 20, 78, 26);     // "Menu"
        expectBox(inkBox(mem, STEAMI_GRAY, 31, 127), 21, 35, 106, 98);  // items 0, 1, 3 and 4
        expectBox(inkBox(mem, STEAMI_WHITE), 4, 63, 94, 69);            // "> " and "Distance"
        expectBox(inkBox(mem, STEAMI_DARK), 1, 61, 127, 74);            // the bar, rows 61..74
        expectPixel(mem, 2, 67, STEAMI_DARK);  // the bar ends on its middle row
        expectPixel(mem, 126, 67, STEAMI_DARK);
        const int16_t tops[6] = {35, 49, 63, 77, 91, 105};
        TEST_ASSERT_EQUAL_size_t(5, inkedItemRows(mem, tops, 6, 8));
    });
    render("06_dpad_menu", 240, true, [](SteamiMemoryBackend& mem, SteamiScreen& s) {
        int selected = 2;
        mockupMenu(s, selected);
        TEST_ASSERT_EQUAL_INT(2, selected);
        expectBox(inkBox(mem, STEAMI_GRAY, 0, 60), 104, 39, 134, 48);
        expectBox(inkBox(mem, STEAMI_GRAY, 61, 239), 76, 67, 162, 210);
        expectBox(inkBox(mem, STEAMI_WHITE), 5, 120, 150, 129);
        expectBox(inkBox(mem, STEAMI_DARK), 2, 115, 238, 140);  // rows 115..140
        expectPixel(mem, 3, 127, STEAMI_DARK);
        expectPixel(mem, 237, 127, STEAMI_DARK);
        const int16_t tops[7] = {65, 91, 118, 144, 170, 196, 223};
        TEST_ASSERT_EQUAL_size_t(6, inkedItemRows(mem, tops, 7, 16));
    });
}

static void test_mockup_07_compass(void) {
    render("07_compass", 128, false, [](SteamiMemoryBackend& mem, SteamiScreen& s) {
        mockupCompass(s, 0.0f);
        expectBox(inkBox(mem, STEAMI_WHITE), 61, 3, 66, 9);  // N
        expectPixel(mem, 64, 20, STEAMI_LIGHT);              // the needle tip
    });
    render("07_compass", 240, true, [](SteamiMemoryBackend& mem, SteamiScreen& s) {
        mockupCompass(s, 0.0f);
        expectBox(inkBox(mem, STEAMI_WHITE), 116, 7, 122, 16);  // N in 8x16 (4b-font)
        expectPixel(mem, 120, 37, STEAMI_LIGHT);
    });
}

static void test_mockup_08_faces(void) {
    // Full render names in SteamiFace order (FACES), not in the review list's order.
    const char* const kFaceNames[6] = {"08_smiley_happy",     "08_smiley_sad",
                                       "08_smiley_surprised", "08_smiley_sleeping",
                                       "08_smiley_angry",     "08_smiley_love"};
    struct Panel {
        int16_t size;
        bool color;
    };
    for (const Panel p : {Panel{128, false}, Panel{240, true}}) {
        for (int i = 0; i < 6; i++) {
            const std::vector<bool> alone = maskAlone(p.size, p.color, STEAMI_LIGHT,
                                                      [i](SteamiScreen& s) { s.face(FACES[i]); });
            TEST_ASSERT_TRUE(any(alone));
            render(kFaceNames[i], p.size, p.color, [&](SteamiMemoryBackend& mem, SteamiScreen& s) {
                mockupFaces(s, i, 0);
                TEST_ASSERT_TRUE(maskOf(mem, STEAMI_LIGHT) == alone);
                TEST_ASSERT_EQUAL_size_t(0, inkBox(mem, STEAMI_GRAY).count);  // no title
            });
        }
    }
}

static void test_mockup_08_reactive(void) {
    render("08_smiley_reactive", 128, false, [](SteamiMemoryBackend& mem, SteamiScreen& s) {
        mockupFaces(s, 6, 100);
        expectBox(inkBox(mem, STEAMI_GRAY), 49, 20, 78, 26);     // "Mood", row 20
        expectBox(inkBox(mem, STEAMI_LIGHT), 31, 31, 96, 96);    // HAPPY
        expectBox(inkBox(mem, STEAMI_GREEN), 45, 108, 82, 114);  // "HAPPY", row 108
    });
    render("08_smiley_reactive", 240, true, [](SteamiMemoryBackend& mem, SteamiScreen& s) {
        mockupFaces(s, 6, 100);
        expectBox(inkBox(mem, STEAMI_GRAY), 104, 39, 134, 48);  // "Mood", row 37
        expectBox(inkBox(mem, STEAMI_LIGHT), 60, 60, 179, 179);
        expectBox(inkBox(mem, STEAMI_GREEN), 100, 204, 138, 213);  // "HAPPY", row 202
    });
}

static void test_mockup_09_watch(void) {
    // 10:10:30. The "12" is the WHITE ink above the "3" and "9" cells (rows 60 / 112 on).
    render("09_watch", 128, false, [](SteamiMemoryBackend& mem, SteamiScreen& s) {
        mockupWatch(s, 10 * 3600UL + 10 * 60 + 30);
        expectBox(inkBox(mem, STEAMI_WHITE, 0, 59), 57, 19, 70, 25);
        expectPixel(mem, 41, 48, STEAMI_LIGHT);   // hour tip
        expectPixel(mem, 101, 45, STEAMI_LIGHT);  // minute tip
    });
    render("09_watch", 240, true, [](SteamiMemoryBackend& mem, SteamiScreen& s) {
        mockupWatch(s, 10 * 3600UL + 10 * 60 + 30);
        expectBox(inkBox(mem, STEAMI_WHITE, 0, 111), 113, 37, 126, 46);  // "12" in 8x16
        expectPixel(mem, 77, 90, STEAMI_LIGHT);
        expectPixel(mem, 189, 85, STEAMI_LIGHT);
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
                expectBox(inkBox(mem, STEAMI_DARK, 177, 196), 113, 184, 126, 184);  // "--"
                expectBox(inkBox(mem, STEAMI_DARK, 197, 239), 92, 199, 146, 210);
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
                expectBox(inkBox(mem, STEAMI_GREEN), 68, 179, 170, 188);  // lines at 177 and 197
            }
        });
        const std::vector<bool> dashes03 =
            maskAlone(p.size, p.color, STEAMI_WHITE, [](SteamiScreen& s) {
                s.value("--", "°C", SteamiAt::W, "TEMP");
                s.value("--", "%", SteamiAt::E, "HUM");
            });
        TEST_ASSERT_TRUE(any(dashes03));
        TEST_ASSERT_TRUE(white == dashes03);

        // 04 with d = -1: an empty gauge (no LIGHT on the ring, 54..60 / 101..113) and "-1 mm".
        const int32_t inner = small ? 54 : 101;
        const int32_t outer = small ? 60 : 113;
        onPanel(p.size, p.color, [&](SteamiMemoryBackend& mem, SteamiScreen& s) {
            mockupGauge(s, -1);
            expectShownOnce(mem);
            white = maskOf(mem, STEAMI_WHITE);
            TEST_ASSERT_EQUAL_size_t(0, countAtDistance(mem, STEAMI_LIGHT, inner - 1, outer + 1));
            TEST_ASSERT_TRUE(countAtDistance(mem, STEAMI_DARK, inner, outer) > 0);  // the track
        });
        const std::vector<bool> minusOne =
            maskAlone(p.size, p.color, STEAMI_WHITE, [](SteamiScreen& s) { s.value(-1, "mm"); });
        TEST_ASSERT_TRUE(any(minusOne));
        TEST_ASSERT_TRUE(white == minusOne);

        // 07 with NAN: the rose of heading 0 with its N, no needle (the LIGHT ticks start at 46 /
        // 87, the needle reaches 44 / 83).
        const int32_t near = small ? 45 : 84;
        const std::vector<bool> north = maskAlone(p.size, p.color, STEAMI_WHITE,
                                                  [](SteamiScreen& s) { mockupCompass(s, 0.0f); });
        TEST_ASSERT_TRUE(any(north));
        onPanel(p.size, p.color, [&](SteamiMemoryBackend& mem, SteamiScreen& s) {
            mockupCompass(s, NAN);
            expectShownOnce(mem);
            TEST_ASSERT_TRUE(maskOf(mem, STEAMI_WHITE) == north);
            TEST_ASSERT_EQUAL_size_t(0, countAtDistance(mem, STEAMI_LIGHT, 0, near));
            TEST_ASSERT_TRUE(countAtDistance(mem, STEAMI_LIGHT, near + 1, p.size / 2) >
                             0);  // ticks
        });

        // 08 with d = -1: the SURPRISED mood, its subtitle in YELLOW.
        const std::vector<bool> surprised = maskAlone(
            p.size, p.color, STEAMI_LIGHT, [](SteamiScreen& s) { s.face(SteamiFace::SURPRISED); });
        onPanel(p.size, p.color, [&](SteamiMemoryBackend& mem, SteamiScreen& s) {
            mockupFaces(s, 6, -1);
            expectShownOnce(mem);
            TEST_ASSERT_TRUE(maskOf(mem, STEAMI_LIGHT) == surprised);
            if (small) {
                expectBox(inkBox(mem, STEAMI_YELLOW), 29, 108, 98, 114);
            } else {
                expectBox(inkBox(mem, STEAMI_YELLOW), 84, 204, 154, 213);
            }
        });

        // 05 with the light sensor missing: 20 NAN values draw no curve (no LIGHT pixel), and the
        // value is the "--" of an empty history.
        SteamiHistory<20> nans;
        for (int i = 0; i < 20; i++) {
            nans.add(NAN);
        }
        const SteamiHistory<20> empty;
        const std::vector<bool> dashes05 =
            maskAlone(p.size, p.color, STEAMI_WHITE,
                      [&](SteamiScreen& s) { mockupGraph(s, empty, true, "APDS-9960"); });
        TEST_ASSERT_TRUE(any(dashes05));
        onPanel(p.size, p.color, [&](SteamiMemoryBackend& mem, SteamiScreen& s) {
            mockupGraph(s, nans, true, "APDS-9960");
            expectShownOnce(mem);
            TEST_ASSERT_EQUAL_size_t(0, inkBox(mem, STEAMI_LIGHT).count);
            TEST_ASSERT_TRUE(maskOf(mem, STEAMI_WHITE) == dashes05);
        });

        // 06 after UP on the first item: selected -1 wraps to 5.
        onPanel(p.size, p.color, [&](SteamiMemoryBackend& mem, SteamiScreen& s) {
            int selected = -1;
            mockupMenu(s, selected);
            expectShownOnce(mem);
            TEST_ASSERT_EQUAL_INT(5, selected);
        });
    }
}

/// mask with every pixel outside the columns x0..x1 and the rows y0..y1 cleared, on a panel
/// `width` pixels wide.
static std::vector<bool> within(std::vector<bool> mask, int16_t width, int16_t x0, int16_t y0,
                                int16_t x1, int16_t y1) {
    for (size_t i = 0; i < mask.size(); i++) {
        const int32_t x = static_cast<int32_t>(i % static_cast<size_t>(width));
        const int32_t y = static_cast<int32_t>(i / static_cast<size_t>(width));
        if (x < x0 || x > x1 || y < y0 || y > y1) {
            mask[i] = false;
        }
    }
    return mask;
}

// D14: the four pages of WeatherStation, the references of its V1c photos, each compared with its
// widgets drawn alone. Page 1's BLUE fill is lifted to grey 10 on the OLED (spec 5.10); page 2's
// "1050" and "950" are DARK inside the box (spec 11 item 9), and the ramp 990..1029 keeps the
// curve off their rows; page 3 is mockup 07, north up.
static void test_weather_station_pages(void) {
    SteamiHistory<40> history;
    for (int i = 0; i < 40; i++) {
        history.add(990.0f + static_cast<float>(i));
    }
    // The graph box interior (golden_layout.py "graph (4c Task 2)"): right of the left axis, above
    // the bottom one.
    struct Panel {
        int16_t size;
        bool color;
        int16_t x0, y0, x1, y1;
    };
    const SteamiRgb lifted = SteamiRgb::gray(10);
    for (const Panel p : {Panel{128, false, 22, 38, 112, 89}, Panel{240, true, 41, 71, 211, 167}}) {
        const std::vector<bool> value =
            maskAlone(p.size, p.color, STEAMI_WHITE, [](SteamiScreen& s) { s.value(23.5f, "°C"); });
        const std::vector<bool> fill = maskAlone(
            p.size, p.color, lifted, [](SteamiScreen& s) { s.gauge(60.0f, 0, 100, STEAMI_BLUE); });
        const std::vector<bool> curve =
            maskAlone(p.size, p.color, STEAMI_RED,
                      [&](SteamiScreen& s) { s.graph(history, 950, 1050, STEAMI_RED); });
        const std::vector<bool> labels =
            within(maskAlone(p.size, p.color, STEAMI_DARK,
                             [&](SteamiScreen& s) { s.graph(history, 950, 1050, STEAMI_RED); }),
                   p.size, p.x0, p.y0, p.x1, p.y1);
        std::vector<uint16_t> compass;
        onPanel(p.size, p.color, [&](SteamiMemoryBackend& mem, SteamiScreen& s) {
            mockupCompass(s, 0.0f);
            compass = mem.pixels();
        });
        TEST_ASSERT_TRUE(any(value) && any(curve) && any(labels));

        render("weather_temperature", p.size, p.color,
               [&](SteamiMemoryBackend& mem, SteamiScreen& s) {
                   weatherPage(s, 0, 23.5f, 60.0f, history, 0.0f);
                   TEST_ASSERT_TRUE(maskOf(mem, STEAMI_WHITE) == value);
               });
        render("weather_humidity", p.size, p.color, [&](SteamiMemoryBackend& mem, SteamiScreen& s) {
            weatherPage(s, 1, 23.5f, 60.0f, history, 0.0f);
            if (p.color) {
                TEST_ASSERT_TRUE(inkBox(mem, STEAMI_BLUE).count > 0);
                return;
            }
            // The title may paint over part of the fill: a subset of the gauge's, not equal.
            const std::vector<bool> page = maskOf(mem, lifted);
            TEST_ASSERT_TRUE(any(page));
            for (size_t i = 0; i < page.size(); i++) {
                TEST_ASSERT_TRUE_MESSAGE(!page[i] || fill[i], "grey 10 pixel off the gauge fill");
            }
        });
        render("weather_pressure", p.size, p.color, [&](SteamiMemoryBackend& mem, SteamiScreen& s) {
            weatherPage(s, 2, 23.5f, 60.0f, history, 0.0f);
            TEST_ASSERT_TRUE(maskOf(mem, STEAMI_RED) == curve);
            TEST_ASSERT_TRUE(within(maskOf(mem, STEAMI_DARK), p.size, p.x0, p.y0, p.x1, p.y1) ==
                             labels);
        });
        render("weather_compass", p.size, p.color, [&](SteamiMemoryBackend& mem, SteamiScreen& s) {
            weatherPage(s, 3, 23.5f, 60.0f, history, 0.0f);
            TEST_ASSERT_TRUE(mem.pixels() == compass);
        });
    }
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_mockup_01_temperature);
    RUN_TEST(test_mockup_02_battery);
    RUN_TEST(test_mockup_03_comfort);
    RUN_TEST(test_mockup_04_gauge);
    RUN_TEST(test_mockup_04_text_clears_the_ring);
    RUN_TEST(test_mockup_05_graph);
    RUN_TEST(test_mockup_05_pressure_v1c);
    RUN_TEST(test_mockup_06_menu);
    RUN_TEST(test_mockup_07_compass);
    RUN_TEST(test_mockup_08_faces);
    RUN_TEST(test_mockup_08_reactive);
    RUN_TEST(test_mockup_09_watch);
    RUN_TEST(test_mockups_without_sensors);
    RUN_TEST(test_weather_station_pages);
    return UNITY_END();
}
