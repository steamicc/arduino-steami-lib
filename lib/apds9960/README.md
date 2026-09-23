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
default global `Wire`). Create a dedicated `TwoWire` instance and pass
it to the driver:

```cpp
#include <Wire.h>
#include <APDS9960.h>

TwoWire internalI2C(I2C_INT_SDA, I2C_INT_SCL);
APDS9960 sensor(internalI2C);

void setup() {
    Serial.begin(115200);
    internalI2C.begin();

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

| Example                                      | What it does                                               |
| -------------------------------------------- | ---------------------------------------------------------- |
| [`ambient_light`](examples/ambient_light/)   | Read the ambient light and RGB colour channels.            |
| [`proximity`](examples/proximity/)           | Detect nearby objects using the infrared proximity sensor. |
| [`gesture`](examples/gesture/)               | Detect left, right, up, down, near and far gestures.       |
| [`light_theremin`](examples/light_theremin/) | Control the STeaMi buzzer pitch using ambient light.       |

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

| Method                                                                       | Description                                                                                                                  |
| ---------------------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------- |
| `APDS9960(TwoWire& wire = Wire, uint8_t address = APDS9960_DEFAULT_ADDRESS)` | Construct the driver.                                                                                                        |
| `bool begin()`                                                               | Probe the device, configure default registers and leave all engines disabled. Returns `false` if the sensor is not detected. |
| `uint8_t deviceId()`                                                         | Read the device ID register.                                                                                                 |
| `void powerOn()` / `void powerOff()`                                         | Enable or disable the sensor.                                                                                                |

### Ambient light and RGB

The driver automatically enables the ambient light engine if a reading
is requested while it is disabled.

| Method                                            | Description                       |
| ------------------------------------------------- | --------------------------------- |
| `bool ambientLight(uint16_t&)`                    | Read the clear channel.           |
| `bool redLight(uint16_t&)`                        | Read the red channel.             |
| `bool greenLight(uint16_t&)`                      | Read the green channel.           |
| `bool blueLight(uint16_t&)`                       | Read the blue channel.            |
| `bool lightReady()`                               | Ambient light data ready.         |
| `void enableLightSensor(bool interrupts = false)` | Enable the ambient light engine.  |
| `void disableLightSensor()`                       | Disable the ambient light engine. |

### Proximity

The driver automatically enables the proximity engine if a reading is
requested while it is disabled.

| Method                                                | Description                |
| ----------------------------------------------------- | -------------------------- |
| `bool proximity(uint8_t&)`                            | Read the proximity value.  |
| `bool proximityReady()`                               | Proximity data ready.      |
| `void enableProximitySensor(bool interrupts = false)` | Enable proximity sensing.  |
| `void disableProximitySensor()`                       | Disable proximity sensing. |

### Gesture

Gesture detection uses the APDS9960 gesture engine and reports one of:

* `LEFT`
* `RIGHT`
* `UP`
* `DOWN`
* `NEAR`
* `FAR`
* `NONE`

| Method                                              | Description                                     |
| --------------------------------------------------- | ----------------------------------------------- |
| `bool gestureAvailable()`                           | Returns whether gesture FIFO data is available. |
| `Gesture readGesture()`                             | Decode and return the detected gesture.         |
| `void enableGestureSensor(bool interrupts = false)` | Enable the gesture engine.                      |
| `void disableGestureSensor()`                       | Disable the gesture engine.                     |

### Configuration

The driver exposes the most useful runtime configuration options:

| Method                                                                         | Description                         |
| ------------------------------------------------------------------------------ | ----------------------------------- |
| `setAmbientLightGain()`                                                        | Ambient light ADC gain.             |
| `setProximityGain()`                                                           | Proximity receiver gain.            |
| `setGestureGain()`                                                             | Gesture receiver gain.              |
| `setLedDrive()`                                                                | Infrared LED drive current.         |
| `setGestureLedDrive()`                                                         | Gesture LED drive current.          |
| `setLedBoost()`                                                                | LED current boost factor.           |
| `setGestureWaitTime()`                                                         | Gesture wait period.                |
| `setGestureEnterThreshold()` / `setGestureExitThreshold()`                     | Gesture engine thresholds.          |
| `setLightInterruptLowThreshold()` / `setLightInterruptHighThreshold()`         | Ambient light interrupt thresholds. |
| `setProximityInterruptLowThreshold()` / `setProximityInterruptHighThreshold()` | Proximity interrupt thresholds.     |

### Interrupts

| Method                           | Description                             |
| -------------------------------- | --------------------------------------- |
| `setAmbientLightInterrupt(bool)` | Enable or disable ALS interrupts.       |
| `setProximityInterrupt(bool)`    | Enable or disable proximity interrupts. |
| `setGestureInterrupt(bool)`      | Enable or disable gesture interrupts.   |
| `clearAmbientLightInterrupt()`   | Clear the ALS interrupt flag.           |
| `clearProximityInterrupt()`      | Clear the proximity interrupt flag.     |

## Register constants

`APDS9960_const.h` exports register addresses, bit masks, default
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

## License

GPL-3.0-or-later — see [LICENSE](../../LICENSE).
