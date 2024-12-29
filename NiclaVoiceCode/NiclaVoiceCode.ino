#include "NDP.h"

//const bool lowestPower = true;
const bool lowestPower = false;

void ledBlueOn(char* label) {
  long startTime = millis();
  nicla::leds.begin();
 if(strcmp(label,"NN0:go")==0){
    Serial.println("GOOOOO");
    digitalWrite(4,LOW);
    nicla::leds.setColor(green);
  }
  if(strcmp(label,"NN0:stop")==0){
    digitalWrite(3,LOW);
    nicla::leds.setColor(red);
  }
  //nicla::leds.setColor(blue);
  //digitalWrite(4,LOW);
  
  if (!lowestPower) {
    Serial.println(label);
    //Serial.println(digitalRead(4));
  }
  
  while(millis()<startTime+1000){};
  nicla::leds.setColor(off);
  nicla::leds.end();
}

void ledGreenOn() {
  nicla::leds.begin();
  nicla::leds.setColor(green);
  delay(200);
  nicla::leds.setColor(off);
  nicla::leds.end();
}

void ledRedBlink() {
  while (1) {
    nicla::leds.begin();
    nicla::leds.setColor(red);
    delay(200);
    nicla::leds.setColor(off);
    delay(200);
    nicla::leds.end();
  }
}

void setup() {
  pinMode(4,OUTPUT);
  pinMode(3,OUTPUT);
  Serial.begin(115200);
  nicla::begin();
  //nicla::disableLDO();
  nicla::leds.begin();

  NDP.onError(ledRedBlink);
  NDP.onMatch(ledBlueOn);
  NDP.onEvent(ledGreenOn);
  Serial.println("Loading synpackages");
  NDP.begin("mcu_fw_120_v91.synpkg");
  NDP.load("dsp_firmware_v91.synpkg");
  NDP.load("ei_model.synpkg");
  Serial.println("packages loaded");
  NDP.getInfo();
  Serial.println("Configure mic");
  NDP.turnOnMicrophone();
  NDP.interrupts();

  // For maximum low power; please note that it's impossible to print after calling these functions
  nicla::leds.end();
  if (lowestPower) {
    NRF_UART0->ENABLE = 0;
  }
  //NDP.turnOffMicrophone();
}

void loop() {
  digitalWrite(4,HIGH);
  digitalWrite(3,HIGH);
  delay(1000);
}