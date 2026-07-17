// SPDX-License-Identifier: GPL-3.0-or-later
//
// LightTheremin — control the STeaMi buzzer pitch with ambient light.
//
// The clear-light channel is mapped to a pentatonic scale.
// Cover the sensor to mute the buzzer.
//
// Open the serial monitor at 115200 baud.

#include <APDS9960.h>
#include <Wire.h>

TwoWire internalI2C(I2C_INT_SDA, I2C_INT_SCL);
APDS9960 sensor(internalI2C);

static const int BUZZER_PIN = SPEAKER;

static const uint16_t MIN_LIGHT = 45;
static const uint16_t MAX_LIGHT = 570;
static const uint32_t SAMPLE_PERIOD_MS = 20;
static const uint32_t PRINT_PERIOD_MS = 200;

static const uint16_t PENTATONIC_NOTES[] = {
    131, 147, 165, 196, 220, 262, 294, 330, 392, 440, 523, 587, 659, 784, 880,
};

static const size_t NOTE_COUNT = sizeof(PENTATONIC_NOTES) / sizeof(PENTATONIC_NOTES[0]);

uint32_t lastSampleMs = 0;
uint32_t lastPrintMs = 0;
uint16_t activeFrequency = 0;

uint16_t clampLight(uint16_t value) {
    if (value < MIN_LIGHT)
        return MIN_LIGHT;
    if (value > MAX_LIGHT)
        return MAX_LIGHT;
    return value;
}

uint16_t lightToFrequency(uint16_t light) {
    if (light < MIN_LIGHT)
        return 0;

    uint16_t clamped = clampLight(light);
    uint32_t range = MAX_LIGHT > MIN_LIGHT ? MAX_LIGHT - MIN_LIGHT : 1;

    size_t index = static_cast<size_t>(
        (static_cast<uint32_t>(clamped - MIN_LIGHT) * (NOTE_COUNT - 1)) / range);

    return PENTATONIC_NOTES[index];
}

void setBuzzerFrequency(uint16_t frequency) {
    if (frequency == activeFrequency)
        return;

    activeFrequency = frequency;

    if (frequency == 0) {
        noTone(BUZZER_PIN);
    } else {
        tone(BUZZER_PIN, frequency);
    }
}

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000)
        ;

    pinMode(BUZZER_PIN, OUTPUT);
    internalI2C.begin();

    if (!sensor.begin()) {
        Serial.println("APDS9960 not detected.");
        while (true)
            delay(1000);
    }

    sensor.enableLightSensor(false);

    Serial.println("=======================");
    Serial.println("    Light theremin     ");
    Serial.println("=======================");
    Serial.println("Move your hand over the sensor.");
    Serial.println("Cover it completely to mute.");
}

void loop() {
    uint32_t now = millis();
    if (now - lastSampleMs < SAMPLE_PERIOD_MS)
        return;

    lastSampleMs = now;

    uint16_t light = 0;
    if (!sensor.ambientLight(light)) {
        setBuzzerFrequency(0);
        return;
    }

    uint16_t frequency = lightToFrequency(light);
    setBuzzerFrequency(frequency);

    if (now - lastPrintMs >= PRINT_PERIOD_MS) {
        lastPrintMs = now;

        Serial.print("Light=");
        Serial.print(light);
        Serial.print(" | ");

        if (frequency == 0) {
            Serial.println("Muted");
        } else {
            Serial.print("Frequency=");
            Serial.print(frequency);
            Serial.println(" Hz");
        }
    }
}
