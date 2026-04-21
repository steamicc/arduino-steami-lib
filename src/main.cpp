// Build smoke-test sketch: validates the board definition + STM32duino variant
// compile. Not an example — see lib/<component>/examples/ for driver usage.
// TODO(#102): once a driver is implemented under lib/, exercise it here so CI
// also validates driver linkage.

#include <Arduino.h>

void setup() {
    // Serial initialized by the variant
    Serial.begin(115200);
    delay(2000);

    Serial.println("STeaMi minimal test");

    // LEDs defined by the variant
    pinMode(LED_RED, OUTPUT);
    pinMode(LED_GREEN, OUTPUT);
    pinMode(LED_BLUE, OUTPUT);
}

void loop() {
    Serial.println("Blink");

    // Turn all LEDs on
    digitalWrite(LED_RED, HIGH);
    digitalWrite(LED_GREEN, HIGH);
    digitalWrite(LED_BLUE, HIGH);
    delay(500);

    // Turn all LEDs off
    digitalWrite(LED_RED, LOW);
    digitalWrite(LED_GREEN, LOW);
    digitalWrite(LED_BLUE, LOW);
    delay(500);
}
