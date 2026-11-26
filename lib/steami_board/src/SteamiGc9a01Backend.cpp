// SPDX-License-Identifier: GPL-3.0-or-later

#include "SteamiGc9a01Backend.h"

#ifdef ARDUINO

SteamiGc9a01Backend::SteamiGc9a01Backend() = default;

bool SteamiGc9a01Backend::begin() {
    return _tft.begin();  // SteamiGC9A01 begins its SPI bus first
}

#else

namespace {

/// The internal SPI bus, as SteamiGC9A01 builds it on target.
SPIClass& panelSpi() {
    static SPIClass spi(SPI_INT_MOSI, SPI_INT_MISO, SPI_INT_SCK);
    return spi;
}

}  // namespace

SteamiGc9a01Backend::SteamiGc9a01Backend()
    : _tft(panelSpi(), SPI_INT_MISO, RST_DISPLAY, CS_DISPLAY) {}

bool SteamiGc9a01Backend::begin() {
    panelSpi().begin();
    return _tft.begin();
}

#endif
