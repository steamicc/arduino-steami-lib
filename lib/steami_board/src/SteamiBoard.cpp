// SPDX-License-Identifier: GPL-3.0-or-later

#include "SteamiBoard.h"

#include <Arduino.h>
#include <stdio.h>
#include <string.h>

#ifdef ARDUINO
void steamiSerialLog(const char* line) {
    if (Serial) {
        Serial.println(line);
    }
}
#endif

SteamiBoard Steami;

const char* steamiChipName(SteamiChip c) {
    switch (c) {
        case SteamiChip::BME280:
            return "BME280";
        case SteamiChip::WSEN_HIDS:
            return "WSEN-HIDS";
        case SteamiChip::WSEN_PADS:
            return "WSEN-PADS";
        case SteamiChip::APDS9960:
            return "APDS-9960";
        case SteamiChip::ISM330DL:
            return "ISM330DL";
        case SteamiChip::LIS2MDL:
            return "LIS2MDL";
        case SteamiChip::VL53L1X:
            return "VL53L1X";
        case SteamiChip::BQ27441:
            return "BQ27441";
        case SteamiChip::MCP23009E:
            return "MCP23009E";
        case SteamiChip::SSD1327:
            return "SSD1327";
        case SteamiChip::GC9A01:
            return "GC9A01";
        default:
            return "";
    }
}

uint8_t steamiChipAddress(SteamiChip c) {
    switch (c) {
        case SteamiChip::BME280:
            return 0x76;
        case SteamiChip::WSEN_HIDS:
            return 0x5F;
        case SteamiChip::WSEN_PADS:
            return 0x5D;
        case SteamiChip::APDS9960:
            return 0x39;
        case SteamiChip::ISM330DL:
            return 0x6B;
        case SteamiChip::LIS2MDL:
            return 0x1E;
        case SteamiChip::VL53L1X:
            return 0x29;
        case SteamiChip::BQ27441:
            return 0x55;
        case SteamiChip::MCP23009E:
            return 0x20;
        default:
            return 0;
    }
}

SteamiBoardPins SteamiBoardPins::steami() {
    return {static_cast<uint32_t>(I2C_INT_SDA),  static_cast<uint32_t>(I2C_INT_SCL),
            static_cast<uint32_t>(RST_EXPANDER), static_cast<uint32_t>(INT_EXPANDER),
            static_cast<uint32_t>(A_BUTTON),     static_cast<uint32_t>(B_BUTTON),
            static_cast<uint32_t>(MENU_BUTTON)};
}

void SteamiBoard::setLogger(void (*log)(const char* line)) {
    _log = log;
}

void SteamiBoard::report(const char* who, SteamiStatus /* s */, const char* detail, uint8_t code) {
    snprintf(_lastError, sizeof(_lastError), "[STeaMi] %s: %s (E%02u)", who, detail,
             static_cast<unsigned>(code));

    for (size_t i = 0; i < _reportedCount; i++) {
        if (_reported[i].code == code && strcmp(_reported[i].who, who) == 0) {
            return;
        }
    }
    if (_reportedCount < MAX_REPORTED) {
        _reported[_reportedCount] = {who, code};
        _reportedCount++;
    }
    if (_log != nullptr) {
        _log(_lastError);
    }
}

const SteamiBoardPins& SteamiBoard::pins() {
    if (!_pinsReady) {
        _pins = SteamiBoardPins::steami();
        _pinsReady = true;
    }
    return _pins;
}

TwoWire& SteamiBoard::wire() {
    if (_wire == nullptr) {
#ifdef ARDUINO
        static TwoWire internal(I2C_INT_SDA, I2C_INT_SCL);
        _wire = &internal;
#else
        _wire = &Wire;
#endif
    }
    if (!_begun) {
        _wire->begin();
        _begun = true;
    }
    return *_wire;
}

void SteamiBoard::end() {
    // Tasks 4 and 5 reset their slots here.
    _lastError[0] = '\0';
    _reportedCount = 0;
    _begun = false;
}
