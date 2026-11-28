// SPDX-License-Identifier: GPL-3.0-or-later

#include "SteamiMicrophone.h"

#include <Arduino.h>
#include <math.h>

#include "SteamiLazy.h"

namespace {

// Longest wait of the first level reading after a start, past the window itself.
constexpr uint32_t FIRST_WINDOW_MARGIN_MS = 20;

// Margin of a blocking recording (frequency(), readInto()) past its own length.
constexpr uint32_t RECORD_MARGIN_MS = 100;

// The one driver of the board, shared by every object and steamiImp34dt05(). Built on first use
// (no constructor runs at boot) and referenced only from this file, so the linker drops it, and
// the driver with it, from the sketches that do not use the microphone.
SteamiLazy<SteamiIMP34DT05> gMicrophone;

SteamiIMP34DT05& driver() {
    return gMicrophone.emplace();
}

// The hooks of SteamiBoard: end() stops the stream, the calibration sets the driver offset.
void stopMicrophone() {
    SteamiIMP34DT05* mic = gMicrophone.get();
    if (mic != nullptr) {
        mic->end();
    }
}

void setMicrophoneOffset(float db) {
    driver().setLevelOffset(db);
}

// Starts the shared driver when it is not running and registers the hooks on `board`. True when
// the driver runs.
bool microphoneProbe(SteamiBoard& board) {
    SteamiIMP34DT05& mic = driver();
    if (!mic.isRunning() && !mic.begin()) {
        return false;
    }
    board.attachMicrophone(&stopMicrophone, &setMicrophoneOffset);
    return true;
}

constexpr SteamiFunctionInfo kInfo = {SteamiFunction::MICROPHONE,
                                      "SteamiMicrophone",
                                      "microphone",
                                      "Check the board or try another one.",
                                      "read() will return NAN.",
                                      &microphoneProbe};

constexpr SteamiIMP34DT05::LevelResult kNoLevel = {NAN, 0, NAN, NAN};

// Waits, at most `limitMs`, for `count` samples; false on timeout.
bool waitSamples(SteamiIMP34DT05& mic, size_t count, uint32_t limitMs) {
    const uint32_t start = millis();
    while (mic.available() < count) {
        if (millis() - start > limitMs) {
            return false;
        }
        delay(1);
    }
    return true;
}

}  // namespace

SteamiMicrophone::SteamiMicrophone(SteamiBoard& board) : _handle(board, kInfo) {}

bool SteamiMicrophone::begin() {
    SteamiIMP34DT05* mic = gMicrophone.get();
    if (_handle.status() == SteamiStatus::OK && (mic == nullptr || !mic->isRunning())) {
        // Stopped since (end(), Steami.end(), the accessor): probe again.
        _handle = SteamiHandle(_handle.board(), kInfo);
    }
    const bool ok = _handle.begin();
    _waitFirst = ok;
    return ok;
}

void SteamiMicrophone::end() {
    stopMicrophone();
    _handle = SteamiHandle(_handle.board(), kInfo);
    _waitFirst = false;
}

SteamiIMP34DT05* SteamiMicrophone::liveDriver() {
    if (_handle.status() == SteamiStatus::NOT_STARTED) {
        begin();
    }
    SteamiIMP34DT05* mic = gMicrophone.get();
    if (!_handle.isAvailable() || mic == nullptr || !mic->isRunning()) {
        return nullptr;  // stopped by end(), Steami.end() or the accessor: no restart
    }
    if (!mic->isAlive()) {
        // Running, but the DMA interrupt no longer delivers blocks: restart the stream once.
        mic->end();
        if (!mic->begin()) {
            return nullptr;
        }
        _waitFirst = true;  // the restart forgets the level: wait for its first window
    }
    return mic;
}

