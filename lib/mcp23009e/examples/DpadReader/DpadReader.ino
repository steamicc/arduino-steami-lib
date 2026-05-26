// SPDX-License-Identifier: GPL-3.0-or-later
//
// DpadReader — print which D-PAD button is currently pressed.
//
// On the STeaMi board the four directional buttons of the D-PAD are wired
// to the MCP23009E I/O expander on the internal I2C bus, with pull-ups
// enabled so an unpressed button reads HIGH and a pressed one reads LOW.
//
// Wiring: no external hookup needed. Flash this sketch and open the
// serial monitor at 115200 baud.

#include <Arduino.h>
#include <MCP23009E.h>
#include <Wire.h>

TwoWire internalI2C(I2C_INT_SDA, I2C_INT_SCL);
MCP23009E expander(internalI2C, RST_EXPANDER, MCP23009_I2C_ADDR, INT_EXPANDER);

static const uint8_t kButtons[] = {MCP23009_BTN_UP, MCP23009_BTN_DOWN, MCP23009_BTN_LEFT,
                                   MCP23009_BTN_RIGHT};
static const char* kButtonNames[] = {"UP", "DOWN", "LEFT", "RIGHT"};

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000) {
        // Wait up to 2 s for a connected monitor — on the STeaMi USB CDC
        // !Serial stays true until the host enumerates.
    }

    internalI2C.begin();
    expander.begin();

    // Configure each D-PAD pin as input with an internal pull-up.
    // Buttons pull the line LOW when pressed.
    for (uint8_t i = 0; i < sizeof(kButtons); ++i) {
        expander.setup(kButtons[i], MCP23009_DIR_INPUT, MCP23009_PULLUP);
    }

    Serial.println("DpadReader — press a D-PAD button.");
}

void loop() {
    for (uint8_t i = 0; i < sizeof(kButtons); ++i) {
        if (expander.getLevel(kButtons[i]) == MCP23009_LOGIC_LOW) {
            Serial.print("Pressed: ");
            Serial.println(kButtonNames[i]);
        }
    }
    delay(100);
}
