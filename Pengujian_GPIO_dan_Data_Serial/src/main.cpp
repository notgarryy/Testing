#include <Arduino.h>

void setup(){
  Serial.begin(115200);
}

void loop(){
  digitalWrite(LED_BUILTIN, HIGH);
  Serial.println("Start Loop!");
  delay(1000);
  digitalWrite(LED_BUILTIN, LOW);
  Serial.println("End Loop!");
  delay(1000);
}