#include <Arduino.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>
#include <sps30.h>

#define WIFI_SSID     "ssid"
#define WIFI_PASSWORD "password"

#define API_KEY       "api_key"
#define DATABASE_URL  "url"

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

unsigned long sendDataPrevMillis = 0;
unsigned long count = 1;

void sps_init(){
  int16_t ret;
  uint8_t auto_clean_days = 4;
  uint32_t auto_clean;

  delay(2000);

  sensirion_i2c_init();

  while (sps30_probe() != 0) {
    Serial.print("SPS sensor probing failed\n");
    delay(500);
  }

  #ifndef PLOTTER_FORMAT
    Serial.print("SPS sensor probing successful\n");
  #endif /* PLOTTER_FORMAT */

  ret = sps30_set_fan_auto_cleaning_interval_days(auto_clean_days);
  if (ret) {
    Serial.print("error setting the auto-clean interval: ");
    Serial.println(ret);
  }

  ret = sps30_start_measurement();
  if (ret < 0) {
    Serial.print("error starting measurement\n");
  }
}

void sps_read(float* pm25, float* pm10){
  struct sps30_measurement m;
  char serial[SPS30_MAX_SERIAL_LEN];
  uint16_t data_ready;
  int16_t ret;

  do {
    ret = sps30_read_data_ready(&data_ready);
    if (ret < 0) {
      Serial.print("error reading data-ready flag: ");
      Serial.println(ret);
    } else if (!data_ready)
      Serial.print("data not ready, no new measurement available\n");
    else
      break;
    delay(100); /* retry in 100ms */
  } while (1);

  ret = sps30_read_measurement(&m);
  if (ret < 0) {
    Serial.print("error reading measurement\n");
  } else {
    Serial.print("PM  2.5: ");
    Serial.println(m.mc_2p5);
    *pm25 = m.mc_2p5;
    Serial.print("PM 10.0: ");
    Serial.println(m.mc_10p0);
    *pm10 = m.mc_10p0;
  }
}

void fb_init() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();

  Serial.printf("Firebase Client v%s\n\n", FIREBASE_CLIENT_VERSION);

  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  config.token_status_callback = tokenStatusCallback;

  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("Anonymous sign-up successful");
  } else {
    Serial.printf("Sign-up failed: %s\n", config.signer.signupError.message.c_str());
  }

  Firebase.begin(&config, &auth);  
  Firebase.reconnectNetwork(true);
}

void fb_send(float* pm25, float* pm10) {
  FirebaseJson json;
  json.set("pm25", *pm25);
  json.set("pm10", *pm10);
  json.set(F("ts/.sv"), F("timestamp")); 

  String path = "/test/data_" + String(count, DEC);  // format angka count
  if (count < 10) {
    path = "/test/data_000" + String(count, DEC);  // Jika count < 10
  } else if (count < 100) {
    path = "/test/data_00" + String(count, DEC);   // Jika count < 100
  } else if (count < 1000) {
    path = "/test/data_0" + String(count, DEC);    // Jika count < 1000
  }
  
  bool success = Firebase.RTDB.set(&fbdo, path.c_str(), &json);
  Serial.printf("Send to %s -> %s\n", path.c_str(), success ? "ok" : fbdo.errorReason().c_str());
  count++;
}

void setup() {
  Serial.begin(115200);
  fb_init();
  delay(100);
  sps_init();
  delay(1000);
}

void loop() {
  float pm25, pm10;

  if (Firebase.ready() && (millis() - sendDataPrevMillis > 10000 || sendDataPrevMillis == 0)) {
    sendDataPrevMillis = millis();  
    sps_read(&pm25, &pm10);
    fb_send(&pm25, &pm10);
  }
}