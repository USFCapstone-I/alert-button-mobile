#include <Arduino.h>
#define ESP32C3_MINI

#include "Freenove_WS2812_Lib_for_ESP32.h"

#define BUTTON_BOX_SKU "GLA-BEACON-01-FW"
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

  state.init(BUTTON_BOX_SKU, FIRMWARE_VERSION, HARDWARE_REVISION, "alertbox", 010);

  strip.setLedColorData(0, 0x00FFFF);
  strip.show();

  console.println("startupcomplete;");
}

void loop() {
 
  commonLoop();
  BT.update();

  showLEDStatus();
  
  delay(50);
}