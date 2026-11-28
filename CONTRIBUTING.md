# Contributing

This repository provides Arduino/C++ drivers for the STeaMi board. Every
driver in the collection is expected to look and behave like every other —
one shape, one set of naming rules, one testing pattern — so that learning
one driver teaches you the whole library.

The source of truth for conventions is this document. When in doubt, take
[`lib/wsen-hids/`](lib/wsen-hids/) as a reference implementation: it exercises
almost every rule listed below.

## Getting started

```bash
make setup   # creates .venv/, installs PlatformIO + clang-format + clang-tidy + ruff,
             # runs npm install for the git hooks
```

Run `make help` for the full list of targets. The ones you'll use most:

| Target | What it does |
|--------|--------------|
| `make format-check` | Run `clang-format --dry-run` on `lib/ src/ tests/`. |
| `make format-fix` | Apply `clang-format -i` in place. |
| `make lint` | Meta-target: `format-check` + `check-spdx` + `clang-tidy` + `ruff`. |
| `make ruff` | Lint the Python tooling with `ruff check` (settings in `ruff.toml`, version pinned in `requirements.txt`). |
| `make tidy` | Run `clang-tidy` on every driver source under `lib/*/src/`. Regenerates `compile_commands.json` as needed. |
| `make check-spdx` | Verify every `.h`/`.cpp`/`.ino` carries the SPDX-License-Identifier header. |
| `make build` | Build the STeaMi firmware (`pio run`; `steami` is the default env). |
| `make test-native` | Run host-side unit tests (no board required). |
| `make test-hardware` | Run on-board unit tests (STeaMi required). |
| `make ci` | `lint + build + test-native` — the quick pre-push check. |
| `make upload` | Flash the firmware to a connected STeaMi. |

Running `make setup` also installs the husky git hooks: they validate the
branch name, the commit message, and run `clang-format` plus the SPDX
header check on every staged `.h`/`.cpp`/`.ino` before each commit. Run
`make setup` on a fresh clone so you don't get surprised by CI enforcing
things your local commit let through.

### Make target naming convention

Two shapes are in use, and they reflect the underlying directory structure:

* **Flat targets** (`build`, `lint`, `test-native`, `test-hardware`, `test-integration`, `setup`, …) are top-level actions that operate on the whole tree. The verb may be compound (`test-native`), but there's no sub-path.
* **Hierarchical targets** use `verb/<path>` whenever the action drills into one specific item:
  * `flash-<driver>/<example>` — flash one example (e.g. `flash-wsen-hids/DewPointMonitor`).
  * `capture-<driver>/<example>` — same, but route serial through `scripts/capture-serial.py` instead of opening miniterm.
  * `test-native/<suite>` / `test-hardware/<suite>` / `test-integration/<suite>` — run one test suite from a single tier. The suite is the directory name under `tests/<tier>/test_*` and isn't always a driver — `wire` and `led` cover mocks and board features rather than lib drivers.
  * `test/<suite>` — composite: chains every tier that exists for that suite.

The `/` separator (rather than another `-`) avoids visual ambiguity: drivers can themselves contain hyphens (`wsen-pads`, `wsen-hids`), and `flash-wsen-pads-altitude` would force the reader to guess where the boundary is. `flash-wsen-pads/altitude` is unambiguous and gives zsh's tab-completion a natural drill-down (`make flash-wsen-pads/<TAB>` lists only that driver's examples).

To discover the targets:

```bash
make list-examples         # all flash-* and capture-* targets
make list-tests            # all test-*/* and composite test/* targets
```

`list-examples` honors an optional `DRIVER=<name>` filter.

### Shell completion for `make` (zsh)

The hierarchical and per-suite targets above are generated dynamically via
`foreach + eval`. zsh's stock `_make` completion can resolve them for you,
but the relevant `zstyle` is off by default. Add to your `~/.zshrc`:

```zsh
zstyle ':completion:*:*:make:*:targets' call-command true
zstyle ':completion:*:*:make:*' tag-order 'targets'
```

