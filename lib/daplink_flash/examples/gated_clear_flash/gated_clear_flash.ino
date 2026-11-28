// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Gated clear flash: erases the user file, only after the word ERASE is
 * typed in the serial monitor (115200 baud, line ending "Newline").
 *
 * WARNING: the content of the user file is lost for good. Copy the file
 * from the STeaMi USB drive first if it matters.
 *
 * The sketch prints the name and the length of the file, then waits for a
 * line. Only the exact line ERASE runs flash.clearFlash(); any other line
 * cancels, and nothing is erased until the board restarts. The sketch
 * never erases on its own: it asks again at every start-up.
 *
 * clearFlash() erases the file in the external W25Q64 flash up to its end:
 * about 0.4 s for a small file, about 20 s for a full flash. The file name
 * stays, and the config zone, kept in the internal flash of the DAPLink
 * chip, is not touched.
 */

#include <SteamiBoard.h>
#include <SteamiDaplinkBridge.h>
#include <SteamiDaplinkFlash.h>

// 40 sectors: the first 10 KB of the file, about 8 s.
static constexpr uint16_t SCAN_SECTORS = 40;
static constexpr size_t LINE_MAX = 16;
static const char CONFIRM_WORD[] = "ERASE";

SteamiDaplinkBridge bridge(Steami.wire());  // the board's internal I2C bus
SteamiDaplinkFlash flash(bridge);

static bool waiting = false;
static char line[LINE_MAX + 1];
static size_t lineLength = 0;

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

void erase() {
    Serial.println("Erasing... (up to about 20 s for a full flash)");
    const uint32_t start = millis();
    const bool ok = flash.clearFlash();
    const uint32_t elapsed = millis() - start;

    Serial.print(ok ? "Done: the file is empty" : "Erase failed: the file does not read empty");
    Serial.print(" (");
    Serial.print(elapsed);
    Serial.println(" ms).");
    if (ok) {
        Serial.println("Unplug and replug the board to update the USB drive.");
    }
}

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000) {
        // Wait up to 2 s for the host USB CDC to enumerate so the
        // diagnostics below aren't silently dropped.
    }

    if (!flash.begin()) {
        Serial.println("DAPLink bridge not found.");
        return;
    }

    SteamiDaplinkFlash::Filename name;
    Serial.print("File:   ");
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

    size_t length = 0;
    bool more = false;
    if (!scanFileLength(SCAN_SECTORS, length, more)) {
        Serial.println("Read failed: the file cannot be checked, nothing is erased.");
        return;
    }
    Serial.print("Length: ");
    if (more) {
        Serial.print("more than ");
    }
    Serial.print(length);
    Serial.println(" bytes");

    if (length == 0) {
        Serial.println("The file is already empty: nothing to erase.");
        return;
    }

    Serial.println();
    Serial.print("Type ");
    Serial.print(CONFIRM_WORD);
    Serial.println(" and press Enter to erase the file. Any other line cancels.");
    waiting = true;
}

void loop() {
    if (!waiting) {
        return;
    }
    while (Serial.available() > 0) {
        const char c = static_cast<char>(Serial.read());
        if (c != '\r' && c != '\n') {
            if (lineLength < LINE_MAX) {
                line[lineLength] = c;
            }
            lineLength++;  // a line too long never matches
            continue;
        }
        if (lineLength == 0) {
            continue;  // the second half of a CR LF
        }
        waiting = false;
        if (lineLength <= LINE_MAX) {
            line[lineLength] = '\0';
        }
        if (lineLength <= LINE_MAX && strcmp(line, CONFIRM_WORD) == 0) {
            erase();
        } else {
            Serial.println("Cancelled: nothing was erased.");
        }
        Serial.println("Restart the board to run this sketch again.");
        return;
    }
}
