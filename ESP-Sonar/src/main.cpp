#include <vector>
#include <Arduino.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <string>
#include <sstream>
#include <fstream>
#include <NTPClient.h>
#include "time.h"
#include "esp_wpa2.h"
#include <HTTPClient.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"
#include <map>



#define WIFI_SSID "TechPublic"
#define WIFI_PASSWORD ""

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

std::map<string, pair<vector<int>, vector<int>>> sensors_dist;

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


  // {sensorID:{TRIG_PIN,ECHO_PIN,distance,total distance, last_seen}
  vector<int> error_vec = {0,0,0,0,0};
  vector<int> vec_s01 = {TRIG_PIN,ECHO_PIN,0,0};
  vector<int> vec_s02 = {0,0,0,0};
  vector<int> vec_s03 = {0,0,0,0};
  vector<int> vec_s04 = {0,0,0,0};
  vector<int> vec_s05 = {0,0,0,0};
  sensors_dist = {{"S-01",{vec_s01, error_vec}}, {"S-02",{vec_s02, error_vec}}, {"S-03",{vec_s03, error_vec}}, 
                  {"S-04",{vec_s04, error_vec}}, {"S-05",{vec_s05, error_vec}}};
}

String serverPath = "http://just-the-time.appspot.com/";

string genCurrTime() {
  HTTPClient http;
  http.begin(serverPath.c_str());
  int httpResponseCode = http.GET();
  String payload;
  if (httpResponseCode > 0) {
    payload = http.getString();
    Serial.print("Payload: ");
    Serial.println(payload);
  }
  else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
  }
  http.end();
  string date = payload.c_str();
  string year = date.substr(2,2);
  string month = date.substr(5,2);
  string day = date.substr(8,2);
  string hour = date.substr(11);
  string formated_date = day + "-" + month + "-" + year + " " + hour;
  struct tm timeinfo;
  strptime(formated_date.c_str(), "%d-%m-%y %H:%M:%S", &timeinfo);
  timeinfo.tm_hour += 3; // Align to Israel clock
  char buffer[32];
  // format - dd/mm/yy hh:mm:ss
  strftime(buffer,32, "%d-%m-%y %H:%M:%S", &timeinfo); 
  string buffer_s = buffer;
  // replace(buffer_s.begin(), buffer_s.end(), '/', '-');
  return buffer_s;
}


int total_count = 0;

string calcaulateSitting(int distance, int tot_distance, vector<int> error_vec) {
  int vec_count = 0, counter = 0;
  if (distance < SITTING_DISTANCE) { // increment count if we see someone sitting currently, otherwise decrement
    counter++;
  }
  else {
    counter--;
  }
  string sitting = "";
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
  if (total_count!= 0 && tot_distance/total_count < 5.5) { // it means that there is an error in the measurement (normal dist is between 15 to 100)
    sitting = "ERROR";
  }
  return sitting;
}


string int2str(int num) {
  ostringstream temp;
  temp << num;
  return temp.str();
}


int calcDistance(int trig_pin, int echo_pin) {
  float duration_us, distance_cm;
  // generate 10-microsecond pulse to TRIG pin (sonar)
  digitalWrite(trig_pin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trig_pin, LOW);
  // measure duration of pulse from ECHO pin
  duration_us = pulseIn(echo_pin, HIGH);
  // calculate the distance
  distance_cm = 0.017 * duration_us;
  return static_cast<int>(distance_cm);
}

void updateDB(string sensorID, vector<int> sensor_data, vector<int> error_vec) {
  string curr_time = genCurrTime();
  Serial.println("[DEBUG] curr_time: ");
  Serial.print(curr_time.c_str());
  string sitting = calcaulateSitting(sensor_data[2], sensor_data[3], error_vec);
  int call_id = 0;
  if (Firebase.RTDB.getInt(&fbdo, "data/call_id/"+sensorID)) {
    call_id = fbdo.to<int>();
    if (call_id == 0) {
      Firebase.RTDB.setInt(&fbdo, "data/call_id/"+sensorID, ++call_id);
    }
  }
  else {
    Serial.println("FAILED");
    Serial.println("REASON: " + fbdo.errorReason());
  }
  string call_id_str = int2str(call_id);
  call_id += 1;
  bool update_data_res = Firebase.RTDB.setString(&fbdo, "data/"+curr_time+" "+sensorID+"/callID", call_id_str) && 
                          Firebase.RTDB.setString(&fbdo, "data/"+curr_time+" "+sensorID+"/value", sitting) &&
                          Firebase.RTDB.setInt(&fbdo, "data/call_id/"+sensorID, call_id);
  bool update_real_data_res = Firebase.RTDB.setString(&fbdo, "real_data/"+sensorID+"/callID", call_id_str) && 
                              Firebase.RTDB.setString(&fbdo, "real_data/"+sensorID+"/value", sitting) &&
                              Firebase.RTDB.setString(&fbdo, "real_data/"+sensorID+"/time", curr_time);
  if (update_data_res && update_real_data_res) {  // updates the firebase
    Serial.print("value: ");
    Serial.println(sitting.c_str());
    Serial.println("PATH: " + fbdo.dataPath());
  }
  else {
    Serial.println("FAILED");
    Serial.println("REASON: " + fbdo.errorReason());
  }
}


unsigned long sendDataPrevMillis = 0;
void loop(){
  // loop through the sensors and get distance data
  for (auto it = sensors_dist.begin(); it != sensors_dist.end(); it++) {  // loop sensors and get data for each of them
    vector<int> data_vec = (it->second).first;
    data_vec[3] = calcDistance(data_vec[0], data_vec[1]); // distance
    data_vec[4] += data_vec[3];  //total distance
  }
  total_count++;  
  // millis() - sendDaeaPrevMillis decides the time interval in which we sending the data to the firebase
  if (Firebase.ready() && signupOK && (millis() - sendDataPrevMillis > FIREBASE_TIME_INTERVAL || sendDataPrevMillis == 0)) {
    sendDataPrevMillis = millis();
    for (auto it = sensors_dist.begin(); it != sensors_dist.end(); it++) {  // loop sensors and update data 
      vector<int> data_vec = (it->second).first;
      vector<int> error_vec = (it->second).second;
      updateDB(it->first, data_vec, error_vec);
      // reset the distance and total distance for new calculation
    }
    total_count = 0;
  }
}