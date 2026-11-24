// SPDX-License-Identifier: GPL-3.0-or-later

#include <unity.h>

#include <vector>

#include "Arduino.h"
#include "GC9A01.h"
#include "SPI.h"
#include "driver_checks.h"

constexpr uint8_t PIN_DC = 10;
constexpr uint8_t PIN_RST = 11;
constexpr uint8_t PIN_CS = 12;

struct Transfer {
    bool data;   // D/C level: true = data, false = command
    bool csLow;  // chip selected during the byte
    bool dcOut;  // D/C configured as an output
    uint8_t value;
};

struct Command {
    uint8_t cmd;
    std::vector<uint8_t> params;
};

static SPIClass spi;
static GC9A01_SPI* display;
static std::vector<Transfer> transfers;

static void recordTransfer(uint8_t value) {
    transfers.push_back({digitalRead(PIN_DC) == HIGH, digitalRead(PIN_CS) == LOW,
                         gpioPinMode()[PIN_DC] == OUTPUT, value});
}

// Group the recorded bytes into commands followed by their parameters.
static std::vector<Command> commands() {
    std::vector<Command> out;
    for (const auto& t : transfers) {
        if (!t.data) {
            out.push_back({t.value, {}});
        } else if (!out.empty()) {
            out.back().params.push_back(t.value);
        }
    }
    return out;
}

