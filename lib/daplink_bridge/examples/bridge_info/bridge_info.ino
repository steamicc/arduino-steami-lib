// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Bridge info: prints what the DAPLink I2C bridge reports, on the serial
 * monitor (115200 baud).
 *
 * The sketch shows the bridge identity (WHO_AM_I), its busy flag and error
 * register, whether the firmware has the 1 KB config zone, then the text
 * stored in that zone (the STeaMi Config JSON on a configured board). It
 * only reads: nothing is written to the flash or to the config zone.
 *
 * On an older firmware without the config zone, the zone is not read: such
 * firmwares lock up until the board is unplugged when they receive a config
 * command, and the driver never sends one to them.
 */

#include <SteamiBoard.h>
#include <SteamiDaplinkBridge.h>

SteamiDaplinkBridge bridge(Steami.wire());  // the board's internal I2C bus

// The whole zone: readConfig() returns 1023 bytes at most.
uint8_t zone[DAPLINK_BRIDGE_CONFIG_SIZE];

void printHex(uint8_t value) {
    Serial.print("0x");
    if (value < 0x10) {
        Serial.print('0');
    }
    Serial.print(value, HEX);
}

// Prints the zone as text, one dot for each byte that is not printable.
void printZone(const uint8_t* data, size_t length) {
    for (size_t i = 0; i < length; ++i) {
        const char c = static_cast<char>(data[i]);
        Serial.print(c >= ' ' && c <= '~' ? c : '.');
    }
    Serial.println();
}

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000) {
        // Wait up to 2 s for the host USB CDC to enumerate so the
        // diagnostics below aren't silently dropped.
    }

    Serial.println("=== DAPLink bridge info ===");

    if (!bridge.begin()) {
        Serial.print("DAPLink bridge not found (WHO_AM_I ");
        printHex(bridge.deviceId());
        Serial.print(", expected ");
        printHex(DAPLINK_BRIDGE_WHO_AM_I);
        Serial.println(").");
        return;
    }

    Serial.print("Address:     ");
    printHex(DAPLINK_BRIDGE_DEFAULT_ADDR);
    Serial.println();

    Serial.print("WHO_AM_I:    ");
    printHex(bridge.deviceId());
    Serial.println();

    Serial.print("Busy:        ");
    Serial.println(bridge.busy() ? "yes" : "no");

    // Reading the error register clears it. The firmware also clears its
    // "last command failed" bit on any later command, register reads
    // included, so this usually reads 0x00.
    uint8_t error = 0;
    Serial.print("Error:       ");
    if (bridge.readError(error)) {
        printHex(error);
        if (error & DAPLINK_BRIDGE_ERROR_BAD_PARAM) {
            Serial.print(" bad-parameter");
        }
        if (error & DAPLINK_BRIDGE_ERROR_CMD_FAILED) {
            Serial.print(" command-failed");
        }
        Serial.println();
    } else {
        Serial.println("(no answer)");
    }

    Serial.print("Config zone: ");
    if (!bridge.hasConfigZone()) {
        Serial.println("no (older firmware: update DAPLink to use it)");
        return;
    }
    Serial.print("yes, ");
    Serial.print(DAPLINK_BRIDGE_CONFIG_SIZE);
    Serial.println(" bytes");

    // The stored data ends at the first 0xFF byte (erased flash). A failed
    // read also returns 0 bytes: the driver cannot tell it from an empty zone.
    const size_t length = bridge.readConfig(zone, sizeof(zone));
    Serial.print("Stored:      ");
    Serial.print(length);
    Serial.println(length == 0 ? " bytes (empty zone, or the read failed)" : " bytes");
    if (length > 0) {
        printZone(zone, length);
    }
}

void loop() {}
