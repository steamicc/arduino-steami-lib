# Testing

## Overview

This repository uses PlatformIO's Unity testing framework to validate Arduino
drivers for the STeaMi board through three complementary test tiers:

* **Native tests (`tests/native/`)**
  Run on the host machine using mocks. Fast, deterministic desktop tests for
  driver logic.

* **Hardware unit tests (`tests/hardware/`)**
  Run directly on a connected STeaMi board to validate atomic driver contracts
  on real silicon.

* **Integration tests (`tests/integration/`)**
  Run on a connected STeaMi board over longer acquisition scenarios to validate
  runtime behaviour over time.

This layered strategy provides:

* fast logic validation without hardware,
* direct on-board contract validation,
* end-to-end behavioural confidence under realistic usage.

---

## Test Structure

```text
tests/
├── native/
│   └── test_<driver>/
│       └── test_main.cpp
├── hardware/
│   └── test_<driver>/
│       └── test_main.cpp
├── integration/
│   └── test_<driver>/
│       └── test_main.cpp
├── shared/
│   └── driver_checks.h
└── TESTING.md
```

Rules:

* Each suite lives inside a folder named `test_<driver>`.
* Each suite contains a `test_main.cpp`.
* All tests use the Unity framework.
* Native, hardware, and integration suites for the same driver should reuse the
  same driver name.

---

## Running Tests

The Makefile generates one phony target per suite under each tier (via
`foreach + eval`, same shape as `flash-<driver>/<example>`). The convention
is `verb/<target>` whenever there's a sub-path: tab-completion on zsh/bash
works for `make test-<tier>/<TAB>` and adding a new suite directory picks
up automatically — no Makefile edits. `make list-tests` prints every
discoverable test target across the three tiers plus the composite.

### Native tests (desktop, mock-based)

```bash
make test-native              # all native suites
make test-native/hts221       # one suite — auto-discovered from tests/native/test_hts221/
make test-native/led
make test-native/wire
```

No hardware required. CI-friendly.

### Hardware unit tests (real board)

```bash
make test-hardware            # all hardware suites
make test-hardware/hts221     # one suite — auto-discovered from tests/hardware/test_hts221/
make test-hardware/led
```

Requires a connected STeaMi over CMSIS-DAP. If no board is detected the recipe
prints `STeaMi not detected — skipping hardware tests.` and exits 0, so a CI
script that calls these without a board attached doesn't fail.

### Integration tests (real board, longer scenarios)

```bash
make test-integration         # all integration suites
make test-integration/hts221  # one suite — auto-discovered from tests/integration/test_hts221/
```

Requires a board (same skip behaviour as hardware unit tests). Integration
suites are intentionally slower than hardware unit tests because they observe
repeated measurements over time — typically tens of seconds per suite.

### Composite per-suite target

`make test/<suite>` chains whichever tiers exist for that suite:

```bash
make test/hts221              # native + hardware-unit + integration
make test/led                 # native + hardware-unit
make test/wire                # native only
```

The placeholder is `<suite>` rather than `<driver>` because some suites
cover board features or test infrastructure (e.g. `wire`, `led`) that
don't have a corresponding driver under `lib/`.

### Direct PlatformIO invocations

For ad-hoc runs outside the Makefile:

```bash
pio test -e native
pio test -e steami                                    # hardware + integration
pio test -e steami --filter hardware/test_<driver>
pio test -e steami --filter integration/test_<driver>
```

---

## Debugging a failing on-board test

By default, on-board test output is intentionally minimal: Unity captures the
serial channel and only prints test names with a final PASS/FAIL status.

This keeps output clean, but makes debugging harder.

### Verbose mode

To see the full Unity stream and debug messages, set `VERBOSE=1` on any
on-board test target — hardware or integration, full suite or per-driver:

```bash
make test-hardware/<suite> VERBOSE=1
make test-integration/<suite> VERBOSE=1
make test-hardware VERBOSE=1
```

This enables:

* full Unity output (`-v`)
* preservation of debug messages via the `TEST_VERBOSE` build flag
* raw (non-humanised) output

For one-off extra flags to PlatformIO test (e.g. `--without-flashing`, a
specific test case filter), pass them through `PIO_TEST_EXTRA_OPTS`:

```bash
make test-hardware/wsen_hids PIO_TEST_EXTRA_OPTS="--without-flashing"
```

### Safe debug prints (Unity-compatible)

Do **not** use `Serial.println()` inside tests.

Unity uses the serial channel for its protocol. Writing directly to it will
corrupt the output and can break test parsing.

Instead, use Unity-safe debug helpers:

```cpp
#include <unity.h>

void test_example() {
    int state = 42;

    TEST_MESSAGE("Starting test");
    TEST_MESSAGE("state: 42");

    TEST_ASSERT_EQUAL(42, state);
}
```

This will:

* print safely without breaking Unity
* be visible in `VERBOSE=1` mode
* keep normal test runs clean

---

## Test Tier Responsibilities

### 1. Native tests (`tests/native/`)

Validate pure driver logic in a deterministic host environment.

Typical responsibilities:

* register decoding,
* configuration writes,
* conversion formulas,
* API behaviour with exact mock-controlled values.

These tests should be fast, deterministic, CI-friendly, and independent from
physical hardware.

### 2. Hardware unit tests (`tests/hardware/`)

Validate atomic contracts on a real STeaMi board.

Typical responsibilities:

