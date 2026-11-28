// SPDX-License-Identifier: GPL-3.0-or-later
//
// STeaMi Board - use the crocodile pads P24, P25, P28 and P29 of the MCP23009E
// expander (115200 baud).
//
// These four pads are GPIO 0 to 3 of the expander, not pins of the MCU: they are
// driven through the shared expander driver, Steami.mcp23009e(), with the
// STEAMI_PAD_* numbers. Works on every release (V1a, V1b and V1c).
//
// Wiring: an LED with a 220 ohm resistor from P28 to GND, and a crocodile wire
// that touches P24 to GND. The LED lights and the buzzer beeps while P24 is
// grounded.

#include <Arduino.h>
#include <SteamiBoard.h>

SteamiMCP23009E* expander = nullptr;
int last = -1;

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000) {
        // Serial is ready as soon as Serial.begin() returns on the STeaMi.
    }

    expander = Steami.mcp23009e();
    if (expander == nullptr) {
        Serial.println("The MCP23009E expander (I2C 0x20) did not answer.");
        return;
    }
    expander->setup(STEAMI_PAD_P24, MCP23009_DIR_INPUT, MCP23009_PULLUP);
    expander->setup(STEAMI_PAD_P28, MCP23009_DIR_OUTPUT);
    expander->setLevel(STEAMI_PAD_P28, MCP23009_LOGIC_LOW);
}

void loop() {
    if (expander == nullptr) {
        return;
    }
    int grounded = expander->getLevel(STEAMI_PAD_P24) == MCP23009_LOGIC_LOW ? 1 : 0;
    if (grounded != last) {
        expander->setLevel(STEAMI_PAD_P28, grounded ? MCP23009_LOGIC_HIGH : MCP23009_LOGIC_LOW);
        if (grounded) {
            tone(SPEAKER, 880, 100);
            Serial.println("P24 grounded: LED on");
        } else {
            Serial.println("P24 open: LED off");
        }
        last = grounded;
    }
    delay(20);
}
