#include "ESP8266WiFi.h"
#include "pitches.h"

// Outputs
const bool activePiezo = false; // analog output

const int greenLedPin = 2; // inverted logic
const int yellowLedPin = 16;
const int redLedPin = 4;
const int vibrationPin = 15;
const int piezoPin = 5;
const int stayOnPin = 14;

// Battery meter
const int batteryLevelPin = A0;
const int batterySamplingSize = 10;
int batteryReadings[batterySamplingSize]; // sampling size
int batterySamples = 0;
const int batteryCheckInterval = 300000; // ms - 5mins
unsigned long lastBatteryCheck = 0;
int batteryCharge = 3;

// Inputs
const int buttonPin = 12; // 12
int reading;
int previous = LOW;

// Button variables
const int shortPush = 200; //ms
const int mediumPush = 1000; //ms
const int longPush = 2000; //ms
const int turnOffPush = 3000; //ms
const int pushInterval = 450; // ms
unsigned long lastShortPush = 0;
unsigned long buttonPressedStart = 0;
int shortPushCounter = 0;

//AP variables
const WLANChannel wifiChannel = WiFiChannels[11];
const int powerLevel = 1.5;
String ssid = "Sx";
byte mac[6];

//Alarm variables
unsigned long lastSoundPeriodStart = 0;
const int onDuration=750;
const int periodDuration=1500;
const int alarmFrequency = 550; //Hz
bool outputTone = false;
bool outputVibration = false;
unsigned long lastLedPeriodStart = 0;
const int ledPeriodDuration=200;
int ledState = LOW;
bool alarmTriggered = false;

bool toneAlarmActive = true;
bool vibrationAlarmActive = true;

//Info variables
unsigned long lastInfoPeriodOnStart = 0;
unsigned long lastInfoPeriodOffStart = 0;
const int infoStandbyOn=300;
const int infoStandbyOff[4] = { 0, 2000, 3500, 6000 }; //ms, based on battery level
int infoLedState = LOW;
int infoCurrentLed;
char msg[64];

// Snooze variables
bool snoozed = false;
const int snoozePeriod = 3000; // 5mins - 300000
unsigned long snoozePeriodStart = 0;

void TriggerAlert(int alarm, String message = "");
void clearFeedback(bool stopLights = true, bool stopTone = true, bool stopVibration = true);

void setup() {

    pinMode(redLedPin, OUTPUT);
    pinMode(yellowLedPin, OUTPUT);
    pinMode(greenLedPin, OUTPUT);
    pinMode(vibrationPin, OUTPUT);
    pinMode(piezoPin, OUTPUT);

    pinMode(stayOnPin, OUTPUT);
    digitalWrite(stayOnPin, HIGH);

    pinMode(buttonPin, INPUT_PULLUP);

    Serial.begin(115200);
    WiFi.enableAP(true);
    delay(100);

    WiFi.mode(WIFI_STA);

    ActivateAccessPoint();

    TriggerAlert(0, "SOCIAL DISTANCER");
}

void ActivateAccessPoint() {
    WiFi.macAddress(mac);

    WiFi.setOutputPower(powerLevel);
    String netName = ssid + macToString(mac);

    Serial.println(WiFi.softAP(netName, "", wifiChannel.channelNumber, true) ? netName + " AP Ready" : "AP Failed!"); // hidden
    delay(200);
    Serial.println("AP setup done");
    Serial.println("");
}

