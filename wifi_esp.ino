#include <WiFi.h>

const char* ssid = "HOTBOX 4-0158";
const char* password = "0544583887";

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  Serial.print("Connetcing to... ");
  Serial.print(ssid);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.println(".");
    delay(500);
  }
  Serial.println("\nConnected successfully!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

void loop() {
  // put your main code here, to run repeatedly:
  if ((WiFi.status() == WL_CONNECTED)) {
    Serial.print("Try to ping me :)");
    delay(5000);
  }
}
