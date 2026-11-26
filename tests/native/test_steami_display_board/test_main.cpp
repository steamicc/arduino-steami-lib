// SPDX-License-Identifier: GPL-3.0-or-later

#include <Arduino.h>
#include <Print.h>
#include <SPI.h>
#include <SteamiGC9A01.h>
#include <SteamiSSD1327.h>
#include <Wire.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unity.h>

#include <initializer_list>
#include <map>
#include <new>
#include <string>
#include <vector>

#include "SteamiBoard.h"
#include "SteamiDisplay.h"
#include "SteamiGallery.h"
#include "SteamiGc9a01Backend.h"
#include "SteamiMemoryBackend.h"
#include "SteamiSsd1327Backend.h"

// Every operator new of this program, counted: a slot built by display() allocates.
static size_t g_allocations = 0;
void* operator new(size_t n) {
    g_allocations++;
    void* p = malloc(n == 0 ? 1 : n);
    if (p == nullptr) {
        throw std::bad_alloc();
    }
    return p;
}
void* operator new[](size_t n) {
    return operator new(n);
}
void operator delete(void* p) noexcept {
    free(p);
}
void operator delete[](void* p) noexcept {
    free(p);
}
void operator delete(void* p, size_t) noexcept {
    free(p);
}
void operator delete[](void* p, size_t) noexcept {
    free(p);
}

static std::vector<std::string> g_lines;
static void capture(const char* line) {
    g_lines.push_back(line);
}

static SteamiBoard board(Wire, SteamiBoardPins::steami());

// Every byte sent on any SPI bus, with the D/C level and the state of the bus that sent it.
struct SpiByte {
    bool data;
    uint8_t value;
    bool begun;
    uint32_t mosi, miso, sclk, clock;
};
static std::vector<SpiByte> g_spi;
static void recordSpi(const SPIClass& bus, uint8_t v) {  // D/C is SPI_INT_MISO on both panels
    g_spi.push_back({digitalRead(SPI_INT_MISO) == HIGH, v, bus.begun(), bus.mosiPin, bus.misoPin,
                     bus.sclkPin, bus.lastClock});
}

void setUp(void) {
    Wire = TwoWire();
    g_lines.clear();
    board.end();
    board.setLogger(&capture);
    g_spi.clear();
    gpioPinState().clear();
    gpioPinMode().clear();
    ++SPIClass::testEpoch;
    SPIClass::onAnyTransfer = recordSpi;
}
void tearDown(void) {
    board.end();
    board.setLogger(nullptr);
    SPIClass::onAnyTransfer = nullptr;
}

/// The bytes sent with D/C low, in order.
static std::vector<uint8_t> commandsSent() {
    std::vector<uint8_t> out;
    for (const SpiByte& b : g_spi) {
        if (!b.data) {
            out.push_back(b.value);
        }
    }
    return out;
}

/// The last n bytes sent with D/C high.
static std::vector<uint8_t> lastData(size_t n) {
    std::vector<uint8_t> all;
    for (const SpiByte& b : g_spi) {
        if (b.data) {
            all.push_back(b.value);
        }
    }
    TEST_ASSERT_TRUE_MESSAGE(all.size() >= n, "fewer data bytes than asked");
    return std::vector<uint8_t>(all.end() - static_cast<ptrdiff_t>(n), all.end());
}

/// The first data byte after the first cmd, -1 if none.
static int paramAfter(uint8_t cmd) {
    bool seen = false;
    for (const SpiByte& b : g_spi) {
        if (!seen) {
            seen = !b.data && b.value == cmd;
        } else if (b.data) {
            return b.value;
        }
    }
    return -1;
}

/// Every byte went over a begun bus on the internal SPI pins at the given clock.
static void assertPanelBus(uint32_t clock) {
    TEST_ASSERT_FALSE(g_spi.empty());
    for (const SpiByte& b : g_spi) {
        TEST_ASSERT_TRUE(b.begun);
        TEST_ASSERT_EQUAL_UINT32(SPI_INT_MOSI, b.mosi);
        TEST_ASSERT_EQUAL_UINT32(SPI_INT_MISO, b.miso);
        TEST_ASSERT_EQUAL_UINT32(SPI_INT_SCK, b.sclk);
        TEST_ASSERT_EQUAL_UINT32(clock, b.clock);
    }
}

static void assertCommands(std::initializer_list<uint8_t> expected) {
    const std::vector<uint8_t> sent = commandsSent();
    TEST_ASSERT_EQUAL_size_t(expected.size(), sent.size());
    size_t i = 0;
    for (uint8_t cmd : expected) {
        TEST_ASSERT_EQUAL_HEX8(cmd, sent[i++]);
    }
}

