// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Flash info: prints what the DAPLink chip reports about the user file,
 * on the serial monitor (115200 baud).
 *
 * The sketch shows the bridge identity (WHO_AM_I), its busy flag and error
 * register, whether the firmware has the config zone (and reads with a
 * 2-byte sector number), the 8.3 name of the user file and its length.
 * It only reads: nothing is written to the flash or to the config zone.
 *
 * Reading the file takes about 0.2 s per 256 bytes, so the length is only
 * looked for in the first SCAN_SECTORS sectors.
 */

#include <SteamiBoard.h>
#include <SteamiDaplinkBridge.h>
#include <SteamiDaplinkFlash.h>

// 40 sectors: the first 10 KB of the file, about 8 s.
static constexpr uint16_t SCAN_SECTORS = 40;

SteamiDaplinkBridge bridge(Steami.wire());  // the board's internal I2C bus
SteamiDaplinkFlash flash(bridge);

void printHex(uint8_t value) {
    Serial.print("0x");
    if (value < 0x10) {
        Serial.print('0');
    }
    Serial.print(value, HEX);
}

// Counts the bytes of the file in its first `maxSectors` sectors. `more`
// is true when the file goes on past them. False if a read failed.
bool scanFileLength(uint16_t maxSectors, size_t& length, bool& more) {
    uint8_t sector[DAPLINK_FLASH_SECTOR_SIZE];
    length = 0;
    more = false;
    for (uint16_t s = 0; s < maxSectors; ++s) {
        if (!flash.readSector(s, sector)) {
            return false;
        }
        for (size_t i = 0; i < DAPLINK_FLASH_SECTOR_SIZE; ++i) {
            // The file ends at its first 0xFF byte.
            if (sector[i] == 0xFF) {
                return true;
            }
            length++;
        }
    }
    more = true;
    return true;
}

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000) {
        // Wait up to 2 s for the host USB CDC to enumerate so the
        // diagnostics below aren't silently dropped.
    }

    Serial.println("=== DAPLink flash info ===");

    if (!flash.begin()) {
        Serial.print("DAPLink bridge not found (WHO_AM_I ");
        printHex(bridge.deviceId());
        Serial.print(", expected ");
        printHex(DAPLINK_BRIDGE_WHO_AM_I);
        Serial.println(").");
        return;
    }

    Serial.print("WHO_AM_I:    ");
    printHex(bridge.deviceId());
    Serial.println();

    Serial.print("Busy:        ");
    Serial.println(bridge.busy() ? "yes" : "no");

    // Reading the error register clears it. The firmware also clears its
    // "last command failed" bit on any later command, register reads
    // included (steamicc/DAPLink#11), so this usually reads 0x00.
    uint8_t error = 0;
    Serial.print("Error:       ");
    if (bridge.readError(error)) {
        printHex(error);
        if (error & DAPLINK_FLASH_ERROR_BAD_PARAM) {
            Serial.print(" bad-parameter");
        }
        if (error & DAPLINK_FLASH_ERROR_BAD_FILENAME) {
            Serial.print(" bad-filename");
        }
        if (error & DAPLINK_FLASH_ERROR_FILE_FULL) {
            Serial.print(" file-full");
        }
        if (error & DAPLINK_BRIDGE_ERROR_CMD_FAILED) {
            Serial.print(" command-failed");
        }
        Serial.println();
    } else {
        Serial.println("(no answer)");
    }

    // The probe sends no config command: it reads flash sector 0 with a
    // 2-byte sector number. The firmwares without the config zone take a
    // 1-byte sector number and refuse it, as they refuse readSector().
    Serial.print("Config zone: ");
    Serial.println(bridge.hasConfigZone() ? "yes" : "no (older firmware, reads may fail)");

    SteamiDaplinkFlash::Filename name;
    Serial.print("Filename:    ");
    if (flash.getFilename(name)) {
        Serial.print(name.name);
        if (name.ext[0] != '\0') {
            Serial.print('.');
            Serial.print(name.ext);
        }
        Serial.println();
    } else {
        Serial.println("(no name set)");
    }

    Serial.print("Capacity:    ");
    Serial.print(DAPLINK_FLASH_FILE_SIZE);
    Serial.println(" bytes");

    Serial.print("Length:      ");
    size_t length = 0;
    bool more = false;
    if (!scanFileLength(SCAN_SECTORS, length, more)) {
        Serial.println("(read failed: this firmware cannot read the file)");
        return;
    }
    if (more) {
        Serial.print("more than ");
    }
    Serial.print(length);
    Serial.println(length == 0 ? " bytes (empty file)" : " bytes");
}

void loop() {}
