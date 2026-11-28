// SPDX-License-Identifier: GPL-3.0-or-later
//
// BQ27441 — tell from the sign of the current whether the battery is
// charging, and show the charge on the screen: the state as the title,
// the charge in percent, a progress bar, the current and an estimate of
// the time left to full (charging) or to empty (on battery).
//
// The gauge reports a positive average current while the battery charges
// and a negative one while it discharges. Within ±5 mA (the gauge's own
// deadband) the battery is idle: full, or charged and discharged at the
// same rate.
//
// The same sketch runs on every release: V1a/V1b have a 128 px screen with
// 16 grey levels, V1c a 240 px colour screen. The screen lays itself out
// for the panel it finds. The state is also printed to the serial monitor
// (115200 baud) every second.
//
// Plug and unplug the USB cable to see the state change: the average
// current takes a few seconds to settle.
//
// screen.begin() comes first in setup(): the V1c frame buffer is 115 200
// bytes in one block, and it is easiest to get while the heap is still
// empty. The BQ27441 sits on the STeaMi internal I2C bus; the driver uses
// the board's bus, Steami.wire().

#include <Arduino.h>
#include <SteamiBQ27441.h>
#include <SteamiBoard.h>
#include <SteamiScreen.h>

SteamiScreen screen;
SteamiBQ27441 fuelGauge(Steami.wire());  // the board's internal I2C bus

// Below this current, in either direction, the battery is idle.
constexpr int16_t IDLE_CURRENT_MA = 5;
constexpr uint32_t REFRESH_PERIOD_MS = 1000;

enum class ChargeState { CHARGING, DISCHARGING, IDLE };

ChargeState chargeState(int16_t currentMa) {
    if (currentMa > IDLE_CURRENT_MA) {
        return ChargeState::CHARGING;
    }
    if (currentMa < -IDLE_CURRENT_MA) {
        return ChargeState::DISCHARGING;
    }
    return ChargeState::IDLE;
}

// "1 h 25" or "40 min" for a duration in minutes.
void formatDuration(char* out, size_t size, uint32_t minutes) {
    if (minutes >= 60) {
        snprintf(out, size, "%lu h %02lu", (unsigned long)(minutes / 60),
                 (unsigned long)(minutes % 60));
    } else {
        snprintf(out, size, "%lu min", (unsigned long)minutes);
    }
}

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000) {
        // Wait up to 2 s for the host USB CDC to enumerate so the
        // "not detected" diagnostic below isn't silently dropped.
    }
    screen.begin();

    if (!fuelGauge.begin()) {
        Serial.println("BQ27441 not detected — check wiring.");
        screen.clear();
        screen.title("Battery");
        screen.text("No gauge", SteamiAt::CENTER, STEAMI_RED);
        screen.show();
        while (true) {
            delay(1000);
        }
    }
}

void loop() {
    const int16_t currentMa = fuelGauge.currentAverage();
    const uint8_t percent = fuelGauge.stateOfCharge();
    const ChargeState state = chargeState(currentMa);

    const char* title = "Idle";
    SteamiRgb barColor = STEAMI_LIGHT;
    char eta[24] = "";
    char duration[12];

    if (state == ChargeState::CHARGING) {
        title = "Charging";
        barColor = STEAMI_GREEN;
        // Time to full: the capacity still missing at the present current.
        const uint16_t full = fuelGauge.capacityFull();
        const uint16_t remaining = fuelGauge.capacityRemaining();
        if (full > remaining) {
            formatDuration(duration, sizeof(duration),
                           (uint32_t)(full - remaining) * 60 / currentMa);
            snprintf(eta, sizeof(eta), "full in %s", duration);
        }
    } else if (state == ChargeState::DISCHARGING) {
        title = "On battery";
        barColor = percent <= 20 ? STEAMI_RED : STEAMI_YELLOW;
        // Time to empty: the capacity left at the present current.
        formatDuration(duration, sizeof(duration),
                       (uint32_t)fuelGauge.capacityRemaining() * 60 / -currentMa);
        snprintf(eta, sizeof(eta), "empty in %s", duration);
    } else if (percent >= 100) {
        title = "Full";
        barColor = STEAMI_GREEN;
    }

    char current[12];
    snprintf(current, sizeof(current), "%+d mA", (int)currentMa);

    screen.clear();
    screen.title(title);
    // yOffset -12 (128-px units) for both, as the ScreenBattery example of
    // the board library: value, bar and two subtitle lines fit together.
    screen.value(percent, "%", SteamiAt::CENTER, nullptr, STEAMI_WHITE, 2, -12);
    screen.bar(percent, 100, -12, barColor);
    screen.subtitle(current, eta);
    screen.show();

    Serial.print(title);
    Serial.print(", ");
    Serial.print(percent);
    Serial.print(" %, ");
    Serial.print(current);
    if (eta[0] != '\0') {
        Serial.print(", ");
        Serial.print(eta);
    }
    Serial.println();

    delay(REFRESH_PERIOD_MS);
}
