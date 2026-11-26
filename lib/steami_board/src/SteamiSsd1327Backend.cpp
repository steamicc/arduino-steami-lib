// SPDX-License-Identifier: GPL-3.0-or-later

#include "SteamiSsd1327Backend.h"

#include "SteamiRgb.h"

uint8_t SteamiSsd1327Backend::gray(uint16_t rgb565) {
    if (rgb565 != _lastColor) {
        _lastColor = rgb565;
        _lastGray = SteamiRgb::fromRgb565(rgb565).gray4();
    }
    return _lastGray;
}

#ifdef ARDUINO

SteamiSsd1327Backend::SteamiSsd1327Backend() = default;

bool SteamiSsd1327Backend::begin() {
    return _oled.begin();  // SteamiSSD1327 begins its SPI bus first
}

#else

namespace {

/// The internal SPI bus, as SteamiSSD1327 builds it on target.
SPIClass& panelSpi() {
    static SPIClass spi(SPI_INT_MOSI, SPI_INT_MISO, SPI_INT_SCK);
    return spi;
}

}  // namespace

SteamiSsd1327Backend::SteamiSsd1327Backend()
    : _oled(128, 128, panelSpi(), SPI_INT_MISO, RST_DISPLAY, CS_DISPLAY) {}

bool SteamiSsd1327Backend::begin() {
    panelSpi().begin();
    return _oled.begin();
}

#endif
