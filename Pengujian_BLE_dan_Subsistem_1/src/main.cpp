#include <Arduino.h>

// BLE Libraries
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// SPS Library
#include <sps30.h>

// Variables for storing data and BLE connection management
unsigned long lastUpdateTime = 0;
unsigned long lastAvgTime = 0;
double sumPm25 = 0;
double sumPm10 = 0;
int sampleCount = 0;
bool deviceConnected = false;
bool oldDeviceConnected = false;
float prev_pm25 = -1;
float prev_pm10 = -1;

// BLE Objects
BLEServer *pServer = NULL;
BLECharacteristic *pCharacteristic = NULL;
BLEDescriptor *pDescr;
BLE2902 *pBLE2902;

// BLE Connection UUIDs
#define SERVICE_UUID "d1f8ef04-1fa7-4806-ba3a-8115c0bcce6e"
#define CHARACTERISTIC_UUID "11d29a29-98ee-453e-8c87-2f95f8676519"

// BLE callbacks
class MyServerCallbacks : public BLEServerCallbacks{
  void onConnect(BLEServer *pServer)
  {
    deviceConnected = true;
  };

  void onDisconnect(BLEServer *pServer)
  {
    deviceConnected = false;
  }
};

// setup for BLE
void BLE_init(){
  BLEDevice::init("Particulate Analyzer");
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  BLEService *pService = pServer->createService(SERVICE_UUID);

  pCharacteristic = pService->createCharacteristic(
      CHARACTERISTIC_UUID,
      BLECharacteristic::PROPERTY_NOTIFY);

  pDescr = new BLEDescriptor((uint16_t)0x2901);
  pDescr->setValue("ESP32 Device");
  pCharacteristic->addDescriptor(pDescr);
  pBLE2902 = new BLE2902();
  pBLE2902->setNotifications(true);
  pCharacteristic->addDescriptor(pBLE2902);
  pService->start();
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(false);
  pAdvertising->setMinPreferred(0x0);
  BLEDevice::startAdvertising();
  Serial.println("Waiting for connection...");
}

// setup for SPS
void sps_init(){
  int16_t ret;
  uint8_t auto_clean_days = 4;
  uint32_t auto_clean;

  delay(2000);

  sensirion_i2c_init();

  while (sps30_probe() != 0)
  {
    Serial.print("SPS sensor probing failed\n");
    delay(500);
  }

#ifndef PLOTTER_FORMAT
  Serial.print("SPS sensor probing successful\n");
#endif /* PLOTTER_FORMAT */

  ret = sps30_set_fan_auto_cleaning_interval_days(auto_clean_days);
  if (ret)
  {
    Serial.print("error setting the auto-clean interval: ");
    Serial.println(ret);
  }

  ret = sps30_start_measurement();
  if (ret < 0)
  {
    Serial.print("error starting measurement\n");
  }

#ifndef PLOTTER_FORMAT
  Serial.print("measurements started\n");
#endif /* PLOTTER_FORMAT */

#ifdef SPS30_LIMITED_I2C_BUFFER_SIZE
  Serial.print("Your Arduino hardware has a limitation that only\n");
  Serial.print("  allows reading the mass concentrations. For more\n");
  Serial.print("  information, please check\n");
  Serial.print("  https://github.com/Sensirion/arduino-sps#esp8266-partial-legacy-support\n");
  Serial.print("\n");
  delay(2000);
#endif
}

// read data from SPS
void sps_read(float *pm25, float *pm10){
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

#ifndef PLOTTER_FORMAT
    // Serial.print("PM  1.0: ");
    // Serial.println(m.mc_1p0);
    Serial.print("PM  2.5: ");
    Serial.println(m.mc_2p5);
    *pm25 = m.mc_2p5;
    // Serial.print("PM  4.0: ");
    // Serial.println(m.mc_4p0);
    Serial.print("PM 10.0: ");
    Serial.println(m.mc_10p0);
    *pm10 = m.mc_10p0;

#ifndef SPS30_LIMITED_I2C_BUFFER_SIZE
    // Serial.print("NC  0.5: ");
    // Serial.println(m.nc_0p5);
    // Serial.print("NC  1.0: ");
    // Serial.println(m.nc_1p0);
    // Serial.print("NC  2.5: ");
    // Serial.println(m.nc_2p5);
    // Serial.print("NC  4.0: ");
    // Serial.println(m.nc_4p0);
    // Serial.print("NC 10.0: ");
    // Serial.println(m.nc_10p0);

    // Serial.print("Typical particle size: ");
    // Serial.println(m.typical_particle_size);
#endif

    Serial.println();

#else
    // since all values include particles smaller than X, if we want to create buckets we 
    // need to subtract the smaller particle count. 
    // This will create buckets (all values in micro meters):
    // - particles        <= 0,5
    // - particles > 0.5, <= 1
    // - particles > 1,   <= 2.5
    // - particles > 2.5, <= 4
    // - particles > 4,   <= 10

    Serial.print(m.nc_0p5);
    Serial.print(" ");
    Serial.print(m.nc_1p0  - m.nc_0p5);
    Serial.print(" ");
    Serial.print(m.nc_2p5  - m.nc_1p0);
    Serial.print(" ");
    Serial.print(m.nc_4p0  - m.nc_2p5);
    Serial.print(" ");
    Serial.print(m.nc_10p0 - m.nc_4p0);
    Serial.println();

#endif /* PLOTTER_FORMAT */

  }

  delay(1000);
}

void setup()
{
  Serial.begin(115200);

  BLE_init();
  delay(1000);
  sps_init();
  delay(1000);

  Serial.println("Setup Completed!");
}

void loop(){
  unsigned long currentTime = millis();
  unsigned long sensorReadInterval = 10 * 1000;

  float pm25 = 0;
  float pm10 = 0;

  // Reads data every interval
  if (currentTime - lastUpdateTime >= sensorReadInterval)
  {
    lastUpdateTime = currentTime;
  
    sps_read(&pm25, &pm10);

    char buffer[32];
    sprintf(buffer, "%.2f#%.2f", pm25, pm10);
    std::string value(buffer);

    if (deviceConnected)
    {
      pCharacteristic->setValue(value.c_str());
      pCharacteristic->notify();
      Serial.print("Sent: ");
      Serial.println(value.c_str());
    }

    sumPm25 += pm25;
    sumPm10 += pm10;
    sampleCount++;

    prev_pm10 = pm10;
    prev_pm25 = pm25;
  }

  // Calculate average readings
  if (currentTime - lastAvgTime >= 60000 && sampleCount >= 12)
  {
    lastAvgTime = currentTime;

    double avgPm25 = sumPm25 / sampleCount;
    double avgPm10 = sumPm10 / sampleCount;

    sumPm25 = 0;
    sumPm10 = 0;
    sampleCount = 0;
  }

  // Wait for connection when disconnected
  if (!deviceConnected && oldDeviceConnected)
  {
    delay(500);
    pServer->startAdvertising();
    Serial.println("Advertising after disconnection...");
    oldDeviceConnected = deviceConnected;
  }

  if (deviceConnected && !oldDeviceConnected)
  {
    oldDeviceConnected = deviceConnected;
  }

  delay(100); // smoother update cycle
}