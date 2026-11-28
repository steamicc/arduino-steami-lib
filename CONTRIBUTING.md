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

- `void setXxxOffset(float offset)` — additive offset on top of the
  factory calibration (e.g. `setTemperatureOffset`).
- `void calibrateXxx(float refLow, float measLow, float refHigh, float measHigh)`
  — two-point linear correction applied after the factory curve.

Temperature-capable parts always implement `setTemperatureOffset` and
`calibrateTemperature`. Extend the same pattern to other channels when
it makes physical sense.

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