static int indexOf(const std::vector<Command>& cmds, uint8_t cmd) {
    for (size_t i = 0; i < cmds.size(); i++) {
        if (cmds[i].cmd == cmd) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

static void assertFullFrame(const Command& ramwr, uint16_t color) {
    TEST_ASSERT_EQUAL_UINT8(GC9A01_CMD_RAMWR, ramwr.cmd);
    TEST_ASSERT_EQUAL_size_t(GC9A01::BUFFER_SIZE, ramwr.params.size());
    for (size_t i = 0; i < ramwr.params.size(); i += 2) {
        if (ramwr.params[i] != (color >> 8) || ramwr.params[i + 1] != (color & 0xFF)) {
            TEST_FAIL_MESSAGE("frame data differs from the fill colour");
        }
    }
}

void setUp(void) {
    gpioPinState().clear();
    gpioPinMode().clear();
    transfers.clear();
    spi.onTransfer = recordTransfer;
    spi.onBeginTransaction = nullptr;
    display = new GC9A01_SPI(spi, PIN_DC, PIN_RST, PIN_CS);
}

void tearDown(void) {
    delete display;
}

// --- colours ---

void test_gc9a01_color565_packs_rgb(void) {
    TEST_ASSERT_EQUAL_HEX16(0x0000, GC9A01::color565(0, 0, 0));
    TEST_ASSERT_EQUAL_HEX16(0xFFFF, GC9A01::color565(255, 255, 255));
    TEST_ASSERT_EQUAL_HEX16(0xF800, GC9A01::color565(255, 0, 0));
    TEST_ASSERT_EQUAL_HEX16(0x07E0, GC9A01::color565(0, 255, 0));
    TEST_ASSERT_EQUAL_HEX16(0x001F, GC9A01::color565(0, 0, 255));
    TEST_ASSERT_EQUAL_HEX16(0x8410, GC9A01::color565(128, 128, 128));
    TEST_ASSERT_EQUAL_HEX16(GC9A01_ORANGE, GC9A01::color565(255, 165, 0));
}

void test_gc9a01_color_constants(void) {
    TEST_ASSERT_EQUAL_HEX16(GC9A01::color565(255, 0, 0), GC9A01_RED);
    TEST_ASSERT_EQUAL_HEX16(GC9A01::color565(0, 255, 0), GC9A01_GREEN);
    TEST_ASSERT_EQUAL_HEX16(GC9A01::color565(0, 0, 255), GC9A01_BLUE);
    TEST_ASSERT_EQUAL_HEX16(GC9A01::color565(255, 255, 0), GC9A01_YELLOW);
    TEST_ASSERT_EQUAL_HEX16(GC9A01::color565(0, 255, 255), GC9A01_CYAN);
    TEST_ASSERT_EQUAL_HEX16(GC9A01::color565(255, 0, 255), GC9A01_MAGENTA);
}

// --- lifecycle and SPI protocol ---

void test_gc9a01_begin_returns_true(void) {
    check_begin(*display);
    TEST_ASSERT_EQUAL_UINT16(240, display->width());
    TEST_ASSERT_EQUAL_UINT16(240, display->height());
}

void test_gc9a01_begin_pulses_reset_and_releases_pins(void) {
    display->begin();
    TEST_ASSERT_EQUAL(OUTPUT, gpioPinMode()[PIN_RST]);
    TEST_ASSERT_EQUAL(HIGH, gpioPinState()[PIN_RST]);
    TEST_ASSERT_EQUAL(HIGH, gpioPinState()[PIN_CS]);
}

void test_gc9a01_begin_init_sequence(void) {
    display->begin();
    const auto cmds = commands();

    const int colmod = indexOf(cmds, GC9A01_CMD_COLMOD);
    TEST_ASSERT_TRUE(colmod >= 0);
    TEST_ASSERT_EQUAL_size_t(1, cmds[colmod].params.size());
    TEST_ASSERT_EQUAL_HEX8(GC9A01_COLMOD_RGB565, cmds[colmod].params[0]);

    const int madctl = indexOf(cmds, GC9A01_CMD_MADCTL);
    TEST_ASSERT_TRUE(madctl >= 0);
    TEST_ASSERT_EQUAL_HEX8(GC9A01_ORIENT_NORMAL, cmds[madctl].params[0]);

    // IPS panel: inversion on for true colours
    TEST_ASSERT_TRUE(indexOf(cmds, GC9A01_CMD_INVON) >= 0);
    TEST_ASSERT_EQUAL(-1, indexOf(cmds, GC9A01_CMD_INVOFF));

    const int slpout = indexOf(cmds, GC9A01_CMD_SLPOUT);
    const int dispon = indexOf(cmds, GC9A01_CMD_DISPON);
    TEST_ASSERT_TRUE(slpout >= 0);
    TEST_ASSERT_TRUE(dispon > slpout);

    // the first command unlocks the extended registers
    TEST_ASSERT_EQUAL_HEX8(GC9A01_CMD_INREGEN2, cmds[0].cmd);
}

void test_gc9a01_begin_clears_screen(void) {
    display->begin();
    const auto cmds = commands();
    assertFullFrame(cmds.back(), GC9A01_BLACK);
}

void test_gc9a01_every_byte_sent_with_cs_low_and_dc_output(void) {
    // Emulate the STM32 core: beginTransaction() gives the D/C pin (the
    // internal MISO) back to the SPI peripheral.
    spi.onBeginTransaction = []() { gpioPinMode()[PIN_DC] = INPUT; };
    display->begin();
    display->show();
    TEST_ASSERT_TRUE(transfers.size() > GC9A01::BUFFER_SIZE);
    for (const auto& t : transfers) {
        if (!t.csLow || !t.dcOut) {
            TEST_FAIL_MESSAGE("byte sent with CS high or D/C not an output");
        }
    }
    TEST_ASSERT_EQUAL(HIGH, gpioPinState()[PIN_CS]);
}

void test_gc9a01_spi_clock(void) {
    display->begin();
    TEST_ASSERT_EQUAL_UINT32(32000000, spi.lastClock);
}

void test_gc9a01_show_sets_full_window_then_streams_buffer(void) {
    display->begin();
    display->fill(0x1234);
    transfers.clear();
    display->show();
    const auto cmds = commands();
    TEST_ASSERT_EQUAL_size_t(3, cmds.size());

    TEST_ASSERT_EQUAL_HEX8(GC9A01_CMD_CASET, cmds[0].cmd);
    const uint8_t window[4] = {0x00, 0x00, 0x00, 0xEF};
    TEST_ASSERT_EQUAL_size_t(4, cmds[0].params.size());
    TEST_ASSERT_EQUAL_HEX8_ARRAY(window, cmds[0].params.data(), 4);
    TEST_ASSERT_EQUAL_HEX8(GC9A01_CMD_RASET, cmds[1].cmd);
    TEST_ASSERT_EQUAL_size_t(4, cmds[1].params.size());
    TEST_ASSERT_EQUAL_HEX8_ARRAY(window, cmds[1].params.data(), 4);

    assertFullFrame(cmds[2], 0x1234);
}

void test_gc9a01_show_before_begin_sends_nothing(void) {
    display->fill(GC9A01_RED);
    display->pixel(1, 1, GC9A01_RED);
    display->show();
    TEST_ASSERT_EQUAL_size_t(0, transfers.size());
    TEST_ASSERT_EQUAL_HEX16(0, display->pixel(1, 1));
}

void test_gc9a01_without_reset_pin_sends_software_reset(void) {
    GC9A01_SPI noReset(spi, PIN_DC, GC9A01_PIN_NONE, PIN_CS);
    noReset.begin();
    const auto cmds = commands();
    TEST_ASSERT_EQUAL_HEX8(GC9A01_CMD_SWRESET, cmds[0].cmd);
}

// --- frame buffer ---

void test_gc9a01_pixel_is_big_endian_on_the_wire(void) {
    display->begin();
    display->pixel(1, 0, 0xABCD);
    transfers.clear();
    display->show();
    const auto cmds = commands();
    const auto& data = cmds[2].params;
    TEST_ASSERT_EQUAL_HEX8(0x00, data[0]);
    TEST_ASSERT_EQUAL_HEX8(0x00, data[1]);
    TEST_ASSERT_EQUAL_HEX8(0xAB, data[2]);
    TEST_ASSERT_EQUAL_HEX8(0xCD, data[3]);
}

void test_gc9a01_pixel_row_major_offset(void) {
    display->begin();
    display->pixel(3, 2, 0x5AA5);
    transfers.clear();
    display->show();
    // keep the commands alive: params is a reference into them
    const auto cmds = commands();
    const auto& data = cmds[2].params;
    const size_t offset = (2 * 240 + 3) * 2;
    TEST_ASSERT_EQUAL_HEX8(0x5A, data[offset]);
    TEST_ASSERT_EQUAL_HEX8(0xA5, data[offset + 1]);
}

void test_gc9a01_pixel_read_back(void) {
    display->begin();
    display->pixel(239, 239, GC9A01_CYAN);
    TEST_ASSERT_EQUAL_HEX16(GC9A01_CYAN, display->pixel(239, 239));
    TEST_ASSERT_EQUAL_HEX16(GC9A01_BLACK, display->pixel(238, 239));
}

void test_gc9a01_pixel_out_of_bounds_ignored(void) {
    display->begin();
    display->pixel(-1, 0, GC9A01_WHITE);
    display->pixel(0, -1, GC9A01_WHITE);
    display->pixel(240, 0, GC9A01_WHITE);
    display->pixel(0, 240, GC9A01_WHITE);
    for (int16_t y = 0; y < 240; y++) {
        for (int16_t x = 0; x < 240; x++) {
            if (display->pixel(x, y) != GC9A01_BLACK) {
                TEST_FAIL_MESSAGE("out-of-bounds pixel wrote into the buffer");
            }
        }
    }
    TEST_ASSERT_EQUAL_HEX16(0, display->pixel(240, 0));
}

void test_gc9a01_fill(void) {
    display->begin();
    display->fill(GC9A01_ORANGE);
    TEST_ASSERT_EQUAL_HEX16(GC9A01_ORANGE, display->pixel(0, 0));
    TEST_ASSERT_EQUAL_HEX16(GC9A01_ORANGE, display->pixel(120, 120));
    TEST_ASSERT_EQUAL_HEX16(GC9A01_ORANGE, display->pixel(239, 239));
    display->fill(GC9A01_WHITE);
    TEST_ASSERT_EQUAL_HEX16(GC9A01_WHITE, display->pixel(17, 200));
}

void test_gc9a01_fill_rect(void) {
    display->begin();
    display->fillRect(10, 20, 3, 2, GC9A01_RED);
    TEST_ASSERT_EQUAL_HEX16(GC9A01_RED, display->pixel(10, 20));
    TEST_ASSERT_EQUAL_HEX16(GC9A01_RED, display->pixel(12, 21));
    TEST_ASSERT_EQUAL_HEX16(GC9A01_BLACK, display->pixel(13, 20));
    TEST_ASSERT_EQUAL_HEX16(GC9A01_BLACK, display->pixel(10, 22));
    TEST_ASSERT_EQUAL_HEX16(GC9A01_BLACK, display->pixel(9, 20));
    TEST_ASSERT_EQUAL_HEX16(GC9A01_BLACK, display->pixel(10, 19));
}

void test_gc9a01_fill_rect_is_clipped(void) {
    display->begin();
    display->fillRect(230, 230, 50, 50, GC9A01_BLUE);
    TEST_ASSERT_EQUAL_HEX16(GC9A01_BLUE, display->pixel(239, 239));
    TEST_ASSERT_EQUAL_HEX16(GC9A01_BLUE, display->pixel(230, 230));
    TEST_ASSERT_EQUAL_HEX16(GC9A01_BLACK, display->pixel(229, 239));

    display->fillRect(-5, -5, 10, 10, GC9A01_GREEN);
    TEST_ASSERT_EQUAL_HEX16(GC9A01_GREEN, display->pixel(0, 0));
    TEST_ASSERT_EQUAL_HEX16(GC9A01_GREEN, display->pixel(4, 4));
    TEST_ASSERT_EQUAL_HEX16(GC9A01_BLACK, display->pixel(5, 5));

    // empty or fully outside rectangles draw nothing
    display->fillRect(50, 50, 0, 10, GC9A01_WHITE);
    display->fillRect(50, 50, -3, 10, GC9A01_WHITE);
    display->fillRect(300, 10, 10, 10, GC9A01_WHITE);
    TEST_ASSERT_EQUAL_HEX16(GC9A01_BLACK, display->pixel(50, 50));
}

void test_gc9a01_line_horizontal_vertical_diagonal(void) {
    display->begin();
    display->line(5, 5, 9, 5, GC9A01_WHITE);
    for (int16_t x = 5; x <= 9; x++) {
        TEST_ASSERT_EQUAL_HEX16(GC9A01_WHITE, display->pixel(x, 5));
    }
    TEST_ASSERT_EQUAL_HEX16(GC9A01_BLACK, display->pixel(10, 5));

    display->line(20, 30, 20, 25, GC9A01_RED);
    for (int16_t y = 25; y <= 30; y++) {
        TEST_ASSERT_EQUAL_HEX16(GC9A01_RED, display->pixel(20, y));
    }

    display->line(100, 100, 110, 110, GC9A01_GREEN);
    for (int16_t i = 0; i <= 10; i++) {
        TEST_ASSERT_EQUAL_HEX16(GC9A01_GREEN, display->pixel(100 + i, 100 + i));
    }
}

void test_gc9a01_line_clipped_outside_screen(void) {
    display->begin();
    display->line(-10, 120, 250, 120, GC9A01_YELLOW);
    TEST_ASSERT_EQUAL_HEX16(GC9A01_YELLOW, display->pixel(0, 120));
    TEST_ASSERT_EQUAL_HEX16(GC9A01_YELLOW, display->pixel(239, 120));
}

void test_gc9a01_text_glyph_placement(void) {
    display->begin();
    // 'A' = {0x7E, 0x11, 0x11, 0x11, 0x7E}, bit 0 is the top row
    display->text("A", 10, 20, GC9A01_WHITE);
    TEST_ASSERT_EQUAL_HEX16(GC9A01_BLACK, display->pixel(10, 20));
    TEST_ASSERT_EQUAL_HEX16(GC9A01_WHITE, display->pixel(10, 21));
    TEST_ASSERT_EQUAL_HEX16(GC9A01_WHITE, display->pixel(10, 26));
    TEST_ASSERT_EQUAL_HEX16(GC9A01_WHITE, display->pixel(11, 20));
    TEST_ASSERT_EQUAL_HEX16(GC9A01_WHITE, display->pixel(12, 24));
    TEST_ASSERT_EQUAL_HEX16(GC9A01_BLACK, display->pixel(12, 25));
    TEST_ASSERT_EQUAL_HEX16(GC9A01_WHITE, display->pixel(14, 21));
    // 6-pixel advance: column 15 is the spacing column
    for (int16_t y = 20; y < 27; y++) {
        TEST_ASSERT_EQUAL_HEX16(GC9A01_BLACK, display->pixel(15, y));
    }
}

void test_gc9a01_text_advances_six_pixels(void) {
    display->begin();
    display->text("AA", 0, 0, GC9A01_RED);
    TEST_ASSERT_EQUAL_HEX16(GC9A01_RED, display->pixel(1, 0));
    TEST_ASSERT_EQUAL_HEX16(GC9A01_RED, display->pixel(7, 0));
    TEST_ASSERT_EQUAL_HEX16(GC9A01_RED, display->pixel(6, 1));
}

void test_gc9a01_text_clipped_at_edges(void) {
    display->begin();
    // glyph columns 0 and 1 are off screen, column 2 (0x11) lands on x = 0
    display->text("A", -2, 0, GC9A01_RED);
    TEST_ASSERT_EQUAL_HEX16(GC9A01_RED, display->pixel(0, 0));
    TEST_ASSERT_EQUAL_HEX16(GC9A01_RED, display->pixel(0, 4));
    TEST_ASSERT_EQUAL_HEX16(GC9A01_BLACK, display->pixel(0, 1));
    display->text("AB", 236, 235, GC9A01_RED);
    TEST_ASSERT_EQUAL_HEX16(GC9A01_RED, display->pixel(236, 236));
}

void test_gc9a01_scroll_moves_content_and_clears(void) {
    display->begin();
    display->pixel(10, 10, GC9A01_MAGENTA);
    display->scroll(5, 3);
    TEST_ASSERT_EQUAL_HEX16(GC9A01_MAGENTA, display->pixel(15, 13));
    TEST_ASSERT_EQUAL_HEX16(GC9A01_BLACK, display->pixel(10, 10));

    display->scroll(-15, -13);
    TEST_ASSERT_EQUAL_HEX16(GC9A01_MAGENTA, display->pixel(0, 0));

    display->fill(GC9A01_WHITE);
    display->scroll(4, 0);
    TEST_ASSERT_EQUAL_HEX16(GC9A01_BLACK, display->pixel(3, 100));
    TEST_ASSERT_EQUAL_HEX16(GC9A01_WHITE, display->pixel(4, 100));
    display->scroll(0, -2);
    TEST_ASSERT_EQUAL_HEX16(GC9A01_BLACK, display->pixel(100, 238));
    TEST_ASSERT_EQUAL_HEX16(GC9A01_WHITE, display->pixel(100, 237));
}

void test_gc9a01_scroll_out_of_range_clears(void) {
    display->begin();
    display->fill(GC9A01_WHITE);
    display->scroll(240, 0);
    TEST_ASSERT_EQUAL_HEX16(GC9A01_BLACK, display->pixel(120, 120));
    TEST_ASSERT_EQUAL_HEX16(GC9A01_BLACK, display->pixel(239, 0));
}

// --- display settings ---

void test_gc9a01_rotate_sets_madctl_and_resends_frame(void) {
    display->begin();
    display->fill(GC9A01_GREEN);
    transfers.clear();
    display->rotate(true);
    auto cmds = commands();
    TEST_ASSERT_EQUAL_HEX8(GC9A01_CMD_MADCTL, cmds[0].cmd);
    TEST_ASSERT_EQUAL_HEX8(GC9A01_ORIENT_ROTATED, cmds[0].params[0]);
    TEST_ASSERT_EQUAL_HEX8(0xC8, GC9A01_ORIENT_ROTATED);
    assertFullFrame(cmds.back(), GC9A01_GREEN);

    // the frame buffer itself is not remapped
    display->pixel(0, 0, GC9A01_RED);
    TEST_ASSERT_EQUAL_HEX16(GC9A01_RED, display->pixel(0, 0));

    transfers.clear();
    display->rotate(false);
    cmds = commands();
    TEST_ASSERT_EQUAL_HEX8(GC9A01_ORIENT_NORMAL, cmds[0].params[0]);
    TEST_ASSERT_EQUAL_HEX8(0x08, GC9A01_ORIENT_NORMAL);
}

void test_gc9a01_rotation_kept_across_begin(void) {
    display->begin();
    display->rotate(true);
    transfers.clear();
    display->begin();
    const auto cmds = commands();
    const int madctl = indexOf(cmds, GC9A01_CMD_MADCTL);
    TEST_ASSERT_TRUE(madctl >= 0);
    TEST_ASSERT_EQUAL_HEX8(GC9A01_ORIENT_ROTATED, cmds[madctl].params[0]);
}

void test_gc9a01_invert_toggles_controller_inversion(void) {
    display->begin();
    transfers.clear();
    display->invert(true);
    TEST_ASSERT_EQUAL_size_t(1, transfers.size());
    TEST_ASSERT_FALSE(transfers[0].data);
    TEST_ASSERT_EQUAL_HEX8(GC9A01_CMD_INVOFF, transfers[0].value);

    transfers.clear();
    display->invert(false);
    TEST_ASSERT_EQUAL_HEX8(GC9A01_CMD_INVON, transfers[0].value);
}

void test_gc9a01_power_off_then_on(void) {
    display->begin();
    transfers.clear();
    display->powerOff();
    auto cmds = commands();
    TEST_ASSERT_EQUAL_size_t(2, cmds.size());
    TEST_ASSERT_EQUAL_HEX8(GC9A01_CMD_DISPOFF, cmds[0].cmd);
    TEST_ASSERT_EQUAL_HEX8(GC9A01_CMD_SLPIN, cmds[1].cmd);

    transfers.clear();
    const uint32_t before = millis();
    display->powerOn();
    cmds = commands();
    TEST_ASSERT_EQUAL_size_t(2, cmds.size());
    TEST_ASSERT_EQUAL_HEX8(GC9A01_CMD_SLPOUT, cmds[0].cmd);
    TEST_ASSERT_EQUAL_HEX8(GC9A01_CMD_DISPON, cmds[1].cmd);
    TEST_ASSERT_TRUE(millis() - before >= 120);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_gc9a01_color565_packs_rgb);
    RUN_TEST(test_gc9a01_color_constants);
    RUN_TEST(test_gc9a01_begin_returns_true);
    RUN_TEST(test_gc9a01_begin_pulses_reset_and_releases_pins);
    RUN_TEST(test_gc9a01_begin_init_sequence);
    RUN_TEST(test_gc9a01_begin_clears_screen);
    RUN_TEST(test_gc9a01_every_byte_sent_with_cs_low_and_dc_output);
    RUN_TEST(test_gc9a01_spi_clock);
    RUN_TEST(test_gc9a01_show_sets_full_window_then_streams_buffer);
    RUN_TEST(test_gc9a01_show_before_begin_sends_nothing);
    RUN_TEST(test_gc9a01_without_reset_pin_sends_software_reset);
    RUN_TEST(test_gc9a01_pixel_is_big_endian_on_the_wire);
    RUN_TEST(test_gc9a01_pixel_row_major_offset);
    RUN_TEST(test_gc9a01_pixel_read_back);
    RUN_TEST(test_gc9a01_pixel_out_of_bounds_ignored);
    RUN_TEST(test_gc9a01_fill);
    RUN_TEST(test_gc9a01_fill_rect);
    RUN_TEST(test_gc9a01_fill_rect_is_clipped);
    RUN_TEST(test_gc9a01_line_horizontal_vertical_diagonal);
    RUN_TEST(test_gc9a01_line_clipped_outside_screen);
    RUN_TEST(test_gc9a01_text_glyph_placement);
    RUN_TEST(test_gc9a01_text_advances_six_pixels);
    RUN_TEST(test_gc9a01_text_clipped_at_edges);
    RUN_TEST(test_gc9a01_scroll_moves_content_and_clears);
    RUN_TEST(test_gc9a01_scroll_out_of_range_clears);
    RUN_TEST(test_gc9a01_rotate_sets_madctl_and_resends_frame);
    RUN_TEST(test_gc9a01_rotation_kept_across_begin);
    RUN_TEST(test_gc9a01_invert_toggles_controller_inversion);
    RUN_TEST(test_gc9a01_power_off_then_on);
    return UNITY_END();
}
