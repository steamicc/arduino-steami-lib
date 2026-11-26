# IMP34DT05

Arduino/C++ driver for the STMicroelectronics
[IMP34DT05](https://www.st.com/en/mems-and-sensors/imp34dt05.html) digital
MEMS microphone of the STeaMi board (all releases: V1a, V1b and V1c).

The chip is an **IMP**34DT05. The driver folder is called `im34dt05` for
historical reasons (it was created with that spelling); the header, the class
and the constants use the real name: `SteamiIMP34DT05.h`,
`SteamiIMP34DT05`, `IMP34DT05_*`.

## Hardware

* Omnidirectional MEMS microphone with a **PDM** output (pulse density
  modulation): the microcontroller sends a clock, the microphone answers one
  bit per clock period, the density of ones follows the sound pressure.
  There is no register, no I2C address and no identification code.
* `MIC_CLK` = **PA3** (`SAI1_CK1`), `MIC_IN` = **PA10** (`SAI1_D1`), both on
  alternate function 3 of the STM32WB55. The `LR` pin of the microphone is
  tied to ground.
* The driver uses the **PDM interface of SAI1** (block A, master receiver,
  one microphone pair on `D1`) and **DMA1 channel 1** in circular mode:
  * SAI1 kernel clock: PLLSAI1 "P" output at 64 MHz (PLL input 8 MHz on the
    STeaMi, N = 16, P = 2). The board's `SystemClock_Config()` is not
    changed: PLLSAI1 is otherwise unused;
  * SAI bit clock 64 MHz / 25 = 2.56 MHz, so **1.28 MHz on `MIC_CLK`** (the
    IMP34DT05 accepts 1.2 to 3.25 MHz in normal mode);
  * each 16-bit SAI frame holds one byte for each microphone of the pair
    (one per clock edge); `begin()` finds which slot carries the STeaMi
    microphone (the other one only sees an undriven line).
* Signal chain, in the DMA interrupt every millisecond: PDM bits ->
  sinc3 filter (decimation 8, lookup tables) -> sinc4 FIR (decimation 10,
  sets the volume) -> high-pass filter (DC removal) -> **16 kHz, 16-bit PCM**.
  The PDM to PCM filter is ST's, see [Credits](#credits).
* Datasheet sensitivity: -26 dBFS for 94 dB SPL at 1 kHz, acoustic overload
  point 122.5 dB SPL, signal-to-noise ratio 64 dB.

## Quick start

```cpp
#include <SteamiIMP34DT05.h>

SteamiIMP34DT05 mic;

void setup() {
    Serial.begin(115200);

    if (!mic.begin()) {
        Serial.println("Microphone not detected");
        while (true) delay(1000);
    }
    mic.setLevelWindowMs(200);  // one level value every 200 ms
}

void loop() {
    if (mic.levelReady()) {
        Serial.print(mic.levelDbfs());
        Serial.println(" dBFS");
    }
}
```

With the `STeaMi` Library Manager package, `#include <SteamiIMP34DT05.h>` (or
`#include <STeaMi.h>`) is enough. The microphone is not on the I2C bus: no
`Steami.wire()` here.

## Examples

| Example                                      | What it does                                                                                 |
| -------------------------------------------- | -------------------------------------------------------------------------------------------- |
| [`SoundLevel`](examples/SoundLevel/)         | Print the RMS level, the dBFS level and an estimated dB SPL value five times per second.     |
| [`ClapDetector`](examples/ClapDetector/)     | Toggle the blue LED on each hand clap (sharp peak above the background noise).               |
| [`FrequencyMeter`](examples/FrequencyMeter/) | Show the dominant frequency and the nearest note (A4 = 440 Hz), like a tuner.                |
| [`BuzzerSelfTest`](examples/BuzzerSelfTest/) | Play tones on the buzzer and check that the microphone hears the right frequency.            |

### Building an example

```bash
make list-examples DRIVER=im34dt05
make flash-im34dt05/SoundLevel
```

This builds, uploads, and opens the serial monitor at 115200 baud. Swap
`flash-` for `capture-` to reliably capture the first lines printed at boot:

```bash
make capture-im34dt05/BuzzerSelfTest DURATION=15
```

## API

All methods follow the collection conventions: `camelCase`, units in method
names only when they carry ambiguity, no `read` / `get` prefixes. The API is
close to the Arduino `PDM` library (`begin`, `onReceive`, `available`,
`read`), with samples counted in `int16_t` rather than in bytes.

### Lifecycle

| Method | Description |
|--------|-------------|
| `SteamiIMP34DT05()` | Construct. One instance per sketch: the board has one microphone. |
| `bool begin(uint32_t sampleRate = IMP34DT05_SAMPLE_RATE_16K)` | Configure SAI1, start the clock, find the microphone slot and wait for the first samples (about 50 ms). Returns `false` for another sample rate, on another board, or when the data line stays flat (no microphone). |
| `void end()` | Stop and release SAI1, the DMA channel and the pins. |
| `void powerOn()` / `void powerOff()` | Restart / stop the PDM clock. Without clock the microphone sleeps. `powerOn()` waits for it to wake up. |
| `bool isRunning()` | `true` while the clock runs. |
| `uint32_t sampleRate()` | Always 16000. |

The IMP34DT05 has no registers: there is no `deviceId()`, `reset()`,
`softReset()` or `reboot()`.

### Samples

The DMA interrupt produces 16 samples every millisecond. They go to a
1024-sample buffer (64 ms) read with `read()`, and to the `onReceive()`
callback.

| Method | Description |
|--------|-------------|
| `size_t available()` | Samples waiting in the buffer. |
| `bool dataReady()` | `available() > 0`. |
| `size_t read(int16_t* samples, size_t count)` | Copy up to `count` waiting samples, oldest first. Never blocks; returns the number copied. |
| `void clear()` | Drop the waiting samples. |
| `uint32_t overruns()` | Samples lost because the buffer was full. |
| `void onReceive(ReceiveCallback callback)` | `void callback(const int16_t* samples, size_t count)`, called **from the interrupt** with each block. Keep it short (no `Serial`, no `delay()`). `nullptr` removes it. |

### Volume

| Method | Description |
|--------|-------------|
| `bool setVolume(uint8_t volume)` | Digital gain of the FIR stage, `0` (mute) to `6`: the signal is multiplied by `2 * volume`. Default `5` (x10, +20 dB) as in ST's examples. Returns `false` when out of range. |
| `uint8_t volume()` | Current volume. |

At volume 5 a 94 dB SPL tone reads about -6 dBFS and the output clips above
about 100 dB SPL: lower the volume for loud sounds (a buzzer right next to the
microphone, a clap at a few centimetres).

### Sound level

The interrupt measures the samples over a window (100 ms by default). The
methods return the values of the last **completed** window, `NAN` before the
first one.

| Method | Description |
|--------|-------------|
| `void setLevelWindowMs(uint16_t windowMs)` | Window length, 1 to 4000 ms. Short (10 ms) to catch claps, long (200 ms or more) for a steady reading. |
| `uint16_t levelWindowMs()` | Current window length. |
| `bool levelReady()` | `true` when a window completed since the last level reading. |
| `float rms()` | RMS amplitude, PCM units (0 to 32768). |
| `uint16_t peak()` | Largest absolute sample of the window. |
| `float levelDbfs()` | `20 * log10(rms * sqrt(2) / 32768)`: 0 dBFS for a full-scale sine (AES17 convention, the one of the datasheet sensitivity). Never below -120. |
| `float levelDbSpl()` | Estimate in dB SPL: `levelDbfs() - 20 * log10(2 * volume) + 26 + 94 + offset`. **Uncalibrated** (sensitivity tolerance, no A-weighting): compare with a sound level meter. `NAN` at volume 0. |
| `LevelResult level()` | `{rms, peak, dbfs, dbSpl}` of the same window, at once. |
| `void setLevelOffset(float offsetDb)` | Additive correction applied by `levelDbSpl()`. |

### Frequency

| Method | Description |
|--------|-------------|
| `float dominantFrequencyHz()` | Record 512 samples (32 ms) and return the frequency of the strongest component, about 1 Hz resolution. Blocks about 35 ms and clears the samples waiting for `read()`. `NAN` when the microphone is stopped or the sound is exactly zero; check the level first, in silence the "dominant" frequency is noise. |
| `static float computeDominantFrequency(const int16_t* samples, size_t count, float sampleRateHz)` | Same on your own samples: Hann window, radix-2 FFT, Gaussian interpolation of the peak. `count` is a power of two from 16 to 512. |
| `static float computeRms(const int16_t* samples, size_t count)` | RMS of a buffer. |
| `static float rmsToDbfs(float rms)` | dBFS as `levelDbfs()`. |

The frequency is measured against the PDM clock, derived from the internal
16 MHz oscillator (HSI): expect about 1 % of absolute error.

### Microphone slot

| Method | Description |
|--------|-------------|
| `uint8_t slot()` | SAI slot (0 or 1) found by `begin()`. |
| `bool setSlot(uint8_t slot)` | Force slot 0 or 1 (skips the detection), or `IMP34DT05_SLOT_AUTO` to detect again at the next `begin()`. |

## Constants

`SteamiIMP34DT05_const.h` exports the audio format (`IMP34DT05_SAMPLE_RATE_16K`,
`IMP34DT05_PDM_CLOCK_HZ`, `IMP34DT05_SAMPLES_PER_BLOCK`), the volume range
(`IMP34DT05_VOLUME_*`), the SAI clocking (`IMP34DT05_SAI_*`), the buffer and
window sizes and the datasheet sensitivity.

## Limitations

* 16 kHz only (PDM clock 1.28 MHz, decimation 80, the configuration ST's
  filter is built for).
* The driver owns SAI1, PLLSAI1 (its "P" output), DMA1 channel 1 and its
  interrupt handler `DMA1_Channel1_IRQHandler()`: a sketch or another
  library using them cannot run at the same time.
* The conversion runs in the DMA interrupt (a small part of the CPU time at
  the STeaMi's 32 MHz clock). A sketch that masks interrupts for more than
  about 1 ms loses audio.
* `levelDbSpl()` is an estimate, not a calibrated sound level meter.
* STM32WB55 only: on another board `begin()` returns `false` (the sources
  still compile, for host tests and for the Library Manager package).

## Testing

```bash
make test-native/im34dt05   # filter, level, buffer, slot detection, FFT
make test-examples/im34dt05
make build
make lint
```

The native suite feeds the filter and the driver with PDM streams made by a
second-order sigma-delta modulator (what a MEMS microphone contains), checks
ST's lookup tables against the sinc3 coefficients, the gain of each volume,
DC removal, saturation, the slot detection, the level window and the
frequency estimate end to end. The hardware suite in
`tests/hardware/test_im34dt05/` needs a STeaMi.

## Credits

The PDM to PCM filter (`SteamiPdmFilter`) and the structure of the SAI/DMA
capture (`SteamiPdmSai`) are adapted from STMicroelectronics'
[X-NUCLEO-IKS02A1-Audio](https://github.com/stm32duino/X-NUCLEO-IKS02A1-Audio)
library (version 1.1.1), which supports the IMP34DT05 on the NUCLEO-F401RE
(I2S3) and NUCLEO-L476RG (SAI1). Those two files keep ST's BSD-3-Clause
licence and copyright notice (`SPDX-License-Identifier: BSD-3-Clause`); the
changes are listed at the top of each file. The SAI1 PDM settings follow ST's
STM32WB5MM-DK board support package.

## License

GPL-3.0-or-later - see [LICENSE](../../LICENSE), except `SteamiPdmFilter.*`
and `SteamiPdmSai.*`: BSD-3-Clause (STMicroelectronics), notice in the files.