The first line tells zsh to invoke `make` in dry-run / print-database
mode to query the resolved target list (instead of parsing the Makefile
textually, which misses generated targets). The second line filters out
variables and files from the completion output so `make <TAB>` only
shows actual targets.

`exec zsh` to reload, then `make flash-<TAB>` will list every example
across every driver, and `make test-<TAB>` every test target across the
three tiers.

A side effect of `call-command true`: every `make <TAB>` runs `make`
under the hood, which evaluates the Makefile — including any
`$(shell ...)` calls made at parse time. In this repo those are cheap
(`find` over `lib/` and `tests/`), but it means the zstyle should be
applied **per project** rather than globally if you also work on
untrusted repos. Scope it with a directory-specific `zstyle`, or set it
inside a per-project `.zshrc.local` sourced by your dotfiles.

If you don't want to touch your zshrc, `make list-examples` and
`make list-tests` print the same information on stdout.

## Driver structure

Each driver lives in its own directory under `lib/`:

```
lib/<component>/
├── README.md
├── library.properties        # Arduino Library Manager metadata
├── keywords.txt              # Arduino IDE syntax highlighting
├── src/
│   ├── Steami<Chip>.h        # Public header
│   ├── Steami<Chip>.cpp      # Implementation
│   └── Steami<Chip>_const.h  # Register addresses, bit masks, mode values
└── examples/
    └── <example_name>/
        └── <example_name>.ino
```

### Naming

The public header of a driver is `Steami<Chip>.h` (constants in `Steami<Chip>_const.h`) and the main class bound to the STeaMi board is `Steami<Chip>`, for example `SteamiBME280`. Several chips have a homonym in the Arduino Library Manager (BME280, APDS9960, MCP23009E, ssd1327, VL53L1X), and the Arduino resolver prefers a library whose name matches the header: the prefix avoids compiling the wrong library. A generic class that drives the chip on another board keeps the chip name (`GC9A01_SPI`). Register constants, enums and structs keep their chip prefix. The folder under `lib/` keeps the plain component name.

### Requirements

- The directory name is lowercase, using either hyphens or underscores
  as separators, and should match how the part is referred to in the
  datasheet. Current drivers mix both for historical reasons
  (`wsen-hids`, `wsen-pads` vs `daplink_flash`, `steami_config`) — pick
  whichever reads most naturally for the new driver and stay consistent
  within its own tree.
