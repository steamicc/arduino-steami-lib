# Testing

## Overview

This repository uses PlatformIO's unit testing framework to validate Arduino drivers for the STeaMi board.

Two complementary testing approaches are used:

* **Hardware tests (`tests/hardware/`)**
  Run directly on a connected STeaMi board.

* **Native tests (`tests/native/`)**
  Run on the host machine (desktop) using mocks to simulate hardware behavior.

This mixed strategy allows both real hardware validation and fast, reproducible tests without requiring a board.

---

## Test Structure

```
tests/
├── hardware/
│   └── test_<name>/
│       └── test_main.cpp
├── native/
│   └── test_<name>/
│       └── test_main.cpp
└── TESTING.md
```

* Each test suite must be inside a folder named `test_<name>`
* Each suite contains a `test_main.cpp`
* Tests use the Unity framework

---

## Running Tests

### Hardware tests (on STeaMi board)

```bash
pio test -e steami
```

Requirements:

* STeaMi board connected via USB
* Correct upload port configured if needed

---

### Native tests (desktop)

```bash
pio test -e native
```

* No hardware required
* Faster execution
* Uses mocked Arduino APIs

---

### Run a specific test suite

```bash
pio test -e steami --filter hardware/test_led
pio test -e native --filter native/test_led
```

---

### Using Makefile

The `make` wrappers route through the venv `pio` installed by `make setup`,
and one phony target is generated per suite under `tests/native/test_*` and
`tests/hardware/test_*` (via `foreach + eval`, same shape as
`flash-<driver>/<example>`):

```bash
make test-native              # all native suites
make test-native-hts221       # one suite — auto-discovered from tests/native/test_hts221/
make test-native-led
make test-native-wire

make test-hardware            # all hardware suites
make test-hardware-led        # one suite — auto-discovered from tests/hardware/test_led/
```

Tab-completion on zsh/bash works for `make test-native-<TAB>` and
`make test-hardware-<TAB>`. Adding a new suite directory picks up
automatically on the next `make` invocation — no Makefile edits.

The hardware targets check for an attached STeaMi via `pio device list`
before they run; if no board is detected they print
`STeaMi not detected — skipping hardware tests.` and exit 0, so a CI script
that calls them without a board attached doesn't fail.

---

## Testing Strategy

### Hardware tests

Used to validate:

* Real hardware behavior
* GPIO, I2C, SPI communication
* Integration with the STeaMi board

Example:

* LED blinking
* Sensor register read

---

### Native tests (mock-based)

Used to validate:

* Driver logic
* Register handling
* Data processing

Mocks replace Arduino functions such as:

* `pinMode`
* `digitalWrite`
* `Wire`

Example:

* Simulated LED state tracking
* Fake I2C register reads

---

## Shared checks

Common driver assertions that should hold in both native and hardware tests
are defined in `tests/shared/driver_checks.h`.

These helpers provide a small, shared contract for drivers:

* `check_begin()` — verifies that the device initializes correctly
* `check_who_am_i()` — verifies device identity via the WHO_AM_I register
* `check_read_plausible()` — verifies that sensor readings fall within a valid range

Usage pattern:

* **Native tests** use these checks with exact, mock-controlled values
* **Hardware tests** use the same checks with realistic plausibility ranges

This ensures both test suites exercise the same core driver behavior, while
keeping native tests precise and hardware tests robust to real-world variation.

See `tests/native/test_hts221/` for a reference implementation.

## Mocking Approach

Native tests use lightweight mocks to simulate Arduino behavior. Mocks live
in `tests/native/helpers/` and are automatically available to every native
test via the `[env:native]` build flag `-I tests/native/helpers`.

### GPIO mock

`tests/native/helpers/Arduino.h` provides:

* `pinMode(pin, mode)` records the mode into `gpioPinMode()`
* `digitalWrite(pin, value)` records the state into `gpioPinState()`
* `digitalRead(pin)` returns the last recorded state
* `delay(ms)` is a no-op

Tests assert expected pin state by querying the two maps directly.

### I2C mock (`TwoWire`)

`tests/native/helpers/Wire.h` provides a minimal host-side implementation
of the Arduino `TwoWire` class, plus a single `inline TwoWire Wire;` global
so drivers typed as `TwoWire&` can be passed either `Wire` or a locally
constructed instance.

Features:

* Preload a register value on a specific address:

  ```cpp
  wire.setRegister(address, reg, value);
  ```

* Simulate a register read (`beginTransmission` / `write` / `endTransmission(false)` / `requestFrom` / `read`) — backed by the preloaded values.
* Capture every `endTransmission` that carries a register write:

  ```cpp
  const auto& writes = wire.getWrites();  // vector<WriteOp{address, reg, value}>
  wire.clearWrites();
  ```

* Multi-byte reads via `requestFrom(address, quantity)` then repeated `read()`.
* Registers are keyed by `(address, reg)`, so a single `TwoWire` instance can simulate several I2C peripherals on the same bus.

The contract is pinned by the `tests/native/test_wire/` suite; see it for the expected invocation sequence.

---

## Example: driver test using the Wire mock

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

## Current Status

* Basic hardware test infrastructure is working
* Native test environment with mocks is functional
* LED mock test covers GPIO recording
* Wire mock test pins the I2C mock contract (register preload, write capture, multi-byte reads, per-address isolation)

Future work:

* Add driver-specific tests (e.g. HTS221, WSEN-HIDS)
* Extend mocks as new peripheral types need host-side coverage (SPI, PDM, etc.)
* Increase coverage of driver APIs

---

## Guidelines

* Prefer testing public driver APIs (`begin()`, `deviceId()`, etc.)
* Keep tests simple and deterministic
* Avoid hardware dependencies in native tests
* Use hardware tests for integration validation

---

## Notes

* Hardware tests require a connected STeaMi board
* Native tests are suitable for CI environments
* Both approaches are complementary and should be used together