void loop() {

    reading = digitalRead(buttonPin);

    if (reading == HIGH && previous == LOW){
        previous = HIGH;
        CheckButton(millis());
    }
    else if (reading == LOW && previous == HIGH)
    {
        previous = LOW;
        buttonPressedStart = millis();
    }

    if (snoozed)
        if (millis() - snoozePeriodStart < snoozePeriod)
          return;
        else
          snoozed = false;

    int n = WiFi.scanNetworks(false, true, wifiChannel.channelNumber); // sync, hidden

    if (n == 0) {
        TriggerAlert(0);
    } else {

    for (int i = 0; i < n; ++i) {

        if (WiFi.isHidden(i))
            {
            int powerPercentage = calculatePercentage(abs(WiFi.RSSI(i)));

            String message = WiFi.SSID(i) + " - POWER: " + powerPercentage; // + " " + WiFi.RSSI(i) + "dBm - Distance: " + String(distance);

            if (powerPercentage < 40) {
                TriggerAlert(0, "OK " + message);
            }
            else if (powerPercentage > 65) {
                TriggerAlert(1, "HIGH ALERT ************ " + message);
            }
            else
            {
              TriggerAlert(3, "LOW alert ************ " + message);
            }
        }
    }
  }

  WiFi.scanDelete();
} // loop

void TriggerAlert(int alarm, String message){
    unsigned long currentMillis = millis();

    if (message.length() > 0)
        Serial.println(message);

    clearFeedback();

    if(alarm == 0)
        PlayInfoLed(currentMillis);
     else
        PlayAlarm(alarm, currentMillis);
}

void CheckButton(unsigned long currentMillis) {
    int elapsedTime = currentMillis - buttonPressedStart;
    int pushType = 0;

    if (elapsedTime <= shortPush){
        pushType = 1;
        if (currentMillis - lastShortPush <= pushInterval) {
          shortPushCounter ++;
        } else {
            shortPushCounter = 1;
        }
        lastShortPush = currentMillis;
    } else if (elapsedTime >= turnOffPush) {
        pushType = 4;
    } else if (elapsedTime >= longPush) {
        pushType = 3;
    } else if (elapsedTime >= mediumPush) {
        pushType = 2; // not in use
    }

    Serial.println(shortPushCounter);

    if (pushType == 1 && shortPushCounter == 3){
        clearFeedback();
        TriggerAlert(0, "Let's Snooze");

        shortPushCounter = 0;
        snoozePeriodStart = currentMillis;
        snoozed = true;

        return;
    }

  if (pushType == 3 && shortPushCounter == 1) {
    Serial.println("Long push - DEACTIVATE the TONE ALARM");
    shortPushCounter = 0;
    PlayPiezo(false);
    toneAlarmActive = false;
    return;
  }

  if (pushType == 3 && shortPushCounter == 2) {
    Serial.println("Long push - DEACTIVATE both TONE ALARM and VIBRATION");
    shortPushCounter = 0;

    PlayPiezo(false);
    digitalWrite(vibrationPin, LOW);
    toneAlarmActive = false;
    vibrationAlarmActive = false;
    return;
  }

  if (pushType == 4 && shortPushCounter == 0) {
    Serial.println("Turn off");
    // call shut down here
    return;
  }
}

void CheckBattery(unsigned long currentMillis) {
  if (currentMillis - lastBatteryCheck >= batteryCheckInterval) {
    lastBatteryCheck = currentMillis;
    batteryReadings[batterySamples] = analogRead(batteryLevelPin);
    batterySamples++;

    if (batterySamples == batterySamplingSize)
    {
        float average;
        for (int i=0; i<batterySamplingSize; i++)
            average += batteryReadings[i];
        average /= batterySamplingSize;

        //remove this when battery reads are calibrated
        average = 100;

        if(average < 10){
         batteryCharge = 0;
        } else if (average < 40){
         batteryCharge = 1;
        } else if (average < 70) {
         batteryCharge = 2;
        } else {
         batteryCharge = 3;
        }
        batterySamples = 0;
        Serial.printf("Battery read at %lf\n", average);
    }
  }
}

void PlayLed(int led, unsigned long currentMillis) {
  if (currentMillis - lastLedPeriodStart >= ledPeriodDuration) {
    clearFeedback();
    lastLedPeriodStart = currentMillis;
    if (ledState == LOW) {
      ledState = HIGH;
    } else {
      ledState = LOW;
    }
    digitalWrite(led, ledState);
  }
}