- The class bound to the STeaMi board is `Steami<Chip>` in `PascalCase`
  (`SteamiWsenHids`, `SteamiMCP23009E`) and lives in `src/Steami<Chip>.h`
  (see [Naming](#naming)). Generic classes meant for other boards
  (`SSD1327_SPI`, `MCP23009Pin`…) keep their chip names.
- Drivers are self-contained by default. When one driver does need to
  call into another's public API (e.g. the DAPLink family, where
  `daplink_flash` and `daplink_config` both speak through
  `daplink_bridge`), declare the dependency explicitly via the
  `depends=` field of the consumer's `library.properties`. This is
  what PlatformIO's library finder uses to resolve `-I` paths across
  libs and what the Arduino Library Manager surfaces to end users.
  Avoid implicit cross-driver `#include`s without a matching
  `depends=` — the build will work locally but break for downstream
  consumers.
- Every new C/C++ source (`.h`, `.cpp`, `.ino`) carries the SPDX license
  header on the very first line (see issue #104):
  ```
  // SPDX-License-Identifier: GPL-3.0-or-later
  ```
- Include guards use `#pragma once`, not `#ifndef`/`#define` wrappers.
- Every driver ships a `library.properties` (Arduino Library Manager
  metadata). Per the Arduino library specification the required fields
  are `name`, `version`, `author`, `maintainer`, `sentence`, `category`,
  `url`, and `architectures` — set `architectures=*` so host-side tests
  can pull the library in (the native platform has no "framework"). The
  collection convention is `author=STeaMi contributors` and
  `maintainer=STeaMi contributors`; the `url` points back to this
  repository. Optional but recommended: a longer `paragraph` describing
  the API, and an `includes=` line listing the public header. Stub
  drivers ship a `library.properties` with `version=0.0.0` to mark them
  as not yet implemented; bump to `1.0.0` on first release.
- When a driver becomes implemented (no longer a stub), add a section
  to `src/main.cpp` that `#include`s its header, instantiates it on
  the right bus, and calls `begin()` + a quick identity probe. The
  sketch is the build smoke test: it links every implemented driver
  in the steami env and lists every driver `.cpp` in
  `compile_commands.json` so `make tidy` runs them with the proper
  compile flags. New drivers that aren't wired in here will lint with
  default flags only (and miss e.g. a cross-driver include resolution).

### Example folders

- One folder per example, folder name in lower snake-case.
- The `.ino` file inside has the exact same name as the folder.
- A driver example builds its driver on the board's single internal I2C
  bus, `Steami.wire()`, and includes `<SteamiBoard.h>`. Never create a
  `TwoWire` on `I2C_INT_SDA` / `I2C_INT_SCL`: two `TwoWire` objects on the
  same I2C peripheral break each other. The default global `Wire` sits on
  different pins and will silently fail to detect on-board sensors.
- Wait for USB CDC enumeration before printing diagnostics:
  `while (!Serial && millis() < 2000) {}` — otherwise early output
  vanishes.

### Driver README

Every `lib/<component>/README.md` uses the same sections, in this order
([`lib/wsen-hids/README.md`](lib/wsen-hids/README.md) is the reference):

1. `# <Chip>` and a one-paragraph description.
2. `## Hardware`: bus, I2C address, what the chip does on the board.
3. `## Quick start`: a short sketch built on `Steami.wire()`.
4. `## Examples`: a table (example, what it does, **Try it** link to the
   wiki page when there is one), then `### Building an example` with the
   `make flash-<driver>/<example>` and `capture-` targets.
5. `## API`: one `###` subsection per group (Lifecycle, Reading, Modes,
   Calibration, ...), one table row per public method with its signature.
6. `## Register constants` (or `## Constants`): what `*_const.h` exports.
   Sections specific to the driver (limitations, firmware notes, JSON
   format, ...) go after it.
7. `## Testing`: the `make test-native/<suite>` command, and
   `make test-hardware/<suite>` with the board it needs when
   `tests/hardware/test_<suite>/` exists.
8. `## References`: the datasheet, the hardware page of the STeaMi wiki and
   the MicroPython counterpart in `steamicc/micropython-steami-lib`.
9. `## License`: `GPL-3.0-or-later — see [LICENSE](../../LICENSE).`

## Coding conventions

- **Naming**: `camelCase` for methods and variables, `UPPER_SNAKE_CASE`
  for constants, `PascalCase` for class names (and the driver's
  header/source files that match the class, e.g. `SteamiWsenHids.h`), lower
  snake-case for example and test folders, and lowercase (with either
  hyphens or underscores) for driver folders.
- **Constants**: `constexpr uint8_t` (preferred) or `#define` in
  `*_const.h`, never inside the public header file body.
- **Formatting**: enforced by `clang-format` (config in `.clang-format`).
  Google-based style, 4-space indent, column limit 100.
- **I2C**: accept a `TwoWire&` reference, default to the global `Wire`.
  Private helpers `readReg(reg)`, `writeReg(reg, value)`, and
  `readRegs(reg, buf, len)` are expected for any register-bank sensor.
  `readRegs` zero-fills its buffer before the bus access so a short
  read leaves defined bytes.
- **Constructor**:
  `Steami<Chip>(TwoWire& wire = Wire, uint8_t address = <DRIVER>_DEFAULT_ADDRESS)`.
- **Attributes**: `_wire` for the I2C bus, `_address` for the device
  address. Store `_wire` as a pointer internally (`TwoWire* _wire`) so
  the class stays default-assignable — the public constructor still
  accepts a `TwoWire&`.
- **No `Serial.print()` in library code**. Diagnostics go in examples,
  not in the driver itself. The only exception is the STeaMi Board logger:
  its default logger prints on `Serial` once it is started, and
  `Steami.setLogger()` replaces it.

## Driver API conventions

Every driver in the collection exposes the same shape. A user who
learned one driver should be able to guess the methods of another.
These were harmonised with the MicroPython sister library
([`steamicc/micropython-steami-lib`](https://github.com/steamicc/micropython-steami-lib))
so the Python and Arduino APIs match.

### Lifecycle

- `bool begin()` — probe the part (typically via WHO_AM_I), load any
  factory calibration, leave the device in a sensible default state.
  Returns `false` if the device can't be detected. **No** custom error
  enum.
- `uint8_t deviceId()` — returns the WHO_AM_I register value. The name
  is `deviceId()`, not `whoAmI()` or `readModelId()`.
- `void reset()` — hardware reset (toggle a reset pin). Skip if the
  part has no reset pin.
- `void softReset()` — software reset (write to the reset bit in a
  control register).
- `void reboot()` — reload trimming / calibration from non-volatile
  memory.
- `void powerOn()` / `void powerOff()` — flip the device's power-down
  bit. Both always implemented.

### Reading

- Bare-noun method per channel when the unit is obvious and standard:
  `temperature()` (°C), `humidity()` (%RH).
- Unit suffix for everything else: `pressureHpa()`, `distanceMm()`,
  `voltageMv()`, `accelerationG()`. Integers for raw counts, floats for
  physical units.
- **No** `read` / `get` prefix: `temperature()`, not `readTemperature()`
  or `getTemperature()`.
- `ReadResult read()` — combined reading returning a struct with every
  channel the sensor produces. Preferred when the caller needs several
  channels consistently.
- Readiness: `uint8_t status()` (private in most cases) returns the raw
  status register, `bool dataReady()` returns `true` when *all* the
  relevant channels are fresh, `bool temperatureReady()` / `bool
  humidityReady()` / … per-channel.

### Auto-trigger contract

If a measurement method is called while the device is powered down,
the driver **must** automatically trigger a one-shot conversion, poll
`dataReady()` with a bounded timeout, and return the result. Callers
should not have to manage modes to read a single value.

On timeout (bus failure, missing sensor), measurement methods return
`NAN` so a silent read of uninitialised bytes can't propagate stale
data. Callers can detect with `isnan()`.

### Modes

- `void setContinuous(uint8_t odr)` — continuous mode at the given
  Output Data Rate. ODR values live in `Steami<Chip>_const.h` as named
  constants (e.g. `WSEN_HIDS_ODR_1_HZ`).
- `void triggerOneShot()` — non-blocking: kick a single conversion.
- `ReadResult readOneShot()` — trigger + wait + return.

### Calibration hooks

Any driver that exposes a physical quantity (temperature, pressure,
humidity, …) provides:

- `void setXxxOffset(float offset)` — single-point correction: gain 1
  and this offset, applied after the factory curve (e.g.
  `setTemperatureOffset`).
- `bool calibrateXxx(float refLow, float measLow, float refHigh, float measHigh)`
  — two-point linear correction applied after the factory curve:
  `gain = (refHigh - refLow) / (measHigh - measLow)`,
  `offset = refLow - gain * measLow`. `measLow` and `measHigh` are
  readings taken without any user correction.

The driver keeps one user correction per channel, a gain and an offset,
and each call **replaces** the previous one: `setXxxOffset()` after
`calibrateXxx()` resets the gain to 1, and `calibrateXxx()` after
`setXxxOffset()` replaces the offset. Nothing stacks, so
`setXxxOffset(0)` always gives back the factory reading.

When `measLow == measHigh`, `calibrateXxx()` returns `false` and keeps
the previous correction, gain and offset unchanged. The MicroPython
drivers raise `ValueError` in that case, with the same effect.

Temperature-capable parts always implement `setTemperatureOffset` and
`calibrateTemperature`, with this contract (it matches
`set_temp_offset()` and `calibrate_temperature()` in MicroPython).
Extend the same pattern to other channels when it makes physical sense.
The native suite of each driver pins the contract: the offset replaces a
two-point calibration, a two-point calibration replaces the offset, and
equal points, starting from a gain different from 1, change nothing.

## Driver design patterns

Three situations came up in several drivers, and each one took a review
round to get right. Each pattern below says where it applies, what goes
wrong without it, the rule, and the code and native tests that lock it in.

### Wake the part before probing it

**Where:** parts with a software shutdown that also silences their I2C
interface, and a separate line that wakes them up. The BQ27441 fuel gauge
is the case in this library: its `SHUTDOWN` control command, woken by a
pulse on GPOUT.

**Symptom:** once the part is shut down (by `powerOff()`, or by a previous
run of the program), it no longer answers on I2C. A `begin()` that probes
`deviceId()` first reads 0 and returns `false`, and the part stays
unreachable until the next power cycle.

**Rule:**

- When the wake pin is wired, `begin()` runs the wake sequence, then waits
  for the part to settle, **before** the `deviceId()` probe.
- With no wake pin, that path does nothing at all: no `pinMode()`, no
  `digitalWrite()`, so the existing wiring keeps its behaviour.
- Hand the line back to its other use only after the probe. The BQ27441
  switches GPOUT back to `INPUT_PULLUP` so the caller can use it as the
  SOC / BAT_LOW interrupt.
- Lock the order in with native tests that check the pin state at the end
  of `begin()`, with and without a wake pin.

**Reference:** `SteamiBQ27441::begin()` and `disableShutdownMode()` in
[`lib/bq27441/src/SteamiBQ27441.cpp`](lib/bq27441/src/SteamiBQ27441.cpp);
`test_begin_pulses_gpout_low_high_when_pin_wired` and
`test_begin_does_not_touch_pins_when_no_gpout` in
[`tests/native/test_bq27441/test_main.cpp`](tests/native/test_bq27441/test_main.cpp).

### Defer interrupt work to `poll()`

**Where:** parts whose interrupt line only says that something happened,
so the driver has to read registers over I2C to find out what. The
MCP23009E expander is the case in this library: it reads `INTF` (which pin
fired) and `INTCAP` (its level at that moment).

**Symptom:** I2C transfers or user callbacks run from the interrupt
handler. STM32duino's `Wire` relies on interrupts itself, so a transfer
started from interrupt context can hang, and a user callback there delays
the other interrupts for as long as it runs.

**Rule:**

- The MCU-side interrupt handler only sets a flag,
  `volatile bool _irqPending = true;`: no I2C, no callback.
- A public `poll()`, called from `loop()`, clears the flag and runs the
  dispatch (`interruptEvent()`) in normal context. With no flag pending,
  `poll()` does nothing: no bus access, no callback.
- Attach the MCU-side interrupt on `CHANGE`, not `FALLING` or `RISING`:
  the polarity of the INT line is user-configurable (the chip's INTPOL
  bit), and a fixed edge would never fire with the other polarity.
- Classify the event from the chip's capture register (`INTCAP`), not
  from the live port register (`GPIO`): the pin may have changed again
  between the interrupt and `poll()`.
- An object that registers callbacks capturing `this` (`MCP23009Pin`)
  removes them in its destructor, so `poll()` never calls into a destroyed
  object.

**Reference:** `irqHandler()`, `poll()` and `interruptEvent()` in
[`lib/mcp23009e/src/SteamiMCP23009E.cpp`](lib/mcp23009e/src/SteamiMCP23009E.cpp);
`testInterruptEventUsesIntcapNotGpio`, `testPollIsNoOpWhenNoPending` and
`testPinDestructorClearsExpanderCallbackSlot` in
[`tests/native/test_mcp23009e/test_main.cpp`](tests/native/test_mcp23009e/test_main.cpp).
The `CHANGE` trigger has no native test: the `attachInterrupt()` mock does
not record the mode.

### Command protocols: the DAPLink bridge

**Where:** I2C peripherals implemented in firmware that take commands
instead of exposing a register bank. The case in this library is the I2C
bridge of the STM32F103 that runs DAPLink, used by `SteamiDaplinkBridge`,
`SteamiDaplinkFlash` and the config zone. Its behaviour comes from
[`steami_i2c.c`](https://github.com/steamicc/DAPLink/blob/1b7e90f6e27b223e9ec5c4355378570ac7044366/source/board/steami/steami_i2c.c)
and
[`steami32.c`](https://github.com/steamicc/DAPLink/blob/1b7e90f6e27b223e9ec5c4355378570ac7044366/source/board/steami/steami32.c)
in [`steamicc/DAPLink`](https://github.com/steamicc/DAPLink), branch
`release_letssteam`.

**How the bridge works:**

- There is no register pointer and no response register to select. Every
  write transaction is a command: the first byte is the opcode, then the
  firmware waits for the exact number of argument bytes that command takes
  (2 for `READ_SECTOR` and `READ_CONFIG`, 11 for `SET_FILENAME`, 31 for
  `WRITE_DATA` and `WRITE_CONFIG`, none for the others) and runs it once
  the last one arrives
  ([`get_argument_byte_number()`](https://github.com/steamicc/DAPLink/blob/1b7e90f6e27b223e9ec5c4355378570ac7044366/source/board/steami/steami_i2c.c#L50-L89)).
  `STATUS` (0x80) and `ERROR_STATUS` (0x81) are commands too, even though
  the firmware documents them as registers.
- Every write transaction, an address-only probe included, empties the
  answer buffer as soon as the address matches
  ([`HAL_I2C_AddrCallback()`](https://github.com/steamicc/DAPLink/blob/1b7e90f6e27b223e9ec5c4355378570ac7044366/source/board/steami/steami_i2c.c#L135-L148)).
- `WHO_AM_I`, `STATUS` and `ERROR_STATUS` fill the answer at once, from
  the I2C interrupt. The other commands are tasks that the F103 runs from
  its 30 ms main-loop hook; `READ_SECTOR`, `READ_CONFIG` (256 bytes each)
  and `GET_FILENAME` (11 bytes) fill the answer when their task runs.
  Until the task is done, `STATUS` reports busy and the firmware drops any
  new task command
  ([`on_I2C_receive_command()`](https://github.com/steamicc/DAPLink/blob/1b7e90f6e27b223e9ec5c4355378570ac7044366/source/board/steami/steami32.c#L140-L279)).
- A read transaction receives the answer from its first byte, sent from
  the main loop. The firmware then resets the answer length, and a read
  with no answer waiting gets a single 0xFF after 500 ms
  ([`steami_i2c_process()`](https://github.com/steamicc/DAPLink/blob/1b7e90f6e27b223e9ec5c4355378570ac7044366/source/board/steami/steami_i2c.c#L282-L326)).
- Reading `ERROR_STATUS` returns the error register and clears it. Every
  command also clears its "last command failed" bit (0x80) when it ends, a
  `STATUS` poll included.

**Rule:**

- `sendCommand(cmd, args, argLen)` sends `[cmd | args...]` in one write,
  with exactly the argument count the firmware expects. A shorter frame
  leaves the firmware waiting for the rest; fixed-size frames
  (`WRITE_DATA`, `WRITE_CONFIG`) are zero-padded to their 31 argument
  bytes.
- The one-byte queries (`WHO_AM_I`, `STATUS`, `ERROR_STATUS`) write the
  opcode, then read one byte (`readReg()`). This looks like a register read
  but is a command followed by a read of its answer.
- `readResponse(buf, len)` is a bare `requestFrom()`: nothing is written
  before it, the opcode included. Sending the opcode again would be a new
  command: it empties the answer and, for `READ_SECTOR` or `READ_CONFIG`,
  leaves the firmware waiting for argument bytes.
- Between the command and the read of its answer, send nothing: no
  `STATUS` poll, no `waitReady()`, no address probe. Wait with `delay()`
  long enough for the task to run on a 30 ms tick
  (`DAPLINK_FLASH_RESPONSE_DELAY_MS`, `DAPLINK_BRIDGE_READ_CONFIG_DELAY_MS`).
- Read the whole answer in one transaction. A second read does not pick
  up where the first one stopped: the firmware resets the answer length as
  soon as it starts sending it, and the native fake models a repeated read
  as starting again at byte 0. When an answer is longer than one read can
  be (STM32duino 2.x caps `requestFrom()` at 255 bytes), send the command
  again for each chunk with an argument that moves the start:
  `readConfig()` sends `READ_CONFIG` again with the next offset and skips
  the bytes it already has. `readResponse()` refuses more than 255 bytes on
  STM32duino 2.x; the PlatformIO build uses STM32duino 3.1.0, where
  `readSector()` reads its 256 bytes at once.
- Check the outcome by reading `ERROR_STATUS` first, then `STATUS`
  (`commandResult()`), since any other command clears the "last command
  failed" bit. For a command with an answer, read the answer first, then
  `ERROR_STATUS` (`readSector()`).
- Retry `readReg()` for a bounded time while the bridge NACKs (it does so
  for about one main-loop tick after a 256-byte answer cut short), but do
  not retry an `ERROR_STATUS` read that the bridge accepted: it has
  already cleared the register.

**Reference:** `sendCommand()`, `readResponse()`, `readReg()`,
`commandResult()` and `readConfig()` in
[`lib/daplink_bridge/src/SteamiDaplinkBridge.cpp`](lib/daplink_bridge/src/SteamiDaplinkBridge.cpp);
`getFilename()` and `readSector()` in
[`lib/daplink_flash/src/SteamiDaplinkFlash.cpp`](lib/daplink_flash/src/SteamiDaplinkFlash.cpp).
The firmware model is
[`tests/native/helpers/FakeDaplinkBridge.h`](tests/native/helpers/FakeDaplinkBridge.h).
The rules are locked by `test_read_sector_returns_256_bytes` (one 256-byte
read) in
[`tests/native/test_daplink_flash/test_main.cpp`](tests/native/test_daplink_flash/test_main.cpp),
and by `test_write_config_sends_fixed_32_byte_frames`,
`test_read_config_reads_past_256_bytes`,
`test_read_config_reaches_end_of_zone` and
`test_write_config_returns_false_on_device_error` in
[`tests/native/test_daplink_bridge/test_main.cpp`](tests/native/test_daplink_bridge/test_main.cpp).

## Testing

Two complementary environments, both wired into PlatformIO:

- **Native tests** under `tests/native/test_<driver>/` run on the host
  using the Arduino + Wire mocks in
  [`tests/native/helpers/`](tests/native/helpers/). They're fast,
  hardware-free, and required for every new driver — they pin the
  contract (calibration math, auto-trigger, register writes, timeout
  handling) so future refactors don't regress silently.
- **Hardware tests** under `tests/hardware/test_<driver>/` run on a
  real STeaMi via `pio test -e steami` when you need to validate bus
  timing or interrupt behaviour.

See [`tests/TESTING.md`](tests/TESTING.md) for the mock Wire API
(`setRegister`, `getWrites`, `clearWrites`, per-address keying). The
WSEN-HIDS suite ([`tests/native/test_wsen_hids/`](tests/native/test_wsen_hids/))
is a good reference.

## Commit messages

Format enforced by [commitlint](https://commitlint.js.org/) via a git
hook (and the `check-commits` workflow on every PR):

```
<type>[(<scope>)]: <Description starting with a capital letter ending with a period.>
```

**Types**: `feat`, `fix`, `docs`, `style`, `refactor`, `test`, `ci`,
`build`, `chore`, `perf`, `revert`, `tooling`.

**Scopes** (optional): driver names (`wsen-hids`, `ism330dl`, `wsen-pads`, …)
or domains (`ci`, `docs`, `style`, `tests`, `tooling`, `boards`). Recommended for
driver-specific changes, omit for cross-cutting ones.

Header length capped at 78 characters. Body optional but encouraged —
explain the *why*, not the *what* (the diff shows the what).

### Examples

```
feat(wsen-hids): Add driver with temperature and humidity reading.
fix(wsen-pads): Correct pressure conversion formula.
docs: Update README driver table.
test(mcp23009e): Add unit tests for GPIO read.
```

## Branches

Branch names must match `<type>/<kebab-case-description>`, with `type`
restricted to the subset allowed for working branches:

```
main
feat|fix|docs|tooling|ci|test|style|chore|refactor/<lowercase-with-hyphens>
release/vX.Y.Z
```

Uppercase, underscores, or other types will be rejected by the local
pre-commit hook (`validate-branch-name`). Examples:

- `feat/wsen-hids-driver` ✓
- `docs/add-gplv3-license` ✓
- `build/platformio-steami` ✗ (`build` isn't in the branch-name
  whitelist even though it's a valid commit type)
- `docs/add-GPLV3-license` ✗ (uppercase `GPLV3`)

## Workflow

1. `make setup` on a fresh clone so the hooks are active.
2. Branch from `main`, matching the branch-name rule above.
3. Write the code, add or extend the native test suite, update the
   driver README and `keywords.txt`.
4. `make ci` locally — catches format drift, build breaks, and native
   test regressions before they hit the CI runner.
5. Commit. The pre-commit hook runs clang-format on staged files and
   validates the branch name; the commit-msg hook runs commitlint.
6. Push and open a PR. Fill the PR template checklist honestly — leave
   the "Tested on hardware" box unchecked if you didn't flash your
   branch.

## Continuous integration

Every PR runs one GitHub Actions workflow, `ci.yml`, which calls the
others as reusable workflows:

| Job | Runs |
|-----|------|
| `changements` | `scripts/exemples-concernes.py` — lists the examples the PR can break. |
| `lint` (`lint.yml`) | `make lint` — clang-format + SPDX header check + clang-tidy + ruff, and the Spleen tables check. |
| `tests` (`tests.yml`) | `make test-native`, the display sanitizers, the mockup review and the tooling tests (`scripts/tests`). |
| `arduino-package` (`arduino-package-check.yml`) | Builds and lints the Library Manager package, then compiles the selected examples with arduino-cli. |
| `build` (`build.yml`) | `make build`, then one selected example per driver with PlatformIO, and the footprint check when `steami_board` or a display driver is concerned. |
| `ci-ok` | Passes when every job above passed or was skipped by the selection. |

The selection works per example: an example is compiled when a driver it
includes, directly or through another driver (`SteamiBoard.h` pulls in
13 drivers), is touched by the PR. A change to a shared or unknown file
(`platformio.ini`, `boards/`, the compile scripts, `ci.yml` and the
workflows it calls…) compiles everything. A change to the package
metadata alone (`keywords.txt`, `scripts/arduino-package/`) builds and
lints the package without compiling, and a change that cannot affect a
compile (docs, Markdown, native tests…) compiles nothing. The rules
live in `scripts/exemples-concernes.py`. On a push to `main`, only `lint`,
`tests` and `caches` run: the PR already compiled its examples, and the
publication compiles all of them again at the tag.

The toolchains are cached: `~/.arduino15` (arduino-cli, pinned in the
workflows, with the STM32 core and STM32duinoBLE) and `~/.platformio`.
Only the `caches` job of `ci.yml` saves them, on `main`, where it installs
the toolchains without compiling; every other job only restores them. A
cache saved on `main` is the one PRs and the merge queue can see. The
keys change with the pinned versions and with `platformio.ini`,
`boards/steami.json` or `requirements.txt`: the first PR after such a
change misses the cache and installs everything, until its merge warms
the new key. `scripts/tests/test_workflow_versions.py` checks the pins,
the keys and that nothing else saves these caches.

`ci-ok` is the only check that matters: once the repository ruleset
requires it, it is the check that gates the merge. If `changements`
fails, `ci-ok` fails too, so that skipped compiles never count as a
pass. `check-commits.yml` (`commitlint` on every commit in the PR range,
and the branch name) and CodeQL run beside `ci.yml`.

## Notes

- Keep implementations simple and readable; the reference drivers
  deliberately skip clever tricks.
- Follow the existing drivers — when a new situation isn't covered
  here, look at how `wsen-hids` handled the equivalent.
- Every driver ships a README with an API table, at least one example
  sketch, and a `keywords.txt` for Arduino IDE syntax highlighting.
- Match the MicroPython sister-library API wherever possible for
  cross-language consistency.