* `begin()` succeeds on real silicon,
* `deviceId()` matches,
* one measurement returns a plausible value,
* low-level bus communication works.

These tests answer:

> "Does this individual driver call work correctly on the board?"

They are intentionally short and fail fast.

### 3. Integration tests (`tests/integration/`)

Validate runtime behaviour across a longer scenario.

Typical responsibilities:

* repeated acquisitions over time,
* expected output data rate,
* sensor values remain plausible across multiple samples,
* outputs are not frozen or stale.

These tests answer:

> "Does this driver still behave correctly when used continuously as intended?"

Integration tests complement hardware unit tests rather than replace them.

### 4. Qualification tests (`tests/qualification/`)

Qualification tests are YAML-driven human-in-the-loop physical coherence
checks run on a real STeaMi board.

They validate a higher-level question than hardware/integration suites:

> "Does a real environmental perturbation cause the sensor reading to react
> coherently?"

Typical examples:

* breathing on a humidity sensor should increase %RH,
* warming the board should raise temperature,
* covering a light sensor should reduce luminosity.

Each qualification scenario consists of:

* a `qualify.ino` sketch streaming machine-readable measurements,
* a `qualify.yaml` file declaring interactive phases, prompts, capture
  windows, and assertions.

Run available scenarios with:

```bash
make list-qualification
make qualify-hts221
```
---

## Shared Driver Checks

Common atomic assertions used by both native and hardware unit tests are
defined in:

```text
tests/shared/driver_checks.h
```

Current shared helpers:

* `check_begin()` — device initializes correctly.
* `check_who_am_i()` — identity register matches expected value.
* `check_read_plausible()` — reading falls within an expected range.

Usage pattern:

* **Native suites** use exact, mock-controlled values.
* **Hardware unit suites** use broad plausibility windows on real hardware.

This keeps the same basic driver contract validated in both environments while
allowing each tier to use assertion ranges appropriate to its execution model.

Integration tests intentionally do **not** use these helpers, because they
validate multi-step runtime behaviour rather than single-call contracts. See
`tests/integration/test_hts221/` for the reference shape.

---

## Mocking Approach (Native Only)

Native tests use lightweight Arduino-compatible mocks stored in
`tests/native/helpers/`. They are automatically included by the `[env:native]`
PlatformIO environment.

### GPIO mock

`Arduino.h` provides:

* `pinMode(pin, mode)` records the mode into `gpioPinMode()`,
* `digitalWrite(pin, value)` records the state into `gpioPinState()`,
* `digitalRead(pin)` returns the last recorded state,
* `delay(ms)` is a no-op.

Tests assert expected pin state by querying the two maps directly.

### I2C mock (`TwoWire`)

`Wire.h` provides a minimal host-side `TwoWire` implementation, plus a single
`inline TwoWire Wire;` global so drivers typed as `TwoWire&` can be passed
either `Wire` or a locally constructed instance.

Features:

* preload a register value on a specific address:

  ```cpp
  wire.setRegister(address, reg, value);
  ```

* simulate a register read (`beginTransmission` / `write` /
  `endTransmission(false)` / `requestFrom` / `read`) backed by the preloaded
  values.
* capture every `endTransmission` that carries a register write:

  ```cpp
  const auto& writes = wire.getWrites();  // vector<WriteOp{address, reg, value}>
  wire.clearWrites();
  ```

* multi-byte reads via `requestFrom(address, quantity)` then repeated `read()`.
* registers are keyed by `(address, reg)`, so a single `TwoWire` instance can
  simulate several I2C peripherals on the same bus.

The contract is pinned by the `tests/native/test_wire/` suite; see it for the
expected invocation sequence.

---

## Example: native test using the Wire mock

```cpp
#include <unity.h>

#include "Wire.h"
#include "MyDriver.h"  // accepts TwoWire& in its constructor

TwoWire wire;
MyDriver drv(wire);

void setUp(void) {
    wire = TwoWire();
    drv = MyDriver(wire);
}

void test_begin_checks_who_am_i(void) {
    wire.setRegister(MyDriver::ADDRESS, MyDriver::REG_WHO_AM_I, 0xBC);
    TEST_ASSERT_TRUE(drv.begin());
}
```

---

## Writing tests for a new driver

When adding a new driver, the expected progression is:

### Native tier

Validate configuration logic, register reads/writes, conversion helpers. Use
`driver_checks.h` for the common contract checks where they apply.

### Hardware unit tier

Validate board connectivity, identity register, one plausible measurement. Use
the same `driver_checks.h` helpers with broad plausibility windows.

### Integration tier

Validate continuous runtime behaviour, acquisition cadence, repeated-sample
plausibility. Don't use `driver_checks.h` — assert time-based properties
directly.

Not every driver needs an integration suite immediately, but sensor drivers
with continuous acquisition modes should strongly prefer one.

---

## Guidelines

* Prefer testing public driver APIs (`begin()`, `deviceId()`, `temperature()`,
  etc.).
* Keep native tests deterministic.
* Keep hardware unit tests short and fail-fast.
* Use integration tests for time-based behavioural assertions.
* Avoid duplicate assertions across tiers unless they validate different
  contexts.
* Add new mocks only when a driver cannot be meaningfully tested without them.

---

## Notes

* Native tests are CI-friendly and hardware-free.
* Hardware unit tests require a connected STeaMi board.
* Integration tests are slower by design.
* All three tiers are complementary and together provide the repository's
  driver confidence model.