SteamiIMP34DT05::LevelResult SteamiMicrophone::currentLevel() {
    SteamiIMP34DT05* mic = liveDriver();
    if (mic == nullptr) {
        return kNoLevel;
    }
    SteamiIMP34DT05::LevelResult level = mic->level();
    if (_waitFirst && isnan(level.rms)) {
        // No window completed since the start yet.
        const uint32_t limit = mic->levelWindowMs() + FIRST_WINDOW_MARGIN_MS;
        const uint32_t start = millis();
        while (!mic->levelReady() && millis() - start < limit) {
            delay(1);
        }
        level = mic->level();
    }
    _waitFirst = false;
    return level;
}

float SteamiMicrophone::read() {
    return currentLevel().dbSpl + _offset;
}

float SteamiMicrophone::dbfs() {
    return currentLevel().dbfs;
}

uint16_t SteamiMicrophone::peak() {
    return currentLevel().peak;
}

bool SteamiMicrophone::isLoud(float thresholdDb) {
    return read() > thresholdDb;
}

bool SteamiMicrophone::levelReady() {
    SteamiIMP34DT05* mic = gMicrophone.get();
    return _handle.isAvailable() && mic != nullptr && mic->isRunning() && mic->levelReady();
}

void SteamiMicrophone::setWindow(uint16_t ms) {
    driver().setLevelWindowMs(ms);
}

uint16_t SteamiMicrophone::window() const {
    const SteamiIMP34DT05* mic = gMicrophone.get();
    return mic != nullptr ? mic->levelWindowMs() : IMP34DT05_LEVEL_WINDOW_DEFAULT_MS;
}

bool SteamiMicrophone::setVolume(uint8_t v) {
    return driver().setVolume(v);
}

uint8_t SteamiMicrophone::volume() const {
    const SteamiIMP34DT05* mic = gMicrophone.get();
    return mic != nullptr ? mic->volume() : IMP34DT05_VOLUME_DEFAULT;
}

float SteamiMicrophone::frequency(float minDb) {
    SteamiIMP34DT05* mic = liveDriver();
    if (mic == nullptr) {
        return NAN;
    }
    const uint8_t v = mic->volume();
    if (v == 0) {
        return NAN;
    }
    mic->clear();
    const uint32_t lengthMs = IMP34DT05_FFT_SIZE * 1000UL / IMP34DT05_SAMPLE_RATE_16K;
    if (!waitSamples(*mic, IMP34DT05_FFT_SIZE, lengthMs + RECORD_MARGIN_MS)) {
        return NAN;
    }
    int16_t samples[IMP34DT05_FFT_SIZE];
    mic->read(samples, IMP34DT05_FFT_SIZE);
    // Silence threshold on the same record, in dB SPL as read() computes it.
    const float rms = SteamiIMP34DT05::computeRms(samples, IMP34DT05_FFT_SIZE);
    const float spl = SteamiIMP34DT05::dbfsToDbSpl(SteamiIMP34DT05::rmsToDbfs(rms), v) +
                      mic->levelOffset() + _offset;
    if (isnan(spl) || spl < minDb) {
        return NAN;
    }
    return SteamiIMP34DT05::computeDominantFrequency(samples, IMP34DT05_FFT_SIZE,
                                                     static_cast<float>(IMP34DT05_SAMPLE_RATE_16K));
}

size_t SteamiMicrophone::readInto(int16_t* samples, size_t count) {
    if (samples == nullptr || count == 0) {
        return 0;
    }
    SteamiIMP34DT05* mic = liveDriver();
    if (mic == nullptr) {
        return 0;
    }
    mic->clear();
    const uint32_t perMs = IMP34DT05_SAMPLE_RATE_16K / 1000;
    const uint32_t limit = static_cast<uint32_t>(count / perMs) + RECORD_MARGIN_MS;
    const uint32_t start = millis();
    size_t copied = 0;
    while (copied < count) {
        copied += mic->read(samples + copied, count - copied);
        if (copied == count || millis() - start > limit) {
            break;
        }
        delay(1);
    }
    return copied;
}

SteamiIMP34DT05* steamiImp34dt05() {
    if (!microphoneProbe(Steami)) {
        return nullptr;
    }
    Steami.calibrate(SteamiChip::IMP34DT05);
    return gMicrophone.get();
}
