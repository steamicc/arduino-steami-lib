// SPDX-License-Identifier: GPL-3.0-or-later

#include "SteamiIMP34DT05.h"

#include <math.h>

#include "SteamiPdmSai.h"

namespace {

constexpr uint16_t BUFFER_MASK = IMP34DT05_BUFFER_SAMPLES - 1;
static_assert((IMP34DT05_BUFFER_SAMPLES & BUFFER_MASK) == 0,
              "IMP34DT05_BUFFER_SAMPLES must be a power of two");

constexpr float PI_F = 3.14159265358979f;
constexpr float SQRT2_F = 1.41421356237f;

// isAlive(): longest time since the last block for a live stream.
constexpr uint32_t ALIVE_TIMEOUT_MS = 50;

// Instance fed by the DMA interrupt (the board has one microphone).
SteamiIMP34DT05* activeMic = nullptr;

void handleBlock(const uint8_t* data, size_t length) {
    SteamiIMP34DT05* mic = activeMic;
    if (mic != nullptr) {
        mic->processBlock(data, length);
    }
}

// Masks the interrupts for the lifetime of the object (no-op on the
// host), to read or change state shared with the DMA interrupt.
class IrqGuard {
   public:
#if defined(ARDUINO_ARCH_STM32)
    IrqGuard() : _primask(__get_PRIMASK()) { __disable_irq(); }
    ~IrqGuard() {
        if (_primask == 0) {
            __enable_irq();
        }
    }

   private:
    uint32_t _primask;
#else
    IrqGuard() {}  // user-provided: no unused-variable warning on the host
#endif
};

// A PDM byte equal to 0x00 or 0xFF means eight identical bits in a row:
// what a data line nobody drives gives. A live microphone, even in
// silence, toggles its output all the time.
bool isLiveByte(uint8_t value) {
    return value != 0x00 && value != 0xFF;
}

// Processor cycle counter (DWT CYCCNT, which the STM32 core starts at
// boot). 0 on the host: the conversion statistics stay at zero there.
uint32_t cycleCounter() {
#if defined(ARDUINO_ARCH_STM32) && defined(DWT_BASE)
    return DWT->CYCCNT;
#else
    return 0;
#endif
}

void enableCycleCounter() {
#if defined(ARDUINO_ARCH_STM32) && defined(DWT_BASE)
    if ((DWT->CTRL & DWT_CTRL_CYCCNTENA_Msk) == 0) {
        CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
        DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    }
#endif
}

// Filter the `perSlot` bytes of one slot of a SAI block, one PCM block
// (or less) at a time, and hand each batch of samples to `sink`.
template <typename Sink>
void filterSlot(SteamiPdmFilter& filter, const uint8_t* data, size_t perSlot, uint8_t slot,
                Sink sink) {
    int16_t pcm[IMP34DT05_SAMPLES_PER_BLOCK + 1];
    const uint8_t* slotData = data + slot;
    size_t remaining = perSlot;
    while (remaining > 0) {
        size_t chunk =
            remaining < IMP34DT05_PDM_BYTES_PER_BLOCK ? remaining : IMP34DT05_PDM_BYTES_PER_BLOCK;
        size_t count = filter.process(slotData, chunk, IMP34DT05_SAI_SLOTS, pcm);
        slotData += chunk * IMP34DT05_SAI_SLOTS;
        remaining -= chunk;
        sink(pcm, count);
    }
}

}  // namespace

SteamiIMP34DT05::SteamiIMP34DT05() {
    _windowSamples = static_cast<uint32_t>(_windowMs) * (IMP34DT05_SAMPLE_RATE_16K / 1000);
    for (SteamiPdmFilter& filter : _detectFilter) {
        (void)filter.setVolume(IMP34DT05_DETECT_VOLUME);
    }
}

// --- Lifecycle ---------------------------------------------------------------