void PlayPiezo(bool makeSound){
    if (activePiezo)
        if (makeSound)
            tone(piezoPin, alarmFrequency, onDuration); // play 550 Hz tone in background for 'onDuration'
        else
            noTone(piezoPin);
    else
        if (makeSound)
            analogWrite(piezoPin, 1000);
        else
            analogWrite(piezoPin, 0);
}

void PlayInfoLed(unsigned long currentMillis) {

  if (infoLedState == HIGH && currentMillis - lastInfoPeriodOnStart >= infoStandbyOn)
  {
    infoLedState = LOW;
    lastInfoPeriodOffStart = currentMillis;
    clearFeedback(true, false, false);
  }
  else if (infoLedState == LOW && currentMillis - lastInfoPeriodOffStart >=  infoStandbyOff[batteryCharge])
  {
    lastInfoPeriodOnStart = currentMillis;

    switch( batteryCharge ){
      case 3:
        infoCurrentLed = greenLedPin;
        break;
      case 2:
        infoCurrentLed = yellowLedPin;
        break;
      case 1:
        infoCurrentLed = redLedPin;
        break;
      default:
        infoCurrentLed = greenLedPin;
        break;
    }

    Serial.printf("idle - Battery charge: %d\n", batteryCharge);

    infoLedState = HIGH;

    if (infoCurrentLed = greenLedPin)
        digitalWrite(infoCurrentLed, LOW); // inverted logic GPIO02
    else
        digitalWrite(infoCurrentLed, HIGH);
  }
}

void clearFeedback(bool stopLights, bool stopTone, bool stopVibration) {
    digitalWrite(redLedPin, LOW);
    digitalWrite(yellowLedPin, LOW);
    digitalWrite(greenLedPin, HIGH); // inverted logic GPIO02
    if (stopTone)
        PlayPiezo(false);
    if (stopVibration)
        digitalWrite(vibrationPin, LOW);
}

void PlayAlarm(int alarm, unsigned long currentMillis) {

    lastInfoPeriodOnStart = currentMillis; // resets info period to zero

    switch (alarm){
        case 1: // high alert
            toneAlarmActive = true;
            PlayLed(redLedPin, currentMillis);
            break;
        case 3: // low alert
            toneAlarmActive = false;
            PlayLed(yellowLedPin, currentMillis);
            break;
        default: // no alert
            toneAlarmActive = false;
            PlayLed(yellowLedPin, currentMillis);
            break;
    }

    if (toneAlarmActive || vibrationAlarmActive) {
        if (outputTone) {
            if (currentMillis-lastSoundPeriodStart >= periodDuration) // tone is on, only turn off if it's been long enough
            {
                lastSoundPeriodStart=currentMillis;
                PlayPiezo(false);
                digitalWrite(vibrationPin, LOW);
                outputTone = false;
            }
        } else {
            if (currentMillis-lastSoundPeriodStart >= periodDuration) { // No tone, turn on if it's time
                lastSoundPeriodStart+=periodDuration;
                outputTone = true;
                if (toneAlarmActive)
                    PlayPiezo(true);
                if (vibrationAlarmActive)
                    digitalWrite(vibrationPin, HIGH);
            }
        }
    }
}

bool isSDNetwork(String wiFiSSID){

  if (wiFiSSID.length() < ssid.length())
    return false;
  if (wiFiSSID.substring(0, ssid.length()) == ssid)
    return true;

  return false;
}

int calculatePercentage(int powerLevel) {
    if (powerLevel < 1 )
      powerLevel = 1;
    else if (powerLevel > 99)
      powerLevel = 99;
    return dBmPercentage[powerLevel];
}

String macToString(const unsigned char* mac) {
    char buf[20];
    snprintf(buf, sizeof(buf), "-%02x%02x%02x%02x%02x%02x",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return String(buf);
}
