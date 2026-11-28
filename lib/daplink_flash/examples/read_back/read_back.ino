// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Read back: prints the user file, then appends each line typed in the
 * serial monitor (115200 baud, line ending "Newline") and reads it back
 * from the flash to check it.
 *
 * The DAPLink chip keeps one user file in its 8 MB external flash. At
 * start-up the sketch prints it sector by sector, up to DUMP_SECTORS
 * sectors, and stops at the end of the file (its first 0xFF byte). Each
 * line typed afterwards is appended to the file, then the bytes just
 * written are read back from the flash and compared with the line.
 *
 * Nothing is erased: the sketch only appends what is typed. If the file has
 * no name yet, it is named NOTES.TXT before the first append. Unplug and
 * replug the board to see the new content on the STeaMi USB drive.
 */

#include <SteamiBoard.h>
#include <SteamiDaplinkBridge.h>
#include <SteamiDaplinkFlash.h>

// 64 sectors: the first 16 KB of the file, about 13 s to print.
static constexpr uint16_t DUMP_SECTORS = 64;
static constexpr size_t LINE_MAX = 64;

SteamiDaplinkBridge bridge(Steami.wire());  // the board's internal I2C bus
SteamiDaplinkFlash flash(bridge);

static uint8_t sector[DAPLINK_FLASH_SECTOR_SIZE];
static size_t fileLength = 0;  // where the next append lands
static bool ready = false;

static char line[LINE_MAX + 1];
static size_t lineLength = 0;

// Prints the file up to its end or to `maxSectors` sectors. Sets
// fileLength and returns true when the end of the file was found.
bool dumpFile(uint16_t maxSectors) {
    fileLength = 0;
    for (uint16_t s = 0; s < maxSectors; ++s) {
        if (!flash.readSector(s, sector)) {
            Serial.println();
            Serial.println("Read failed (DAPLink firmware older than March 2026?).");
            return false;
        }
        for (size_t i = 0; i < DAPLINK_FLASH_SECTOR_SIZE; ++i) {
            if (sector[i] == 0xFF) {
                return true;
            }
            Serial.write(sector[i]);
            fileLength++;
        }
    }
    Serial.println();
    Serial.print("(stopped after ");
    Serial.print(fileLength);
    Serial.println(" bytes: the end of the file was not reached)");
    return false;
}

// Reads `length` bytes of the file from `offset` into `buf`.
bool readBack(size_t offset, uint8_t* buf, size_t length) {
    size_t done = 0;
    while (done < length) {
        const size_t pos = offset + done;
        const uint16_t s = static_cast<uint16_t>(pos / DAPLINK_FLASH_SECTOR_SIZE);
        if (!flash.readSector(s, sector)) {
            return false;
        }
        for (size_t i = pos % DAPLINK_FLASH_SECTOR_SIZE;
             i < DAPLINK_FLASH_SECTOR_SIZE && done < length; ++i) {
            buf[done++] = sector[i];
        }
    }
    return true;
}

void appendAndCheck(const char* text) {
    SteamiDaplinkFlash::Filename name;
    if (!flash.getFilename(name) && !flash.setFilename("NOTES", "TXT")) {
        Serial.println("Could not name the file.");
        return;
    }

    // What the file should now end with: the line and its newline.
    uint8_t want[LINE_MAX + 1];
    const size_t expected = strlen(text) + 1;
    memcpy(want, text, expected - 1);
    want[expected - 1] = '\n';

    const size_t written = flash.writeLine(text);
    if (written != expected) {
        Serial.print("Only ");
        Serial.print(written);
        Serial.print(" of ");
        Serial.print(expected);
        Serial.println(" bytes appended (flash full, or a 0xFF byte).");
        if (written == 0) {
            return;
        }
    }

    uint8_t got[LINE_MAX + 1];
    const bool readOk = readBack(fileLength, got, written);
    fileLength += written;
    if (!readOk) {
        Serial.println("Read back failed.");
        return;
    }

    Serial.print("Read back: ");
    Serial.write(got, written);
    if (got[written - 1] != '\n') {
        Serial.println();
    }
    Serial.println(memcmp(got, want, written) == 0 ? "Match." : "MISMATCH.");
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
    Serial.print("File: ");
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

    Serial.println("----");
    if (!dumpFile(DUMP_SECTORS)) {
        // The end of the file is unknown: appending would still work, but
        // the bytes to read back could not be found.
        Serial.println("Typing lines is disabled.");
        return;
    }
    Serial.println("----");
    Serial.print(fileLength);
    Serial.println(fileLength == 0 ? " bytes (empty file)" : " bytes");
    Serial.println();
    Serial.println("Type a line and press Enter to append it to the file.");
    ready = true;
}

void loop() {
    if (!ready) {
        return;
    }
    while (Serial.available() > 0) {
        const char c = static_cast<char>(Serial.read());
        if (c == '\r' || c == '\n') {
            if (lineLength > 0) {
                line[lineLength] = '\0';
                appendAndCheck(line);
                lineLength = 0;
            }
        } else if (lineLength < LINE_MAX) {
            line[lineLength++] = c;
        }
    }
}