bool SteamiIMP34DT05::begin(uint32_t sampleRate) {
    if (sampleRate != IMP34DT05_SAMPLE_RATE_16K) {
        return false;
    }
    if (_begun) {
        end();
    }
    if (!SteamiPdmSai::supported()) {
        return false;
    }

    enableCycleCounter();
    activeMic = this;
    if (!SteamiPdmSai::begin(handleBlock)) {
        activeMic = nullptr;
        return false;
    }
    _begun = true;
    _slotLocked = false;

    if (!startAndWait()) {
        end();
        return false;
    }
    return true;
}

void SteamiIMP34DT05::end() {
    if (_begun) {
        SteamiPdmSai::end();
    }
    _running = false;
    _begun = false;
    if (activeMic == this) {
        activeMic = nullptr;
    }
}

void SteamiIMP34DT05::powerOn() {
    if (_begun && !_running) {
        (void)startAndWait();
    }
}

void SteamiIMP34DT05::powerOff() {
    if (_running) {
        SteamiPdmSai::stop();
        _running = false;
    }
}

bool SteamiIMP34DT05::isRunning() const {
    return _running;
}

bool SteamiIMP34DT05::isAlive() const {
    if (!_running) {
        return false;
    }
    // Read the block time first: a block arriving in between only makes
    // the stream look older, never younger than it is.
    uint32_t last = _lastBlockMs;
    return millis() - last <= ALIVE_TIMEOUT_MS;
}

uint32_t SteamiIMP34DT05::sampleRate() const {
    return IMP34DT05_SAMPLE_RATE_16K;
}

bool SteamiIMP34DT05::startAndWait() {
    resetStream();
    resetLevel();
    resetConversionStats();
    _filter.reset();
    _blocks = 0;
    _settleBlocks = IMP34DT05_SETTLE_BLOCKS;
    for (uint8_t s = 0; s < IMP34DT05_SAI_SLOTS; s++) {
        _slotScore[s] = 0;
        _slotEnergy[s] = 0;
        _detectFilter[s].reset();
    }
    if (_slotMode != IMP34DT05_SLOT_AUTO) {
        _slot = _slotMode;
        _slotLocked = true;
        _streamAlive = true;  // forced slot: nothing to check against
    } else if (_redetect) {
        _slotLocked = false;  // setSlot(AUTO) after a forced slot
    }
    _redetect = false;

    if (!SteamiPdmSai::start()) {
        return false;
    }
    _running = true;

    uint32_t start = millis();
    // Running once the slot is known and the filters settled.
    while (!_slotLocked || _blocks <= IMP34DT05_WARMUP_BLOCKS || _settleBlocks != 0) {
        if (millis() - start > IMP34DT05_START_TIMEOUT_MS) {
            powerOff();  // no DMA interrupt: SAI or clock not running
            return false;
        }
        delay(1);
    }
    if (!_streamAlive) {
        powerOff();  // the data line stays flat: no microphone answers
        return false;
    }
    return true;
}

// --- Interrupt side ----------------------------------------------------------

void SteamiIMP34DT05::processBlock(const uint8_t* data, size_t length) {
    uint32_t startCycles = cycleCounter();
    size_t perSlot = length / IMP34DT05_SAI_SLOTS;
    if (perSlot == 0) {
        return;
    }

    _lastBlockMs = millis();
    uint32_t block = _blocks + 1;
    _blocks = block;
    if (block <= IMP34DT05_WARMUP_BLOCKS) {
        return;  // the microphone is waking up
    }

    if (!_slotLocked) {
        detectSlot(data, perSlot, block - IMP34DT05_WARMUP_BLOCKS);
        return;
    }

    bool deliver = _settleBlocks == 0;
    filterSlot(_filter, data, perSlot, _slot, [this, deliver](const int16_t* pcm, size_t count) {
        if (deliver) {
            publish(pcm, count);
        }
    });
    if (_settleBlocks > 0) {
        _settleBlocks = _settleBlocks - 1;
    }
    if (deliver) {
        // Steady state only: warm-up, slot detection (two filters) and
        // settling blocks returned above or are not delivered.
        uint32_t cycles = cycleCounter() - startCycles;
        _statBlocks = _statBlocks + 1;
        _statCycles += cycles;
        if (cycles > _statMaxCycles) {
            _statMaxCycles = cycles;
        }
    }
}

