#include <Arduino.h>

#ifndef PIN_LED_RED
#define PIN_LED_RED PC12
#endif
#ifndef PIN_LED_GREEN
#define PIN_LED_GREEN PC11
#endif
#ifndef PIN_LED_BLUE
#define PIN_LED_BLUE PC10
#endif

static void setRgb(bool r, bool g, bool b) {
  digitalWrite(PIN_LED_RED, r ? HIGH : LOW);
  digitalWrite(PIN_LED_GREEN, g ? HIGH : LOW);
  digitalWrite(PIN_LED_BLUE, b ? HIGH : LOW);
}

void setup() {
  pinMode(PIN_LED_RED, OUTPUT);
  pinMode(PIN_LED_GREEN, OUTPUT);
  pinMode(PIN_LED_BLUE, OUTPUT);

  Serial.begin(115200);
  delay(200);
  Serial.println("STeaMi alive");
}

void loop() {
  Serial.println("STeaMi alive and blinking RED");
  setRgb(true, false, false);
  delay(1000);
  setRgb(false, true, false);
  Serial.println("STeaMi alive and blinking BLUE");
  delay(1000);
  setRgb(false, false, true);
  Serial.println("STeaMi alive and blinking GREEN");
  delay(1000);
}
