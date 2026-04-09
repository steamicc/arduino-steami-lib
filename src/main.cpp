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
#ifndef PIN_SERIAL_RX
#define PIN_SERIAL_RX PB10
#endif
#ifndef PIN_SERIAL_TX
#define PIN_SERIAL_TX PB11
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

  Serial.setRx(PIN_SERIAL_RX);
  Serial.setTx(PIN_SERIAL_TX);
  Serial.begin(115200);
  while (!Serial) {
    delay(10); // attendre USB
  }
  Serial.println("STeaMi alive");
}

void loop() {
  setRgb(true, false, false);

  Serial.println("STeaMi alive");
}