/// Draws on a memory backend injected in the board, through a SteamiDisplay, after end().
static void drawOnMemory(SteamiMemoryBackend& mem, void (*draw)(SteamiDisplay&)) {
    board.end();
    board.setLogger(&capture);
    board.setDisplayBackend(mem);
    SteamiDisplay d(board);
    draw(d);
    TEST_ASSERT_EQUAL_UINT32(0, mem.outOfBounds());
}

static constexpr size_t kGc9a01Frame = 240 * 240 * 2;

static void test_gc9a01_backend_identity_and_quiet_constructor(void) {
    SteamiGc9a01Backend tft;
    TEST_ASSERT_EQUAL_STRING("GC9A01", tft.chip());
    TEST_ASSERT_EQUAL_INT16(240, tft.width());
    TEST_ASSERT_EQUAL_INT16(240, tft.height());
    TEST_ASSERT_TRUE(tft.isColor());
    TEST_ASSERT_TRUE(gpioPinMode().empty());  // no pin before begin()
    TEST_ASSERT_EQUAL_size_t(0, g_spi.size());
}

static void test_gc9a01_backend_begin_initialises_the_panel(void) {
    SteamiGc9a01Backend tft;
    TEST_ASSERT_TRUE(tft.begin());
    assertPanelBus(16000000);  // internal pins, SPI begun before the first byte
    size_t displayOn = 0;
    for (uint8_t cmd : commandsSent()) {
        displayOn += (cmd == GC9A01_CMD_DISPON) ? 1 : 0;
    }
    TEST_ASSERT_EQUAL_size_t(1, displayOn);
    const std::vector<uint8_t> frame = lastData(kGc9a01Frame);
    for (uint8_t v : frame) {
        TEST_ASSERT_EQUAL_HEX8(0x00, v);  // cleared to black
    }
}

static void test_gc9a01_backend_passes_rgb565_through(void) {
    SteamiGc9a01Backend tft;
    TEST_ASSERT_TRUE(tft.begin());

    g_spi.clear();
    tft.fill(0x1234);
    tft.show();
    const std::vector<uint8_t> filled = lastData(kGc9a01Frame);
    for (size_t i = 0; i < filled.size(); i += 2) {
        TEST_ASSERT_EQUAL_HEX8(0x12, filled[i]);
        TEST_ASSERT_EQUAL_HEX8(0x34, filled[i + 1]);
    }

    g_spi.clear();
    tft.pixel(239, 239, 0xF800);
    tft.show();
    const std::vector<uint8_t> last = lastData(2);
    TEST_ASSERT_EQUAL_HEX8(0xF8, last[0]);
    TEST_ASSERT_EQUAL_HEX8(0x00, last[1]);

    g_spi.clear();
    tft.fillRect(0, 0, 240, 1, 0x07E0);
    tft.show();
    const std::vector<uint8_t> row = lastData(kGc9a01Frame);
    for (size_t i = 0; i < 480; i += 2) {
        TEST_ASSERT_EQUAL_HEX8(0x07, row[i]);
        TEST_ASSERT_EQUAL_HEX8(0xE0, row[i + 1]);
    }
    for (size_t i = 480; i < 960; i += 2) {  // h = 1: row 1 keeps the fill
        TEST_ASSERT_EQUAL_HEX8(0x12, row[i]);
        TEST_ASSERT_EQUAL_HEX8(0x34, row[i + 1]);
    }
}

static void test_gc9a01_backend_controls_forward(void) {
    SteamiGc9a01Backend tft;
    TEST_ASSERT_TRUE(tft.begin());

    g_spi.clear();
    tft.invert(true);
    assertCommands({GC9A01_CMD_INVOFF});  // the V1c IPS panel is inverted at rest
    g_spi.clear();
    tft.invert(false);
    assertCommands({GC9A01_CMD_INVON});

    g_spi.clear();
    tft.rotate(true);
    TEST_ASSERT_FALSE(commandsSent().empty());
    TEST_ASSERT_EQUAL_HEX8(GC9A01_CMD_MADCTL, commandsSent()[0]);
    TEST_ASSERT_EQUAL_INT(GC9A01_ORIENT_ROTATED, paramAfter(GC9A01_CMD_MADCTL));
    lastData(kGc9a01Frame);  // the frame is sent again
    g_spi.clear();
    tft.rotate(false);
    TEST_ASSERT_EQUAL_INT(GC9A01_ORIENT_NORMAL, paramAfter(GC9A01_CMD_MADCTL));

    g_spi.clear();
    tft.powerOff();
    assertCommands({GC9A01_CMD_DISPOFF, GC9A01_CMD_SLPIN});
    g_spi.clear();
    tft.powerOn();
    assertCommands({GC9A01_CMD_SLPOUT, GC9A01_CMD_DISPON});
}

