#include <Arduino.h>
#define ESP32C3_MINI

#include "Freenove_WS2812_Lib_for_ESP32.h"
#include  <BLEDevice.h>
#include <BLEUtils.h> 
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

#define BUTTON_BOX_SKU "GLA-BEACON-02-USF"
#define FIRMWARE_VERSION "0.5.0"
#define HARDWARE_REVISION "0.0.0"
#define BT_ENABLED

#define LEDS_COUNT  1
#define LEDS_PIN	3
#define CHANNEL		0

Freenove_ESP32_WS2812 strip = Freenove_ESP32_WS2812(LEDS_COUNT, LEDS_PIN, CHANNEL);

#include <NuvIoT.h>

bool flashOn = false;
long toggleFlash = 0;


bool sentMessage = false;

#define TARGET_BUTTON_ID "singleone001"
#define BUTTON_RSSI_TOPIC_SUFFIX "/button/rssi"
const int BUTTON_RSSI_AT_1M = -50;
const float pathLossExponent = 3.0;

const int SCAN_SECONDS = 2;
const unsigned long SCAN_PAUSE_MS = 200;
const unsigned long PUBLISH_MS = 5000;
const unsigned long BUTTON_STALE_MS = 10000;

BLEScan* pBLEScan;
bool scanRunning = false;
unsigned long nextScanAt = 0;
unsigned long lastPublishMs = 0;


char buttonAddress[18] = "";
unsigned long buttonLastSeenMs = 0;
float buttonRssi = 0;
bool buttonEverSeen = false;

void beaconAdvertiseInit() {
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMaxPreferred(0x12);
  BLEDevice::startAdvertising();
  console.println("advertising started");
}


float estimateDistance(float rssi, int rssiAt1m) {
 return pow(10.0, (rssiAt1m - rssi) / (10.0* pathLossExponent));
}

class ButtonScanCallbacks : public BLEAdvertisedDeviceCallbacks {
	void onResult(BLEAdvertisedDevice advertisedDevice) {
		std::string address = advertisedDevice.getAddress().toString();
		bool isTarget = false;
		
		if (advertisedDevice.haveName()) {
		String name = String(advertisedDevice.getName().c_str());
		if (name.indexOf(TARGET_BUTTON_ID) >= 0) {
		strncpy(buttonAddress, address.c_str(), sizeof(buttonAddress));
    buttonAddress[sizeof(buttonAddress) - 1] = '\0';
		isTarget = true;
	}
}
 if (!isTarget && buttonAddress[0] != '\0' && strcmp(address.c_str(), buttonAddress) == 0) {
      isTarget = true;
    }
if (!isTarget) return;

int rssi = advertisedDevice.getRSSI();
buttonRssi = rssi;
buttonLastSeenMs = millis();
buttonEverSeen = true;

  }
};

void onScanComplete(BLEScanResults results) {
	scanRunning = false;
  pBLEScan->clearResults();  
	nextScanAt = millis() + SCAN_PAUSE_MS;
}

void buttonScanInit() {
  if(!BLEDevice::getInitialized()) {
  BLEDevice::init("GLA-Beacon");
}

  pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new ButtonScanCallbacks());
  pBLEScan->setActiveScan(true);   
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(50);        
}

void serviceButtonScan() {
  if (!scanRunning && millis() >= nextScanAt) {
    scanRunning = pBLEScan->start(SCAN_SECONDS, onScanComplete, false);
    if (!scanRunning) {
      nextScanAt = millis() + SCAN_PAUSE_MS;  
    }
  }
}

void reportButton() {
  if (millis() - lastPublishMs < PUBLISH_MS) return;
  if (!wifiMQTT.isConnected()) return;
  lastPublishMs = millis();
  unsigned long lastSeen;
  float rssi;
  bool seen;
  rssi = buttonRssi;
  lastSeen = buttonLastSeenMs;
  seen = buttonEverSeen;
  
 if (!seen || (millis() - lastSeen) >= BUTTON_STALE_MS) {
    console.println("BUTTON NOT SEEN recently: " + String(TARGET_BUTTON_ID));
    return;
  }
  lastPublishMs = millis();
 
  float dist = estimateDistance(rssi, BUTTON_RSSI_AT_1M);
  String payload = "{\"button\":\"" + String(TARGET_BUTTON_ID) +
                   "\",\"rssi\":" + String((int)roundf(rssi)) +
                   ",\"dist\":" + String(dist, 2) + "}";
  String topic = "nuviot/srvr/dvcsrvc/" + sysConfig.DeviceId + BUTTON_RSSI_TOPIC_SUFFIX;
  wifiMQTT.publish(topic, payload);
  console.println("SEND BUTTON RSSI: " + topic + " " + payload);
}
 

void showLEDStatus() {
 if(!sysConfig.Commissioned) {
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

  strip.show();
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
  String btName = "GLA - " + (sysConfig.DeviceId == "" ? "Beacon" : sysConfig.DeviceId);

  BT.begin(btName.c_str(), BUTTON_BOX_SKU);

beaconAdvertiseInit();
buttonScanInit();


	

  state.init(BUTTON_BOX_SKU, FIRMWARE_VERSION, HARDWARE_REVISION, "alertbox", 010);

  strip.setLedColorData(0, 0x00FFFF);
  strip.show();

  console.println("startupcomplete;");
}

void loop() {
 
  commonLoop();
  BT.update();

serviceButtonScan();
reportButton();

  showLEDStatus();
  
  delay(50);
}