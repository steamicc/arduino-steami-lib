# Contributing

This repository provides Arduino/C++ drivers for the STeaMi board.
The goal is to maintain a consistent, reliable, and maintainable codebase across all drivers.

## Driver structure

Each driver must follow this layout:

```
lib/<component>/
├── README.md
├── library.properties      # Arduino library metadata
├── src/
│   ├── <DriverName>.h      # Public header
│   ├── <DriverName>.cpp    # Implementation
│   └── <DriverName>_const.h # Register constants
└── examples/
    └── <example_name>/
        └── <example_name>.ino
```

### Requirements

* The directory name must match the driver name (e.g. `mcp23009e`, `wsen-hids`)
* The main class must be declared in `src/<DriverName>.h`
* Drivers must be self-contained (no cross-driver dependencies)
* Each driver must have a `library.properties` file for Arduino Library Manager compatibility

## Coding conventions

- **Naming**: `camelCase` for methods, `UPPER_SNAKE_CASE` for constants, `PascalCase` for class names
- **Constants**: use `constexpr` or `#define` in `*_const.h` files
- **Class style**: classes inherit from nothing (no Arduino `Stream` unless needed)
- **I2C**: use `TwoWire&` reference, default to `Wire`
- **Constructor**: `DriverName(TwoWire& wire = Wire, uint8_t address = DEFAULT_ADDR)`
- **Attributes**: `_wire` for I2C bus, `_address` for the device address
- **I2C helpers**: private `readReg()`, `writeReg()` methods
- **Formatting**: enforced by clang-format (config in `.clang-format`)
- **No `Serial.print()`** in library code (only in examples)

## Driver API conventions

- **Initialization**: `bool begin()` — returns false if device not found
- **Device identification**: `uint8_t deviceId()` — returns WHO_AM_I register value
- **Reset methods**: `void reset()` for hardware reset, `void softReset()` for software reset, `void reboot()` for memory reboot
- **Power methods**: `void powerOn()` / `void powerOff()`
- **Status methods**: `uint8_t status()` returns raw status register, `bool dataReady()` returns true when all channels have new data, `bool temperatureReady()` etc. for per-channel readiness
- **Measurement methods**: `float temperature()` (°C), `float humidity()` (%RH), `float pressureHpa()`, `uint16_t distanceMm()`, `uint16_t voltageMv()`, etc.
- **Mode methods**: `void setContinuous(uint8_t odr)`, `void triggerOneShot()`, `std::tuple<float,float> readOneShot()`

## Commit messages

Commit messages follow the [Conventional Commits](https://www.conventionalcommits.org/) format, enforced by commitlint via a git hook:

```
<type>[(<scope>)]: <Description starting with a capital letter ending with a period.>
```

**Types**: `feat`, `fix`, `docs`, `style`, `refactor`, `test`, `ci`, `build`, `chore`, `perf`, `revert`, `tooling`

**Scopes** (optional): driver names (`hts221`, `ism330dl`, `wsen-pads`...) or domains (`ci`, `docs`, `style`, `tests`, `tooling`). The scope is recommended for driver-specific changes but can be omitted for cross-cutting changes.

### Examples

```
feat(hts221): Add driver with temperature and humidity reading.
fix(wsen-pads): Correct pressure conversion formula.
docs: Update README driver table.
test(mcp23009e): Add unit tests for GPIO read.
```

## Workflow

1. Set up your environment: `make setup`
2. Create a branch from main (format: `feat/`, `fix/`, `docs/`, `tooling/`, `ci/`, `test/`, `style/`, `chore/`, `refactor/`)
3. Write your code and add tests
4. Run `make lint` to check formatting
5. Commit — the git hooks will automatically check your commit message and format staged files
6. Push your branch and open a Pull Request

## Continuous Integration

All pull requests must pass these checks:

| Check | Description |
|-------|-------------|
| Commit messages | Validates commit message format (commitlint) |
| Formatting | Runs clang-format check |
| Build | Builds with PlatformIO |
| Tests | Runs unit tests |

## Notes

* Keep implementations simple and readable
* Follow existing drivers as reference
* Ensure documentation (`README.md`) and examples are included for each driver
* Match the MicroPython driver API as closely as possible for consistency across the project
