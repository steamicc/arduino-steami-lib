# daplink_flash

Arduino driver for the user file that the DAPLink chip of the STeaMi
keeps in its 8 MB external flash (W25Q64). The file shows up on the
STeaMi USB drive under its 8.3 name: a sketch can log data that the
computer then reads as a plain file.

## Hardware

* The STM32F103 DAPLink chip talks to the STM32WB55 over the internal
  I2C bus (address `0x3B`) and drives the external flash. This driver is
  built on [`daplink_bridge`](../daplink_bridge/README.md).
* There is one file. It starts at the beginning of the flash and ends at
  its first `0xFF` byte, so a `0xFF` byte cannot be stored. It holds up
  to 8 384 512 bytes; the last 4 KB of the flash keep its name.
* The USB drive lists the file as it was when the DAPLink chip started:
  unplug and replug the board to see what the sketch wrote.
* Reading needs a DAPLink firmware from March 2026 or later (`READ_SECTOR`
  with a 2-byte sector number, steamicc/DAPLink#1). Older firmwares
  answer every read with an error.

## Quick start

```cpp
#include <DaplinkBridge.h>
#include <SteamiDaplinkFlash.h>
#include <Wire.h>

TwoWire internalI2C(I2C_INT_SDA, I2C_INT_SCL);
DaplinkBridge bridge(internalI2C);
SteamiDaplinkFlash flash(bridge);

void setup() {
    Serial.begin(115200);
    internalI2C.begin();
    if (!flash.begin()) {
        Serial.println("DAPLink bridge not found");
        return;
    }

    flash.setFilename("DATA", "CSV");
    flash.writeLine("temperature,pressure");
    flash.writeLine("23.5,1013.2");

    uint8_t content[256];
    size_t length = flash.read(content, sizeof(content));
    Serial.write(content, length);
}

void loop() {}
```

`clearFlash()` erases the whole file: call it on purpose (a button, a
serial command), not on every start-up.

## API

### Lifecycle

| Method | Description |
|--------|-------------|
| `SteamiDaplinkFlash(DaplinkBridge& bridge)` | Use the given bridge. |
| `bool begin()` | Check that the DAPLink bridge answers (`DaplinkBridge::begin()`). |

### Filename

| Method | Description |
|--------|-------------|
| `bool setFilename(const char* name, const char* ext)` | Set the 8.3 name: `name` 1 to 8 characters, `ext` 0 to 3. Lower case is turned into upper case; spaces (except first), control characters and `"*+,./:;<=>?[\]\|` are refused. Only renames: the content stays. Reads the name back and returns `true` if it was stored. |
| `bool getFilename(Filename& filename)` | Fill `filename.name` and `filename.ext` without their padding. `false` if no name was ever set or the bridge did not answer. |

### Writing

| Method | Description |
|--------|-------------|
| `size_t write(const uint8_t* data, size_t length)` | Append bytes to the file, 30 per bridge command. Stops before a `0xFF` byte, when the file is full or when the bridge reports an error, and returns the number of bytes appended. |
| `size_t write(const char* text)` | Append a NUL-terminated string. |
| `size_t writeLine(const char* text)` | Append `text` and `\n`. Returns the bytes appended, newline included. |
| `bool clearFlash(uint32_t timeoutMs = DAPLINK_FLASH_CLEAR_TIMEOUT_MS)` | Erase the file (the name stays) and check that it is empty. The DAPLink chip erases 64 KB at a time and does nothing else meanwhile: about 0.4 s for a small file, about 20 s for a full flash (typical W25Q64 timings). |

Appending is slow: about 6 ms per byte (30 bytes every 180 ms), and
slower as the file grows because the firmware looks for the end of the
file before each command.

### Reading

| Method | Description |
|--------|-------------|
| `size_t read(uint8_t* buf, size_t maxLen)` | Copy the file into `buf`, up to `maxLen` bytes. Returns the number of bytes copied. |
| `bool readSector(uint16_t sector, uint8_t* buf)` | Copy the 256 bytes of flash sector `sector` (0 to `DAPLINK_FLASH_MAX_SECTORS - 1`) into `buf`, which must hold 256 bytes. Bytes past the end of the file read `0xFF`. |

Reading takes about 0.2 s per 256-byte sector.

## Firmware limits

* The DAPLink firmware clears the "last command failed" bit of its error
  register as soon as it receives another command. A flash failure in
  the middle of a write is therefore not reported: `write()` only sees
  the errors that stay set (file full, bad parameter). `setFilename()`
  and `clearFlash()` check their result by reading back.
* The bridge sends 256-byte blocks. STM32duino 2.x reads at most 255
  bytes at once, so `readSector()` and `read()` need STM32duino 3.x (the
  Arduino IDE default, and the PlatformIO env of this repository).

## If the bridge stops answering

Once, after a sketch that was writing to the file was replaced by
another one, the DAPLink chip kept acknowledging its I2C address but
refused every command byte: `begin()` returned `false` until the chip
restarted. Restart it without unplugging anything:

1. create an empty file named `START_BL.ACT` on the STeaMi drive: the
   chip restarts as the `MAINTENANCE` drive;
2. create an empty file named `START_IF.ACT` on the `MAINTENANCE` drive:
   the chip restarts as the STeaMi drive.

Unplugging USB is not enough when the battery is connected: it keeps the
DAPLink chip powered.

## Constants

`SteamiDaplinkFlash_const.h` exports the command codes
(`DAPLINK_FLASH_CMD_*`), error bits (`DAPLINK_FLASH_ERROR_*`), sizes
(`DAPLINK_FLASH_SECTOR_SIZE`, `DAPLINK_FLASH_MAX_SECTORS`,
`DAPLINK_FLASH_FILE_SIZE`) and timeouts.

## Examples

| Example | Description |
|---------|-------------|
| [`data_logger`](examples/data_logger/data_logger.ino) | Print the file, then append a `seconds,light` line every 10 seconds. |

## Testing

* `make test-native`: `tests/native/test_daplink_flash/` runs the driver
  against a model of the DAPLink firmware (`FakeDaplinkBridge`).
* `make test-hardware/daplink_flash`: renames, writes and erases the file
  on a real board, after saving it (up to 4 KB) and its name; both are
  written back at the end.