static void test_gc9a01_frame_matches_the_memory_gallery(void) {
    SteamiGc9a01Backend tft;
    board.setDisplayBackend(tft);
    SteamiDisplay d(board);
    drawGallery(d);
    const std::vector<uint8_t> frame = lastData(kGc9a01Frame);

    SteamiMemoryBackend mem(240, 240, true);
    drawOnMemory(mem, drawGallery);
    TEST_ASSERT_TRUE(mem.litCount() > 0);
    for (int16_t y = 0; y < 240; y++) {
        for (int16_t x = 0; x < 240; x++) {
            const size_t i = 2 * (static_cast<size_t>(y) * 240 + static_cast<size_t>(x));
            const uint16_t sent = static_cast<uint16_t>((frame[i] << 8) | frame[i + 1]);
            TEST_ASSERT_EQUAL_HEX16(mem.at(x, y), sent);
        }
    }
}

static constexpr size_t kOledFrame = 128 * 128 / 2;

/// The command byte that follows the first occurrence of cmd in the commands sent, -1 if none.
static int commandAfter(uint8_t cmd) {
    const std::vector<uint8_t> sent = commandsSent();
    for (size_t i = 0; i + 1 < sent.size(); i++) {
        if (sent[i] == cmd) {
            return sent[i + 1];
        }
    }
    return -1;
}

/// Every nibble of the OLED frame is the grey level of the memory backend's colour at that pixel.
static void assertOledFrameEquals(const SteamiMemoryBackend& grey128,
                                  const std::vector<uint8_t>& frame) {
    TEST_ASSERT_EQUAL_size_t(kOledFrame, frame.size());
    for (int16_t y = 0; y < 128; y++) {
        for (int16_t x = 0; x < 128; x++) {
            const uint8_t byte = frame[static_cast<size_t>(y) * 64 + static_cast<size_t>(x) / 2];
            const uint8_t nibble =
                (x % 2 == 0) ? static_cast<uint8_t>(byte >> 4) : static_cast<uint8_t>(byte & 0x0F);
            TEST_ASSERT_EQUAL_UINT8(SteamiRgb::fromRgb565(grey128.at(x, y)).gray4(), nibble);
        }
    }
}

/// Primitives that start or end outside the panel; nothing may reach the buffer out of range.
static void drawClippingCases(SteamiDisplay& d) {
    d.pixel(-1, 0);
    d.pixel(200, 5);
    d.text("Clip", 10, 125);
    d.text("Clip", -20, 10);
    d.line(-50, -50, 300, 300);
    d.rect(-10, -10, 300, 300);
    d.circle(0, 0, 20);
    d.circle(127, 127, 20, STEAMI_WHITE, true);
    d.circle(64, -5, 30);
    d.circle(-5, 64, 30, STEAMI_GRAY, true);
    d.triangle(-50, -50, 300, 60, 60, 300, STEAMI_LIGHT, true);
    d.arc(0, 0, 40, 0, 360, STEAMI_WHITE, 3);
    d.arc(127, 0, 40, 90, 270, STEAMI_GREEN, 5);
    d.arc(64, 127, 70, 180, 180, STEAMI_RED, 2);
    d.arc(127, 127, 200, 0, 360, STEAMI_BLUE, 8);
    d.show();
}

static void test_ssd1327_backend_identity(void) {
    SteamiSsd1327Backend oled;
    TEST_ASSERT_EQUAL_STRING("SSD1327", oled.chip());
    TEST_ASSERT_EQUAL_INT16(128, oled.width());
    TEST_ASSERT_EQUAL_INT16(128, oled.height());
    TEST_ASSERT_FALSE(oled.isColor());
}

static void test_ssd1327_constructor_resets_without_spi(void) {
    SteamiSsd1327Backend oled;
    TEST_ASSERT_EQUAL_INT(OUTPUT, gpioPinMode()[RST_DISPLAY]);
    TEST_ASSERT_EQUAL_INT(HIGH, gpioPinState()[RST_DISPLAY]);  // the pulse ended high
    TEST_ASSERT_EQUAL_INT(HIGH, gpioPinState()[CS_DISPLAY]);
    TEST_ASSERT_EQUAL_size_t(0, g_spi.size());
}

