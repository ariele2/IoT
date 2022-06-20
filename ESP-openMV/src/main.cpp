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
#include <FirebaseFS.h>


// Insert your network credentials
#define WIFI_SSID "TechPublic"
#define WIFI_PASSWORD ""

// Insert Firebase project API Key
#define API_KEY "AIzaSyCuLvDQQROn9LRXuxdiRhzE1ZmHgk_Bv4E"

// Insert RTDB URLefine the RTDB URL */
#define DATABASE_URL "https://iotprojdb-default-rtdb.europe-west1.firebasedatabase.app/" 

// Read pin from the open_mv
#define RXD2 16
#define TXD2 17

#define WIFI_CHECK_INTERVAL 1000*60*5  // 5 minutes interval   

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


const string sensorID = "D-01";

void setup() {
  Serial.begin(9600);
  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);  // initialize the UART connection
  // connect to the wifi
  connect2Wifi();

  // connect to firebase
  connect2Firebase();
  Serial.print("setup - Free firebase heap: ")
  Serial.println(Firebase.getFreeHeap());
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

string int2str(int num) {
  ostringstream temp;
  temp << num;
  return temp.str();
}

void removeCharsFromString(string &str, char* charsToRemove) {
   for ( unsigned int i = 0; i < strlen(charsToRemove); ++i ) {
      str.erase( remove(str.begin(), str.end(), charsToRemove[i]), str.end() );
   }
}

void checkWifiConnection() {
  unsigned long prevMillis = 0;
  if ((WiFi.status() != WL_CONNECTED ) && (millis() - prevMillis > 5000 || prevMillis == 0)) {
    WiFi.disconnect();
    Serial.println("Reconnecting Wifi...");
    WiFi.reconnect();
    prevMillis = millis();
  }
}

void checkAction() {
  unsigned long prevMillis = 0, wifiPrevMillis = 0;
  checkWifiConnection();
  if (Firebase.RTDB.getString(&fbdo, "action/")) {
    string action = fbdo.to<string>();
    while (action.compare("off")==0) {
      if (millis() - prevMillis > 5000 || prevMillis == 0) {
        Serial.print("checkAction - Free firebase heap: ")
        Serial.println(Firebase.getFreeHeap());
        if (Firebase.RTDB.getString(&fbdo, "action/")) {
          action = fbdo.to<string>();
        }
        else {
          Serial.println("Cannot access Firebase");
        }
        prevMillis = millis();
        Serial.print("action = ");
        Serial.print(action.c_str());
        Serial.println(" - system is off!");
      }
      if ((millis() - wifiPrevMillis > WIFI_CHECK_INTERVAL || wifiPrevMillis == 0 )) {
        checkWifiConnection();
        wifiPrevMillis = millis();
      }
    }
  }
}

void updateDB(string value) {
  string curr_time = genCurrTimeStr();
  Serial.print("[DEBUG] curr_time: ");
  Serial.println(curr_time.c_str());
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
  real_data_set.set("value",value);
  real_data_set.set("time",curr_time);
  data_set.set("callID", call_id_str);
  data_set.set("value", value);
  if (call_id_problem) {
    call_id = -1;
  }
  bool update_real_data_res = Firebase.RTDB.updateNode(&fbdo, "real_data/"+sensorID, &real_data_set);
  bool update_data_res = Firebase.RTDB.updateNode(&fbdo, "data/"+curr_time+" "+sensorID, &data_set) && 
                         Firebase.RTDB.setInt(&fbdo, "call_id/"+sensorID, call_id);
  if (update_data_res && update_real_data_res) {  // updates the firebase
    Serial.print("value: ");
    Serial.println(value.c_str());
    Serial.println("PATH: " + fbdo.dataPath());
  }
  else {
    Serial.println("FAILED");
    Serial.println("REASON: " + fbdo.errorReason());
  }
  Serial.print("updateDB - Free firebase heap: ")
  Serial.println(Firebase.getFreeHeap());
}


string fixReceivedData(string res) {
  if (res.find("out") != string::npos) {
      res = string("OUT");
      if (res.find("in") != string::npos) {
        if (res.find("in") > res.find("out")) {
          res = string("OUT");
        }
        else {
          res = string("IN");
        }
      }
    }
    else if (res.find("in") != string::npos) {
      res = string("IN");
  }
  return res;
}


unsigned long recvDataPrevMillis = 0;

void loop() {
  checkAction();
  Serial.print("main loop - Free firebase heap: ")
  Serial.println(Firebase.getFreeHeap());
  if (millis() - recvDataPrevMillis > 500 || recvDataPrevMillis == 0) {
    string res = string("");
    recvDataPrevMillis = millis(); 
    String rec_data = Serial2.readString();
    Serial.print("Recieved: ");
    Serial.println(rec_data);
    char buf[50] = "";
    rec_data.toCharArray(buf, 50);
    res = buf;
    res = fixReceivedData(res);
    Serial.print("res to cloud: ");
    Serial.println(res.c_str());
    if (Firebase.ready() && signupOK && (res.compare("IN") == 0 || res.compare("OUT") == 0)) {  
      updateDB(res);
      res = string("");
    }
  }
}