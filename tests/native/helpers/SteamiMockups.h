// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <math.h>
#include <stdio.h>

#include "SteamiScreen.h"

// The drawing code of mockups 01 to 03 (spec section 6), shared by the native mockups suite. Each
// function is the loop() body of its example up to show(), the sensor readings taken as
// parameters; a script checks that the examples make the same screen calls with the same
// literals. The 240 px colour renders are the references for the V1c photos.

/// 01 Temperature (ScreenTemperature): t in degrees Celsius, NAN when the sensor is missing.
inline void mockupTemperature(SteamiScreen& screen, float t, const char* chip) {
    screen.clear();
    screen.title("Temperature");
    screen.value(t, "°C");
    screen.subtitle(chip);  // "WSEN-HIDS" on V1a/V1b, "BME280" on V1c
    screen.show();
}

/// 02 Battery (ScreenBattery): percent 0 and v NAN when the gauge does not answer.
inline void mockupBattery(SteamiScreen& screen, uint8_t percent, float v) {
    char volts[12] = "--";
    if (!isnan(v)) {
        snprintf(volts, sizeof volts, "%d mV", (int)lroundf(v * 1000.0f));
    }
    screen.clear();
    screen.title("Battery");
    // yOffset -12 (128-px units) for both, as the tutorial:
    // value, bar and two subtitle lines fit together
    screen.value(percent, "%", SteamiAt::CENTER, nullptr, STEAMI_WHITE, 2, -12);
    screen.bar(percent, 100, -12, STEAMI_GREEN);  // rows 72..79 on 128, as mockup 02
    screen.subtitle(volts, "BQ27441");
    screen.show();
}

/// 03 Comfort, two values (ScreenComfort): t and h NAN when a sensor is missing.
inline void mockupComfort(SteamiScreen& screen, float t, float h, const char* chip) {
    bool comfortable = t >= 19 && t <= 24 && h >= 40 && h <= 60;
    screen.clear();
    screen.title("Comfort");
    screen.vline(screen.centerX(), screen.centerY() - screen.radius() / 2, screen.radius(),
                 STEAMI_DARK);
    screen.value(t, "°C", SteamiAt::W, "TEMP");
    screen.value(h, "%", SteamiAt::E, "HUM");  // a float: "45.3", and "--" if the sensor is missing
    screen.subtitle(comfortable ? "Comfortable" : "Uncomfortable", chip, STEAMI_GREEN);
    screen.show();
}
