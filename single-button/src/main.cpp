#include <Arduino.h>
#define ESP32C3_MINI

#include "Freenove_WS2812_Lib_for_ESP32.h"

#define BUTTON_BOX_SKU "SA-WIFI-02-FW"
#define FIRMWARE_VERSION "3.0.0"
#define HARDWARE_REVISION "2.0.0"
#define BT_ENABLED

//#define SL_MINI
#ifdef SL_MINI
#define LEDS_COUNT  1
#define LEDS_PIN	3
#define BUTTON_PIN1 1
#define BUTTON_PIN2 6
#define CHANNEL		0
#else
#define LEDS_COUNT  1
#define LEDS_PIN	3
#define BUTTON_PIN1 1
#define BUTTON_PIN2 4
#define BUTTON_PIN3 6
#define CHANNEL		0
#endif

bool sw1Debounce = true;
bool sw2Debounce = true;
bool sw3Debounce = true;

Freenove_ESP32_WS2812 strip = Freenove_ESP32_WS2812(LEDS_COUNT, LEDS_PIN, CHANNEL);

#include <NuvIoT.h>

bool flashOn = false;
bool ackMessage = false;
long toggleFlash = 0;


bool sentMessage = false;

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

  state.init(BUTTON_BOX_SKU, FIRMWARE_VERSION, HARDWARE_REVISION, "alertbox", 010);
  state.registerString("alert1msg","hot");
  state.registerString("alert2msg","warm");
  state.registerString("alert3msg","cold");

  pinMode(BUTTON_PIN1, INPUT_PULLDOWN);
  pinMode(BUTTON_PIN2, INPUT_PULLDOWN);
  pinMode(BUTTON_PIN3, INPUT_PULLDOWN);

  client.setMessageReceivedCallback(commandHandler);

  strip.setLedColorData(0, 0x00FFFF);
  strip.show();

  console.println("startupcomplete;");
}

void loop() {
 if (digitalRead(BUTTON_PIN1) == HIGH && !sw1Debounce){
    String topic = "nuviot/srvr/dvcsrvc/" + sysConfig.DeviceId + "/notification/" + state.getString("alert1msg");
    wifiMQTT.publish(topic,"");
    sw1Debounce = true;
    console.println("SEND TOPIC: " + topic);
    sentMessage = true;
    toggleFlash = 0;
  }
  else if(digitalRead(BUTTON_PIN1) == LOW){
    sw1Debounce = false;
  }

  if (digitalRead(BUTTON_PIN2) == HIGH && !sw2Debounce){
    String topic = "nuviot/srvr/dvcsrvc/" + sysConfig.DeviceId + "/notification/" + state.getString("alert2msg");
    mqttPublish(topic);
    console.println("SEND TOPIC: " + topic);
    sw2Debounce = true;
    sentMessage = true;
    toggleFlash = 0;
  }
  else if(digitalRead(BUTTON_PIN2) == LOW){
    sw2Debounce = false;
  }

  if (digitalRead(BUTTON_PIN3) == HIGH && !sw3Debounce){
    String topic = "nuviot/srvr/dvcsrvc/" + sysConfig.DeviceId + "/notification/" + state.getString("alert3msg");
    mqttPublish(topic);
    console.println("SEND TOPIC: " + topic);
    sw3Debounce = true;
    sentMessage = true;
    toggleFlash = 0;
  }
  else if(digitalRead(BUTTON_PIN3) == LOW){
    sw3Debounce = false;
  }

  commonLoop();
  BT.update();

  showLEDStatus();
  
  delay(50);
}