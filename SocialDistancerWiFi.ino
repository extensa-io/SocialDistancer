#include "ESP8266WiFi.h"
#include "math.h"
#include "pitches.h"

// Outputs
const int redLedPin = 16;
const int yellowLedPin = 13;
const int greenLedPin = 12;
const int motorLedPin = 5; // blue LED in dev
const int piezoPin = 15;

// Inputs
const int buttonPin = 4;
int reading; 
int previous = HIGH;

const int shortPush = 200; //ms
const int mediumPush = 1000; //ms
const int longPush = 2000; //ms
const int turnOffPush = 3000; //ms
const int pushInterval = 450; // ms
unsigned long lastShortPush = 0;
unsigned long buttonPressedStart = 0;
int shortPushCounter = 0;  

//AP variables
const WLANchannel wifiChannel = WiFiChannels[9];
const int powerLevel = 10;
String ssid = APSSID;
byte mac[6];

//Alarm variables
unsigned long lastSoundPeriodStart = 0;
const int onDuration=750;
const int periodDuration=1500;
const int alarmFrequency = 550; //Hz
boolean outputTone = false;  
unsigned long lastLedPeriodStart = 0;
const int ledPeriodDuration=200;
int ledState = LOW;

bool toneAlarmActive = true;
bool vibrationAlarmActive = true;

//Info variables
unsigned long lastInfoPeriodOnStart = 0;
unsigned long lastInfoPeriodOffStart = 0;
const int infoStandbyOn=300;
const int infoStandbyOff=3000;
int infoLedState = LOW;

// Snooze variables
bool snoozed = false;
const int snoozePeriod = 5000; //300000; // 5mins
unsigned long snoozePeriodStart = 0;

void setup() {
  
  pinMode(redLedPin, OUTPUT);
  pinMode(yellowLedPin, OUTPUT);
  pinMode(greenLedPin, OUTPUT);
  pinMode(motorLedPin, OUTPUT);
  pinMode(piezoPin, OUTPUT);
  
  pinMode(buttonPin, INPUT_PULLUP);
  
  Serial.begin(115200);
  
  WiFi.mode(WIFI_STA);
  
  ActivateAccessPoint();

  TriggerAlert(0, "SOCIAL DISTANCER");
}

void ActivateAccessPoint() {
  WiFi.macAddress(mac);
  
  WiFi.setOutputPower(powerLevel); 

  Serial.println(WiFi.softAP(ssid + macToString(mac), "", wifiChannel.channelNumber) ? ssid + " AP Ready" : "AP Failed!");
  Serial.println("AP setup done"); 
  Serial.println("");
}

void loop() {

  reading = digitalRead(buttonPin);

  if(reading == HIGH && previous == LOW){
    previous = HIGH;
    digitalWrite(motorLedPin, LOW);
    CheckButton(millis());
  }
  else if (reading == LOW && previous == HIGH)
  {
    previous = LOW;
    digitalWrite(motorLedPin, HIGH);
    buttonPressedStart = millis();
  }

  if(snoozed) {
    if(millis() - snoozePeriodStart < snoozePeriod) {
      return;
    }
    else {
      snoozed = false;
    }
  }
 
  int n = WiFi.scanNetworks(false, false, wifiChannel.channelNumber);
  
  if (n == 0) {
    TriggerAlert(0, ".");
  } else {

    for (int i = 0; i < n; ++i) {

      if (isSDnetwork(WiFi.SSID(i)))
      {
        int powerPercentage = calculatePercentage(abs(WiFi.RSSI(i)));
        double distance = calculateDistance(abs(WiFi.RSSI(i))) ;
        
        String message = WiFi.SSID(i); // + " " + WiFi.RSSI(i) + "dBm - Distance: " + String(distance) + " - Percentage: " + powerPercentage;
  
        if (powerPercentage < 40) {
          TriggerAlert(0, "OK " + message);
        } 
        else if (powerPercentage > 65) {
          TriggerAlert(1, "HIGH ALERT ************ " + message);
        }
        //else if (powerPercentage > 60) {
        //  TriggerAlert(2, "MID alert ************ " + message);
        //}
        else
        {   
          TriggerAlert(3, "MID alert ************ " + message);
        }
      }
    }
  }
  
  WiFi.scanDelete();  
}// loop

