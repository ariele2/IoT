#include <vector>
#include <Arduino.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>

//Provide the token generation process info.
#include "addons/TokenHelper.h"
//Provide the RTDB payload printing info and other helper functions.
#include "addons/RTDBHelper.h"

// Insert your network credentials
#define WIFI_SSID "TechPublic"
#define WIFI_PASSWORD ""

// Insert Firebase project API Key
#define API_KEY "AIzaSyCuLvDQQROn9LRXuxdiRhzE1ZmHgk_Bv4E"

// Insert RTDB URLefine the RTDB URL */
#define DATABASE_URL "https://iotprojdb-default-rtdb.europe-west1.firebasedatabase.app/" 

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
  
  if (Firebase.ready() && signupOK && (millis() - sendDataPrevMillis > 5000 || sendDataPrevMillis == 0)){
    sendDataPrevMillis = millis();
    // update the error_vector
   
    if (Firebase.RTDB.getString(&fbdo, "test/sitting")){  // updates the firebase
      // Serial.print("count: ");
      // Serial.println(count);
      // Serial.print("error_vec_counter: ");
      // Serial.println(sitting_counter);
      // Serial.print("total_avg: ");
      // Serial.println(total_avg);
      // Serial.print("total_count: ");
      // Serial.println(total_count);
      Serial.print("value: ");
      Serial.println(fbdo.stringData());
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