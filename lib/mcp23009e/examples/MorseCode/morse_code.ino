// SPDX-License-Identifier: GPL-3.0-or-later
//
// MorseDecoder — enter Morse code using the UP button on the D-PAD and
// decode it to a letter printed on the serial monitor. A short press
// (< 300 ms) inputs a dot, a long press inputs a dash. After 1 second
// of inactivity the accumulated code is looked up in the Morse table
// and the matching letter is printed.
//
// The MCP23009E sits on the STeaMi internal I2C bus, so spin up a
// dedicated TwoWire pointed at the variant pin macros and hand it to the
// driver. Open the serial monitor at 115200 baud to see the decoded letters.

#include <Arduino.h>
#include <MCP23009E.h>
#include <Wire.h>

#include <functional>
#include <map>

static const std::map<String, char> kMorseTable = {
    {".-", 'A'},   {"-...", 'B'}, {"-.-.", 'C'}, {"-..", 'D'},  {".", 'E'},    {"..-.", 'F'},
    {"--.", 'G'},  {"....", 'H'}, {"..", 'I'},   {".---", 'J'}, {"-.-", 'K'},  {".-..", 'L'},
    {"--", 'M'},   {"-.", 'N'},   {"---", 'O'},  {".--.", 'P'}, {"--.-", 'Q'}, {".-.", 'R'},
    {"...", 'S'},  {"-", 'T'},    {"..-", 'U'},  {"...-", 'V'}, {".--", 'W'},  {"-..-", 'X'},
    {"-.--", 'Y'}, {"--..", 'Z'},
};

String code = "";
uint32_t lastReleaseTime = 0;
bool hasRelease = false;

void decoderMorse() {
    if (code == "") {
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

TwoWire internalI2C(I2C_INT_SDA, I2C_INT_SCL);
MCP23009E expander(internalI2C, RST_EXPANDER, MCP23009_I2C_ADDR, INT_EXPANDER);

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000) {
        // Wait up to 2 s for a connected monitor — on the STeaMi USB CDC
        // !Serial stays true until the host enumerates.
    }

    internalI2C.begin();
    expander.begin();

    Serial.println("MorseDecoder — use the UP button: press < 300 ms for dot, >= 300 ms for dash.");
}

void loop() {
    if (expander.getLevel(MCP23009_GPIO1) == MCP23009_LOGIC_LOW) {
        uint32_t pressStart = millis();

        while (expander.getLevel(MCP23009_GPIO1) == MCP23009_LOGIC_LOW) {
            delay(10);
        }
        uint32_t pressDuration = millis() - pressStart;

        if (pressDuration < 300) {
            code += ".";
        } else {
            code += "-";
        }

        Serial.print("Current code: ");
        Serial.println(code);

        lastReleaseTime = millis();
        hasRelease = true;
    }

    if (hasRelease && millis() - lastReleaseTime > 1000) {
        decoderMorse();
        hasRelease = false;
    }
}
