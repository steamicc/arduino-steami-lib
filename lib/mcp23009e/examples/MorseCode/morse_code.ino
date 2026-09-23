// SPDX-License-Identifier: GPL-3.0-or-later
//
// MorseDecoder — enter Morse code using the UP button on the D-PAD.
// A short UP press (< 300 ms) inputs a dot.
// A long UP press (>= 300 ms) inputs a dash.
// Press DOWN to decode the current Morse sequence.

#include <Arduino.h>
#include <MCP23009E.h>
#include <Wire.h>

#include <map>

constexpr uint32_t kDotThresholdMs = 300;
constexpr uint32_t kDebounceDelayMs = 50;

static const std::map<String, char> kMorseTable = {
    {".-", 'A'},   {"-...", 'B'}, {"-.-.", 'C'}, {"-..", 'D'},  {".", 'E'},    {"..-.", 'F'},
    {"--.", 'G'},  {"....", 'H'}, {"..", 'I'},   {".---", 'J'}, {"-.-", 'K'},  {".-..", 'L'},
    {"--", 'M'},   {"-.", 'N'},   {"---", 'O'},  {".--.", 'P'}, {"--.-", 'Q'}, {".-.", 'R'},
    {"...", 'S'},  {"-", 'T'},    {"..-", 'U'},  {"...-", 'V'}, {".--", 'W'},  {"-..-", 'X'},
    {"-.--", 'Y'}, {"--..", 'Z'},
};

String code = "";

bool upWasPressed = false;
uint32_t upPressStart = 0;

TwoWire internalI2C(I2C_INT_SDA, I2C_INT_SCL);
MCP23009E expander(internalI2C, RST_EXPANDER, MCP23009_I2C_ADDR, INT_EXPANDER);

void decodeMorse() {
    if (code == "") {
        Serial.println("No Morse code to decode.");
        return;
    }

    auto it = kMorseTable.find(code);
    if (it != kMorseTable.end()) {
        Serial.print("Decoded: ");
        Serial.println(it->second);
    } else {
        Serial.print("Unknown code: ");
        Serial.println(code);
    }

    code = "";
}

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000) {
        // Wait up to 2 s for the serial monitor.
    }

    internalI2C.begin();
    expander.begin();

    expander.setup(MCP23009_BTN_UP, MCP23009_DIR_INPUT, MCP23009_PULLUP);
    expander.setup(MCP23009_BTN_DOWN, MCP23009_DIR_INPUT, MCP23009_PULLUP);

    Serial.println("MorseDecoder");
    Serial.println("UP short press: dot");
    Serial.println("UP long press: dash");
    Serial.println("DOWN: decode current code");
}

void loop() {
    bool upPressed = expander.getLevel(MCP23009_BTN_UP) == MCP23009_LOGIC_LOW;
    bool downPressed = expander.getLevel(MCP23009_BTN_DOWN) == MCP23009_LOGIC_LOW;

    if (upPressed && !upWasPressed) {
        upPressStart = millis();
        upWasPressed = true;
    }

    if (!upPressed && upWasPressed) {
        uint32_t pressDuration = millis() - upPressStart;

        if (pressDuration < kDotThresholdMs) {
            code += ".";
            Serial.println("Dot");
        } else {
            code += "-";
            Serial.println("Dash");
        }

        Serial.print("Current code: ");
        Serial.println(code);

        upWasPressed = false;
        delay(kDebounceDelayMs);
    }

    if (downPressed) {
        decodeMorse();

        while (expander.getLevel(MCP23009_BTN_DOWN) == MCP23009_LOGIC_LOW) {
            delay(10);
        }

        delay(kDebounceDelayMs);
    }
}