static void test_ssd1327_backend_begin(void) {
    SteamiSsd1327Backend oled;
    TEST_ASSERT_TRUE(oled.begin());
    assertPanelBus(10000000);
    for (uint8_t v : lastData(kOledFrame)) {
        TEST_ASSERT_EQUAL_HEX8(0x00, v);  // initDisplay clears the panel
    }
}

static void test_ssd1327_converts_colours_to_grey(void) {
    SteamiSsd1327Backend oled;
    TEST_ASSERT_TRUE(oled.begin());

    g_spi.clear();
    oled.fill(STEAMI_GRAY.rgb565());
    oled.show();
    for (uint8_t v : lastData(kOledFrame)) {
        TEST_ASSERT_EQUAL_HEX8(0x99, v);
    }

    g_spi.clear();
    oled.fill(0);
    oled.pixel(0, 0, STEAMI_WHITE.rgb565());
    oled.pixel(127, 127, STEAMI_BLUE.rgb565());
    oled.fillRect(126, 0, 2, 1, STEAMI_RED.rgb565());
    oled.show();
    const std::vector<uint8_t> frame = lastData(kOledFrame);
    TEST_ASSERT_EQUAL_HEX8(0xF0, frame[0]);
    TEST_ASSERT_EQUAL_HEX8(0x88, frame[63]);
    TEST_ASSERT_EQUAL_HEX8(0x06, frame[kOledFrame - 1]);
}

static void test_ssd1327_colour_cache_follows_every_colour(void) {
    SteamiSsd1327Backend oled;
    TEST_ASSERT_TRUE(oled.begin());
    g_spi.clear();
    oled.fill(0);
    const uint16_t colours[] = {STEAMI_BLUE.rgb565(), STEAMI_WHITE.rgb565(),
                                STEAMI_BLUE.rgb565(), STEAMI_DARK.rgb565(),
                                STEAMI_DARK.rgb565(), SteamiRgb::gray(1).rgb565()};
    for (int16_t x = 0; x < 6; x++) {
        oled.pixel(x, 0, colours[x]);
    }
    oled.show();
    const std::vector<uint8_t> frame = lastData(kOledFrame);
    TEST_ASSERT_EQUAL_HEX8(0x6F, frame[0]);
    TEST_ASSERT_EQUAL_HEX8(0x66, frame[1]);
    TEST_ASSERT_EQUAL_HEX8(0x61, frame[2]);
}

static void test_ssd1327_controls_forward(void) {
    SteamiSsd1327Backend oled;
    TEST_ASSERT_TRUE(oled.begin());

    g_spi.clear();
    oled.invert(true);
    assertCommands({SET_DISP_MODE_INVERSE});
    g_spi.clear();
    oled.invert(false);
    assertCommands({SET_DISP_MODE_NORMAL});

    g_spi.clear();
    oled.rotate(true);
    TEST_ASSERT_EQUAL_INT(128, commandAfter(SET_DISP_OFFSET));
    TEST_ASSERT_EQUAL_INT(0x42, commandAfter(SET_SEG_REMAP));
    g_spi.clear();
    oled.rotate(false);
    TEST_ASSERT_EQUAL_INT(0, commandAfter(SET_DISP_OFFSET));
    TEST_ASSERT_EQUAL_INT(0x51, commandAfter(SET_SEG_REMAP));

    g_spi.clear();
    oled.powerOff();
    assertCommands({SET_FN_SELECT_A, 0x00, SET_DISP});
    g_spi.clear();
    oled.powerOn();
    assertCommands({SET_FN_SELECT_A, 0x01, SET_DISP | 1});
}

/// Draws on the OLED backend and on a grey memory backend, and compares the two frames.
static void assertOledMatchesMemory(void (*draw)(SteamiDisplay&)) {
    SteamiSsd1327Backend oled;
    board.setDisplayBackend(oled);
    SteamiDisplay d(board);
    draw(d);
    const std::vector<uint8_t> frame = lastData(kOledFrame);

    SteamiMemoryBackend mem(128, 128, false);
    drawOnMemory(mem, draw);
    TEST_ASSERT_TRUE(mem.litCount() > 0);
    assertOledFrameEquals(mem, frame);
}

static void test_ssd1327_frame_matches_the_grey_gallery(void) {
    assertOledMatchesMemory(drawGallery);
}

static void test_ssd1327_clipping_cases_stay_in_the_buffer(void) {
    assertOledMatchesMemory(drawClippingCases);
}

static_assert(sizeof(SteamiSsd1327Backend) < 128 && sizeof(SteamiGc9a01Backend) < 128,
              "the slots hold drivers, not frame buffers");

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

