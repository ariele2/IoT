/*
  Rui Santos
  Complete project details at our blog.
    - ESP32: https://RandomNerdTutorials.com/esp32-firebase-realtime-database/
    - ESP8266: https://RandomNerdTutorials.com/esp8266-nodemcu-firebase-realtime-database/
  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files.
  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
  Based in the RTDB Basic Example by Firebase-ESP-Client library by mobizt
  https://github.com/mobizt/Firebase-ESP-Client/blob/main/examples/RTDB/Basic/Basic.ino
*/

#include <vector>
#include <Arduino.h>
#if defined(ESP32)
  #include <WiFi.h>
#endif
#include <Firebase_ESP_Client.h>

//Provide the token generation process info.
#include "addons/TokenHelper.h"
//Provide the RTDB payload printing info and other helper functions.
#include "addons/RTDBHelper.h"

// Insert your network credentials
#define WIFI_SSID "HOTBOX 4-0158"
#define WIFI_PASSWORD "0544583887"

// Insert Firebase project API Key
#define API_KEY "AIzaSyCuLvDQQROn9LRXuxdiRhzE1ZmHgk_Bv4E"

// Insert RTDB URLefine the RTDB URL */
#define DATABASE_URL "https://iotprojdb-default-rtdb.europe-west1.firebasedatabase.app/" 

#define TRIG_PIN 23 // ESP32 pin GIOP23 connected to Ultrasonic Sensor's TRIG pin
#define ECHO_PIN 22 // ESP32 pin GIOP22 connected to Ultrasonic Sensor's ECHO pin
#define ERROR_VEC_LEN 5

//Define Firebase Data object
FirebaseData fbdo;

FirebaseAuth auth;
FirebaseConfig config;

unsigned long sendDataPrevMillis = 0;
int count = 0, dbg_count = 0;
bool signupOK = false;
float duration_us, distance_cm;
std::vector<int> error_vec = {0,0,0,0,0};
int vec_count = 0, dist_avg = 0;
char* sitting = "NO";

void setup(){
  Serial.begin(9600);
  // configure the trigger pin to output mode
  pinMode(TRIG_PIN, OUTPUT);
  // configure the echo pin to input mode
  pinMode(ECHO_PIN, INPUT);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED){
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();

  /* Assign the api key (required) */
  config.api_key = API_KEY;

  /* Assign the RTDB URL (required) */
  config.database_url = DATABASE_URL;

  /* Sign up */
  if (Firebase.signUp(&config, &auth, "", "")){
    Serial.println("ok");
    signupOK = true;
  }
  else{
    Serial.printf("%s\n", config.signer.signupError.message.c_str());
  }

  /* Assign the callback function for the long running token generation task */
  config.token_status_callback = tokenStatusCallback; //see addons/TokenHelper.h
  
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
}

void loop(){
  // generate 10-microsecond pulse to TRIG pin
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  // measure duration of pulse from ECHO pin
  duration_us = pulseIn(ECHO_PIN, HIGH);
  // calculate the distance
  distance_cm = 0.017 * duration_us;
  dist_avg += distance_cm;
  dbg_count++;
  if (distance_cm < 90) {
    count++;
  }
  else {
    count--;
  }
  if (Firebase.ready() && signupOK && (millis() - sendDataPrevMillis > 6000 || sendDataPrevMillis == 0)){
    sendDataPrevMillis = millis();
    // Write an Int number on the database path test/int
    error_vec[vec_count] = count;
    vec_count++;
    vec_count = vec_count % ERROR_VEC_LEN;
    int sitting_counter = 0;
    for (int i=0; i<ERROR_VEC_LEN; i++) {
      if (error_vec[i] > 0) {
        sitting_counter++;
      }
      else {
        sitting_counter--;
      }
    }
    // last 2 elements were positive or in the last 6 there is a major of positive - someone is sitting
    if ((count > 0 && error_vec[(vec_count-1)%ERROR_VEC_LEN] > 0) || (count < 0 && sitting_counter - 1 >= 0)) {
      sitting = "YES";
    }
    else {
      sitting = "NO";
    }
    if (dist_avg/dbg_count < 5.5) {
      sitting = "ERROR";
    }
    if (Firebase.RTDB.setString(&fbdo, "test/sitting", std::string(sitting))){
      // Serial.print("count: ");
      // Serial.println(count);
      // Serial.print("error_vec_counter: ");
      // Serial.println(sitting_counter);
      // Serial.print("dist_avg: ");
      // Serial.println(dist_avg);
      // Serial.print("dbg_count: ");
      // Serial.println(dbg_count);
      Serial.print("value: ");
      Serial.println(sitting);
      Serial.println("PATH: " + fbdo.dataPath());
    }
    else {
      Serial.println("FAILED");
      Serial.println("REASON: " + fbdo.errorReason());
    }
    count = 0;
    dist_avg = 0;
    dbg_count = 0;
  }
}