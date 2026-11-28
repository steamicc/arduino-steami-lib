# DAPLink Bridge

Arduino/C++ driver for the STM32F103 DAPLink I2C bridge on the STeaMi
board.

## Hardware

* I2C device, default 7-bit address `0x3B` (the same chip is referred to
  as `0x76` in 8-bit format in the CODAL convention).
* Provides access to a 1 KB persistent config zone backed by the F103
  internal flash. The bridge protocol uses small frames (`CMD | offset |
  length | payload`) so any single transfer fits in the default Arduino
  Wire receive buffer.
* Exposed only on the STeaMi **internal** I2C bus, not the default
  global `Wire`.

## Quick start

```cpp
#include <SteamiBoard.h>
#include <SteamiDaplinkBridge.h>

SteamiDaplinkBridge bridge(Steami.wire());  // the board's internal I2C bus

void setup() {
    Serial.begin(115200);

    if (!bridge.begin()) {
        Serial.println("DAPLink bridge not detected");
        while (true) delay(1000);
    }

    Serial.println(bridge.deviceId(), HEX);  // 4C

    if (bridge.hasConfigZone()) {
        uint8_t buf[64];
        size_t len = bridge.readConfig(buf, sizeof(buf));
        Serial.write(buf, len);
        Serial.println();
    }
}

void loop() {}
```

The config zone holds the board settings of the
[STeaMi Config](../steami_config/README.md) library (board revision, sensor
calibrations, boot counter). `clearConfig()` and `writeConfig()` change it
directly: on a configured board, go through STeaMi Config instead, or the
calibrations are lost.

See [examples/bridge_info/](examples/bridge_info/) for the full sketch.

## Examples

| Example | What it does |
|---------|--------------|
| [`bridge_info`](examples/bridge_info/) | Print WHO_AM_I, the busy flag, the error register, the config zone support and the text stored in the zone. Read only. |

### Building an example

```bash
make list-examples DRIVER=daplink_bridge
make flash-daplink_bridge/bridge_info
```

This builds, uploads, and opens the serial monitor at 115200 baud. Swap
`flash-` for `capture-` to reliably capture the first lines printed at boot:

```bash
make capture-daplink_bridge/bridge_info DURATION=30
```

## API

### Lifecycle

| Method | Description |
|--------|-------------|
| `SteamiDaplinkBridge(TwoWire& wire = Wire, uint8_t address = DAPLINK_BRIDGE_DEFAULT_ADDR)` | Construct. Defaults to the global `Wire` and address `0x3B`. |
| `bool begin()` | Probe the bridge by checking WHO_AM_I. Returns `false` if the device is not present or the identity register doesn't match. |
| `uint8_t deviceId()` | Read the WHO_AM_I register (always `0x4C`). |

### Status

| Method | Description |
|--------|-------------|
| `bool busy()` | Read `STATUS.BUSY`. The bridge is busy while erasing or writing flash. Also `true` when the bridge does not answer, as it does for a short while after sending a block. |

### Config zone

A 1 KB block (`DAPLINK_BRIDGE_CONFIG_SIZE`) of persistent storage in
the F103 internal flash. Sentinel value for unused bytes is `0xFF`,
matching the erased-flash state.

| Method | Description |
|--------|-------------|
| `bool hasConfigZone()` | `true` when the DAPLink firmware provides the config zone. Older firmwares (e.g. the 2024 `release_letssteam` builds) lock up, until the board is unplugged, when they receive a config command: every config method checks this first and fails without sending anything. The result of the first check is kept; when the bridge does not answer, the answer is `false` and the check runs again next time. |
| `bool clearConfig()` | Erase the entire 1 KB config zone. Waits for the busy bit to clear and returns `false` if the device reports an error. |
| `bool writeConfig(const uint8_t* data, size_t length, uint16_t offset = 0)` | Write raw bytes to the zone starting at `offset`. The driver chunks the payload into fixed 32-byte frames carrying at most `DAPLINK_BRIDGE_MAX_WRITE_CHUNK` (28) bytes each, and waits for the busy bit between chunks. Returns `false` if the range overflows the zone or the device reports an error. |
| `bool writeConfig(const char* str, uint16_t offset = 0)` | Convenience overload for null-terminated strings. |
| `size_t readConfig(uint8_t* result, size_t maxLen)` | Read up to `maxLen` bytes from the start of the zone, stopping at the first `0xFF` (end-of-data sentinel) or when the buffer is full. Returns the number of bytes copied. Reads the whole zone but its last byte (1023 bytes at most), in blocks of 255 bytes: an I2C error or a busy timeout stops the read and returns the bytes of the blocks already read (0 when the first block fails, as for an empty zone). |

### Raw protocol

For drivers built on the bridge, such as
[`daplink_flash`](../daplink_flash/README.md).

| Method | Description |
|--------|-------------|
| `bool waitReady(uint32_t timeoutMs)` | Wait until the bridge is idle. `false` after `timeoutMs`. |
| `bool sendCommand(uint8_t cmd, const uint8_t* args = nullptr, size_t argLen = 0)` | Send `[cmd | args...]`. The firmware runs a command only once it has received all its argument bytes: pass exactly the count it expects (at most `DAPLINK_BRIDGE_MAX_ARGS`, 31). |
| `bool commandSucceeded(uint32_t settleMs, uint32_t timeoutMs)` | Wait `settleMs` for the command just sent to run, then return `true` only if it is known to have succeeded. Send nothing in between: the firmware clears the "last command failed" error bit as soon as it receives another command, `STATUS` polls included. |
| `bool readError(uint8_t& error)` | Read the error register, which clears it. `false` if the bridge did not answer. |
| `size_t readResponse(uint8_t* buf, size_t length)` | Read `length` bytes of the answer the last command left in the bridge TX buffer, from its first byte. Reading `STATUS` or `ERROR` replaces that answer: wait with `delay()`, not `waitReady()`, before calling it. Blocks of 256 bytes need STM32duino 3.x. |

## Register constants

`SteamiDaplinkBridge_const.h` exports command codes (`DAPLINK_BRIDGE_CMD_*`),
register addresses (`DAPLINK_BRIDGE_REG_*`), status / error bits, the
`DAPLINK_BRIDGE_CONFIG_SIZE` constant, and the protocol's chunk-size and
timeout limits. Applications can reach for them if they need something
outside the driver's API surface.

## Testing

Host-side unit tests under
[`tests/native/test_daplink_bridge/`](../../tests/native/test_daplink_bridge/)
exercise the driver against the `TwoWire` mock and the shared
[`driver_checks.h`](../../tests/shared/driver_checks.h) helpers. Run them
without hardware with:

```bash
make test-native/daplink_bridge
```

## References

* Firmware: [steamicc/DAPLink](https://github.com/steamicc/DAPLink), the firmware of the STM32F103 that answers on `0x3B`.
* STeaMi wiki: [STM32F103 hardware page](https://wiki.steami.cc/docs/hardware/main-components/stm32f103).
* MicroPython driver: [`daplink_bridge`](https://github.com/steamicc/micropython-steami-lib/tree/main/lib/daplink_bridge).

## License

GPL-3.0-or-later — see [LICENSE](../../LICENSE).
