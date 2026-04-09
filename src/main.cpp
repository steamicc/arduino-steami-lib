#include <Arduino.h>

void setup() {
  // Initialisation du port série (défini par la variant)
  Serial.begin(115200);
  delay(2000);

  Serial.println("STeaMi minimal test");

  // LEDs définies dans la variant
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_BLUE, OUTPUT);
}

void loop() {
  Serial.println("Blink");

  // Allume toutes les LEDs
  digitalWrite(LED_RED, HIGH);
  digitalWrite(LED_GREEN, HIGH);
  digitalWrite(LED_BLUE, HIGH);
  delay(500);

  // Éteint toutes les LEDs
  digitalWrite(LED_RED, LOW);
  digitalWrite(LED_GREEN, LOW);
  digitalWrite(LED_BLUE, LOW);
  delay(500);
}
