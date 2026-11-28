# APDS9960

Arduino/C++ driver for the Broadcom APDS9960 digital proximity, ambient
light, RGB colour, and gesture sensor on the STeaMi board.

## Hardware

* I2C sensor, default 7-bit address `0x39`.
* Integrated proximity, ambient light, RGB colour and gesture detection.
* Mounted on the **internal** STeaMi I2C bus.

## Quick start

On the STeaMi board, the APDS9960 is connected to the **internal** I2C
bus (pins `I2C_INT_SDA` / `I2C_INT_SCL` from the board variant, not the
default global `Wire`).
The board owns that bus: build the driver on `Steami.wire()`, which starts it on first use:

```cpp
#include <SteamiBoard.h>
#include <SteamiAPDS9960.h>

SteamiAPDS9960 sensor(Steami.wire());  // the board's internal I2C bus

void setup() {
    Serial.begin(115200);

    if (!sensor.begin()) {
        Serial.println("APDS9960 not detected");
        while (true)
            delay(1000);
    }

    sensor.enableLightSensor(false);
}

void loop() {
    uint16_t clear;

    if (sensor.ambientLight(clear)) {
        Serial.print("Clear: ");
        Serial.println(clear);
    }

    delay(100);
}
```

See [`examples/ambient_light/`](examples/ambient_light/) for the complete
example.

## Examples

