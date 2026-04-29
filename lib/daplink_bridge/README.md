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
#include <Wire.h>
#include <DaplinkBridge.h>

TwoWire internalI2C(I2C_INT_SDA, I2C_INT_SCL);
DaplinkBridge bridge(internalI2C);

void setup() {
    Serial.begin(115200);
    internalI2C.begin();

    if (!bridge.begin()) {
        Serial.println("DAPLink bridge not detected");
        while (true) delay(1000);
    }

    bridge.clearConfig();
    bridge.writeConfig("hello");

    uint8_t buf[64];
    size_t len = bridge.readConfig(buf, sizeof(buf));
    Serial.write(buf, len);
    Serial.println();
}
```

## API

### Lifecycle

| Method | Description |
|--------|-------------|
| `DaplinkBridge(TwoWire& wire = Wire, uint8_t address = DAPLINK_BRIDGE_DEFAULT_ADDR)` | Construct. Defaults to the global `Wire` and address `0x3B`. |
| `bool begin()` | Probe the bridge by checking WHO_AM_I. Returns `false` if the device is not present or the identity register doesn't match. |
| `uint8_t deviceId()` | Read the WHO_AM_I register (always `0x4C`). |

### Status

| Method | Description |
|--------|-------------|
| `bool busy()` | Read `STATUS.BUSY`. The bridge is busy while erasing or writing flash. |

### Config zone

A 1 KB block (`DAPLINK_BRIDGE_CONFIG_SIZE`) of persistent storage in
the F103 internal flash. Sentinel value for unused bytes is `0xFF`,
matching the erased-flash state.

| Method | Description |
|--------|-------------|
| `bool clearConfig()` | Erase the entire 1 KB config zone. Waits for the busy bit to clear and returns `false` if the device reports an error. |
| `bool writeConfig(const uint8_t* data, size_t length, uint16_t offset = 0)` | Write raw bytes to the zone starting at `offset`. The driver chunks the payload into frames of at most `DAPLINK_BRIDGE_MAX_WRITE_CHUNK` bytes and waits for the busy bit between chunks. Returns `false` if the range overflows the zone or the device reports an error. |
| `bool writeConfig(const char* str, uint16_t offset = 0)` | Convenience overload for null-terminated strings. |
| `size_t readConfig(uint8_t* result, size_t maxLen)` | Read up to `maxLen` bytes, stopping at the first `0xFF` (end-of-data sentinel) or when the buffer is full. Returns the number of bytes copied. |

## Register constants

`daplink_bridge_const.h` exports command codes (`DAPLINK_BRIDGE_CMD_*`),
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

## License

GPL-3.0-or-later — see [LICENSE](../../LICENSE).
