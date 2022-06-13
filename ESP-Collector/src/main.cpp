#include <vector>
#include <Arduino.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"
#include "string.h"
#include <map>
#include <vector>
#include "time.h"
#include <HTTPClient.h>


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

const bool debug = false;

void connect2Wifi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  if (debug) {
    Serial.print("Connecting to Wi-Fi");
  }
  while (WiFi.status() != WL_CONNECTED){ //wait for the connection to succeed
    if (debug) {
      Serial.print(".");
    }
    delay(300);
  }
  if (debug) {
    Serial.println();
    Serial.print("Connected with IP: ");
    Serial.println(WiFi.localIP());
    Serial.println();
  }
}

bool signupOK = false;

void connect2Firebase() {
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;

  /* Sign up */
  if (Firebase.signUp(&config, &auth, "", "")){ //sign up to the database (can add credentials if needed)
    if (debug) {
      Serial.println("ok");
    }
    signupOK = true;
  }
  else{ 
    if (debug) {
      Serial.printf("%s\n", config.signer.signupError.message.c_str());
    }
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
  if (Firebase.RTDB.getString(&fbdo, "sensors/")) {
    int sens_count = 0;
    string sensors_string = fbdo.to<string>();
    if (debug) {
      Serial.print("sensors_string: ");
      Serial.println(sensors_string.c_str());
    }
    int next_sensor_pos = 0;
    while (sensors_string.length() > 0) {
      if (which.compare("S1-5") == 0) {
        sen_to_find = sens_count < 5;
      }
      else if (which.compare("S6-10") == 0) {
        sen_to_find = sens_count > 5 && sens_count < 10;
      }
      int sensor_pos = sensors_string.find_first_of(':');
      string sensor = sensors_string.substr(0,sensor_pos);
      removeCharsFromString(sensor, "{\"},");
      int next_sensor_pos = sensors_string.find_first_of('}');
      sensors_string = sensors_string.substr(next_sensor_pos+1);
      if (sensor[0] != 'S' && which.compare("all") != 0) {
        continue;
      }
      if (debug) {
        Serial.print(sens_count);
        Serial.print(" [DEBUG] sensor: ");
        Serial.println(sensor.c_str());
      }
      if (sen_to_find && sensor.length() > 0) {
        sensors_ids.push_back(sensor);
      }
      sens_count++;
    }
  }
  else {
    if (debug) {
      Serial.println("FAILED");
      Serial.println("REASON: " + fbdo.errorReason());
    }
  }
  return sensors_ids;
}

// S - Sonar, D - Door, V - Volume
vector<string> sensors_names;
std::map<string, string> sensors_map;

void setup() {
  Serial.begin(9600);

  // connect to the wifi
  connect2Wifi();

  // connect to firebase
  connect2Firebase();

  sensors_names = getSensorsNames("all");

  for (int i = 0; i < sensors_names.size(); i++) {
    if (debug) {
      Serial.print("Inserted: ");
      Serial.print(sensors_names[i].c_str());
      Serial.println(" to sensors_map");
    }
    sensors_map[sensors_names[i]] = "0";
  }
}

String serverPath = "http://just-the-time.appspot.com/";

time_t genCurrTime() {
  HTTPClient http;
  http.begin(serverPath.c_str());
  int httpResponseCode = http.GET();
  String payload;
  if (httpResponseCode > 0) {
    payload = http.getString();
  }
  else {
    if (debug) {
      Serial.print("Error code: ");
      Serial.println(httpResponseCode);
    }
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
  time_t c_time = mktime(&timeinfo);
  return c_time;
}

vector<string> getSensorNameTime(string &ret_data) {
  string curr_sensor = "";
  int sesnsor_pos = ret_data.find_first_of(':');
  string sensor_name = ret_data.substr(0, sesnsor_pos);
  removeCharsFromString(sensor_name, "{}\",");
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
  return res;
}


time_t getSchedulerTime(string sched_time_str) {
  removeCharsFromString(sched_time_str, "{}\"");
  if (debug) {
    Serial.print("sched_time_str: ");
    Serial.println(sched_time_str.c_str());
  }
  struct tm sched_time;
  strptime(sched_time_str.c_str(), "%d-%m-%y %H:%M:%S", &sched_time);
  return mktime(&sched_time);
}

string divideRawString(string str) {
  string res = "";
  int col_pos = str.find(':');
  string elem = str.substr(0, col_pos);
  str = str.substr(col_pos+1);
  removeCharsFromString(elem, "{\"}");
  res += elem + ","; 
  if (debug) {
    Serial.print("divideRawString - str: ");
    Serial.println(str.c_str());
  }
  while(str.find_first_of(':') != string::npos) {
    int col_pos = str.find(string(":\""));
    string elem = str.substr(0, col_pos);
    if (debug) {
      Serial.print("elem: ");
      Serial.println(elem.c_str());
    }
    str = str.substr(col_pos+1);
    if (debug) {
      Serial.print("str: ");
      Serial.println(str.c_str());
    }
    removeCharsFromString(elem, "{\"}");
    res += elem + ","; 
  }
  removeCharsFromString(str, "{\"}");
  res += str;
  return res;
}


void checkScheduler(time_t curr_time) {
  if (Firebase.RTDB.getArray(&fbdo, "scheduler/")) {
    FirebaseJsonArray *scheduler_data_json = fbdo.to<FirebaseJsonArray*>();
    FirebaseJsonData curr_sched;
    scheduler_data_json->get(curr_sched, 0);
    if (curr_sched.success) {
      string curr_sched_s = curr_sched.to<string>();
      int m_pos = curr_sched_s.find("\":");
      string start_sched_s = curr_sched_s.substr(0, m_pos);
      string end_sched_s = curr_sched_s.substr(m_pos+3);
      time_t s_0 = getSchedulerTime(start_sched_s);
      time_t e_0 = getSchedulerTime(end_sched_s);
      if (Firebase.RTDB.getString(&fbdo, "action/")) {
        string action = fbdo.to<string>();
        if (difftime(curr_time, e_0) > 0) {
          scheduler_data_json->remove(0);
          if (scheduler_data_json->size() == 0) {
            Firebase.RTDB.deleteNode(&fbdo, "scheduler/");
          }
          else {
            Firebase.RTDB.setArray(&fbdo, "scheduler/", scheduler_data_json);
          }
          if (debug) {
            Serial.print("2 scheduler_data_json: ");
            Serial.println(scheduler_data_json->raw());
          }
          if (action.compare("off")!=0) {
            Firebase.RTDB.set(&fbdo, "/action", "off");
            if (debug) {
              Serial.println("Turning system off!");
            }
          }
        }
        else if (difftime(curr_time, s_0) > 0 && action.compare("on")!=0) {
          Firebase.RTDB.set(&fbdo, "/action", "on");
          if (debug) {
            Serial.println("Turning system on!");
          }
        }
      }
    }
  }
}


void checkAction() {
  if (Firebase.RTDB.getString(&fbdo, "action/")) {
    string action = fbdo.to<string>();
    if (debug) {
      Serial.print("[DEBUG] action: ");
      Serial.println(action.c_str());
    }
    while (action.compare("off")==0) {
      time_t curr_time = genCurrTime();
      checkScheduler(curr_time);
      Firebase.RTDB.getString(&fbdo, "action/");
      action = fbdo.to<string>();
      if (debug) {
        Serial.println("System is off!");
      }
      vTaskDelay(5000);
    }
  }
}


unsigned long recvDataPrevMillis = 0;

void loop() {
  if (Firebase.ready() && signupOK && (millis() - recvDataPrevMillis > 2000 || recvDataPrevMillis == 0)) {
    time_t curr_time = genCurrTime();
    checkScheduler(curr_time);
    checkAction();
    recvDataPrevMillis = millis();
   
    if (Firebase.RTDB.getString(&fbdo, "real_data/")){  // reads real_date from the firebase
      string ret_data = fbdo.to<string>();
      string ret_str = "";
      while(ret_data.length() > 1) {
        vector<string> sensor_name_time = getSensorNameTime(ret_data);
        if (sensor_name_time[1].compare(sensors_map[sensor_name_time[0]]) != 0) {
          sensors_map[sensor_name_time[0]] = sensor_name_time[1];
          string sensor_str = divideRawString(sensor_name_time[2]);
          ret_str += sensor_str + "\n";
        } 
      }
      if (debug) {
        Serial.print("ret_str: ");
      }
      Serial.print(ret_str.c_str());
    }
    else {
      if (debug) {
        Serial.println("FAILED");
        Serial.println("REASON: " + fbdo.errorReason());
      }
    }
  }
}