static void presentBme280(void) {  // detected as a V1c
    Wire.setRegister(0x76, 0xD0, 0x60);
}
static void presentHids(void) {  // detected as a V1a/V1b
    Wire.setRegister(0x5F, 0x0F, 0xBC);
}

// Starts the next case of a test on a fresh bus and a fresh board.
static void restart(void) {
    board.end();
    Wire = TwoWire();
    g_lines.clear();
}

/// How many times cmd was sent with D/C low.
static size_t commandCount(uint8_t cmd) {
    size_t count = 0;
    for (uint8_t sent : commandsSent()) {
        count += (sent == cmd) ? 1 : 0;
    }
    return count;
}

/// What printInfo(Print&) prints.
static std::string printedInfo() {
    StringPrint p;
    board.printInfo(p);
    return p.text;
}

static bool printed(const std::string& text, const char* what) {
    return text.find(what) != std::string::npos;
}

/// display() builds the slot of `chip`, only that one, sends no SPI byte and keeps it.
static void assertOnlySlot(const char* chip, SteamiChip slot, SteamiChip other) {
    SteamiDisplayBackend* b = board.display();
    TEST_ASSERT_NOT_NULL(b);
    TEST_ASSERT_EQUAL_STRING(chip, b->chip());
    TEST_ASSERT_TRUE(board.built(slot));
    TEST_ASSERT_FALSE(board.built(other));
    TEST_ASSERT_EQUAL_PTR(b, board.display());
    TEST_ASSERT_EQUAL_size_t(0, g_spi.size());  // built, not begun
}

static void test_v1c_builds_only_the_gc9a01_slot(void) {
    presentBme280();
    assertOnlySlot("GC9A01", SteamiChip::GC9A01, SteamiChip::SSD1327);
}

static void test_v1ab_builds_only_the_ssd1327_slot(void) {
    presentHids();
    TEST_ASSERT_TRUE(board.release() == SteamiRelease::V1AB);
    assertOnlySlot("SSD1327", SteamiChip::SSD1327, SteamiChip::GC9A01);
    for (SteamiRelease r : {SteamiRelease::V1A, SteamiRelease::V1B}) {
        board.end();
        TEST_ASSERT_FALSE(board.built(SteamiChip::SSD1327));
        board.begin(r);
        assertOnlySlot("SSD1327", SteamiChip::SSD1327, SteamiChip::GC9A01);
    }
}

/// Claims the first chip of every function but the screen, as a sensor sketch does.
static void useEverythingButTheScreen(void) {
    const SteamiRelease r = board.release();
    for (uint8_t f = 0; f < static_cast<uint8_t>(SteamiFunction::COUNT); f++) {
        if (static_cast<SteamiFunction>(f) != SteamiFunction::SCREEN) {
            board.claim(steamiRoute(r, static_cast<SteamiFunction>(f), 0));
        }
    }
    TEST_ASSERT_TRUE(board.has(SteamiFunction::SCREEN));
    StringPrint p;
    board.printInfo(p);
    TEST_ASSERT_FALSE(board.built(SteamiChip::SSD1327));
    TEST_ASSERT_FALSE(board.built(SteamiChip::GC9A01));
    TEST_ASSERT_EQUAL_size_t(0, g_spi.size());
}

static void test_sketch_without_screen_builds_neither(void) {
    presentBme280();
    TEST_ASSERT_TRUE(board.release() == SteamiRelease::V1C);
    useEverythingButTheScreen();

    restart();
    presentHids();
    TEST_ASSERT_TRUE(board.release() == SteamiRelease::V1AB);
    useEverythingButTheScreen();
}

static void test_unknown_gives_e30_and_no_slot(void) {  // Review Focus 1
    SteamiDisplay d(board);
    TEST_ASSERT_EQUAL_INT16(0, d.width());
    TEST_ASSERT_EQUAL_INT16(0, d.radius());
    TEST_ASSERT_FALSE(d.begin());
    d.clear(STEAMI_WHITE);
    d.show();
    TEST_ASSERT_EQUAL_size_t(1, countLogged("(E30)"));
    TEST_ASSERT_EQUAL_STRING("unknown board", d.error());
    TEST_ASSERT_FALSE(board.built(SteamiChip::SSD1327));
    TEST_ASSERT_FALSE(board.built(SteamiChip::GC9A01));
    TEST_ASSERT_EQUAL_size_t(0, g_spi.size());
    for (int pin : {CS_DISPLAY, RST_DISPLAY, SPI_INT_MISO}) {  // SPI_INT_MISO: the SSD1327 D/C
        TEST_ASSERT_EQUAL_size_t(0, gpioPinMode().count(pin));
    }
}

