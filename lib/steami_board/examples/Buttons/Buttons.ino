// SPDX-License-Identifier: GPL-3.0-or-later
//
// STeaMi Board - print each press and release of the seven buttons (A, B,
// MENU and the four D-PAD arrows) (115200 baud).
//
// Works on every release (V1a, V1b and V1c). Buttons are debounced over 20 ms,
// so loop() polls often and does not wait.
//
// If the D-PAD expander does not answer, begin() returns false and error() says
// why, but A, B and MENU still work and the arrows read as released. A button
// held at start is not reported as a press.

#include <Arduino.h>
#include <SteamiButtons.h>

SteamiButtons buttons;

static const SteamiButton kAll[] = {SteamiButton::A,    SteamiButton::B,    SteamiButton::MENU,
                                    SteamiButton::UP,   SteamiButton::DOWN, SteamiButton::LEFT,
                                    SteamiButton::RIGHT};
static const char* const kNames[] = {"A", "B", "MENU", "UP", "DOWN", "LEFT", "RIGHT"};

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000) {
        // Serial is ready as soon as Serial.begin() returns on the STeaMi.
    }

    if (buttons.begin()) {
        Serial.println("Buttons ready.");
    } else {
        Serial.print("D-PAD unavailable: ");
        Serial.println(buttons.error());
    }
}

void loop() {
    for (size_t i = 0; i < sizeof(kAll) / sizeof(kAll[0]); i++) {
        if (buttons.wasPressed(kAll[i])) {
            Serial.print(kNames[i]);
            Serial.println(" pressed");
        }
        if (buttons.wasReleased(kAll[i])) {
            Serial.print(kNames[i]);
            Serial.println(" released");
        }
    }
    delay(5);
}
