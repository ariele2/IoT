#include <vector>
#include <Arduino.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"
#include "string.h"
#include <map>
#include <vector>


// Insert your network credentials
#define WIFI_SSID "TechPublic"
#define WIFI_PASSWORD ""

// Insert Firebase project API Key
#define API_KEY "AIzaSyCuLvDQQROn9LRXuxdiRhzE1ZmHgk_Bv4E"

// Insert RTDB URLefine the RTDB URL */
#define DATABASE_URL "https://iotprojdb-default-rtdb.europe-west1.firebasedatabase.app/" 

using namespace std;

//Define Firebase Data object
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

void connect2Wifi() {
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


void removeCharsFromString(string &str, char* charsToRemove) {
   for ( unsigned int i = 0; i < strlen(charsToRemove); ++i ) {
      str.erase( remove(str.begin(), str.end(), charsToRemove[i]), str.end() );
   }
}


vector<string> getSensorsNames(string which) {
  vector<string> sensors_ids;
  bool sen_to_find;
  int sens_count = 0;
  if (which.compare("all") == 0) {
    sen_to_find = true;
  }
  else if (which.compare("S1-5") == 0) {
    sen_to_find = sens_count < 5;
  }
  else if (which.compare("S6-10") == 0) {
    sen_to_find = sens_count > 5 && sens_count < 10;
  }
  if (Firebase.RTDB.getString(&fbdo, "sensors/")) {
    int sens_count = 0;
    string sensors_string = fbdo.to<string>();
    Serial.print("sensors_string: ");
    Serial.println(sensors_string.c_str());
    int next_sensor_pos = 0;
    while (next_sensor_pos != string::npos) {
      int sensor_pos = sensors_string.find_first_of(':');
      string sensor = sensors_string.substr(0,sensor_pos);
      removeCharsFromString(sensor, "{\"}");
      int next_sensor_pos = sensors_string.find_first_of(',');
      sensors_string = sensors_string.substr(next_sensor_pos+1);
      if (sensor[0] != 'S' and which.compare("all") != 0) {
        continue;
      }
      Serial.print(sens_count);
      Serial.print(" [DEBUG] sensor: ");
      Serial.println(sensor.c_str());
      if (sen_to_find) {
        sensors_ids.push_back(sensor);
      }
      sens_count++;
    }
  }
  else {
    Serial.println("FAILED");
    Serial.println("REASON: " + fbdo.errorReason());
  }
  return sensors_ids;
}

vector<string> sensors_names = getSensorsNames("all");

// S - Sonar, D - Door, V - Volume
std::map<string, string> sensors_map;

void setup() {
  Serial.begin(9600);
  Serial2.begin(11500);

  // connect to the wifi
  connect2Wifi();

  // connect to firebase
  connect2Firebase();

  for (int i = 0; i < sensors_names.size(); i++) {
    Serial.print("Inserted: ");
    Serial.print(sensors_names[i].c_str());
    Serial.println(" to sensors_map");
    sensors_map[sensors_names[i]] = "0";
  }
}


vector<string> getSensorNameTime(string &ret_data) {
  string curr_sensor = "";
  int sesnsor_pos = ret_data.find_first_of(':');
  string sensor_name = ret_data.substr(0, sesnsor_pos);
  removeCharsFromString(sensor_name, "{}\"");
  curr_sensor += sensor_name;
  ret_data = ret_data.substr(sesnsor_pos); 
  int time_pos = ret_data.find("time") + 7;
  curr_sensor += ret_data.substr(0,time_pos);
  ret_data = ret_data.substr(time_pos);
  int end_time_pos = ret_data.find_first_of('"');
  string sensor_time = ret_data.substr(0,end_time_pos);
  int end_sensor_pos = ret_data.find_first_of('}');
  curr_sensor += ret_data.substr(0, end_sensor_pos+1); 
  ret_data = ret_data.substr(end_sensor_pos+1);
  vector<string> res = {sensor_name, sensor_time, curr_sensor};
  Serial.print("sensor_name: ");
  Serial.print(sensor_name.c_str());
  Serial.print("; sensor_time: ");
  Serial.print(sensor_time.c_str());
  Serial.print("; curr_sensor: ");
  Serial.println(curr_sensor.c_str());
  return res;
}


unsigned long recvDataPrevMillis = 0;

void loop() {
  if (Firebase.ready() && signupOK && (millis() - recvDataPrevMillis > 20000 || recvDataPrevMillis == 0)) {
    
    recvDataPrevMillis = millis();
   
    if (Firebase.RTDB.getString(&fbdo, "real_data/")){  // reads real_date from the firebase
      string ret_data = fbdo.to<string>();
      string ret_str = "";
      while(ret_data.length() > 1) {
        vector<string> sensor_name_time = getSensorNameTime(ret_data);
        if (sensor_name_time[1].compare(sensors_map[sensor_name_time[0]]) != 0) {
          sensors_map[sensor_name_time[0]] = sensor_name_time[1];
          ret_str += sensor_name_time[2] + " ";
        } 
      }
      Serial.print("ret_str: ");
      Serial.println(ret_str.c_str());
      Serial.println("PATH: " + fbdo.dataPath());
    }
    else {
      Serial.println("FAILED");
      Serial.println("REASON: " + fbdo.errorReason());
    }
  }
}