static void test_v1c_geometry_before_begin(void) {  // Review Focus 2
    board.begin(SteamiRelease::V1C);
    SteamiDisplay d(board);
    TEST_ASSERT_EQUAL_INT16(240, d.width());
    TEST_ASSERT_EQUAL_INT16(240, d.height());
    TEST_ASSERT_EQUAL_INT16(120, d.radius());
    TEST_ASSERT_TRUE(d.isColor());
    TEST_ASSERT_TRUE(board.built(SteamiChip::GC9A01));
    TEST_ASSERT_EQUAL_size_t(0, g_spi.size());
    for (int pin : {CS_DISPLAY, RST_DISPLAY, SPI_INT_MISO}) {
        TEST_ASSERT_EQUAL_size_t(0, gpioPinMode().count(pin));
    }

    TEST_ASSERT_TRUE(d.begin());
    TEST_ASSERT_EQUAL_size_t(1, commandCount(GC9A01_CMD_DISPON));
    const size_t sent = g_spi.size();
    assertStatus(SteamiStatus::OK, board.beginDisplay());
    TEST_ASSERT_EQUAL_size_t(sent, g_spi.size());
}

static void test_release_forced_after_geometry_keeps_one_slot(void) {
    board.begin(SteamiRelease::V1B);
    TEST_ASSERT_NOT_NULL(board.display());
    TEST_ASSERT_TRUE(board.built(SteamiChip::SSD1327));

    board.begin(SteamiRelease::V1C);
    SteamiDisplayBackend* b = board.display();
    TEST_ASSERT_NOT_NULL(b);
    TEST_ASSERT_EQUAL_STRING("GC9A01", b->chip());
    TEST_ASSERT_TRUE(board.built(SteamiChip::GC9A01));
    TEST_ASSERT_FALSE(board.built(SteamiChip::SSD1327));
}

static void test_release_forced_after_begin_keeps_the_begun_slot(void) {
    board.begin(SteamiRelease::V1C);
    SteamiDisplay d(board);
    TEST_ASSERT_TRUE(d.begin());
    SteamiDisplayBackend* begun = board.display();
    TEST_ASSERT_NOT_NULL(begun);
    const size_t sent = g_spi.size();
    const std::map<int, int> modes = gpioPinMode();
    const std::map<int, int> levels = gpioPinState();

    board.begin(SteamiRelease::V1B);
    const size_t allocations = g_allocations;
    TEST_ASSERT_EQUAL_PTR(begun, board.display());
    TEST_ASSERT_EQUAL_size_t(allocations, g_allocations);
    TEST_ASSERT_TRUE(board.built(SteamiChip::GC9A01));
    TEST_ASSERT_FALSE(board.built(SteamiChip::SSD1327));
    TEST_ASSERT_EQUAL_PTR(begun, d.backend());
    TEST_ASSERT_TRUE(modes == gpioPinMode());  // no reset pulse, D/C pin untouched
    TEST_ASSERT_TRUE(levels == gpioPinState());
    TEST_ASSERT_EQUAL_size_t(sent, g_spi.size());
    const std::string info = printedInfo();
    TEST_ASSERT_TRUE(printed(info, "GC9A01 screen OK\n"));
    TEST_ASSERT_FALSE(printed(info, "SSD1327 screen"));
}

static void test_v1ab_geometry_before_begin(void) {  // Review Focus 2
    board.begin(SteamiRelease::V1B);
    SteamiDisplay d(board);
    TEST_ASSERT_EQUAL_INT16(128, d.width());
    TEST_ASSERT_EQUAL_INT16(64, d.radius());
    TEST_ASSERT_FALSE(d.isColor());
    TEST_ASSERT_TRUE(board.built(SteamiChip::SSD1327));
    TEST_ASSERT_EQUAL_size_t(0, g_spi.size());
    TEST_ASSERT_EQUAL_INT(OUTPUT, gpioPinMode()[RST_DISPLAY]);  // the constructor's reset pulse
    TEST_ASSERT_EQUAL_INT(HIGH, gpioPinState()[RST_DISPLAY]);

    TEST_ASSERT_TRUE(d.begin());
    TEST_ASSERT_FALSE(g_spi.empty());
    const size_t sent = g_spi.size();
    assertStatus(SteamiStatus::OK, board.beginDisplay());
    TEST_ASSERT_EQUAL_size_t(sent, g_spi.size());
}

