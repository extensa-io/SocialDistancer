// THE SOCIAL DISTANCER
// Author: Néstor Daza - nestor@extensa.io
// http://gosocialdistancer.com/

#include "ESP8266WiFi.h"
#include "FastRunningMedian.h"
#include "pitches.h"

char message[100];

// Read
const byte readsSize = 24;
FastRunningMedian<unsigned int,readsSize, 0> readsMedian;

// Outputs
const bool activePiezo = false; // analog output

const int greenLedPin = 2; // inverted logic
const int yellowLedPin = 16;
const int redLedPin = 4;
const int vibrationPin = 15;
const int piezoPin = 5;
const int stayOnPin = 14;

const int analogWriteFrequency = 100;
const int maxAnalogDutyCycle = 512;

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

// Debounce
unsigned long lastDebounce = 0;
unsigned long debounceDelay = 20;

// Alarms and info notification
byte alarmState = 0;
byte currentAlarm = 0;
int lowAlarmLevel = 0;
int highAlarmLevel = 68; // <---------- ALARM LEVEL
bool outputTone = false;
bool outputVibration = false;
unsigned long lastNetworkFound = 0;
int ledState = LOW;
bool toneAlarmActive = true;
bool vibrationAlarmActive = true;
byte batteryLevel = 3; // 1-3

int lastPeriodOnStart = 0;
int lastPeriodOffStart = 0;

// Alarm period
int alarmCheckInterval = 1500; // ms
unsigned long alarmCheckStarts = 0;
const int alarmOnDuration=250;
const int alarmStandByDuration=250;

// Info period
const int infoOnDuration=500;
const int infoStandbyDuration[4] = { 0, 1000, 3500, 6000 }; //ms, based on battery level

// Button
const int shortPush = 200; //ms
const int mediumPush = 1000; //ms
const int longPush = 2000; //ms
const int turnOffPush = 3000; //ms
const int pushInterval = 450; // ms
unsigned long lastShortPush = 0;
unsigned long buttonPressedStart = 0;
int shortPushCounter = 0;
bool buttonPressed = false;

// Physical feedback
const int alarmFrequency = 550; //Hz

//AP variables
const WLANChannel wifiChannel = WiFiChannels[11];
const byte powerLevel = 0;
String ssid = "Sx";
byte mac[6];
String netName;

char msg[64];

// Snooze
bool snoozed = false;
const int snoozePeriod = 10000; // 5mins - 300000
unsigned long snoozeStart = 0;

////////////////////////////////////////////////////////////////////////////////////////////

void ClearFeedback(bool stopLights = true, bool stopTone = true, bool stopVibration = true);

void setup() {

    pinMode(redLedPin, OUTPUT);
    pinMode(yellowLedPin, OUTPUT);
    pinMode(greenLedPin, OUTPUT);
    pinMode(vibrationPin, OUTPUT);
    pinMode(piezoPin, OUTPUT);

    pinMode(stayOnPin, OUTPUT);
    digitalWrite(stayOnPin, HIGH);

    pinMode(buttonPin, INPUT_PULLUP);

    analogWriteFreq(analogWriteFrequency); //should give 1Khz

    Serial.begin(115200);

    WiFi.macAddress(mac);
    netName = ssid + MacToString(mac);

    ActivateAccessPoint();
}

void ActivateAccessPoint() {

    ResetWifi();

    WiFi.enableAP(true);
    delay(200);
    WiFi.mode(WIFI_STA);
    WiFi.setOutputPower(powerLevel);

    Serial.println(WiFi.softAP(netName, "", wifiChannel.channelNumber, true) ? netName + " AP Ready" : "AP Failed!"); // hidden
    delay(200);
    Serial.println("AP setup done");
    Serial.println("SOCIAL DISTANCER READY");
}

unsigned long startMeasureMillis = 0;
unsigned long endMeasureMillis = 0;

void loop() {
    char incomingMac[25];
    char incomingSSID[25];
    byte readPowerPercentage = 0;
    bool networksFound = false;
    unsigned long currentMillis = millis();

    reading = digitalRead(buttonPin);
    if (reading == HIGH) {
        analogWrite(yellowLedPin, 0);
    } else {
        analogWrite(yellowLedPin, maxAnalogDutyCycle);
    }

    if(reading != previous) {
        lastDebounce = millis();
    }

    if ((currentMillis - lastDebounce) > debounceDelay) {
        if (reading == HIGH && previous == LOW) {
            previous = HIGH;
            CheckButton(currentMillis);
        }
        else if (reading == LOW && previous == HIGH) {
            previous = LOW;
            ClearFeedback();
            buttonPressed = true;
            buttonPressedStart = currentMillis;
        }
    }

    if (snoozed) {
        if (currentMillis - snoozeStart < snoozePeriod) {
            return;
        } else {
            snoozed = false;
        }
    }

    int n = WiFi.scanNetworks(false, true, wifiChannel.channelNumber); // sync, hidden

    PlayLed(currentMillis);

    int highestPower = 0;
    for (int i = 0; i < n; i++) {
        if (WiFi.isHidden(i))
        {
            readPowerPercentage = CalculatePercentage(WiFi.RSSI(i));
            highestPower = readPowerPercentage > highestPower ? readPowerPercentage : highestPower;
            lastNetworkFound = currentMillis;

            networksFound = true;
            //WiFi.BSSIDstr(i).toCharArray(incomingMac, 25);
            //WiFi.SSID(i).toCharArray(incomingSSID, 25);
            //Serial.printf("[%s] [%s]\n ", incomingSSID, incomingMac);
        }
    }
    if (highestPower > 0) {
        readsMedian.addValue(highestPower);
    }

    if(currentMillis - lastNetworkFound >= alarmCheckInterval) {
        ClearReads();
    }

    if (currentMillis - alarmCheckStarts >= alarmCheckInterval) {
        TriggerAlarm();
    }

    WiFi.scanDelete();
}

