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
#define RESTART_INTERVAL 1000*60*15 

using namespace std;

//Define Firebase Data object
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

/* serial debug should be false when uploading code to the esps to run, otherwise it will get stuck 
   Turn it on to watch Serial prints */
const bool serial_debug = false;

void connect2Wifi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  if (serial_debug) {
    Serial.print("Connecting to Wi-Fi");
  }
  int not_connected_ctr = 0;
  while (WiFi.status() != WL_CONNECTED){ //wait for the connection to succeed
    if (not_connected_ctr > 60) {
      ESP.restart();
    }
    if (serial_debug) {
      Serial.print(".");
    }
    delay(1000);
    not_connected_ctr++;
  }
  if (serial_debug) {
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
    if (serial_debug) {
      Serial.println("ok");
    }
    signupOK = true;
  }
  else{ 
    if (serial_debug) {
      Serial.printf("%s\n", config.signer.signupError.message.c_str());
    }
    ESP.restart();
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
}


String serverPath = "http://just-the-time.appspot.com/";
// get the current time from appspot, because the NTP servers are blocked within the technion's wifi 
string genCurrTimeStr() {
  HTTPClient http;
  http.begin(serverPath.c_str());
  int httpResponseCode = http.GET();
  String payload;
  if (httpResponseCode > 0) {
    payload = http.getString();
  }
  else {
    if (serial_debug) {
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

// utility function 
void removeCharsFromString(string &str, char* charsToRemove) {
   for ( unsigned int i = 0; i < strlen(charsToRemove); ++i ) {
      str.erase( remove(str.begin(), str.end(), charsToRemove[i]), str.end() );
   }
}

// checks if the wifi is connected, if it cant connect for a minute - restart the esp
void checkWifiConnection() {
  if (serial_debug) {
    Serial.println("checking wifi conection...");
  }
  unsigned long prevMillis = 0;
  int fail_connect_ctr = 0;
  if (WiFi.status() != WL_CONNECTED) {
    if (serial_debug) {
      Serial.println("wifi is not connected");
    }
    WiFi.disconnect();
    while((WiFi.status() != WL_CONNECTED ) && (millis() - prevMillis > 5000 || prevMillis == 0)) {
      if (fail_connect_ctr > 60) {
        if (serial_debug) {
          Serial.println("Restarting device");
        }
        ESP.restart();
      }
      if (serial_debug) {
        Serial.println("reconnecting wifi...");
      }
      WiFi.reconnect();
      prevMillis = millis();
      fail_connect_ctr++;
    }
  }
}

// for the reset button on the web - can restart the esp with a button press
void checkReset() {
  string reset_str = "resetmv/"; 
  if (Firebase.ready() && Firebase.RTDB.getString(&fbdo, reset_str)) { 
    string reset = fbdo.to<string>();
    if (reset.compare("yes") == 0) {
      if (serial_debug) {
        Serial.println("Reseting system.....");
      }
      Firebase.RTDB.setString(&fbdo, reset_str, "no"); 
      vTaskDelay(5000);
      ESP.restart();
    }
  }
  else {
    if (serial_debug) {
      Serial.println("checkReset - FAILED");
      Serial.println("REASON: " + fbdo.errorReason());
    }
  }
}

// check if the system is on, if it is off - keep checking for 'on' state until changed.
void checkAction() {
  unsigned long prevMillis = 0, wifiPrevMillis = 0;
  checkReset();
  checkWifiConnection();
  int counter = 0;
  if (Firebase.ready() && Firebase.RTDB.getString(&fbdo, "action/")) {
    string action = fbdo.to<string>();
    while (action.compare("off")==0) {
      if (millis() - prevMillis > 5000 || prevMillis == 0) {
        checkReset();
        if (Firebase.ready() && Firebase.RTDB.getString(&fbdo, "action/")) {
          action = fbdo.to<string>();
        }
        else {
          if (serial_debug) {
            Serial.println("checkAction inside loop - FAILED");
            Serial.println("REASON: " + fbdo.errorReason());
          }
        }
        prevMillis = millis();
        if (serial_debug) {
          Serial.print("action = ");
          Serial.print(action.c_str());
          Serial.println(" - system is off!");
        }
      }
      if ((millis() - wifiPrevMillis > WIFI_CHECK_INTERVAL || wifiPrevMillis == 0 )) {
        checkWifiConnection();
        wifiPrevMillis = millis();
      }
    }
  }
  else {
    if (serial_debug) {
      Serial.println("checkAction outside loop - FAILED");
      Serial.println("REASON: " + fbdo.errorReason());
    }
  }
}

void updateDB(string value) {
  string curr_time = genCurrTimeStr();
  if (serial_debug) {
    Serial.print("[DEBUG] curr_time: ");
    Serial.println(curr_time.c_str());
  }
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
    if (serial_debug) {
      Serial.println("updateDB - FAILED");
      Serial.println("REASON: " + fbdo.errorReason());
    }
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
  if (serial_debug) {
      Serial.print("value: ");
      Serial.println(value.c_str());
      Serial.println("PATH: " + fbdo.dataPath());
  }
  }
  else {
    if (serial_debug) {
      Serial.println("updateDB - FAILED");
      Serial.println("REASON: " + fbdo.errorReason());
    }
  }
}

// makes the letters received from the openMV capital letters
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
int off_reset_ctr = 0;
void loop() {
  checkAction();
  if (millis() - recvDataPrevMillis > 500 || recvDataPrevMillis == 0) {
    string res = string("");
    recvDataPrevMillis = millis(); 
    String rec_data = Serial2.readString();
    if (serial_debug) {
      Serial.print("Recieved: ");
      Serial.println(rec_data);
    }
    char buf[50] = "";
    rec_data.toCharArray(buf, 50);
    res = buf;
    res = fixReceivedData(res);
    if (serial_debug) {
      Serial.print("res to cloud: ");
      Serial.println(res.c_str());
    }
    if (Firebase.ready() && signupOK) {
      if (res.compare("IN") == 0 || res.compare("OUT") == 0) {  
        updateDB(res);
        res = string("");
      }
    }
    else {
      if (serial_debug) {
        Serial.println("main loop - FAILED");
        Serial.println("REASON: " + fbdo.errorReason());
      }
    }
  }
}