static void test_injected_backend_skips_detection(void) {  // Review Focus 3
    SteamiMemoryBackend mem(240, 240);
    board.setDisplayBackend(mem);
    TEST_ASSERT_EQUAL_PTR(&mem, board.display());
    TEST_ASSERT_EQUAL_UINT32(0, Wire.totalTransactions());
    TEST_ASSERT_FALSE(board.built(SteamiChip::GC9A01));
    TEST_ASSERT_FALSE(board.built(SteamiChip::SSD1327));
}

static void test_injection_frees_an_unbegun_slot(void) {  // Review Focus 3
    board.begin(SteamiRelease::V1B);
    TEST_ASSERT_NOT_NULL(board.display());
    TEST_ASSERT_TRUE(board.built(SteamiChip::SSD1327));
    SteamiMemoryBackend mem(240, 240);
    board.setDisplayBackend(mem);
    TEST_ASSERT_FALSE(board.built(SteamiChip::SSD1327));
    TEST_ASSERT_EQUAL_PTR(&mem, board.display());

    board.end();
    board.begin(SteamiRelease::V1C);
    SteamiDisplay d(board);
    TEST_ASSERT_TRUE(d.begin());
    board.setDisplayBackend(mem);  // ignored: the panel is begun
    SteamiDisplayBackend* b = board.display();
    TEST_ASSERT_NOT_NULL(b);
    TEST_ASSERT_EQUAL_STRING("GC9A01", b->chip());
    TEST_ASSERT_TRUE(board.built(SteamiChip::GC9A01));
}

static void test_injected_failing_backend_gives_e31(void) {
    board.begin(SteamiRelease::V1C);
    SteamiMemoryBackend mem(240, 240, true, false);
    board.setDisplayBackend(mem);
    SteamiDisplay d(board);
    d.clear(STEAMI_WHITE);
    d.pixel(1, 1);
    d.show();
    assertStatus(SteamiStatus::NO_MEMORY, d.status());
    TEST_ASSERT_EQUAL_STRING("no memory", d.error());
    TEST_ASSERT_EQUAL_size_t(1, countLogged("(E31)"));
    TEST_ASSERT_EQUAL_UINT32(0, mem.fillCalls());
    TEST_ASSERT_EQUAL_UINT32(0, mem.pixelCalls());
    TEST_ASSERT_EQUAL_UINT32(0, mem.showCalls());
    TEST_ASSERT_FALSE(board.built(SteamiChip::GC9A01));
    TEST_ASSERT_EQUAL_size_t(0, g_spi.size());
}

// Spec section 7, suite 2, with two SteamiDisplay objects: SteamiScreen comes in 4a.
static void test_two_displays_share_the_gc9a01_slot(void) {
    board.begin(SteamiRelease::V1C);
    SteamiDisplay a(board), b(board);
    a.pixel(1, 1);
    b.pixel(2, 2);
    b.show();
    TEST_ASSERT_NOT_NULL(a.backend());
    TEST_ASSERT_EQUAL_PTR(a.backend(), b.backend());
    TEST_ASSERT_EQUAL_PTR(board.display(), a.backend());
    TEST_ASSERT_EQUAL_size_t(1, commandCount(GC9A01_CMD_DISPON));
    const std::vector<uint8_t> frame = lastData(kGc9a01Frame);
    for (size_t xy = 1; xy <= 2; xy++) {
        const size_t i = 2 * (xy * 240 + xy);
        TEST_ASSERT_EQUAL_HEX8(0xFF, frame[i]);
        TEST_ASSERT_EQUAL_HEX8(0xFF, frame[i + 1]);
    }
    TEST_ASSERT_EQUAL_HEX8(0x00, frame[2 * (3 * 240 + 3)]);  // nothing else drawn
}

static void test_v1ab_slot_clipping_cases(void) {  // the SSD1327 under ASan through the real slot
    board.begin(SteamiRelease::V1B);
    SteamiDisplay d(board);
    drawClippingCases(d);
    TEST_ASSERT_TRUE(board.built(SteamiChip::SSD1327));
    const std::vector<uint8_t> frame = lastData(kOledFrame);

    SteamiMemoryBackend mem(128, 128, false);
    drawOnMemory(mem, drawClippingCases);
    assertOledFrameEquals(mem, frame);
}

static void test_end_destroys_panel_slots_and_restarts(void) {  // Review Focus 4
    board.begin(SteamiRelease::V1C);
    SteamiDisplay d(board);
    TEST_ASSERT_TRUE(d.begin());
    TEST_ASSERT_TRUE(board.built(SteamiChip::GC9A01));
    board.end();
    TEST_ASSERT_FALSE(board.built(SteamiChip::GC9A01));

    board.setLogger(&capture);
    board.begin(SteamiRelease::V1C);
    d.pixel(1, 1);
    TEST_ASSERT_TRUE(board.built(SteamiChip::GC9A01));
    TEST_ASSERT_EQUAL_size_t(2, commandCount(GC9A01_CMD_DISPON));
}

