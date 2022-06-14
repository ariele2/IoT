#include <vector>
#include <Arduino.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <string>
#include <sstream>
#include <fstream>
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

#define TRIG_PIN1 23 // ESP32 pin GIOP23 connected to Ultrasonic Sensor's TRIG pin
#define ECHO_PIN1 22 // ESP32 pin GIOP22 connected to Ultrasonic Sensor's ECHO pin
//connected to these
#define TRIG_PIN2 3
#define ECHO_PIN2 2
//conected to these
#define TRIG_PIN3 18
#define ECHO_PIN3 19
//connected to these
#define TRIG_PIN4 33 
#define ECHO_PIN4 32 
//connected to these
#define TRIG_PIN5 26
#define ECHO_PIN5 27

// S1-5
#define SITTING_DISTANCE1 70
#define SITTING_DISTANCE2 70
#define SITTING_DISTANCE3 60
#define SITTING_DISTANCE4 70
#define SITTING_DISTANCE5 70
// S6-10
// #define SITTING_DISTANCE1 47
// #define SITTING_DISTANCE2 50
// #define SITTING_DISTANCE3 65
// #define SITTING_DISTANCE4 50
// #define SITTING_DISTANCE5 50

#define FIREBASE_TIME_INTERVAL 3000 // time interval to send data for the firebase in ms


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
    Serial.print("sensors_string: ");
    Serial.println(sensors_string.c_str());
    int next_sensor_pos = 0;
    while (sensors_string.length() > 0) {
      if (which.compare("S1-5") == 0) {
        sen_to_find = sens_count < 5;
      }
      else if (which.compare("S6-10") == 0) {
        sen_to_find = sens_count >= 5 && sens_count < 10;
      }
      int sensor_pos = sensors_string.find_first_of(':');
      string sensor = sensors_string.substr(0,sensor_pos);
      removeCharsFromString(sensor, "{\"},");
      int next_sensor_pos = sensors_string.find_first_of('}');
      sensors_string = sensors_string.substr(next_sensor_pos+1);
      if (sensor[0] != 'S' && which.compare("all") != 0) {
        continue;
      }
      Serial.print(sens_count);
      Serial.print(" [DEBUG] sensor: ");
      Serial.println(sensor.c_str());
      if (sen_to_find && sensor.length() > 0) {
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

std::map<string, vector<int>> sensors_dist;

void setup() {
  Serial.begin(9600);
  // configure the trigger pins to sonar's output mode
  pinMode(TRIG_PIN1, OUTPUT);
  pinMode(TRIG_PIN2, OUTPUT);
  pinMode(TRIG_PIN3, OUTPUT);
  pinMode(TRIG_PIN4, OUTPUT);
  pinMode(TRIG_PIN5, OUTPUT);
  // configure the echo pins to sonar's input mode
  pinMode(ECHO_PIN1, INPUT);
  pinMode(ECHO_PIN2, INPUT);
  pinMode(ECHO_PIN3, INPUT);
  pinMode(ECHO_PIN4, INPUT);
  pinMode(ECHO_PIN5, INPUT);

  // connect to the wifi
  connect2Wifi();

  // connect to firebase
  connect2Firebase();

  vector<string> sensors_ids = getSensorsNames("S1-5");

  // {sensorID:{TRIG_PIN,ECHO_PIN,counter,total distance, sitting_distance}
  vector<int> vec_s01 = {TRIG_PIN1, ECHO_PIN1, 0, 0, SITTING_DISTANCE1};
  vector<int> vec_s02 = {TRIG_PIN2, ECHO_PIN2, 0, 0, SITTING_DISTANCE2};
  vector<int> vec_s03 = {TRIG_PIN3, ECHO_PIN3, 0, 0, SITTING_DISTANCE3};
  vector<int> vec_s04 = {TRIG_PIN4, ECHO_PIN4, 0, 0, SITTING_DISTANCE4};
  vector<int> vec_s05 = {TRIG_PIN5, ECHO_PIN5, 0, 0, SITTING_DISTANCE5};
  sensors_dist = {{sensors_ids[0], vec_s01}, {sensors_ids[1], vec_s02}, {sensors_ids[2], vec_s03}, 
                  {sensors_ids[3], vec_s04}, {sensors_ids[4], vec_s05}};
}

String serverPath = "http://just-the-time.appspot.com/";

string genCurrTimeStr() {
  HTTPClient http;
  http.begin(serverPath.c_str());
  int httpResponseCode = http.GET();
  String payload;
  if (httpResponseCode > 0) {
    payload = http.getString();
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
  mktime(&timeinfo);
  char buffer[32];
  // format - dd/mm/yy hh:mm:ss
  strftime(buffer,32, "%d-%m-%y %H:%M:%S", &timeinfo); 
  string buffer_s = buffer;
  return buffer_s;
}


int tot_count = 0; // depends on the loops iteration we provide

string int2str(int num) {
  ostringstream temp;
  temp << num;
  return temp.str();
}


// calculates counter for each sensor
vector<int> calcSitCounter(int trig_pin, int echo_pin, int sensor_counter, int tot_distance, int sitting_distance) {
  float distance;
  digitalWrite(trig_pin, LOW);
  delayMicroseconds(2);
  digitalWrite(trig_pin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trig_pin, LOW);
  unsigned long duration = pulseIn(echo_pin, HIGH);
  distance = (duration/2) / 29.1;
  Serial.print("distance measured: ");
  Serial.println(distance);
  if (distance < sitting_distance) {
    sensor_counter++;
  }
  else {
    sensor_counter--;
  }
  tot_distance += static_cast<int>(distance);
  vector<int> res = {sensor_counter, tot_distance};
  return res;
}


string isSitting(int counter, int tot_distance) {
  string sitting = "";
  if (counter > 0) {
    sitting = "YES";
  }
  else {
    sitting = "NO";
  }
  if (tot_count != 0 && tot_distance/tot_count < 5.5) { // it means that there is an error in the measurement (normal dist is between 15 to 100)
    sitting = "ERROR";
  }
  return sitting;
}


void updateDB(string sensorID, vector<int> sensor_data) {
  string curr_time = genCurrTimeStr();
  Serial.print("[DEBUG] curr_time: ");
  Serial.println(curr_time.c_str());
  string sitting = isSitting(sensor_data[2], sensor_data[3]);
  int call_id = 0;
  bool call_id_problem = false;
  if (Firebase.ready() && Firebase.RTDB.getInt(&fbdo, "call_id/"+sensorID)) {
    call_id = fbdo.to<int>();
    if (call_id == 0) {
      Firebase.RTDB.setInt(&fbdo, "call_id/"+sensorID, 1);
    }
  }
  else {
    call_id_problem = true;
    Serial.println("FAILED");
    Serial.println("REASON: " + fbdo.errorReason());
  }
  string call_id_str = int2str(call_id);
  call_id += 1;
  FirebaseJson real_data_set, data_set;
  real_data_set.set("callID",call_id_str);
  real_data_set.set("value",sitting);
  real_data_set.set("time",curr_time);
  data_set.set("callID", call_id_str);
  data_set.set("value", sitting);
  if (call_id_problem) {
    call_id = -1;
  }
  bool update_real_data_res = Firebase.RTDB.updateNode(&fbdo, "real_data/"+sensorID, &real_data_set);
  bool update_data_res = Firebase.RTDB.updateNode(&fbdo, "data/"+curr_time+" "+sensorID, &data_set) && 
                         Firebase.RTDB.setInt(&fbdo, "call_id/"+sensorID, call_id);
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

time_t getSchedulerTime(string sched_time_str) {
  removeCharsFromString(sched_time_str, "{}\"");
  struct tm sched_time;
  strptime(sched_time_str.c_str(), "%d-%m-%y %H:%M:%S", &sched_time);
  return mktime(&sched_time);
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
          if (action.compare("off")!=0) {
            Firebase.RTDB.set(&fbdo, "/action", "off");
            Serial.println("Turning system off!");
          }
        }
        else if (difftime(curr_time, s_0) > 0 && action.compare("on")!=0) {
          Firebase.RTDB.set(&fbdo, "/action", "on");
          Serial.println("Turning system on!");
        }
      }
    }
  }
}

