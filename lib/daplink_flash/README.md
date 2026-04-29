# daplink_flash

Arduino/C++ driver for high-level flash file operations via the DAPLink
I2C bridge on the STeaMi board.

## Hardware

* Depends on `daplink_bridge` for I2C communication.
* Provides access to a FAT-like flash file system on the DAPLink interface.

## Quick start

```cpp
#include <Wire.h>
#include <daplink_bridge.h>
#include <daplink_flash.h>

TwoWire internalI2C(I2C_INT_SDA, I2C_INT_SCL);
daplink_bridge bridge(internalI2C);
DaplinkFlash flash(bridge);

void setup() {
    Serial.begin(115200);
    internalI2C.begin();

    flash.clearFlash();
    flash.setFilename("DATA", "CSV");
    flash.writeLine("temperature,pressure");
    flash.writeLine("23.5,1013.2");
}
```

## API

### Lifecycle

| Method | Description |
|--------|-------------|
| `DaplinkFlash(daplink_bridge& bridge)` | Construct. Requires an initialized `daplink_bridge` instance. |

### Filename management

| Method | Description |
|--------|-------------|
| `void setFilename(const char* name, const char* ext)` | Set the 8.3 filename. `name` is max 8 chars, `ext` is max 3 chars. Both are uppercased and space-padded automatically. |
| `FilenameResult getFilename()` | Read the current filename. Returns a `FilenameResult` struct with `name` and `ext` fields, both stripped of trailing spaces. |

### Flash operations

| Method | Description |
|--------|-------------|
| `void clearFlash()` | Erase the entire flash memory. |
| `size_t write(const uint8_t* data, size_t length)` | Append raw bytes to the current file. Returns the number of bytes written, or `0` on error. |
| `size_t write(const char* data)` | Append a null-terminated string to the current file. |
| `size_t writeLine(const char* text)` | Append a string followed by a newline character. |

### Read operations

| Method | Description |
|--------|-------------|
| `void readSector(uint16_t sector, uint8_t* buf)` | Read a 256-byte sector from flash into `buf`. |
| `size_t read(uint8_t* result, size_t maxLen, bool limitLen = false)` | Read file content from flash. If `limitLen` is `false`, reads until the first `0xFF` sentinel. If `true`, reads up to `maxLen` bytes. Returns the number of bytes read. |

## Register constants

`daplink_flash_const.h` exports command codes (`CMD_*`) and protocol
limits (`MAX_SECTORS`, `FILENAME_LEN`, `EXT_LEN`).

## Testing

Host-side unit tests under `tests/native/test_daplink_flash/` exercise
the driver against the `TwoWire` mock. Run them without hardware with:

```bash
make test-native
```

## License

GPL-3.0-or-later — see [LICENSE](../../LICENSE).