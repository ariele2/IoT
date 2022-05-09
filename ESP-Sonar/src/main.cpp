#include <vector>
#include <Arduino.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <string>
#include <sstream>
#include <ctime>
#include <NTPClient.h>
#include "time.h"
#include "esp_wpa2.h"
#include <HTTPClient.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"


#define WIFI_SSID "S20-Ariel"
#define WIFI_PASSWORD "04061997"

// Insert Firebase project API Key
#define API_KEY "AIzaSyCuLvDQQROn9LRXuxdiRhzE1ZmHgk_Bv4E"

// Insert RTDB URLefine the RTDB URL */
#define DATABASE_URL "https://iotprojdb-default-rtdb.europe-west1.firebasedatabase.app/" 

#define TRIG_PIN 23 // ESP32 pin GIOP23 connected to Ultrasonic Sensor's TRIG pin
#define ECHO_PIN 22 // ESP32 pin GIOP22 connected to Ultrasonic Sensor's ECHO pin
#define ERROR_VEC_LEN 5 // error vec holds the last sitting events (yes/no/error)
#define SITTING_DISTANCE 90 // the distance in cm in which we determine if someone is sitting
#define FIREBASE_TIME_INTERVAL 30000 // time interval to send data for the firebase in ms


using namespace std;

// ntp server to get the time
const char* ntpServer = "pool.ntp.org"; 
const long  gmtOffset_sec = 0;
const int   daylightOffset_sec = 3600;



//Define Firebase Data object
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;


void connect2Wifi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi ");
  Serial.print(WIFI_SSID);
  while (WiFi.status() != WL_CONNECTED){ //wait for the connection to succeed
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();
}

bool signupOK = false;

void connect2Firebase() {
  config.api_key = API_KEY;
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


void setup() {
  Serial.begin(9600);
  // configure the trigger pins to sonar's output mode
  pinMode(TRIG_PIN, OUTPUT);
  // configure the echo pins to sonar's input mode
  pinMode(ECHO_PIN, INPUT);

  // connect to the wifi
  connect2Wifi();

  // ntp server configuration to get time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  // connect to firebase
  connect2Firebase();
}


string genCurrTime() {
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
  }
  timeinfo.tm_hour += 2; // Align to Israel clock
  char buffer[32];
  // format - dd/mm/yy hh:mm:ss
  strftime(buffer,32, "%d-%m-%y %H:%M:%S", &timeinfo); 
  string buffer_s = buffer;
  // replace(buffer_s.begin(), buffer_s.end(), '/', '-');
  return buffer_s;
}


unsigned long sendDataPrevMillis = 0;
int counter = 0, total_count = 0;
float duration_us, distance_cm;
int vec_count = 0, total_avg = 0;

string calcaulateSitting() {
  string sitting = "NO";
  std::vector<int> error_vec = {0,0,0,0,0};
  error_vec[vec_count] = counter;
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
    if ((counter > 0 && error_vec[(vec_count-1)%ERROR_VEC_LEN] > 0) || (counter < 0 && sitting_counter - 1 >= 0)) {
      sitting = "YES";
    }
    else {
      sitting = "NO";
    }
    if (total_avg/total_count < 5.5) { // it means that there is an error in the measurement (normal dist is between 15 to 100)
      sitting = "ERROR";
    }
  return sitting;
}


string int2str(int num) {
  ostringstream temp;
  temp << num;
  return temp.str();
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
    counter++;
  }
  else {
    counter--;
  }
  string curr_time = genCurrTime();
  string sensorID = "000";
  Serial.println("[DEBUG] curr_time: ");
  Serial.print(curr_time.c_str());
  // millis() - sendDaeaPrevMillis decides the time interval in which we sending the data to the firebase
  if (Firebase.ready() && signupOK && (millis() - sendDataPrevMillis > FIREBASE_TIME_INTERVAL || sendDataPrevMillis == 0)){
    sendDataPrevMillis = millis();
    string sitting = calcaulateSitting();
    int call_id = Firebase.RTDB.getInt(&fbdo, "data/call_id");
    call_id ++;
    string call_id_str = int2str(call_id);
    bool update_data_res = Firebase.RTDB.setString(&fbdo, "data/"+curr_time+" "+sensorID+"/callID", call_id_str) && 
                           Firebase.RTDB.setString(&fbdo, "data/"+curr_time+" "+sensorID+"/value/", sitting) &&
                           Firebase.RTDB.setInt(&fbdo, "data/call_id", call_id);
    bool update_real_data_res = Firebase.RTDB.setString(&fbdo, "real_data/"+sensorID+"/callID", call_id_str) && 
                                Firebase.RTDB.setString(&fbdo, "data/"+sensorID+"/value/", sitting) &&
                                Firebase.RTDB.setString(&fbdo, "data/time", curr_time);
    if (update_data_res && update_real_data_res) {  // updates the firebase
      Serial.print("value: ");
      Serial.println(sitting.c_str());
      Serial.println("PATH: " + fbdo.dataPath());
    }
    else {
      Serial.println("FAILED");
      Serial.println("REASON: " + fbdo.errorReason());
    }
    counter = 0;
    total_avg = 0;
    total_count = 0;
  }
}