#include <Arduino.h>
#define ESP32C3_MINI

#include "Freenove_WS2812_Lib_for_ESP32.h"
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

#define BUTTON_BOX_SKU "GLA_Single_Button"
#define FIRMWARE_VERSION "3.0.0"
#define HARDWARE_REVISION "0.0.5"
#define BT_ENABLED

#define LEDS_COUNT  1
#define LEDS_PIN	3
#define BUTTON_PIN 4
#define CHANNEL		0

unsigned long nextScanTime = 0;
const unsigned long BEACON_STALE_MS = 10000;
const int SCAN_DURATION_S = 2;

const float pathLossExponent = 3;
unsigned long lastButtonPress = 0;
const unsigned long DEBOUNCE_MS = 300;
bool swDebounce = true;


Freenove_ESP32_WS2812 strip = Freenove_ESP32_WS2812(LEDS_COUNT, LEDS_PIN, CHANNEL);

#include <NuvIoT.h>

int SCAN_INTERVAL_MS = 200;

bool flashOn = false;
bool ackMessage = false;
long toggleFlash = 0;


bool sentMessage = false;

typedef struct {
  const char* deviceId;   
  int rssiAt1m;          
} BeaconConfig;

const BeaconConfig BEACONS[] = {
  {"Beacon01", -48},
  {"Beacon02", -49},
  {"Beacon03", -50},
  {"Beacon04", -48},
  {"Beacon05", -49},
  {"Beacon06", -50},
  {"Beacon07", -48},
  {"Beacon08", -49},
  {"Beacon09", -50},
  {"Beacon10", -48},
  {"Beacon11", -49},
  {"Beacon12", -50},
  {"Beacon13", -48},
  {"Beacon14", -49},
  {"Beacon15", -50}
};
const size_t NUM_BEACONS = sizeof(BEACONS) / sizeof(BEACONS[0]);

typedef struct {
  bool seen;
  float rssi;
  unsigned long lastSeenMs;
} BeaconReading;
BeaconReading beaconReadings[NUM_BEACONS];

BLEScan* pBLEScan;
bool beaconFound = false;
bool scanning = false;
std::map<String, String> detectedBeacons; 

float estimateDistance(int rssi, int rssiAt1m) {
  return pow(10.0, (rssiAt1m - rssi) / (10.0 * pathLossExponent));
}

int matchBeacon(BLEAdvertisedDevice &adv) {
  if (!adv.haveName()) return -1;
  String name = String(adv.getName().c_str());
  for (size_t i = 0; i < NUM_BEACONS;++i) {
    if (name.indexOf(BEACONS[i].deviceId) >= 0) {
      return (int)i;
    }
  }
  return -1;
}

class BeaconScanCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    int idx = matchBeacon(advertisedDevice);
    if (idx < 0) return;
    int rssi = advertisedDevice.getRSSI();
      beaconFound = true;
      beaconReadings[idx].seen = true;
      beaconReadings[idx].rssi = advertisedDevice.getRSSI();
      beaconReadings[idx].lastSeenMs = millis();

      Serial.printf("BEACON FOUND: %s rssi=%d dist=%.2fm\n",
                  BEACONS[idx].deviceId, rssi,
                  estimateDistance(rssi, BEACONS[idx].rssiAt1m));
  }
};

void ScanComplete(BLEScanResults scanResults) {
    scanning = false;
    pBLEScan->clearResults();
    nextScanTime = millis() + SCAN_INTERVAL_MS;
    nextScanTime = millis() + SCAN_INTERVAL_MS;
}

void beaconScanInit() {
Serial.begin(115200);
  BLEDevice::init("ASSET-SCANNER");

  pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new BeaconScanCallbacks());
  
  pBLEScan->setActiveScan(true);
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(50);
}

void beaconAdvertiseInit() {
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMaxPreferred(0x12);
  BLEDevice::startAdvertising();
}

String buildBeaconJson() {
  String json = "{\"beacons\":[";
  int count = 0;
  for (size_t i = 0; i < NUM_BEACONS; ++i) {
    const BeaconReading& reading = beaconReadings[i];
   if (reading.seen) {
      float distance = estimateDistance(reading.rssi, BEACONS[i].rssiAt1m);
      if (count > 0) json += ",";
      json += "{\"deviceId\":\"" + String(BEACONS[i].deviceId) + "\",";
      json += "\"rssi\":" + String(reading.rssi, 2) + ",";
      json += "\"distance\":" + String(distance, 2) + "}";
      count++;
    }
  }
  json += "]}";
  return json;
}

