# MCP23009E

Arduino/C++ driver for the Microchip MCP23009E 8-bit I/O expander on the
STeaMi board.

## Hardware

* I2C device, default 7-bit address `0x20`.
* 8 configurable GPIO pins with optional pull-ups, polarity inversion, and
  interrupt-on-change support.
* Hardware reset via a dedicated reset pin.

## Quick start

On the STeaMi board, the MCP23009E is routed to the **internal** I2C bus.
Spin up a dedicated `TwoWire` and hand it to the driver:

```cpp
#include <Wire.h>
#include <MCP23009E.h>

TwoWire internalI2C(I2C_INT_SDA, I2C_INT_SCL);
MCP23009E expander(internalI2C, RST_EXPANDER, MCP23009_I2C_ADDR, INT_EXPANDER);

void setup() {
    Serial.begin(115200);
    internalI2C.begin();

    if (!expander.begin()) {
        Serial.println("MCP23009E not detected");
        while (true) delay(1000);
    }

    // Configure GPIO 0 as output, GPIO 1 as input with pull-up
    expander.setup(0, MCP23009_DIR_OUTPUT);
    expander.setup(1, MCP23009_DIR_INPUT, MCP23009_PULLUP);
}

void loop() {
    expander.setLevel(0, MCP23009_LOGIC_HIGH);
    delay(500);
    expander.setLevel(0, MCP23009_LOGIC_LOW);
    delay(500);
}
```

## API

All methods follow the collection conventions: `camelCase`, units included
in method names only where they carry ambiguity.

### Lifecycle

| Method | Description |
|--------|-------------|
| `MCP23009E(TwoWire& wire, uint8_t resetPin, uint8_t address = 0x20, int interruptPin = -1)` | Construct. Pass the reset pin number; interrupt pin is optional. |
| `bool begin()` | Configure the reset pin, attach the interrupt handler if provided, perform a hardware reset, then probe the I2C bus. Returns `false` if the expander doesn't ACK. |
| `void reset()` | Toggle the reset pin LOW then HIGH to perform a hardware reset. |
| `void powerOn()` | Drive the reset pin HIGH. |
| `void powerOff()` | Drive the reset pin LOW. |

### GPIO configuration

| Method | Description |
|--------|-------------|
| `void setup(uint8_t gpx, uint8_t direction, uint8_t pullup = MCP23009_NO_PULLUP, uint8_t polarity = MCP23009_POL_SAME)` | Configure a GPIO pin. `gpx` is 0–7. |
| `void setLevel(uint8_t gpx, uint8_t level)` | Set the output level of a GPIO configured as output. |
| `uint8_t getLevel(uint8_t gpx)` | Read the current level of a GPIO. |

### Register accessors

Direct read/write access to every register in the MCP23009E register map.

| Method | Description |
|--------|-------------|
| `void setIodir(uint8_t value)` / `uint8_t getIodir()` | I/O direction register. |
| `void setIpol(uint8_t value)` / `uint8_t getIpol()` | Input polarity register. |
| `void setGppu(uint8_t value)` / `uint8_t getGppu()` | Pull-up register. |
| `void setGpio(uint8_t value)` / `uint8_t getGpio()` | GPIO register. |
| `void setOlat(uint8_t value)` / `uint8_t getOlat()` | Output latch register. |
| `void setIocon(MCP23009Config config)` / `MCP23009Config getIocon()` | I/O configuration register. |
| `uint8_t getIntf()` | Interrupt flag register (read-only). |
| `uint8_t getIntcap()` | Interrupt capture register (read-only). |

### Interrupts

| Method | Description |
|--------|-------------|
| `void interruptOnChange(uint8_t gpx, std::function<void(uint8_t)> callback)` | Call `callback(level)` on any level change of `gpx`. |
| `void interruptOnFalling(uint8_t gpx, std::function<void()> callback)` | Call `callback()` on falling edge of `gpx`. |
| `void interruptOnRaising(uint8_t gpx, std::function<void()> callback)` | Call `callback()` on rising edge of `gpx`. |
| `void disableInterrupt(uint8_t gpx)` | Disable and clear all callbacks for `gpx`. |
| `void poll()` | Drain the ISR-set pending flag and dispatch the registered callbacks. Call this from `loop()`. The ISR itself only sets the flag — running callbacks (and the I²C reads they involve) from interrupt context is unsafe on STM32duino. |
| `void interruptEvent()` | Force a dispatch pass regardless of the pending flag. Usually you'll call `poll()` instead. |

### IOCON configuration — `MCP23009Config`

`MCP23009Config` wraps the IOCON register with named bit accessors. Methods
return `MCP23009Config&` for chaining.

| Method | Description |
|--------|-------------|
| `setSeqop()` / `clearSeqop()` / `hasSeqop()` | Sequential operation mode. |
| `setOdr()` / `clearOdr()` / `hasOdr()` | Open-drain INT output. |
| `setIntpol()` / `clearIntpol()` / `hasIntpol()` | INT pin polarity (active-high when set). |
| `setIntcc()` / `clearIntcc()` / `hasIntcc()` | Interrupt clearing control. |
| `uint8_t getRegisterValue()` | Raw register byte. |

### Pin classes

Two helper classes emulate the MicroPython `Pin` API for use with the
expander GPIOs.

#### `MCP23009Pin`

Standard active-high pin. GPIO HIGH = logical 1.

```cpp
MCP23009Pin led(expander, /*pin=*/0, MCP23009Pin::OUT);
led.on();    // GPIO HIGH
led.off();   // GPIO LOW
led.toggle();
led.value(1);              // write
uint8_t v = led.value();   // read
```

#### `MCP23009ActiveLowPin`

Active-low pin for components wired between VCC and the GPIO (e.g. LEDs
where the MCP23009E sinks current). Logical values are inverted before
being applied to the GPIO.

```cpp
MCP23009ActiveLowPin led(expander, /*pin=*/0, MCP23009Pin::OUT);
led.on();    // GPIO LOW  → LED on
led.off();   // GPIO HIGH → LED off
```

Both classes share the same interface: `on()`, `off()`, `toggle()`,
`value(x)`, `init(mode, pull, value)`, `mode()`, `pull()`, `irq()`.

## Register constants

`MCP23009E_const.h` exports register addresses (`MCP23009_IODIR`, …),
bit masks (`MCP23009_IOCON_*`), direction/level/polarity values, interrupt
enable constants, and the GPIO-to-D-PAD button mappings
(`MCP23009_BTN_UP`, …) so applications can access the IC directly if they
need something outside the driver's API surface.

## Testing

Host-side unit tests under [`tests/native/test_mcp23009e/`](../../tests/native/test_mcp23009e/)
exercise the driver against the `TwoWire` mock from
`tests/native/helpers/Wire.h`. Run them without hardware with:

```bash
make test-native
```

## License

GPL-3.0-or-later — see [LICENSE](../../LICENSE).