#include <stdio.h>
#include <string.h>
#include <iostream>

#define RXD2 16
#define TXD2 17

void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);
}

void loop() {
  delay(1000);
  Serial.println("Recieved: ");
  Serial.println(Serial2.readString());
}
