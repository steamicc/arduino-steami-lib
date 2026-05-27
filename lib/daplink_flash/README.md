# daplink_flash

Arduino/C++ driver for high-level flash file operations via the DAPLink
I2C bridge on the STeaMi board.

## Hardware

* Depends on `daplink_bridge` for I2C communication.
* Provides access to a FAT-like flash file system on the DAPLink interface.

## Quick start

The snippet below opens the existing file, appends two CSV rows, and
exits. It does **not** call `clearFlash()` — that command is destructive
and currently unsafe on real silicon (see [Destructive
operations](#destructive-operations) below).

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
    if (!flash.begin()) {
        Serial.println("DAPLink Flash not found!");
        return;
    }

    flash.setFilename("DATA", "CSV");
    flash.writeLine("temperature,pressure");
    flash.writeLine("23.5,1013.2");
}
```

## Destructive operations

> ⚠️ **`clearFlash()` is known to brick STeaMi boards into DAPLink
> maintenance mode** until DAPLink firmware issue
> [`steamicc/DAPLink#9`](https://github.com/steamicc/DAPLink/issues/9)
> is fixed. Recovery requires a manual maintenance-mode reset of the
> board.
>
> The call is part of the public API to match the MicroPython sister
> project, but **do not invoke it on production hardware** until the
> firmware fix lands. If you must exercise it on a development board,
> gate it behind an explicit, one-shot user action (a button held at
> boot, an external trigger, a `--clear` build flag) and never on
> every `setup()`.

```cpp
// Example: only wipe when GPIO0 is held low at boot.
if (digitalRead(0) == LOW) {
    if (!flash.clearFlash()) {
        Serial.println("clearFlash() failed");
    }
}
```

## API

### Lifecycle

| Method | Description |
|--------|-------------|
| `DaplinkFlash(DaplinkBridge& bridge)` | Construct. Requires an initialized `DaplinkBridge` instance. |
| `bool begin()` | Initialize the flash interface. Must be called after the bridge is ready. Returns `true` if successful. |

### Filename management

| Method | Description |
|--------|-------------|
| `bool setFilename(const char* name, const char* ext)` | Set the 8.3 filename. `name` is max 8 chars, `ext` is max 3 chars. Both are uppercased and space-padded automatically. Returns `true` on success, `false` if either argument is `nullptr` or the bridge reports an error. |
| `FilenameResult getFilename()` | Read the current filename. Returns a `FilenameResult` struct with `name` and `ext` fields, both stripped of trailing spaces. |

### Flash operations

| Method | Description |
|--------|-------------|
| `bool clearFlash()` | Erase the entire flash memory. **Currently unsafe on real STeaMi silicon** — see [Destructive operations](#destructive-operations). Returns `true` on success, `false` if the bridge times out or reports an error. |
| `size_t write(const uint8_t* data, size_t length)` | Append raw bytes to the current file. Returns the number of bytes successfully written — `length` on full success, `0` if the first chunk fails, and the number of bytes that landed in flash before a mid-stream failure. Use this to detect partial writes and avoid duplicate retries (writes are append-only and not atomic). |
| `size_t write(const char* data)` | Append a null-terminated string to the current file. |
| `size_t writeLine(const char* text)` | Append a string followed by a newline character. |

### Read operations

| Method | Description |
|--------|-------------|
| `bool readSector(uint16_t sector, uint8_t* buf)` | Read a 256-byte sector from flash into `buf`. Returns `false` if `buf` is `nullptr`, the bridge fails, or fewer than 256 bytes are returned. |
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