// The SAI samples D1 on both clock edges: one slot carries the
// microphone, the other reads the line while the microphone leaves it
// undriven (PA10 has no pull resistor). That line holds the last bit (a
// copy of the stream, harmless), drifts (long runs of 0 or 1) or picks
// up the clock (a random bit stream). A random stream toggles as much as
// a real one, but once filtered it is far louder than a microphone in a
// room, which gives about -70 dBFS at the detection volume. Hence: keep
// the slots whose bytes toggle, then pick the quietest one.
void SteamiIMP34DT05::detectSlot(const uint8_t* data, size_t perSlot, uint32_t detectBlock) {
    bool measure = detectBlock > IMP34DT05_DETECT_SETTLE_BLOCKS;
    for (uint8_t s = 0; s < IMP34DT05_SAI_SLOTS; s++) {
        uint32_t live = 0;
        for (size_t i = 0; i < perSlot; i++) {
            if (isLiveByte(data[i * IMP34DT05_SAI_SLOTS + s])) {
                live++;
            }
        }
        _slotScore[s] += live;

        uint64_t energy = 0;
        filterSlot(_detectFilter[s], data, perSlot, s, [&energy](const int16_t* pcm, size_t count) {
            for (size_t i = 0; i < count; i++) {
                int32_t value = pcm[i];
                energy += static_cast<uint64_t>(value * value);
            }
        });
        if (measure) {
            _slotEnergy[s] += energy;
        }
    }
    if (detectBlock < IMP34DT05_DETECT_BLOCKS) {
        return;
    }

    // Alive when at least a quarter of the bytes toggle.
    uint32_t needed = static_cast<uint32_t>(IMP34DT05_DETECT_BLOCKS) * perSlot / 4;
    bool live0 = _slotScore[0] >= needed;
    bool live1 = _slotScore[1] >= needed;
    uint8_t best = 0;
    if (live0 && live1) {
        best = (_slotEnergy[1] < _slotEnergy[0]) ? 1 : 0;
    } else if (live1) {
        best = 1;
    }
    _streamAlive = live0 || live1;
    _slot = best;
    _filter.reset();
    _settleBlocks = IMP34DT05_SETTLE_BLOCKS;
    _slotLocked = true;
}

void SteamiIMP34DT05::publish(const int16_t* samples, size_t count) {
    // Runs in the interrupt: work on local copies, store them once.
    uint16_t head = _head;
    const uint16_t tail = _tail;  // read() only frees space meanwhile
    uint32_t overruns = _overruns;
    uint64_t accSquares = _accSquares;
    uint32_t accCount = _accCount;
    uint16_t accPeak = _accPeak;
    const uint32_t windowSamples = _windowSamples;

    for (size_t i = 0; i < count; i++) {
        int16_t sample = samples[i];

        if (static_cast<uint16_t>(head - tail) >= IMP34DT05_BUFFER_SAMPLES) {
            overruns++;
        } else {
            _buffer[head & BUFFER_MASK] = sample;
            head++;
        }

        int32_t value = sample;
        uint16_t magnitude = static_cast<uint16_t>(value < 0 ? -value : value);
        accSquares += static_cast<uint32_t>(value * value);
        if (magnitude > accPeak) {
            accPeak = magnitude;
        }
        if (++accCount >= windowSamples) {
            _meanSquare = static_cast<float>(accSquares) / static_cast<float>(accCount);
            _windowPeak = accPeak;
            _levelValid = true;
            _levelReady = true;
            accSquares = 0;
            accCount = 0;
            accPeak = 0;
        }
    }

    _head = head;
    _overruns = overruns;
    _accSquares = accSquares;
    _accCount = accCount;
    _accPeak = accPeak;
    if (_callback != nullptr) {
        _callback(samples, count);
    }
}

