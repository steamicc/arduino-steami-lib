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
pio test -e steami -f hardware/test_led
pio test -e native -f native/test_led
```

---

### Using Makefile

```bash
make test-native      # Host-side tests, no board required
make test-hardware    # On-board tests, STeaMi required
```

Each target is a thin wrapper around `pio test -e <env>`, pinned to
the venv pio installed by `make setup`.

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

## Mocking Approach

Native tests use lightweight mocks to simulate Arduino behavior.

Example for GPIO:

* `digitalWrite(pin, value)` stores state in memory
* Tests assert expected pin values

Example for I2C (future):

* Fake `Wire` implementation
* Simulated register map

This allows testing driver logic without hardware.

---

## Current Status

* Basic hardware test infrastructure is working
* Native test environment with mocks is functional
* LED mock test provides a minimal working example

Future work:

* Add driver-specific tests (e.g. HTS221, WSEN-HIDS)
* Extend I2C mock (`Wire`)
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
