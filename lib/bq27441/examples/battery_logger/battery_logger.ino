// SPDX-License-Identifier: GPL-3.0-or-later
//
// BQ27441 — log the battery every minute to a CSV file on the STeaMi USB
// drive, to plot how long a charge lasts.
//
// Each line holds the seconds since start-up, the voltage (mV), the state
// of charge (%), the average current (mA, negative while the battery
// discharges) and the remaining capacity (mAh):
//
//     seconds,voltage_mv,charge_percent,current_ma,remaining_mah
//     0,3987,82,-48,512
//     60,3984,82,-47,511
//
// Run the board on its battery, USB unplugged, for the time of the test.
// To read the file, disconnect the battery too: it keeps the DAPLink chip
// powered, and the STeaMi drive lists the file as it was when that chip
// started. Then plug in the USB cable and open BATTERY.CSV on the drive
// (without a battery the gauge does not answer, so the sketch stops
// before appending anything). Each line is also printed to the serial
// monitor (115200 baud) while USB is connected.
//
// The DAPLink chip keeps a single file: a new run appends to it, and the
// seconds start again from 0. Set ERASE_ON_START to true to start from an
// empty file (erasing a full flash takes about 20 s).
//
// The BQ27441 and the DAPLink chip both sit on the STeaMi internal I2C
// bus; the drivers use the board's bus, Steami.wire().

#include <Arduino.h>
#include <SteamiBQ27441.h>
#include <SteamiBoard.h>
#include <SteamiDaplinkBridge.h>
#include <SteamiDaplinkFlash.h>

SteamiBQ27441 fuelGauge(Steami.wire());     // the board's internal I2C bus
SteamiDaplinkBridge bridge(Steami.wire());  // same bus
SteamiDaplinkFlash flash(bridge);

// One line a minute: about 25 bytes, so a day takes about 36 KB of the
// 8 MB flash.
constexpr uint32_t LOG_PERIOD_MS = 60000;
constexpr bool ERASE_ON_START = false;

// Print a message and stop: the logger cannot work without the gauge or
// the flash.
void halt(const char* message) {
    Serial.println(message);
    while (true) {
        delay(1000);
    }
}

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000) {
        // Wait up to 2 s for the host USB CDC to enumerate so the
        // diagnostics below aren't silently dropped.
    }

    if (!fuelGauge.begin()) {
        halt("BQ27441 not detected — check wiring.");
    }
    if (!flash.begin()) {
        halt("DAPLink bridge not found.");
    }

    if (ERASE_ON_START && !flash.clearFlash()) {
        halt("Could not erase the file.");
    }
    if (!flash.setFilename("BATTERY", "CSV")) {
        halt("Could not name the file.");
    }

    // The file ends at its first 0xFF byte: reading one byte is enough to
    // know whether it is empty and needs the CSV header.
    uint8_t first;
    if (flash.read(&first, 1) == 0) {
        flash.writeLine("seconds,voltage_mv,charge_percent,current_ma,remaining_mah");
    }

    Serial.println("Logging to BATTERY.CSV every minute.");
}

void loop() {
    char line[64];
    snprintf(line, sizeof(line), "%lu,%u,%u,%d,%u", millis() / 1000UL,
             (unsigned)fuelGauge.voltageMv(), (unsigned)fuelGauge.stateOfCharge(),
             (int)fuelGauge.currentAverage(), (unsigned)fuelGauge.capacityRemaining());

    if (flash.writeLine(line) == strlen(line) + 1) {
        Serial.print("Logged: ");
        Serial.println(line);
    } else {
        Serial.println("Write failed (flash full?).");
    }

    delay(LOG_PERIOD_MS);
}