// --- Samples -----------------------------------------------------------------

size_t SteamiIMP34DT05::available() const {
    return static_cast<uint16_t>(_head - _tail);
}

bool SteamiIMP34DT05::dataReady() const {
    return available() > 0;
}

size_t SteamiIMP34DT05::read(int16_t* samples, size_t count) {
    uint16_t tail = _tail;
    size_t waiting = static_cast<uint16_t>(_head - tail);
    size_t n = count < waiting ? count : waiting;
    for (size_t i = 0; i < n; i++) {
        samples[i] = _buffer[(tail + i) & BUFFER_MASK];
    }
    _tail = static_cast<uint16_t>(tail + n);
    return n;
}

void SteamiIMP34DT05::clear() {
    _tail = _head;
}

uint32_t SteamiIMP34DT05::overruns() const {
    return _overruns;
}

void SteamiIMP34DT05::onReceive(ReceiveCallback callback) {
    IrqGuard guard;
    _callback = callback;
}

void SteamiIMP34DT05::resetStream() {
    IrqGuard guard;
    _head = 0;
    _tail = 0;
    _overruns = 0;
}

// --- Volume ------------------------------------------------------------------

bool SteamiIMP34DT05::setVolume(uint8_t volume) {
    IrqGuard guard;
    return _filter.setVolume(volume);
}

uint8_t SteamiIMP34DT05::volume() const {
    return _filter.volume();
}

// --- Sound level -------------------------------------------------------------

void SteamiIMP34DT05::setLevelWindowMs(uint16_t windowMs) {
    if (windowMs < IMP34DT05_LEVEL_WINDOW_MIN_MS) {
        windowMs = IMP34DT05_LEVEL_WINDOW_MIN_MS;
    } else if (windowMs > IMP34DT05_LEVEL_WINDOW_MAX_MS) {
        windowMs = IMP34DT05_LEVEL_WINDOW_MAX_MS;
    }
    IrqGuard guard;
    _windowMs = windowMs;
    _windowSamples = static_cast<uint32_t>(windowMs) * (IMP34DT05_SAMPLE_RATE_16K / 1000);
    _accSquares = 0;
    _accCount = 0;
    _accPeak = 0;
}

uint16_t SteamiIMP34DT05::levelWindowMs() const {
    return _windowMs;
}

void SteamiIMP34DT05::resetLevel() {
    IrqGuard guard;
    _accSquares = 0;
    _accCount = 0;
    _accPeak = 0;
    _meanSquare = 0.0f;
    _windowPeak = 0;
    _levelValid = false;
    _levelReady = false;
}

bool SteamiIMP34DT05::levelReady() const {
    return _levelReady;
}

SteamiIMP34DT05::LevelResult SteamiIMP34DT05::level() {
    float meanSquare;
    uint16_t peak;
    bool valid;
    {
        IrqGuard guard;
        meanSquare = _meanSquare;
        peak = _windowPeak;
        valid = _levelValid;
        _levelReady = false;
    }

    LevelResult result;
    if (!valid) {
        result.rms = NAN;
        result.peak = 0;
        result.dbfs = NAN;
        result.dbSpl = NAN;
        return result;
    }
    result.rms = sqrtf(meanSquare);
    result.peak = peak;
    result.dbfs = rmsToDbfs(result.rms);
    result.dbSpl = dbSplFromDbfs(result.dbfs);
    return result;
}

float SteamiIMP34DT05::rms() {
    return level().rms;
}

uint16_t SteamiIMP34DT05::peak() {
    return level().peak;
}

float SteamiIMP34DT05::levelDbfs() {
    return level().dbfs;
}

