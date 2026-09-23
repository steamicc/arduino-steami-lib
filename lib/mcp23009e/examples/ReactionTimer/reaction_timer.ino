// SPDX-License-Identifier: GPL-3.0-or-later
//
// ReactionTime — measure how fast you press the D-PAD button after the
// red LED lights up. The LED turns on after a random delay between 300
// and 1500 ms; your reaction time is printed to the serial monitor.
//
// The MCP23009E sits on the STeaMi internal I2C bus, so spin up a
// dedicated TwoWire pointed at the variant pin macros and hand it to the
// driver. Open the serial monitor at 115200 baud to see your results.

#include <Arduino.h>
#include <MCP23009E.h>
#include <Wire.h>

TwoWire internalI2C(I2C_INT_SDA, I2C_INT_SCL);
MCP23009E expander(internalI2C, RST_EXPANDER, MCP23009_I2C_ADDR, INT_EXPANDER);

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000) {
    }

    internalI2C.begin();
    expander.begin();

    expander.setup(MCP23009_BTN_UP, MCP23009_DIR_INPUT, MCP23009_PULLUP);

    pinMode(LED_RED, OUTPUT);
    randomSeed(analogRead(0));
}

void loop() {
    while (expander.getLevel(MCP23009_BTN_UP) == MCP23009_LOGIC_LOW) {
        delay(10);
    }

    delay(random(300, 1500));
    digitalWrite(LED_RED, HIGH);
    uint32_t startTime = millis();
    while (true) {
        if (expander.getLevel(MCP23009_BTN_UP) == MCP23009_LOGIC_LOW) {
            uint32_t reactionTime = millis() - startTime;
            digitalWrite(LED_RED, LOW);
            Serial.println("Reaction time: " + String(reactionTime) + " ms");
            break;
        }
    }
    delay(2000);
}