# Contributing

This repository provides Arduino/C++ drivers for the STeaMi board. Every
driver in the collection is expected to look and behave like every other —
one shape, one set of naming rules, one testing pattern — so that learning
one driver teaches you the whole library.

The source of truth for conventions is this document. When in doubt, take
[`lib/hts221/`](lib/hts221/) as a reference implementation: it exercises
almost every rule listed below.

## Getting started

```bash
make setup   # creates .venv/, installs PlatformIO + clang-format + clang-tidy,
             # runs npm install for the git hooks
```

Run `make help` for the full list of targets. The ones you'll use most:

| Target | What it does |
|--------|--------------|
| `make format-check` | Run `clang-format --dry-run` on `lib/ src/ tests/`. |
| `make format-fix` | Apply `clang-format -i` in place. |
| `make lint` | Meta-target: `format-check` + `check-spdx` + `clang-tidy`. |
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

### Shell completion for `make` (zsh)

Many of the most useful targets are generated dynamically via
`foreach + eval` — `flash-<driver>/<example>`, `capture-<driver>/<example>`,
`test-native-<driver>`, `test-hardware-<driver>`, `test-integration-<driver>`,
`test-<driver>`. zsh's stock `_make` completion can resolve these for you,
but the relevant `zstyle` is off by default. Add to your `~/.zshrc`:

```zsh
zstyle ':completion:*:*:make:*:targets' call-command true
zstyle ':completion:*:*:make:*' tag-order 'targets'
```

The first line tells zsh to invoke `make -nsp` to get the resolved
target database (instead of parsing the Makefile textually, which misses
generated targets). The second line filters out variables and files from
the completion output so `make <TAB>` only shows actual targets.

`exec zsh` to reload, then `make flash-<TAB>` will list every example
across every driver, and `make test-<TAB>` every test target across the
three tiers.

If you don't want to touch your zshrc, `make list` and `make list-examples`
print the same information on stdout.

## Driver structure

Each driver lives in its own directory under `lib/`:

```
lib/<component>/
├── README.md
├── library.properties        # Arduino Library Manager metadata
├── keywords.txt              # Arduino IDE syntax highlighting
├── src/
│   ├── <DriverName>.h        # Public header
│   ├── <DriverName>.cpp      # Implementation
│   └── <DriverName>_const.h  # Register addresses, bit masks, mode values
└── examples/
    └── <example_name>/
        └── <example_name>.ino
```

### Requirements

- The directory name is lowercase, using either hyphens or underscores
  as separators, and should match how the part is referred to in the
  datasheet. Current drivers mix both for historical reasons
  (`wsen-hids`, `wsen-pads` vs `daplink_flash`, `steami_config`) — pick
  whichever reads most naturally for the new driver and stay consistent
  within its own tree.
- The class name is `PascalCase` (`HTS221`, `Mcp23009e`) and lives in
  `src/<DriverName>.h`.
- Each driver is self-contained — no cross-driver dependencies.
- Every new C/C++ source (`.h`, `.cpp`, `.ino`) carries the SPDX license
  header on the very first line (see issue #104):
  ```
  // SPDX-License-Identifier: GPL-3.0-or-later
  ```
- Include guards use `#pragma once`, not `#ifndef`/`#define` wrappers.
- `library.properties` declares `architectures=*` so host-side tests can
  pull the library in (the native platform has no "framework").

### Example folders

- One folder per example, folder name in lower snake-case.
- The `.ino` file inside has the exact same name as the folder.
- Every sketch targets the STeaMi **internal** I2C bus when talking to
  on-board peripherals: `TwoWire internalI2C(I2C_INT_SDA, I2C_INT_SCL);`
  and pass it to the driver. The default global `Wire` sits on different
  pins and will silently fail to detect on-board sensors.
- Wait for USB CDC enumeration before printing diagnostics:
  `while (!Serial && millis() < 2000) {}` — otherwise early output
  vanishes.

## Coding conventions

- **Naming**: `camelCase` for methods and variables, `UPPER_SNAKE_CASE`
  for constants, `PascalCase` for class names (and the driver's
  header/source files that match the class, e.g. `HTS221.h`), lower
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
  `DriverName(TwoWire& wire = Wire, uint8_t address = <DRIVER>_DEFAULT_ADDRESS)`.
- **Attributes**: `_wire` for the I2C bus, `_address` for the device
  address. Store `_wire` as a pointer internally (`TwoWire* _wire`) so
  the class stays default-assignable — the public constructor still
  accepts a `TwoWire&`.
- **No `Serial.print()` in library code**. Diagnostics go in examples,
  not in the driver itself.

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
  Output Data Rate. ODR values live in `<driver>_const.h` as named
  constants (e.g. `HTS221_ODR_1_HZ`).
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
HTS221 suite ([`tests/native/test_hts221/`](tests/native/test_hts221/))
is a good reference.

## Commit messages

Format enforced by [commitlint](https://commitlint.js.org/) via a git
hook (and the `check-commits` workflow on every PR):

```
<type>[(<scope>)]: <Description starting with a capital letter ending with a period.>
```

**Types**: `feat`, `fix`, `docs`, `style`, `refactor`, `test`, `ci`,
`build`, `chore`, `perf`, `revert`, `tooling`.

**Scopes** (optional): driver names (`hts221`, `ism330dl`, `wsen-pads`, …)
or domains (`ci`, `docs`, `style`, `tests`, `tooling`). Recommended for
driver-specific changes, omit for cross-cutting ones.

Header length capped at 78 characters. Body optional but encouraged —
explain the *why*, not the *what* (the diff shows the what).

### Examples

```
feat(hts221): Add driver with temperature and humidity reading.
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

- `feat/hts221-driver` ✓
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

Every PR runs four GitHub Actions workflows in parallel:

| Workflow | Runs |
|----------|------|
| `build.yml` | `make setup` + `make build` — firmware compile check. |
| `tests.yml` | `make test-native` — host-side test suite. |
| `lint.yml` | `make lint` — clang-format + SPDX header check + clang-tidy. |
| `check-commits.yml` | `commitlint` on every commit in the PR range. |

All four need to go green before a merge.

## Notes

- Keep implementations simple and readable; the reference drivers
  deliberately skip clever tricks.
- Follow the existing drivers — when a new situation isn't covered
  here, look at how `hts221` handled the equivalent.
- Every driver ships a README with an API table, at least one example
  sketch, and a `keywords.txt` for Arduino IDE syntax highlighting.
- Match the MicroPython sister-library API wherever possible for
  cross-language consistency.