void TriggerAlarm() {

    byte currentPower = readsMedian.getMedian();
    Serial.printf("[%d]\n", currentPower);

    ClearReads();

    if (currentPower >= highAlarmLevel) {
        alarmState = 1;
    } else {
        alarmState = 0;
    }
    unsigned long currentMillis = millis();

    if (alarmState != currentAlarm) {
        currentAlarm = alarmState;
        switch(alarmState) {
            case 1:
                Serial.printf("HIGH ALARM [%d]\n", currentPower);
                break;
            case 2:
                Serial.printf("LOW [%d]\n", currentPower);
                break;
            default:
                Serial.printf("no alarm\n");
                break;
        }
    }

    alarmCheckStarts = currentMillis;
}

void PlayLed(unsigned long currentMillis) {

    int onPeriod = 0;
    int standByPeriod = 0;
    int currentLed;
    bool givePhysicalFeedback = false;

    switch(alarmState) {
        case 1:
            currentLed = redLedPin;
            onPeriod = alarmOnDuration;
            standByPeriod = alarmStandByDuration;
            givePhysicalFeedback = true;
            break;
        case 2:
            break;
        default:
            onPeriod = infoOnDuration;
            standByPeriod = infoStandbyDuration[batteryLevel];
            switch (batteryLevel) {
                case 3:
                    currentLed = greenLedPin;
                    break;
                default:
                    currentLed = yellowLedPin;
                    break;
            }
            break;
    }

    if(ledState == HIGH && currentMillis - lastPeriodOnStart >= onPeriod) {

        ledState = LOW;
        ClearFeedback(); // stop all feedback;
        lastPeriodOffStart = currentMillis;

    } else if (ledState == LOW && currentMillis - lastPeriodOffStart >=  standByPeriod) {

        ledState = HIGH;
        lastPeriodOnStart = currentMillis;
        if (currentLed == greenLedPin) {
            analogWrite(currentLed, 0); // inverted logic GPIO02
        } else {
            analogWrite(currentLed, maxAnalogDutyCycle);
        }
        if(givePhysicalFeedback) {
            PlayPhysicalFeedback(true);
        }
    }
}

void PlayPhysicalFeedback(bool turnOn) {
    if (activePiezo) {
        if (turnOn && toneAlarmActive) {
            tone(piezoPin, alarmFrequency, alarmOnDuration); // play 550 Hz tone in background for 'onDuration'
        } else {
            noTone(piezoPin);
        }
    } else {
        if (turnOn & toneAlarmActive) {
            analogWrite(piezoPin, 1000);
        } else {
            analogWrite(piezoPin, 0);
        }
    }

    if (turnOn && vibrationAlarmActive) {
        digitalWrite(vibrationPin, HIGH);
    } else {
        digitalWrite(vibrationPin, LOW);
    }
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

    if (pushType == 1 && shortPushCounter == 3){
        ClearFeedback();
        Serial.printf("Let's Snooze\n");

        shortPushCounter = 0;
        snoozeStart = currentMillis;
        snoozed = true;

        return;
    }

  if (pushType == 3 && shortPushCounter == 1) {
    Serial.println("Long push - DEACTIVATE the TONE ALARM");
    shortPushCounter = 0;

    toneAlarmActive = false;
    return;
  }

  if (pushType == 3 && shortPushCounter == 2) {
    Serial.println("Long push - DEACTIVATE both TONE ALARM and VIBRATION");
    shortPushCounter = 0;

    toneAlarmActive = false;
    vibrationAlarmActive = false;
    return;
  }

  if (pushType == 4 && shortPushCounter == 0) {
    Serial.println("Turn off");

    digitalWrite(stayOnPin, LOW);
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

void ClearFeedback(bool stopLights, bool stopTone, bool stopVibration) {
    analogWrite(redLedPin, 0);
    analogWrite(yellowLedPin, 0);
    analogWrite(greenLedPin, 1023); // inverted logic GPIO02
    PlayPhysicalFeedback(false);
}

void ClearReads() {
    for (int i = 0; i<readsSize; i++) {
        readsMedian.addValue(0);
    }
}

void ResetWifi() {

    WiFi.mode(WIFI_OFF);
    delay( 1 );

    WiFi.mode(WIFI_STA);
    delay( 1 );
}

int CalculatePercentage(int powerLevel) {
    return dBmPercentage[abs(powerLevel)];
}

double CalculateDistance(int wifiPower) {
  double exp = (27.55 - (20 * log10(wifiChannel.frequency)) + abs(wifiPower)) / 20.0;
  return pow(10.0, exp);
}


String MacToString(const unsigned char* mac) {
    char buf[20];
    snprintf(buf, sizeof(buf), "-%02x%02x%02x%02x%02x%02x",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return String(buf);
}