float SteamiIMP34DT05::levelDbSpl() {
    return level().dbSpl;
}

void SteamiIMP34DT05::setLevelOffset(float offsetDb) {
    _levelOffset = offsetDb;
}

float SteamiIMP34DT05::levelOffset() const {
    return _levelOffset;
}

float SteamiIMP34DT05::dbSplFromDbfs(float dbfs) const {
    return dbfsToDbSpl(dbfs, _filter.volume()) + _levelOffset;
}

// --- Frequency ---------------------------------------------------------------

float SteamiIMP34DT05::dominantFrequencyHz() {
    if (!_running) {
        return NAN;
    }
    clear();
    const uint32_t timeoutMs = IMP34DT05_FFT_SIZE * 1000UL / IMP34DT05_SAMPLE_RATE_16K + 100;
    uint32_t start = millis();
    while (available() < IMP34DT05_FFT_SIZE) {
        if (millis() - start > timeoutMs) {
            return NAN;
        }
        delay(1);
    }
    int16_t samples[IMP34DT05_FFT_SIZE];
    read(samples, IMP34DT05_FFT_SIZE);
    return computeDominantFrequency(samples, IMP34DT05_FFT_SIZE,
                                    static_cast<float>(IMP34DT05_SAMPLE_RATE_16K));
}

// --- Slot --------------------------------------------------------------------

bool SteamiIMP34DT05::setSlot(uint8_t slot) {
    if (slot == IMP34DT05_SLOT_AUTO) {
        IrqGuard guard;
        _slotMode = slot;
        if (_running) {
            _redetect = true;  // keep the current slot until the next start
        } else {
            _slotLocked = false;
        }
        return true;
    }
    if (slot >= IMP34DT05_SAI_SLOTS) {
        return false;
    }
    IrqGuard guard;
    _slotMode = slot;
    _redetect = false;
    if (_slot != slot || !_slotLocked) {
        _slot = slot;
        _filter.reset();
        _settleBlocks = IMP34DT05_SETTLE_BLOCKS;
    }
    _slotLocked = true;
    _streamAlive = true;
    return true;
}

uint8_t SteamiIMP34DT05::slot() const {
    return _slot;
}

// --- Conversion cost ---------------------------------------------------------

SteamiIMP34DT05::ConversionStats SteamiIMP34DT05::conversionStats() const {
    uint32_t blocks;
    uint64_t cycles;
    uint32_t maxCycles;
    {
        IrqGuard guard;
        blocks = _statBlocks;
        cycles = _statCycles;
        maxCycles = _statMaxCycles;
    }
    ConversionStats stats;
    stats.blocks = blocks;
    stats.averageCycles = blocks == 0 ? 0 : static_cast<uint32_t>(cycles / blocks);
    stats.maxCycles = maxCycles;
    return stats;
}

void SteamiIMP34DT05::resetConversionStats() {
    IrqGuard guard;
    _statBlocks = 0;
    _statCycles = 0;
    _statMaxCycles = 0;
}

// --- Pure helpers ------------------------------------------------------------

float SteamiIMP34DT05::computeRms(const int16_t* samples, size_t count) {
    if (count == 0) {
        return 0.0f;
    }
    uint64_t sum = 0;
    for (size_t i = 0; i < count; i++) {
        int32_t value = samples[i];
        sum += static_cast<uint64_t>(value * value);
    }
    return sqrtf(static_cast<float>(sum) / static_cast<float>(count));
}

float SteamiIMP34DT05::rmsToDbfs(float rms) {
    if (isnan(rms)) {
        return NAN;
    }
    if (rms <= 0.0f) {
        return IMP34DT05_LEVEL_FLOOR_DBFS;
    }
    float dbfs = 20.0f * log10f(rms * SQRT2_F / IMP34DT05_FULL_SCALE);
    return dbfs < IMP34DT05_LEVEL_FLOOR_DBFS ? IMP34DT05_LEVEL_FLOOR_DBFS : dbfs;
}