| Example                                      | What it does                                               | Try it                                                                                    |
| -------------------------------------------- | ---------------------------------------------------------- | ----------------------------------------------------------------------------------------- |
| [`ambient_light`](examples/ambient_light/)   | Read the ambient light and RGB colour channels.            | [▶ Try it](https://wiki.steami.cc/docs/software/arduino/exemples/apds9960#ambient_light)  |
| [`proximity`](examples/proximity/)           | Detect nearby objects using the infrared proximity sensor. | [▶ Try it](https://wiki.steami.cc/docs/software/arduino/exemples/apds9960#proximity)      |
| [`gesture`](examples/gesture/)               | Detect left, right, up, down, near and far gestures.       | [▶ Try it](https://wiki.steami.cc/docs/software/arduino/exemples/apds9960#gesture)        |
| [`light_theremin`](examples/light_theremin/) | Control the STeaMi buzzer pitch using ambient light.       | [▶ Try it](https://wiki.steami.cc/docs/software/arduino/exemples/apds9960#light_theremin) |

Each example is explained, and can be flashed from the browser, on the [STeaMi wiki](https://wiki.steami.cc/docs/software/arduino/exemples/apds9960).

### Building an example

List the available examples:

```bash
make list-examples
```

Then flash one:

```bash
make flash-apds9960/ambient_light
```

This builds, uploads and opens the serial monitor at 115200 baud.

To reliably capture the first boot messages, use:

```bash
make capture-apds9960/ambient_light
make capture-apds9960/ambient_light DURATION=30
```

## API

All methods follow the collection conventions: `camelCase`, include
units only when required for clarity, and avoid redundant `read` /
`get` prefixes.

### Lifecycle

| Method | Description |
|--------|-------------|
| `SteamiAPDS9960(TwoWire& wire = Wire, uint8_t address = APDS9960_DEFAULT_ADDRESS)` | Construct the driver. |
| `bool begin()` | Probe the device, configure default registers and leave all engines disabled. Returns `false` if the sensor is not detected. |
| `uint8_t deviceId()` | Read the device ID register. |
| `void powerOn()` / `void powerOff()` | Enable or disable the sensor. |
| `uint8_t status()` | Raw `STATUS` register. |
| `uint8_t mode()` | Raw `ENABLE` register (one bit per engine). |
| `bool setMode(Mode mode, bool enable = true)` | Set or clear one `ENABLE` bit (`Mode::POWER`, `AMBIENT_LIGHT`, `PROXIMITY`, `WAIT`, `AMBIENT_LIGHT_INTERRUPT`, `PROXIMITY_INTERRUPT`, `GESTURE`), or all of them with `Mode::ALL`. |
| `bool dataReady()` | Both ambient light and proximity data are valid. |

### Ambient light and RGB

The driver automatically enables the ambient light engine if a reading
is requested while it is disabled.

| Method | Description |
|--------|-------------|
| `bool ambientLight(uint16_t&)` | Read the clear channel. |
| `bool redLight(uint16_t&)` | Read the red channel. |
| `bool greenLight(uint16_t&)` | Read the green channel. |
| `bool blueLight(uint16_t&)` | Read the blue channel. |
| `bool lightReady()` | Ambient light data ready. |
| `void enableLightSensor(bool interrupts = false)` | Enable the ambient light engine. |
| `void disableLightSensor()` | Disable the ambient light engine. |

### Proximity

The driver automatically enables the proximity engine if a reading is
requested while it is disabled.

| Method | Description |
|--------|-------------|
| `bool proximity(uint8_t&)` | Read the proximity value. |
| `bool proximityReady()` | Proximity data ready. |
| `void enableProximitySensor(bool interrupts = false)` | Enable proximity sensing. |
| `void disableProximitySensor()` | Disable proximity sensing. |

### Gesture

Gesture detection uses the APDS9960 gesture engine and reports one of:

* `LEFT`
* `RIGHT`
* `UP`
* `DOWN`
* `NEAR`
* `FAR`
* `NONE`

| Method | Description |
|--------|-------------|
| `bool gestureAvailable()` | Returns whether gesture FIFO data is available. |
| `Gesture readGesture(uint32_t timeoutMs = 1000)` | Read the gesture FIFO while it holds data (at most `timeoutMs`), then decode and return the gesture. `NONE` when the engine is off or no data is waiting. |
| `void resetGestureParameters()` | Forget the gesture data collected so far. |
| `void enableGestureSensor(bool interrupts = false)` | Enable the gesture engine. |
| `void disableGestureSensor()` | Disable the gesture engine. |
| `bool gestureModeEnabled()` / `void setGestureMode(bool enable)` | Read or set `GCONF4.GMODE`, the gesture state machine. |

### Configuration

Each setting has a getter (no `get` prefix) and a setter:

| Getter / setter | Description |
|-----------------|-------------|
| `ambientLightGain()` / `setAmbientLightGain(AmbientLightGain)` | Ambient light ADC gain (`X1`, `X4`, `X16`, `X64`). |
| `proximityGain()` / `setProximityGain(ProximityGain)` | Proximity receiver gain (`X1` to `X8`). |
| `gestureGain()` / `setGestureGain(GestureGain)` | Gesture receiver gain (`X1` to `X8`). |
| `ledDrive()` / `setLedDrive(LedDrive)` | Infrared LED drive current (`MA_100`, `MA_50`, `MA_25`, `MA_12_5`). |
| `gestureLedDrive()` / `setGestureLedDrive(LedDrive)` | Gesture LED drive current. |
| `ledBoost()` / `setLedBoost(LedBoost)` | LED current boost factor (`PERCENT_100` to `PERCENT_300`). |
| `gestureWaitTime()` / `setGestureWaitTime(GestureWaitTime)` | Gesture wait period (`MS_0` to `MS_39_2`). |
| `gestureEnterThreshold()` / `setGestureEnterThreshold(uint8_t)` | Gesture engine entry threshold. |
| `gestureExitThreshold()` / `setGestureExitThreshold(uint8_t)` | Gesture engine exit threshold. |
| `proximityGainCompensationEnabled()` / `setProximityGainCompensation(bool)` | Proximity gain compensation (`CONFIG3.PCMP`). |
| `proximityPhotodiodeMask()` / `setProximityPhotodiodeMask(uint8_t)` | Proximity photodiode mask (`CONFIG3.PMASK`, low 4 bits). |
| `lightInterruptLowThreshold()` / `setLightInterruptLowThreshold(uint16_t)` | Ambient light interrupt low threshold. |
| `lightInterruptHighThreshold()` / `setLightInterruptHighThreshold(uint16_t)` | Ambient light interrupt high threshold. |
| `proximityInterruptLowThreshold()` / `setProximityInterruptLowThreshold(uint8_t)` | Proximity interrupt low threshold. |
| `proximityInterruptHighThreshold()` / `setProximityInterruptHighThreshold(uint8_t)` | Proximity interrupt high threshold. |

### Interrupts

| Method | Description |
|--------|-------------|
| `ambientLightInterruptEnabled()` / `setAmbientLightInterrupt(bool)` | Read, enable or disable ALS interrupts. |
| `proximityInterruptEnabled()` / `setProximityInterrupt(bool)` | Read, enable or disable proximity interrupts. |
| `gestureInterruptEnabled()` / `setGestureInterrupt(bool)` | Read, enable or disable gesture interrupts. |
| `clearAmbientLightInterrupt()` | Clear the ALS interrupt flag. |
| `clearProximityInterrupt()` | Clear the proximity interrupt flag. |

### MicroPython names

The MicroPython `apds9960` driver keeps the names of the original
MicroPython library, with `get_` prefixes and short forms. The two drivers
are not renamed to match: this table gives the correspondence.

| Arduino | MicroPython |
|---------|-------------|
| `mode()` / `setMode()` | `get_mode()` / `set_mode()` |
| `gestureAvailable()` | `is_gesture_available()` |
| `readGesture(timeoutMs)` (returns a `Gesture`) | `gesture()` (returns an `APDS9960_DIR_*` constant) |
| `ambientLightGain()` / `setAmbientLightGain()` | `get_ambient_light_gain()` / `set_ambient_light_gain()` |
| `proximityGain()` / `setProximityGain()` | `get_proximity_gain()` / `set_proximity_gain()` |
| `gestureGain()` / `setGestureGain()` | `get_gesture_gain()` / `set_gesture_gain()` |
| `ledDrive()` / `setLedDrive()` | `get_led_drive()` / `set_led_drive()` |
| `gestureLedDrive()` / `setGestureLedDrive()` | `get_gesture_led_drive()` / `set_gesture_led_drive()` |
| `ledBoost()` / `setLedBoost()` | `get_led_boost()` / `set_led_boost()` |
| `gestureWaitTime()` / `setGestureWaitTime()` | `get_gesture_wait_time()` / `set_gesture_wait_time()` |
| `gestureEnterThreshold()` / `setGestureEnterThreshold()` | `get_gesture_enter_thresh()` / `set_gesture_enter_thresh()` |
| `gestureExitThreshold()` / `setGestureExitThreshold()` | `get_gesture_exit_thresh()` / `set_gesture_exit_thresh()` |
| `proximityGainCompensationEnabled()` / `setProximityGainCompensation()` | `get_prox_gain_comp_enable()` / `set_prox_gain_comp_enable()` |
| `proximityPhotodiodeMask()` / `setProximityPhotodiodeMask()` | `get_prox_photo_mask()` / `set_prox_photo_mask()` |
| `lightInterruptLowThreshold()` / `setLightInterruptLowThreshold()` | `get_light_int_low_threshold()` / `set_light_int_low_threshold()` |
| `lightInterruptHighThreshold()` / `setLightInterruptHighThreshold()` | `get_light_int_high_threshold()` / `set_light_int_high_threshold()` |
| `proximityInterruptLowThreshold()` / `setProximityInterruptLowThreshold()` | `get_proximity_int_low_threshold()` / `set_proximity_int_low_threshold()` (also `get_prox_int_low_thresh()` / `set_prox_int_low_thresh()`) |
| `proximityInterruptHighThreshold()` / `setProximityInterruptHighThreshold()` | `get_proximity_int_high_threshold()` / `set_proximity_int_high_threshold()` (also `get_prox_int_high_thresh()` / `set_prox_int_high_thresh()`) |
| `ambientLightInterruptEnabled()` / `setAmbientLightInterrupt()` | `get_ambient_light_int_enable()` / `set_ambient_light_int_enable()` |
| `proximityInterruptEnabled()` / `setProximityInterrupt()` | `get_proximity_int_enable()` / `set_proximity_int_enable()` |
| `gestureInterruptEnabled()` / `setGestureInterrupt()` | `get_gesture_int_enable()` / `set_gesture_int_enable()` |
| `clearAmbientLightInterrupt()` / `clearProximityInterrupt()` | `clear_ambient_light_int()` / `clear_proximity_int()` |
| `gestureModeEnabled()` / `setGestureMode()` | `get_gesture_mode()` / `set_gesture_mode()` |
| `status()` | no public equivalent (`_status()` is private) |

The other methods only change case (`ambientLight()` and `ambient_light()`,
`enableLightSensor()` and `enable_light_sensor()`, ...). One default differs:
the `enable*Sensor()` methods leave the engine interrupt off unless asked
(`interrupts = false`), while the MicroPython `enable_*_sensor()` methods turn
it on (`interrupts=True`).

## Register constants

`SteamiAPDS9960_const.h` exports register addresses, bit masks, default
configuration values and enumeration constants so applications can
access advanced device features when required.

## Testing

Build-only verification:

```bash
make build
```

Formatting:

```bash
make lint
```

Run the native unit tests:

```bash
make test-native/apds9960
```

Run the hardware validation suite on a connected STeaMi:

```bash
make test-hardware/apds9960
```

The hardware tests validate:

* successful `begin()`
* supported device ID
* ambient light acquisition
* RGB acquisition
* proximity acquisition
* gesture engine configuration
* configuration register round-trips

## References

* Datasheet: [Broadcom APDS-9960](https://docs.broadcom.com/doc/AV02-4191EN).
* STeaMi wiki: [APDS-9960 hardware page](https://wiki.steami.cc/docs/hardware/main-components/sensors/apds9960).
* MicroPython driver: [`apds9960`](https://github.com/steamicc/micropython-steami-lib/tree/main/lib/apds9960).

## License

GPL-3.0-or-later — see [LICENSE](../../LICENSE).
