#include <Arduino.h>
void setup() {
  pinMode(PB6, OUTPUT);
  pinMode(PB7, OUTPUT);
}

void loop() {
  // Turn both ON
  digitalWrite(PB6, HIGH);
  digitalWrite(PB7, HIGH);
  delay(500);

  // Turn both OFF
  digitalWrite(PB7, LOW);
  delay(500);
}
