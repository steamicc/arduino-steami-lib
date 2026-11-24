// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Data logger: appends a CSV line to the user file every 10 seconds.
 *
 * The DAPLink chip keeps one user file in its 8 MB external flash. At
 * start-up the sketch prints the file, names it LOG.CSV if it has no name
 * yet, then appends "seconds,light" lines. Unplug and replug the board to
 * see the new content in the LOG.CSV file of the STeaMi USB drive.
 *
 * The file is never erased here. To start over, call flash.clearFlash().
 */

#include <SteamiDaplinkBridge.h>
#include <SteamiDaplinkFlash.h>
#include <Wire.h>

static constexpr uint32_t PERIOD_MS = 10000;

TwoWire internalI2C(I2C_INT_SDA, I2C_INT_SCL);
SteamiDaplinkBridge bridge(internalI2C);
SteamiDaplinkFlash flash(bridge);

static uint8_t content[1024];
static bool ready = false;

void setup() {
    Serial.begin(115200);
    internalI2C.begin();

    if (!flash.begin()) {
        Serial.println("DAPLink bridge not found.");
        return;
    }

    SteamiDaplinkFlash::Filename name;
    if (flash.getFilename(name)) {
        Serial.print("File: ");
        Serial.print(name.name);
        Serial.print('.');
        Serial.println(name.ext);
    } else if (!flash.setFilename("LOG", "CSV")) {
        Serial.println("Could not name the file.");
        return;
    }

    const size_t length = flash.read(content, sizeof(content));
    Serial.print("Current content (first ");
    Serial.print(length);
    Serial.println(" bytes):");
    Serial.write(content, length);
    Serial.println();

    if (length == 0) {
        flash.writeLine("seconds,light");
    }
    ready = true;
}

void loop() {
    if (!ready) {
        return;
    }
    char line[32];
    snprintf(line, sizeof(line), "%lu,%d", millis() / 1000UL, analogRead(A0));
    if (flash.writeLine(line) == strlen(line) + 1) {
        Serial.print("Logged: ");
        Serial.println(line);
    } else {
        Serial.println("Write failed (flash full?).");
    }
    delay(PERIOD_MS);
}
