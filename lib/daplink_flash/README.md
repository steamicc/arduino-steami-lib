# daplink_flash

Arduino/C++ driver for high-level flash file operations via the DAPLink
I2C bridge on the STeaMi board.

## Hardware

* Depends on `daplink_bridge` for I2C communication.
* Provides access to a FAT-like flash file system on the DAPLink interface.

## Quick start

```cpp
#include <Wire.h>
#include <DaplinkBridge.h>
#include <daplink_flash.h>

TwoWire internalI2C(I2C_INT_SDA, I2C_INT_SCL);
DaplinkBridge bridge(internalI2C);
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
| `DaplinkFlash(DaplinkBridge& bridge)` | Construct. Requires an initialized `DaplinkBridge` instance. |
| `void begin()` | Initialize the flash interface. Must be called after the bridge is ready. |

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
| `size_t readN(uint8_t* result, size_t maxLen)` | Read up to `maxLen` bytes of file content into `result`. Returns the number of bytes read. |
 | `size_t readUntilSentinel(uint8_t* result, size_t maxLen)` | Read file content into `result` until the first `0xFF` sentinel is encountered, or until `maxLen` bytes have been read. Returns the number of bytes read. |
 
## Register constants

`daplink_flash_const.h` exports command codes (`DAPLINK_FLASH_CMD_*`) and protocol
limits (`DAPLINK_FLASH_MAX_SECTORS`, `DAPLINK_FLASH_FILENAME_LEN`, `DAPLINK_FLASH_EXT_LEN`).

## Testing

Host-side unit tests under `tests/native/test_daplink_flash/` exercise
the driver against the `TwoWire` mock. Run them without hardware with:

```bash
make test-native
```

## License

GPL-3.0-or-later — see [LICENSE](../../LICENSE).