#include "ESP8266WiFi.h"
#include "math.h"
#include "pitches.h"

const int redLedPin = 16;
const int yellowLedPin = 13;
const int piezoPin = 15;

//AP variables
const WLANchannel wifiChannel = WiFiChannels[9];
const int powerLevel = 10;
String ssid = APSSID;
byte mac[6];

//Alarm variables
unsigned long lastSoundPeriodStart;
const int onDuration=750;
const int periodDuration=1500;
boolean outputTone = false;  
unsigned long lastLedPeriodStart;
const int ledPeriodDuration=300;
int ledState = LOW;

void setup() {

  pinMode(redLedPin, OUTPUT);
  pinMode(yellowLedPin, OUTPUT);
  
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
        else if (powerPercentage > 75) {
          TriggerAlert(1, "HIGH ALERT ************ " + message);
        }
        else if (powerPercentage > 60) {
          TriggerAlert(2, "MID alert ************ " + message);
        }
        else
        {   
          TriggerAlert(3, "low alert ************ " + message);
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
      PlayLed(redLedPin, currentMillis);
      PlaySound(currentMillis);
      break;
    case 2:
      digitalWrite(yellowLedPin, LOW);
      PlayLed(redLedPin, currentMillis);
      break;
    case 3:
      digitalWrite(redLedPin, LOW);
      PlayLed(yellowLedPin, currentMillis);
      break;
    default:
      digitalWrite(redLedPin, LOW);
      digitalWrite(yellowLedPin, LOW);
      break;
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

void PlaySound(unsigned long currentMillis) {
  
  if (outputTone) {
  // tone is on, only turn off if it's been long enough
    if (currentMillis-lastSoundPeriodStart>=periodDuration)
    {
      lastSoundPeriodStart=currentMillis;
      noTone(piezoPin);
      outputTone = false;
    }
  } else {
  // No tone, turn on if it's time 
      if (currentMillis - lastSoundPeriodStart >= periodDuration) {
        lastSoundPeriodStart+=periodDuration;
        tone(piezoPin,550, onDuration); // play 550 Hz tone in background for 'onDuration'
        outputTone = true;
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