void showLEDStatus() {
  if(ackMessage) {
    strip.setLedColorData(0, 0x00ff00);

    if(millis() > toggleFlash) {  
      ackMessage = false;
    }
  }
  else if(sentMessage) {
    if(millis() > toggleFlash) {       
      strip.setLedColorData(0, flashOn ? 0xff0000 : 0x000000);
      toggleFlash += 150;
      flashOn = !flashOn;

    }
 }
 else if(!sysConfig.Commissioned) {
    if(millis() > toggleFlash) {
      flashOn = !flashOn;
      toggleFlash += 2000;
      strip.setLedColorData(0, flashOn ? 0x00ff00 : 0x000000);
    }
  }
  else if(!wifiMgr.isConnected()){
    if(millis() > toggleFlash) {
      flashOn = !flashOn;
      toggleFlash += 250;
      strip.setLedColorData(0, flashOn ? 0xff0000 : 0x000000);
    }
  }
  else if(!wifiMQTT.isConnected()) {
      if(millis() > toggleFlash) {
        flashOn = !flashOn;
        toggleFlash += 750;
        strip.setLedColorData(0, flashOn ? 0xff0000 : 0x000000);
      }
  }
  else if(wifiMQTT.isConnected()) {    
    
      if(millis() > toggleFlash) {      
          if(flashOn) {
            strip.setLedColorData(0, 0x0000ff);
            flashOn = false;
            toggleFlash += 250;
          }
          else {
            strip.setLedColorData(0, 0x000000);
            toggleFlash += 2500;
            flashOn = true;
          }     
      }
  }  

//  console.println("mqtt=showledstatus; // FlashOn: " + String(flashOn) + " ToggleFlash: " + String(toggleFlash) + " SentMessage: " + String(sentMessage) + " AckMessage: " + String(ackMessage));

  

  strip.show();
}

void commandHandler(String topic, byte *buffer, size_t len)
{
  if(topic.endsWith("ack")) {
    sentMessage = false;
    ackMessage = true;
    toggleFlash = millis() + 2500;
  }

  console.println("mqtt=handltopic; // Topic: " + topic);
}


void setup() {
  strip.begin();
  delay(25)  ;
  strip.setLedColorData(0, 0xffffff);
  delay(25)  ;
  strip.show();
  delay(25)  ;

  initPins();

  configureFileSystem();
  configureConsole();

  ioConfig.load();
  sysConfig.load();


  strip.setLedColorData(0, 0xFFFF00);
  strip.show();

  welcome(BUTTON_BOX_SKU, FIRMWARE_VERSION);
  String btName = "NuvIoT - " + (sysConfig.DeviceId == "" ? "Alert Button Box" : sysConfig.DeviceId);

  BT.begin(btName.c_str(), BUTTON_BOX_SKU);

  beaconScanInit();
  beaconAdvertiseInit();

  state.init(BUTTON_BOX_SKU, FIRMWARE_VERSION, HARDWARE_REVISION, "alertbox", 010);
  state.registerString("alert1msg","hot");
  state.registerString("alert2msg","warm");
  state.registerString("alert3msg","cold");

  pinMode(BUTTON_PIN, INPUT_PULLDOWN);
 
  client.setMessageReceivedCallback(commandHandler);

  strip.setLedColorData(0, 0x00FFFF);
  strip.show();

  console.println("startupcomplete;");
}

void loop() {
 if (digitalRead(BUTTON_PIN) == HIGH && !swDebounce){
    String topic = "nuviot/srvr/dvcsrvc/" + sysConfig.DeviceId + "/notification/" + state.getString("alert1msg");
    wifiMQTT.publish(topic,"");
    swDebounce = true;
    console.println("SEND TOPIC: " + topic);
    sentMessage = true;
    toggleFlash = 0;

    String beaconTopic = "nuviot/srvr/dvcsrvc/" + sysConfig.DeviceId + "/beacons";
    String beaconJson = buildBeaconJson();
    wifiMQTT.publish(beaconTopic, beaconJson);
    console.println("SEND BEACON TOPIC: " + beaconTopic + " Payload: " + beaconJson);
  }
  else if(digitalRead(BUTTON_PIN) == LOW){
    swDebounce = false;
  }

  commonLoop();
  BT.update();

  showLEDStatus();


if (!scanning && millis() >= nextScanTime) {
  scanning = true;
  Serial.println("Starting BLE scan...");
  pBLEScan->start(SCAN_DURATION_S, ScanComplete, false);
}
  delay(50);
}