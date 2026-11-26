// SPDX-License-Identifier: GPL-3.0-or-later
//
// STeaMi Board - draw the primitives gallery on the screen: panel edge, gauge arcs, text with
// accents, rectangles, a disc, a triangle and a line. It is the same scene as the native gallery
// whose 240 px dump is the V1c reference.
//
// Works on V1a/V1b (128 px, 16 grey levels) and V1c (240 px, colour): the scene is laid out with
// centerX(), centerY() and radius(), so it fits either panel.
//
// screen.begin() comes first in setup(): the V1c frame buffer is 115 200 bytes in one block, and
// it is easiest to get while the heap is still empty.
//
// The Serial monitor (115200 baud) shows how long show() takes.

#include <Arduino.h>
#include <SteamiDisplay.h>

SteamiDisplay screen;

static void drawGallery(SteamiDisplay& d) {
    const int16_t cx = d.centerX(), cy = d.centerY(), r = d.radius();
    const uint8_t s = d.width() >= 240 ? 2 : 1;
    d.clear();
    d.circle(cx, cy, r - 1, STEAMI_DARK);                         // panel edge
    d.arc(cx, cy, r - 4 * s, 225, 270, STEAMI_GRID_DARK, 6 * s);  // gauge track
    d.arc(cx, cy, r - 4 * s, 225, 135, STEAMI_GREEN, 6 * s);      // half fill
    d.text("STeaMi", cx - d.textWidth("STeaMi", s) / 2, cy - r / 2, STEAMI_WHITE, s);
    const char* temp =
        "20\xC2\xB0"
        "C \xC3\xA9";  // "20°C é"
    d.text(temp, cx - d.textWidth(temp, s) / 2, cy - r / 2 + 12 * s, STEAMI_YELLOW, s);
    d.rect(cx - r / 2, cy - r / 8, r / 3, r / 4, STEAMI_LIGHT);
    d.rect(cx - r / 2 + 2, cy - r / 8 + 2, r / 3 - 4, r / 4 - 4, STEAMI_BLUE, true);
    d.circle(cx + r / 3, cy, r / 8, STEAMI_RED, true);
    d.triangle(cx - r / 4, cy + r / 2, cx + r / 4, cy + r / 2, cx, cy + r / 4, STEAMI_YELLOW, true);
    d.line(cx - r / 2, cy + r / 6, cx + r / 2, cy + r / 6, STEAMI_GRAY);
    d.show();
}

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000) {
        // Up to 2 s for a serial monitor to open, so it sees a "No screen" error; never forever.
    }

    if (!screen.begin()) {
        Serial.print("No screen: ");
        Serial.println(screen.error());
        return;
    }
    drawGallery(screen);
}

void loop() {
    if (!screen.isAvailable()) {
        return;
    }
    const uint32_t start = micros();
    screen.show();
    Serial.print("show(): ");
    Serial.print(micros() - start);
    Serial.println(" us");
    delay(2000);
}
