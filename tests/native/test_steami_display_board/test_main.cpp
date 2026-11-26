// SPDX-License-Identifier: GPL-3.0-or-later

#include <Arduino.h>
#include <SPI.h>
#include <SteamiGC9A01.h>
#include <Wire.h>
#include <stddef.h>
#include <stdint.h>
#include <unity.h>

#include <cstddef>
#include <initializer_list>
#include <string>
#include <vector>

#include "SteamiBoard.h"
#include "SteamiDisplay.h"
#include "SteamiGallery.h"
#include "SteamiGc9a01Backend.h"
#include "SteamiMemoryBackend.h"

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
    return std::vector<uint8_t>(all.end() - static_cast<std::ptrdiff_t>(n), all.end());
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

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_gc9a01_backend_identity_and_quiet_constructor);
    RUN_TEST(test_gc9a01_backend_begin_initialises_the_panel);
    RUN_TEST(test_gc9a01_backend_passes_rgb565_through);
    RUN_TEST(test_gc9a01_backend_controls_forward);
    RUN_TEST(test_gc9a01_frame_matches_the_memory_gallery);
    return UNITY_END();
}
