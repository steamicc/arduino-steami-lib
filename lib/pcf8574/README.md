# PCF8574

Arduino/C++ driver for the PCF8574 8-channel I2C GPIO expander, integrated
into the STeaMi board library.

This driver is authored by RobTillaart and sourced from the upstream repository:
[github.com/RobTillaart/PCF8574](https://github.com/RobTillaart/PCF8574).

## Hardware

* I2C GPIO expander providing 8 bidirectional I/O pins.
* Default 7-bit address `0x20` (configurable via A0–A2 pins).
* On the STeaMi robot board, a PCF8574 at address `0x38` is wired to the
  motor driver direction pins (IN1–IN4 of both L298 breakouts).

## Quick start

On the STeaMi robot board, the PCF8574 is routed to the **external** I2C bus
(pins `I2C_EXT_SDA` / `I2C_EXT_SCL`, i.e. `PC1` / `PC0`). Spin up a dedicated
`TwoWire` and hand it to the driver:

```cpp
#include <PCF8574.h>
#include <Wire.h>

#define I2C3_SDA PC1
#define I2C3_SCL PC0

TwoWire Wire3(I2C3_SDA, I2C3_SCL);
PCF8574 pcf8574_0(0x38, &Wire3);

void setup() {
    Wire3.begin();
    pcf8574_0.begin();
    pinMode(ENA_ARD, OUTPUT);
}

void loop() {
    pcf8574_0.write(IN1_ARD, LOW);
    pcf8574_0.write(IN2_ARD, HIGH);
    analogWrite(ENA_ARD, 00);
}
```

See [examples/](examples/) for the motor control sketch.

## Examples

| Example | What it does |
|---------|--------------|
| [`drive`](examples/drive/) | Drive all four motors forward using both L298 breakouts via the PCF8574. |

### Building an example

List available examples (each line is a runnable Make target):

```bash
make list-examples
```

Then flash one:

```bash
make flash-PCF8574/drive
```

This builds, uploads, and opens the serial monitor at 115200 baud.

To capture the first lines printed at boot:

```bash
make capture-PCF8574/drive             # 10 seconds, OpenOCD reset, stdout
make capture-PCF8574/drive DURATION=30 # longer window
```

## API

All methods follow the upstream conventions from RobTillaart's library.

### Lifecycle

| Method | Description |
|--------|-------------|
| `PCF8574(uint8_t address = 0x20, TwoWire *wire = &Wire)` | Construct. Pass the expander I2C address and the `TwoWire` instance to use. |
| `bool begin(uint8_t value = 0xFF)` | Probe the device and set all pins to `value`. Returns `false` if not detected. |
| `bool isConnected()` | Check whether the device acknowledges on the bus. |
| `bool setAddress(uint8_t address)` | Change the I2C address at runtime. Returns `false` if the new address does not respond. |
| `uint8_t getAddress()` | Return the current I2C address. |

### Reading and writing

| Method | Description |
|--------|-------------|
| `uint8_t read8()` | Read all 8 pins at once as a bitmask. |
| `uint8_t read(uint8_t pin)` | Read a single pin (0–7). Returns `0` or `1`. |
| `uint8_t value()` | Return the last value read by `read8()` without a new I2C transaction. |
| `void write8(uint8_t value)` | Write all 8 pins at once from a bitmask. |
| `void write(uint8_t pin, uint8_t value)` | Write a single pin (0–7) to `HIGH` or `LOW`. |
| `uint8_t valueOut()` | Return the last value written without a new I2C transaction. |
| `bool writeArray(uint8_t *array, uint8_t size)` | Write a sequence of bytes in a single I2C transaction (experimental). |
| `bool readArray(uint8_t *array, uint8_t size)` | Read a sequence of bytes in a single I2C transaction (experimental). |

### Button helpers

Useful when some pins are configured as inputs (buttons). These methods
temporarily set the target pins high before reading so the PCF8574 releases
the line and lets an external pull-down drive it.

| Method | Description |
|--------|-------------|
| `uint8_t readButton(uint8_t pin)` | Read a single button pin safely. |
| `uint8_t readButton8(uint8_t mask)` | Read all button pins selected by `mask`. |
| `uint8_t readButton8()` | Read using the stored button mask. |
| `void setButtonMask(uint8_t mask)` | Set which pins are buttons. |
| `uint8_t getButtonMask()` | Return the current button mask. |

### Bitwise operations

These expect all pins to be configured as outputs.

| Method | Description |
|--------|-------------|
| `void toggle(uint8_t pin)` | Invert a single pin. |
| `void toggleMask(uint8_t mask = 0xFF)` | Invert all pins selected by `mask`. |
| `void shiftRight(uint8_t n = 1)` | Shift the output register right by `n` bits. |
| `void shiftLeft(uint8_t n = 1)` | Shift the output register left by `n` bits. |
| `void rotateRight(uint8_t n = 1)` | Rotate the output register right by `n` bits. |
| `void rotateLeft(uint8_t n = 1)` | Rotate the output register left by `n` bits. |
| `void reverse()` | Reverse the bit order of the output register. |

### Selection helpers

| Method | Description |
|--------|-------------|
| `void select(uint8_t pin)` | Set only `pin` high, all others low. |
| `void selectN(uint8_t pin)` | Set pins 0 through `pin` high, all others low. |
| `void selectNone()` | Set all pins low. |
| `void selectAll()` | Set all pins high. |

### Error handling

| Method | Description |
|--------|-------------|
| `int lastError()` | Return the last error code and reset it to `PCF8574_OK`. |

Error codes:

| Constant | Value | Meaning |
|----------|-------|---------|
| `PCF8574_OK` | `0x00` | No error. |
| `PCF8574_PIN_ERROR` | `0x81` | Pin number out of range (> 7). |
| `PCF8574_I2C_ERROR` | `0x82` | I2C communication failure. |
| `PCF8574_BUFFER_LENGTH_ERROR` | `0x83` | Array too large for I2C buffer. |

## License

MIT — see the upstream repository for the full license text:
[github.com/RobTillaart/PCF8574](https://github.com/RobTillaart/PCF8574).