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
The board owns that bus: build the driver on `Steami.wire()`, which starts it on first use:

```cpp
#include <SteamiBoard.h>
#include <SteamiMCP23009E.h>

SteamiMCP23009E expander(Steami.wire(), RST_EXPANDER, MCP23009_I2C_ADDR, INT_EXPANDER);

void setup() {
    Serial.begin(115200);

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

## Examples

| Example | What it does | Try it |
|---------|--------------|--------|
| [`dpad_reader`](examples/dpad_reader/) | Print which D-PAD button is currently pressed on the serial monitor. A good first sketch to check the expander and its pull-ups. | [▶ Try it](https://wiki.steami.cc/docs/software/arduino/exemples/mcp23009e#dpad_reader) |
| [`SimonGame`](examples/SimonGame/) | Memory game inspired by *Simon Says*. The board flashes an increasingly long sequence using the on-board LEDs and the player must reproduce it using the D-PAD. Score is printed to the serial monitor. | [▶ Try it](https://wiki.steami.cc/docs/software/arduino/exemples/mcp23009e#simongame) |
| [`ReactionTimer`](examples/ReactionTimer/) | Reaction-time trainer. After a random delay, the red LED lights up and the player must press the UP button as quickly as possible. The measured reaction time is printed in milliseconds. | [▶ Try it](https://wiki.steami.cc/docs/software/arduino/exemples/mcp23009e#reactiontimer) |
| [`MusicPlayer`](examples/MusicPlayer/) | Turn the D-PAD into a mini jukebox. Each direction plays a different 8-bit melody on the on-board speaker (Tetris, Mario, Zelda, or Pokémon). | [▶ Try it](https://wiki.steami.cc/docs/software/arduino/exemples/mcp23009e#musicplayer) |
| [`MorseCode`](examples/MorseCode/) | Enter Morse code using the D-PAD. Short and long presses on the UP button generate dots and dashes, while the DOWN button decodes the current sequence and prints the matching letter. | [▶ Try it](https://wiki.steami.cc/docs/software/arduino/exemples/mcp23009e#morsecode) |

Each example is explained, and can be flashed from the browser, on the [STeaMi wiki](https://wiki.steami.cc/docs/software/arduino/exemples/mcp23009e).

### Building an example

List available examples (each line is a runnable Make target):

```bash
make list-examples
```

Then flash one — copy a line from the listing:

```bash
make flash-mcp23009e/MorseCode
```

This builds, uploads, and opens the serial monitor at 115200 baud.

To reliably capture the first lines printed at boot (which the interactive monitor often misses), swap `flash-` for `capture-`:

```bash
make capture-mcp23009e/MorseCode             # 10 seconds, OpenOCD reset, stdout
make capture-mcp23009e/MorseCode DURATION=30 # longer window
```

## API

All methods follow the collection conventions: `camelCase`, units included
in method names only where they carry ambiguity.

### Lifecycle

| Method | Description |
|--------|-------------|
| `SteamiMCP23009E(TwoWire& wire, uint8_t resetPin, uint8_t address = 0x20, int interruptPin = -1)` | Construct. Pass the reset pin number; interrupt pin is optional. |
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
| `void setGpinten(uint8_t value)` / `uint8_t getGpinten()` | Interrupt-on-change enable register. |
| `void setDefval(uint8_t value)` / `uint8_t getDefval()` | Default compare value for interrupt-on-change. |
| `void setIntcon(uint8_t value)` / `uint8_t getIntcon()` | Interrupt control register (compare with `DEFVAL` or with the previous value). |
| `void setGppu(uint8_t value)` / `uint8_t getGppu()` | Pull-up register. |
| `void setGpio(uint8_t value)` / `uint8_t getGpio()` | GPIO register. |
| `bool gpio(uint8_t& value)` | Read the GPIO register in one transaction. Returns `false` on a bus error and leaves `value` unchanged. |
| `void setOlat(uint8_t value)` / `uint8_t getOlat()` | Output latch register. |
| `void setIocon(MCP23009Config config)` / `MCP23009Config getIocon()` | I/O configuration register. |
| `uint8_t getIntf()` | Interrupt flag register (read-only). |
| `uint8_t getIntcap()` | Interrupt capture register (read-only). |

### Interrupts

| Method | Description |
|--------|-------------|
| `void interruptOnChange(uint8_t gpx, std::function<void(uint8_t)> callback)` | Call `callback(level)` on any level change of `gpx`. |
| `void interruptOnFalling(uint8_t gpx, std::function<void()> callback)` | Call `callback()` on falling edge of `gpx`. |
| `void interruptOnRising(uint8_t gpx, std::function<void()> callback)` | Call `callback()` on rising edge of `gpx`. |
| `void interruptOnRaising(uint8_t gpx, std::function<void()> callback)` | Deprecated former name of `interruptOnRising()`, kept for the whole 1.x series and removed in 2.0.0. |
| `void disableInterrupt(uint8_t gpx)` | Disable and clear all callbacks for `gpx`. |
| `void poll()` | Dispatch the registered callbacks in normal (non-interrupt) context. Call this from `loop()`. With an interrupt pin, it reads the chip only once the ISR has flagged an interrupt — the ISR itself only sets the flag, because running callbacks (and the I²C reads they involve) from interrupt context is unsafe on STM32duino. Without an interrupt pin, it reads the chip at each call while at least one pin is enabled. |
| `bool interruptEvent()` | Read the chip and dispatch now, regardless of the pending flag. Returns `false` on a bus error (no event is reported then). Usually you'll call `poll()` instead. |

How events are reported:

- **No event at registration.** Registering a callback stores it, reads and
  clears the pending interrupt, then enables the pin. A change made before
  the call fires nothing, and the level read then becomes the last known
  level of the pin. A registration replaces the earlier callback of the same
  kind.
- **Every edge, in order.** Each read takes INTF, INTCAP (the port captured
  at the interrupt) and GPIO. For each enabled pin, the driver reports the
  captured level if it differs from the last known one, then the current
  GPIO level if it still differs. A short tap released before the read gives
  the press, then the release. While an interrupt is pending, the chip does
  not signal changes on the other pins: the GPIO read finds them.
- **Queue.** Reads are queued (8 records) and delivered in FIFO order, so a
  read made from inside a callback (`interruptEvent()`, `poll()` or a
  registration) is delivered after the events already queued. Past 8 queued
  reads, the changes are read again from GPIO once the queue is empty.
  Events read before a pin is registered again are dropped for that pin.
- **Reset.** `reset()`, `powerOff()` and the soft reset disable every pin:
  register the callbacks again afterwards.
- **Limit.** A press and its release that both happen while an interrupt is
  pending cancel out: the chip keeps no trace of them.

Pin handlers keep the no-argument signature. In the handler,
`pin.irqLevel()` gives the level of the event being delivered (captured in
INTCAP, or read in GPIO for a change the chip did not signal), while
`pin.value()` reads the current level. `MCP23009ActiveLowPin::irqLevel()`
returns it inverted. Both return `IRQ_LEVEL_NONE` (`0xFF`) before any
interrupt.

```cpp
SteamiMCP23009E expander(Steami.wire(), RST_EXPANDER, MCP23009_I2C_ADDR, INT_EXPANDER);
MCP23009Pin up(expander, MCP23009_BTN_UP);

