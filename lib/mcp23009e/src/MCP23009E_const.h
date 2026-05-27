// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stdint.h>

// Registres MCP23009E
constexpr uint8_t MCP23009_IODIR = 0x00;    // Registre de direction I/O (1=entrée, 0=sortie)
constexpr uint8_t MCP23009_IPOL = 0x01;     // Registre de polarité d'entrée
constexpr uint8_t MCP23009_GPINTEN = 0x02;  // Registre d'activation d'interruption
constexpr uint8_t MCP23009_DEFVAL = 0x03;   // Valeur par défaut pour comparaison d'interruption
constexpr uint8_t MCP23009_INTCON = 0x04;   // Registre de configuration d'interruption
constexpr uint8_t MCP23009_IOCON = 0x05;    // Registre de configuration I/O
constexpr uint8_t MCP23009_GPPU = 0x06;     // Registre de pull-up GPIO
constexpr uint8_t MCP23009_INTF = 0x07;     // Registre de flag d'interruption
constexpr uint8_t MCP23009_INTCAP = 0x08;   // Registre de capture d'interruption
constexpr uint8_t MCP23009_GPIO = 0x09;     // Registre GPIO (lecture/écriture)
constexpr uint8_t MCP23009_OLAT = 0x0A;     // Registre de latch de sortie

// Valeurs pour IOCON
constexpr uint8_t MCP23009_IOCON_SEQOP = 0x20;   // Mode d'opération séquentielle
constexpr uint8_t MCP23009_IOCON_DISSLW = 0x10;  // Désactive le slew rate
constexpr uint8_t MCP23009_IOCON_ODR = 0x04;     // Configure INT comme drain ouvert
constexpr uint8_t MCP23009_IOCON_INTPOL = 0x02;  // Polarité de la sortie INT
constexpr uint8_t MCP23009_IOCON_INTCC = 0x01;   // Mode de capture d'interruption

// Adresse I2C mise à jour pour 0x20
constexpr uint8_t MCP23009_I2C_ADDR = 0x20;

// Énumérations pour la configuration des GPIO
// Direction
constexpr uint8_t MCP23009_DIR_OUTPUT = 0;
constexpr uint8_t MCP23009_DIR_INPUT = 1;

// Pull-up
constexpr uint8_t MCP23009_NO_PULLUP = 0;
constexpr uint8_t MCP23009_PULLUP = 1;

// Polarité
constexpr uint8_t MCP23009_POL_SAME = 0;
constexpr uint8_t MCP23009_POL_INVERTED = 1;

// Niveau logique
constexpr uint8_t MCP23009_LOGIC_LOW = 0;
constexpr uint8_t MCP23009_LOGIC_HIGH = 1;

// Interruptions
constexpr uint8_t MCP23009_INTEN_DISABLE = 0;
constexpr uint8_t MCP23009_INTEN_ENABLE = 1;

// Comparaison interruption
constexpr uint8_t MCP23009_INTCON_PREVIOUS_STATE = 0;
constexpr uint8_t MCP23009_INTCON_DEFVAL = 1;

// GPIO mapping for the D-PAD
constexpr uint8_t MCP23009_BTN_UP = 7;
constexpr uint8_t MCP23009_BTN_DOWN = 5;
constexpr uint8_t MCP23009_BTN_LEFT = 6;
constexpr uint8_t MCP23009_BTN_RIGHT = 4;

// GPIO mapping for the croc connectors
constexpr uint8_t MCP23009_GPIO1 = 0;
constexpr uint8_t MCP23009_GPIO2 = 1;
constexpr uint8_t MCP23009_GPIO3 = 2;
constexpr uint8_t MCP23009_GPIO4 = 3;
constexpr uint8_t MCP23009_GPIO5 = 4;
constexpr uint8_t MCP23009_GPIO6 = 5;
constexpr uint8_t MCP23009_GPIO7 = 6;
constexpr uint8_t MCP23009_GPIO8 = 7;

constexpr uint8_t MCP23009_GPIOS[] = {
    MCP23009_GPIO1, MCP23009_GPIO2, MCP23009_GPIO3, MCP23009_GPIO4,
    MCP23009_GPIO5, MCP23009_GPIO6, MCP23009_GPIO7, MCP23009_GPIO8,
};