float SteamiIMP34DT05::dbfsToDbSpl(float dbfs, uint8_t volume) {
    if (volume == 0 || isnan(dbfs)) {
        return NAN;
    }
    float gainDb = 20.0f * log10f(2.0f * volume);
    return dbfs - gainDb - IMP34DT05_SENSITIVITY_DBFS + IMP34DT05_SENSITIVITY_REF_DB_SPL;
}

float SteamiIMP34DT05::computeDominantFrequency(const int16_t* samples, size_t count,
                                                float sampleRateHz) {
    if (count < 16 || count > IMP34DT05_FFT_MAX_SIZE || (count & (count - 1)) != 0) {
        return NAN;
    }

    float re[IMP34DT05_FFT_MAX_SIZE];
    float im[IMP34DT05_FFT_MAX_SIZE];

    // Remove the mean, apply a Hann window.
    float mean = 0.0f;
    for (size_t i = 0; i < count; i++) {
        mean += samples[i];
    }
    mean /= static_cast<float>(count);
    for (size_t i = 0; i < count; i++) {
        float window =
            0.5f - 0.5f * cosf(2.0f * PI_F * static_cast<float>(i) / static_cast<float>(count));
        re[i] = (static_cast<float>(samples[i]) - mean) * window;
        im[i] = 0.0f;
    }

    // In-place radix-2 FFT: bit-reversal permutation, then butterflies.
    for (size_t i = 1, j = 0; i < count; i++) {
        size_t bit = count >> 1;
        while ((j & bit) != 0) {
            j ^= bit;
            bit >>= 1;
        }
        j ^= bit;
        if (i < j) {
            float t = re[i];
            re[i] = re[j];
            re[j] = t;
            t = im[i];
            im[i] = im[j];
            im[j] = t;
        }
    }
    for (size_t len = 2; len <= count; len <<= 1) {
        float angle = -2.0f * PI_F / static_cast<float>(len);
        float stepRe = cosf(angle);
        float stepIm = sinf(angle);
        for (size_t start = 0; start < count; start += len) {
            float wRe = 1.0f;
            float wIm = 0.0f;
            for (size_t k = 0; k < len / 2; k++) {
                size_t a = start + k;
                size_t b = a + len / 2;
                float vRe = re[b] * wRe - im[b] * wIm;
                float vIm = re[b] * wIm + im[b] * wRe;
                re[b] = re[a] - vRe;
                im[b] = im[a] - vIm;
                re[a] += vRe;
                im[a] += vIm;
                float nextRe = wRe * stepRe - wIm * stepIm;
                wIm = wRe * stepIm + wIm * stepRe;
                wRe = nextRe;
            }
        }
    }

    // Strongest bin, DC excluded. Reuse re[] for the power spectrum.
    size_t half = count / 2;
    for (size_t k = 0; k <= half; k++) {
        re[k] = re[k] * re[k] + im[k] * im[k];
    }
    size_t best = 1;
    for (size_t k = 2; k < half; k++) {
        if (re[k] > re[best]) {
            best = k;
        }
    }
    if (!(re[best] > 0.0f)) {
        return NAN;  // silence
    }

    // Gaussian interpolation between the neighbours (exact for the
    // Gaussian-like main lobe of the Hann window).
    const float tiny = 1e-20f;
    float left = logf(re[best - 1] + tiny);
    float centre = logf(re[best] + tiny);
    float right = logf(re[best + 1] + tiny);
    float denominator = left - 2.0f * centre + right;
    float offset = 0.0f;
    if (denominator < 0.0f) {
        offset = 0.5f * (left - right) / denominator;
        if (offset > 0.5f) {
            offset = 0.5f;
        } else if (offset < -0.5f) {
            offset = -0.5f;
        }
    }
    return (static_cast<float>(best) + offset) * sampleRateHz / static_cast<float>(count);
}