static void test_print_info_lists_the_screen_state(void) {
    presentBme280();
    TEST_ASSERT_TRUE(printed(printedInfo(), "GC9A01 screen not started\n"));
    TEST_ASSERT_FALSE(board.built(SteamiChip::GC9A01));  // printInfo() builds no slot

    SteamiDisplay d(board);
    TEST_ASSERT_TRUE(d.begin());
    TEST_ASSERT_TRUE(printed(printedInfo(), "GC9A01 screen OK\n"));

    // An injected backend gives its own name; the line comes after the chips and before the
    // last error, replayed here because no logger delivered it.
    board.end();
    board.setLogger(nullptr);
    SteamiMemoryBackend mem(128, 128, true, false);
    board.setDisplayBackend(mem);
    SteamiDisplay m(board);
    TEST_ASSERT_FALSE(m.begin());
    const std::string text = printedInfo();
    const size_t screen = text.find("memory screen no memory\n");
    TEST_ASSERT_TRUE(screen != std::string::npos);
    TEST_ASSERT_TRUE(text.find("MCP23009E 0x20 ") < screen);
    TEST_ASSERT_TRUE(screen < text.find("(E31)\n"));

    restart();
    TEST_ASSERT_FALSE(board.begin());
    TEST_ASSERT_FALSE(printed(printedInfo(), " screen "));
}

static void test_claim_stays_not_started_for_panels(void) {
    board.begin(SteamiRelease::V1C);
    TEST_ASSERT_NOT_NULL(board.display());
    assertStatus(SteamiStatus::NOT_STARTED, board.claim(SteamiChip::GC9A01));
    assertStatus(SteamiStatus::NOT_STARTED, board.claim(SteamiChip::SSD1327));
    TEST_ASSERT_TRUE(board.built(SteamiChip::GC9A01));
}

// Spec section 7, suite 2 lists this gate in this suite, beside test_steami_board's.
static void test_sizeof_board_with_panel_slots(void) {
    TEST_ASSERT_LESS_THAN(2560, sizeof(SteamiBoard));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_gc9a01_backend_identity_and_quiet_constructor);
    RUN_TEST(test_gc9a01_backend_begin_initialises_the_panel);
    RUN_TEST(test_gc9a01_backend_passes_rgb565_through);
    RUN_TEST(test_gc9a01_backend_controls_forward);
    RUN_TEST(test_gc9a01_frame_matches_the_memory_gallery);
    RUN_TEST(test_ssd1327_backend_identity);
    RUN_TEST(test_ssd1327_constructor_resets_without_spi);
    RUN_TEST(test_ssd1327_backend_begin);
    RUN_TEST(test_ssd1327_converts_colours_to_grey);
    RUN_TEST(test_ssd1327_colour_cache_follows_every_colour);
    RUN_TEST(test_ssd1327_controls_forward);
    RUN_TEST(test_ssd1327_frame_matches_the_grey_gallery);
    RUN_TEST(test_ssd1327_clipping_cases_stay_in_the_buffer);
    RUN_TEST(test_v1c_builds_only_the_gc9a01_slot);
    RUN_TEST(test_v1ab_builds_only_the_ssd1327_slot);
    RUN_TEST(test_sketch_without_screen_builds_neither);
    RUN_TEST(test_unknown_gives_e30_and_no_slot);
    RUN_TEST(test_v1c_geometry_before_begin);
    RUN_TEST(test_release_forced_after_geometry_keeps_one_slot);
    RUN_TEST(test_release_forced_after_begin_keeps_the_begun_slot);
    RUN_TEST(test_v1ab_geometry_before_begin);
    RUN_TEST(test_injected_backend_skips_detection);
    RUN_TEST(test_injection_frees_an_unbegun_slot);
    RUN_TEST(test_injected_failing_backend_gives_e31);
    RUN_TEST(test_two_displays_share_the_gc9a01_slot);
    RUN_TEST(test_v1ab_slot_clipping_cases);
    RUN_TEST(test_end_destroys_panel_slots_and_restarts);
    RUN_TEST(test_print_info_lists_the_screen_state);
    RUN_TEST(test_claim_stays_not_started_for_panels);
    RUN_TEST(test_sizeof_board_with_panel_slots);
    return UNITY_END();
}
