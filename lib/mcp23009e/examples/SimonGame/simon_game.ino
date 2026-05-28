// SPDX-License-Identifier: GPL-3.0-or-later
//
// SimonSays — memory game using the MCP23009E D-PAD and the on-board LEDs.
// Watch the LED sequence, then replay it on the D-PAD. The sequence grows
// by one direction every round.
//
// The MCP23009E sits on the STeaMi internal I2C bus, so spin up a
// dedicated TwoWire pointed at the variant pin macros and hand it to the
// driver. Open the serial monitor at 115200 baud to see your score.

#include <Arduino.h>
#include <MCP23009E.h>
#include <Wire.h>

#include <map>
#include <vector>

TwoWire internalI2C(I2C_INT_SDA, I2C_INT_SCL);
MCP23009E expander(internalI2C, RST_EXPANDER, MCP23009_I2C_ADDR, INT_EXPANDER);

static const std::map<uint8_t, String> kButtons = {
    {MCP23009_BTN_UP, "UP"},
    {MCP23009_BTN_DOWN, "DOWN"},
    {MCP23009_BTN_LEFT, "LEFT"},
    {MCP23009_BTN_RIGHT, "RIGHT"},
};

void wait_all_released() {
    while (true) {
        bool released = true;
        for (const auto& [pinNumber, name] : kButtons) {
            if (expander.getLevel(pinNumber) == MCP23009_LOGIC_LOW) {
                released = false;
                break;
            }
        }
        if (released) {
            break;
        }
        delay(20);
    }
}

String wait_for_button() {
    wait_all_released();
    while (true) {
        for (const auto& [pinNumber, name] : kButtons) {
            if (expander.getLevel(pinNumber) == MCP23009_LOGIC_LOW) {
                while (expander.getLevel(pinNumber) == MCP23009_LOGIC_LOW) {
                    delay(20);
                }
                return name;
            }
        }
        delay(20);
    }
}

void showDirection(const String& name) {
    digitalWrite(LED_RED, LOW);
    digitalWrite(LED_GREEN, LOW);
    digitalWrite(LED_BLUE, LOW);

    if (name == "UP") {
        digitalWrite(LED_RED, HIGH);
    } else if (name == "DOWN") {
        digitalWrite(LED_GREEN, HIGH);
    } else if (name == "RIGHT") {
        digitalWrite(LED_BLUE, HIGH);
    } else if (name == "LEFT") {
        digitalWrite(LED_RED, HIGH);
        digitalWrite(LED_GREEN, HIGH);
        digitalWrite(LED_BLUE, HIGH);
    }

    delay(500);
    digitalWrite(LED_RED, LOW);
    digitalWrite(LED_GREEN, LOW);
    digitalWrite(LED_BLUE, LOW);
}

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000) {
    }

    internalI2C.begin();
    expander.begin();

    for (const auto& [pinNumber, name] : kButtons) {
        expander.setup(pinNumber, MCP23009_DIR_INPUT, MCP23009_PULLUP);
    }

    pinMode(LED_RED, OUTPUT);
    pinMode(LED_GREEN, OUTPUT);
    pinMode(LED_BLUE, OUTPUT);

    randomSeed(analogRead(0));

    Serial.println("SimonGame — watch the LED sequence, then repeat it on the D-PAD.");
}

void loop() {
    int score = 0;
    std::vector<String> sequence;

    while (true) {
        auto it = kButtons.begin();
        std::advance(it, random(kButtons.size()));
        String nextName = it->second;
        sequence.push_back(nextName);

        for (const auto& name : sequence) {
            showDirection(name);
            delay(200);
        }

        bool success = true;
        for (const auto& expected : sequence) {
            String received = wait_for_button();
            showDirection(received);
            if (received != expected) {
                success = false;
                break;
            }
        }

        if (!success) {
            Serial.println("Game over! Your score:");
            Serial.println(score);
            sequence.clear();
            score = 0;
        } else {
            score++;
            Serial.println("Correct! Current score:");
            Serial.println(score);
        }
        delay(800);
    }
}