#include <vector>
#include <Arduino.h>
#include <WiFi.h>
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
#define ERROR_VEC_LEN 5 // error vec holds the last sitting events (yes/no/error)
#define SITTING_DISTANCE 90 // the distance in cm in which we determine if someone is sitting
#define FIREBASE_TIME_INTERVAL 6000 // time interval to send data for the firebase in ms

//Define Firebase Data object
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

unsigned long sendDataPrevMillis = 0;
int count = 0, total_count = 0;
bool signupOK = false;
float duration_us, distance_cm;
std::vector<int> error_vec = {0,0,0,0,0};
int vec_count = 0, total_avg = 0;
char* sitting = "NO";

void setup(){
  Serial.begin(9600);
  // configure the trigger pin to output mode
  pinMode(TRIG_PIN, OUTPUT);
  // configure the echo pin to input mode
  pinMode(ECHO_PIN, INPUT);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED){ //wait for the connection to succeed
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
  if (Firebase.signUp(&config, &auth, "", "")){ //sign up to the database (can add credentials if needed)
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
  // generate 10-microsecond pulse to TRIG pin (sonar)
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  // measure duration of pulse from ECHO pin
  duration_us = pulseIn(ECHO_PIN, HIGH);
  // calculate the distance
  distance_cm = 0.017 * duration_us;
  total_avg += distance_cm; // calculate the total distance so we can measure error with the sonar.
  total_count++;  
  if (distance_cm < SITTING_DISTANCE) { // increment count if we see someone sitting currently, otherwise decrement
    count++;
  }
  else {
    count--;
  }
  // millis() - sendDaeaPrevMillis decides the time interval in which we sending the data to the firebase
  if (Firebase.ready() && signupOK && (millis() - sendDataPrevMillis > FIREBASE_TIME_INTERVAL || sendDataPrevMillis == 0)){
    sendDataPrevMillis = millis();
    // update the error_vector
    error_vec[vec_count] = count;
    vec_count++;
    vec_count = vec_count % ERROR_VEC_LEN;  // make sure it is updated regularly withing the length range
    int sitting_counter = 0;
    for (int i=0; i<ERROR_VEC_LEN; i++) { // calculate the number of times someone was sitting in the last elements in the vector
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
    if (total_avg/total_count < 5.5) { // it means that there is an error in the measurement (normal dist is between 15 to 100)
      sitting = "ERROR";
    }
    if (Firebase.RTDB.setString(&fbdo, "test/sitting", std::string(sitting))){  // updates the firebase
      // Serial.print("count: ");
      // Serial.println(count);
      // Serial.print("error_vec_counter: ");
      // Serial.println(sitting_counter);
      // Serial.print("total_avg: ");
      // Serial.println(total_avg);
      // Serial.print("total_count: ");
      // Serial.println(total_count);
      Serial.print("value: ");
      Serial.println(sitting);
      Serial.println("PATH: " + fbdo.dataPath());
    }
    else {
      Serial.println("FAILED");
      Serial.println("REASON: " + fbdo.errorReason());
    }
    count = 0;
    total_avg = 0;
    total_count = 0;
  }
}