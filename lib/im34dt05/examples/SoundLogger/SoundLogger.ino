// SPDX-License-Identifier: GPL-3.0-or-later
//
// SoundLogger - log the sound level to a CSV file on the STeaMi USB drive.
//
// The microphone (IMP34DT05) measures the sound over 1 s windows. Every
// LOG_PERIOD_S seconds, the sketch appends one line to the user file that
// the DAPLink chip keeps in its external flash:
//
//   seconds,leq_db_spl,max_db_spl,peak
//
//   - seconds: time since start-up,
//   - leq_db_spl: average level of the period, as a sound level meter
//     computes it (the mean of the sound energy, not of the dB values),
//   - max_db_spl: level of the loudest 1 s window of the period,
//   - peak: largest sample of the period, in PCM units (32767 = clipping).
//
// The dB SPL values are estimated from the datasheet sensitivity, not
// calibrated: compare with a sound level meter and use setLevelOffset()
// (see the SoundLevel example).
//
// The DAPLink chip keeps a single user file, shared with the other logging
// sketches: the sketch names it SOUND.CSV if it has no name yet, and never
// erases it on its own. Hold button A while the board starts to erase it.
// The blue LED lights up while a line is written. Unplug and replug the
// board to see the lines in the file of the STeaMi USB drive, then open it
// in a spreadsheet to draw the curve.
//
// Flash and open the serial monitor at 115200 baud to follow the lines.

#include <Arduino.h>
#include <SteamiBoard.h>
#include <SteamiDaplinkBridge.h>
#include <SteamiDaplinkFlash.h>
#include <SteamiIMP34DT05.h>

SteamiIMP34DT05 mic;
SteamiDaplinkBridge bridge(Steami.wire());  // the board's internal I2C bus
SteamiDaplinkFlash flash(bridge);

static const uint16_t WINDOW_MS = 1000;   // one level per second
static const uint16_t LOG_PERIOD_S = 10;  // one CSV line every 10 windows
static const int WRITE_LED = LED_BLUE;

// Accumulated over the current period.
float sumMeanSquare = 0.0f;  // sum of the squared RMS of the windows
float maxRms = 0.0f;
uint16_t maxPeak = 0;
uint16_t windows = 0;

void stop(const char* message) {
    Serial.println(message);
    while (true) {
        delay(1000);
    }
}

// The dB SPL estimate is the dBFS value shifted by a constant that depends
// on the volume and on setLevelOffset(): take it from a measured window.
float toDbSpl(float rms, const SteamiIMP34DT05::LevelResult& reference) {
    float splMinusFs = reference.dbSpl - reference.dbfs;
    return SteamiIMP34DT05::rmsToDbfs(rms) + splMinusFs;
}

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000) {
        // Wait up to 2 s for a connected monitor.
    }

    pinMode(WRITE_LED, OUTPUT);
    digitalWrite(WRITE_LED, LOW);
    pinMode(A_BUTTON, INPUT_PULLUP);

    if (!flash.begin()) {
        stop("DAPLink bridge not found.");
    }

    SteamiDaplinkFlash::Filename name;
    if (!flash.getFilename(name)) {
        if (!flash.setFilename("SOUND", "CSV") || !flash.getFilename(name)) {
            stop("Could not name the file.");
        }
    }
    Serial.print("File: ");
    Serial.print(name.name);
    Serial.print('.');
    Serial.println(name.ext);

    if (digitalRead(A_BUTTON) == LOW) {
        Serial.println("Button A held: erasing the file...");
        if (!flash.clearFlash()) {
            stop("Could not erase the file.");
        }
    }

    // A new or erased file starts with the CSV header.
    uint8_t first;
    if (flash.read(&first, 1) == 0) {
        flash.writeLine("seconds,leq_db_spl,max_db_spl,peak");
    }

    if (!mic.begin()) {
        stop("Microphone not detected - check that this is a STeaMi board.");
    }
    mic.setLevelWindowMs(WINDOW_MS);

    Serial.print("Logging the sound level every ");
    Serial.print(LOG_PERIOD_S);
    Serial.println(" s.");
}

void loop() {
    if (!mic.levelReady()) {
        return;  // the current 1 s window is not finished yet
    }

    SteamiIMP34DT05::LevelResult level = mic.level();
    if (isnan(level.rms)) {
        return;
    }
    sumMeanSquare += level.rms * level.rms;
    if (level.rms > maxRms) {
        maxRms = level.rms;
    }
    if (level.peak > maxPeak) {
        maxPeak = level.peak;
    }
    windows++;
    if (windows < LOG_PERIOD_S) {
        return;
    }

    float leq = toDbSpl(sqrtf(sumMeanSquare / windows), level);
    float loudest = toDbSpl(maxRms, level);
    uint16_t peak = maxPeak;
    sumMeanSquare = 0.0f;
    maxRms = 0.0f;
    maxPeak = 0;
    windows = 0;

    // With the default C library (Newlib Nano), snprintf() does not print
    // floats: dtostrf() formats them.
    char leqText[12];
    char loudestText[12];
    dtostrf(leq, 1, 1, leqText);
    dtostrf(loudest, 1, 1, loudestText);
    char line[48];
    snprintf(line, sizeof(line), "%lu,%s,%s,%u", millis() / 1000UL, leqText, loudestText,
             static_cast<unsigned>(peak));

    // About 6 ms per byte; the microphone keeps measuring meanwhile.
    digitalWrite(WRITE_LED, HIGH);
    size_t written = flash.writeLine(line);
    digitalWrite(WRITE_LED, LOW);

    if (written == strlen(line) + 1) {
        Serial.println(line);
    } else {
        Serial.println("Write failed (flash full?).");
    }
}