void TriggerAlert(int alarm, String message){
  Serial.println(message);
  unsigned long currentMillis = millis();
  
  switch (alarm){
    case 1: // red alert
      digitalWrite(yellowLedPin, LOW);
      digitalWrite(greenLedPin, LOW);
      PlayLed(redLedPin, currentMillis);
      PlayAlarm(currentMillis);
      break;
    case 2:
      digitalWrite(yellowLedPin, LOW);
      digitalWrite(greenLedPin, LOW);
      PlayLed(redLedPin, currentMillis);
      break;
    case 3:
      digitalWrite(redLedPin, LOW);
      digitalWrite(greenLedPin, LOW);
      PlayLed(yellowLedPin, currentMillis);
      break;
    default:
      digitalWrite(redLedPin, LOW);
      digitalWrite(yellowLedPin, LOW);
      PlayInfoLed(currentMillis);
      break;
  }
}

void CheckButton(unsigned long currentMillis) {
  int elapsedTime = currentMillis - buttonPressedStart;
  
  int pushType = 0;
  if(elapsedTime <= shortPush){
    pushType = 1;
    if (currentMillis - lastShortPush <= pushInterval) {
      shortPushCounter ++;
    }
    else
      shortPushCounter = 1;
    lastShortPush = currentMillis;
  } else if (elapsedTime >= turnOffPush) {
    pushType = 4;
  } else if (elapsedTime >= longPush) {
    pushType = 3;
  } else if (elapsedTime >= mediumPush) {
    pushType = 2;
  }
  
  if(pushType == 1 && shortPushCounter == 3){
    TriggerAlert(0, "Let's Snooze");

    shortPushCounter = 0;
    snoozePeriodStart = currentMillis;
    snoozed = true;

    return;
  }

  if(pushType == 2 & shortPushCounter == 0) {
    Serial.println("medium push - battery indicator");
    // call battery indicator here
    return;
  }
    
  if(pushType == 3 && shortPushCounter == 1) {
    Serial.println("Long push - DEACTIVATE the TONE ALARM");
    shortPushCounter = 0;
    toneAlarmActive = false;
    return;
  }
  
  if(pushType == 3 && shortPushCounter == 2) {
    Serial.println("Long push - DEACTIVATE both TONE ALARM and VIBRATION");
    shortPushCounter = 0;
    toneAlarmActive = false;
    vibrationAlarmActive = false;
    return;
  }
  
  if(pushType == 4 && shortPushCounter == 0) {
    Serial.println("Turn off");
    // call shut down here
    return;
  }
}

void PlayLed(int led, unsigned long currentMillis) {
  if (currentMillis - lastLedPeriodStart >= ledPeriodDuration) {
    lastLedPeriodStart = currentMillis;
    if (ledState == LOW) {
      ledState = HIGH;
    } else {
      ledState = LOW;
    }
    digitalWrite(led, ledState);
  }
}

void PlayInfoLed(unsigned long currentMillis) {
  if(infoLedState == HIGH && currentMillis - lastInfoPeriodOnStart >= infoStandbyOn)
  {
    infoLedState = LOW; 
    lastInfoPeriodOffStart = currentMillis; 
    digitalWrite(greenLedPin, infoLedState);
  }
  else if (infoLedState == LOW && currentMillis - lastInfoPeriodOffStart >= infoStandbyOff)
  {
    infoLedState = HIGH; 
    lastInfoPeriodOnStart = currentMillis;
    digitalWrite(greenLedPin, infoLedState);
  }
}

void PlayAlarm(unsigned long currentMillis) {

  if(toneAlarmActive || vibrationAlarmActive) {
    if (outputTone) {
      if (currentMillis-lastSoundPeriodStart >= periodDuration) // tone is on, only turn off if it's been long enough
      {
        lastSoundPeriodStart=currentMillis;
        noTone(piezoPin);
        outputTone = false;
      }
    } else {
      if (currentMillis-lastSoundPeriodStart >= periodDuration) { // No tone, turn on if it's time 
        lastSoundPeriodStart+=periodDuration;
        if (toneAlarmActive)
          tone(piezoPin, alarmFrequency, onDuration); // play 550 Hz tone in background for 'onDuration'
         if(vibrationAlarmActive) {
          // vibrate here
         }
         
        outputTone = true;
      }
    }
  }
}

bool isSDnetwork(String wiFiSSID){
  
  if(wiFiSSID.length() < ssid.length())
    return false;
  if(wiFiSSID.substring(0, ssid.length()) == ssid) 
    return true;

  return false;
}

double calculateDistance(int wifiPower) {
  double exp = (27.55 - (20 * log10(wifiChannel.frequency)) + wifiPower) / 20.0;
  return pow(10.0, exp);
}

int calculatePercentage(int powerLevel) {
    if (powerLevel < 1 ) {
      powerLevel = 1;
    }
    else if (powerLevel > 99) {
      powerLevel = 99;
    }
  return dBmPercentage[powerLevel];
}

String macToString(const unsigned char* mac) {
  char buf[20];
  snprintf(buf, sizeof(buf), "-%02x%02x%02x%02x%02x%02x",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(buf);
}