void setup() {
    expander.begin();
    up.init(MCP23009Pin::IN, MCP23009Pin::PULL_UP);
    up.irq([]() { Serial.println(up.irqLevel() == 0 ? "UP pressed" : "UP released"); });
}

void loop() {
    expander.poll();
}
```

The same behaviour is implemented in the MicroPython driver
([steamicc/micropython-steami-lib#466](https://github.com/steamicc/micropython-steami-lib/pull/466)),
where `irq_level()` is the counterpart of `irqLevel()`.

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
`value(x)`, `init(mode, pull, value)`, `mode()`, `pull()`, `irq()`, `irqLevel()`.
`MCP23009ActiveLowPin` also has `pinNumber()`, the expander GPIO it drives.

## Register constants

`SteamiMCP23009E_const.h` exports register addresses (`MCP23009_IODIR`, …),
bit masks (`MCP23009_IOCON_*`), direction/level/polarity values, interrupt
enable constants, and the GPIO-to-D-PAD button mappings
(`MCP23009_BTN_UP`, …) so applications can access the IC directly if they
need something outside the driver's API surface.

## Testing

Host-side unit tests under [`tests/native/test_mcp23009e/`](../../tests/native/test_mcp23009e/)
exercise the driver against the `TwoWire` mock from
`tests/native/helpers/Wire.h`. Run them without hardware with:

```bash
make test-native/mcp23009e
```

## References

* Datasheet: [Microchip MCP23009](https://ww1.microchip.com/downloads/en/DeviceDoc/20002121C.pdf).
* STeaMi wiki: [Buttons and audio hardware page](https://wiki.steami.cc/docs/hardware/main-components/buttons-audio).
* MicroPython driver: [`mcp23009e`](https://github.com/steamicc/micropython-steami-lib/tree/main/lib/mcp23009e).

## License

GPL-3.0-or-later — see [LICENSE](../../LICENSE).