time_t genCurrTime() {
  HTTPClient http;
  http.begin(serverPath.c_str());
  int httpResponseCode = http.GET();
  String payload;
  if (httpResponseCode > 0) {
    payload = http.getString();
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
  time_t c_time = mktime(&timeinfo);
  return c_time;
}

void checkAction() {
  if (Firebase.RTDB.getString(&fbdo, "action/")) {
    string action = fbdo.to<string>();
    while (action.compare("off")==0) {
      time_t curr_time = genCurrTime();
      checkScheduler(curr_time);
      Firebase.RTDB.getString(&fbdo, "action/");
      action = fbdo.to<string>();
        Serial.println("System is off!");
      vTaskDelay(5000);
    }
  }
}


unsigned long sendDataPrevMillis = 0;
void loop() {
  // check if system is on
  checkAction();
  Serial.println("[DEBUG] check action finished ");
  Serial.println();
  // loop through the sensors and get distance data
  unsigned long pull_data_time = millis();
  Serial.println("[DEBUG] calculating how many times someone signaled as sit stage ");
  while(pull_data_time + 10000 > millis()) { // get data from sensors in 2 seconds
    Serial.print("[DEBUG] millis: ");
    Serial.println(millis());
    Serial.print("[DEBUG] pull_data_time: ");
    Serial.println(pull_data_time);
    for (auto it = sensors_dist.begin(); it != sensors_dist.end(); it++) {  // loop sensors and get data for each of them
      Serial.print("[DEBUG] sensorID: ");
      Serial.print((it->first).c_str());
      vector<int> res = calcSitCounter(it->second[0], it->second[1], it->second[2], it->second[3], it->second[4]); // distance
      it->second[2] = res[0];
      it->second[3] = res[1];
      Serial.print("; counter: ");
      Serial.print(it->second[2]);
      Serial.print("; total distance: ");
      Serial.println(it->second[3]);
    }
    tot_count++;
  }
  Serial.print("[DEBUG] Finished pulling data - tot_count: ");
  Serial.println(tot_count);
  // millis() - sendDaeaPrevMillis decides the time interval in which we sending the data to the firebase
  if (Firebase.ready() && (millis() - sendDataPrevMillis > FIREBASE_TIME_INTERVAL || sendDataPrevMillis == 0)) {
    sendDataPrevMillis = millis();
    for (auto it = sensors_dist.begin(); it != sensors_dist.end(); it++) {  // loop sensors and update data 
      updateDB(it->first, it->second);
      // reset the distance and total distance for new calculation
      it->second[2] = 0;
      it->second[3] = 0;
    }
  }
  tot_count = 0;
}