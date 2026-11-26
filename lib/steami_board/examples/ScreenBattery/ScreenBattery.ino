// SPDX-License-Identifier: GPL-3.0-or-later
//
// STeaMi Board - show the battery charge on the screen, as mockup 02 of the tutorials
// (02_battery_ref.png): a title, the charge in percent, a bar filled in green, and the voltage
// in millivolts above the name of the fuel gauge.
//
// The same sketch runs on every release: V1a/V1b have a 128 px screen with 16 grey levels, V1c a
// 240 px colour screen; both read the BQ27441 gauge. The screen lays itself out for the panel it
// finds.
//
// screen.begin() comes first in setup(): the V1c frame buffer is 115 200 bytes in one block, and
// it is easiest to get while the heap is still empty.
//
// If the gauge does not answer (for example with no battery plugged in), the screen shows "0",
// an empty bar and "--" in place of the voltage.

#include <Arduino.h>
#include <SteamiBattery.h>
#include <SteamiScreen.h>

SteamiScreen screen;
SteamiBattery battery;

void setup() {
    Serial.begin(115200);  // the screen and the gauge report their errors here
    screen.begin();
    battery.begin();
}

void loop() {
    float v = battery.voltage();  // NAN when the gauge does not answer
    char volts[12] = "--";
    if (!isnan(v)) {
        snprintf(volts, sizeof volts, "%d mV", (int)lroundf(v * 1000.0f));
    }
    screen.clear();
    screen.title("Battery");
    // yOffset -12 (128-px units) for both, as the tutorial:
    // value, bar and two subtitle lines fit together
    screen.value(battery.percent(), "%", SteamiAt::CENTER, nullptr, STEAMI_WHITE, 2, -12);
    screen.bar(battery.percent(), 100, -12, STEAMI_GREEN);  // rows 72..79 on 128, as mockup 02
    screen.subtitle(volts, "BQ27441");
    screen.show();
    delay(1000);
}
