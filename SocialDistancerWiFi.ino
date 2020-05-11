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
const int greenLedPin = 2; // inverted logic
const int yellowLedPin = 16;
const int redLedPin = 4;
const int vibrationPin = 15;
const int piezoPin = 5;
const int stayOnPin = 14;

const int analogWriteFrequency = 3500;
const int onAnalogDutyCycle  = 512;

// Battery meter
const int batteryLevelPin = A0;
const int batterySamplingSize = 10;
int batteryReadings[batterySamplingSize]; // sampling size
int batterySamples = 0;
const int batteryCheckInterval = 300000; // ms - 5 mins
unsigned long lastBatteryCheck = 0;
int batteryCharge = 3;

// Inputs
const int buttonPin = 12; // 12
const int usb5vPin = 13; // 13
byte reading = LOW;
byte unbounce = LOW;
byte previous = HIGH;
byte usb5vState;

// Alarms and info notification
byte highAlarmLevel = 50; // <------------------------ ALARM LEVEL
byte alarmState = 0;
byte currentAlarm = 0;
int lowAlarmLevel = 0;
bool outputTone = false;
bool outputVibration = false;
unsigned long lastNetworkFound = 0;
int ledState = LOW;
bool toneAlarmActive = true;
bool vibrationAlarmActive = true;
byte batteryLevel = 3; // 1-3

bool feedbackEnabled  = false;

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

// Button Code
const int shortPush = 200; //ms
const int longPush = shortPush * 4; //ms
const int turnOffPush = 3000;
const int endRead = shortPush * 5; //ms - pause after a character is enter
byte charsRead; // max 4
int codeRead;
bool readingButton = false;
unsigned long lastReadStart = 0;
unsigned long lastReadEnd = 0;

// Physical feedback
const int alarmFrequency = 550; //Hz

//AP variables
const WLANChannel wifiChannel = WiFiChannels[11];
const byte powerLevel = 0; // <------------------------ POWER LEVEL
String ssid = "Sx";
byte mac[6];
String netName;

char msg[64];

// Snooze
bool snoozed = false;
const int snoozePeriod = 300000; // 5mins - 300000
unsigned long snoozeStart = 0;

////////////////////////////////////////////////////////////////////////////////////////////

void setup() {

    ResetButtonRead();

    pinMode(redLedPin, OUTPUT);
    pinMode(yellowLedPin, OUTPUT);
    pinMode(greenLedPin, OUTPUT);
    pinMode(vibrationPin, OUTPUT);
    pinMode(piezoPin, OUTPUT);

    pinMode(stayOnPin, OUTPUT);
    digitalWrite(stayOnPin, HIGH);

    pinMode(buttonPin, INPUT_PULLUP);

    analogWriteFreq(analogWriteFrequency);

    Serial.begin(115200);

    StartUp();

    ActivateAccessPoint();

    feedbackEnabled  = true;
}

void ActivateAccessPoint() {

    WiFi.macAddress(mac);
    netName = ssid + MacToString(mac);

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

void loop() {
    byte readPowerPercentage = 0;
    bool networksFound = false;
    unsigned long currentMillis = millis();

    CheckUSB5v(currentMillis);

    unbounce = digitalRead(buttonPin);
    delay(25);
    reading = digitalRead(buttonPin);

    if (reading == unbounce) {
        if (reading == HIGH && previous == LOW) {
            analogWrite(yellowLedPin, 0);
            previous = HIGH;
            readingButton = false;
            lastReadEnd = currentMillis;

            int elapsedTime = lastReadEnd - lastReadStart;
            CheckButton(elapsedTime);
        }
        else if (reading == LOW && previous == HIGH) {
            analogWrite(yellowLedPin, onAnalogDutyCycle );
            previous = LOW;
            readingButton = true;
            lastReadStart = currentMillis;
        }
    }

    if (charsRead > 1 && !readingButton) {
        if (currentMillis - lastReadEnd > endRead) {
            if(charsRead > 2) {
                ProcessButtonCommand();
            } else {
                ResetButtonRead();
            }
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

            //char incomingMac[25];
            //char incomingSSID[25];
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
        if (feedbackEnabled) {
            if (currentLed == greenLedPin) {
                analogWrite(currentLed, 0); // inverted logic GPIO02
            } else {
                analogWrite(currentLed, onAnalogDutyCycle );
            }
            if(givePhysicalFeedback) {
                PlayPhysicalFeedback(true);
            }
        }
    }
}

void PlayPhysicalFeedback(bool turnOn) {

    if (turnOn & toneAlarmActive) {
        analogWrite(piezoPin, onAnalogDutyCycle);
    } else {
        analogWrite(piezoPin, 0);
    }

    if (turnOn && vibrationAlarmActive) {
        digitalWrite(vibrationPin, HIGH);
    } else {
        digitalWrite(vibrationPin, LOW);
    }
}

void CheckButton(int elapsedTime) {

    if (elapsedTime <= shortPush) {
        Serial.printf("short push\n");
        codeRead += (1 *  pow(10, 5 - charsRead));
    } else if (elapsedTime <= longPush) {
        Serial.printf("long push\n");
        codeRead += (2 *  pow(10, 5 - charsRead));
    } else if (elapsedTime >= turnOffPush) {
        ByeBye();
        return;
    }

    charsRead++;

    if(charsRead > 5) {
        ProcessButtonCommand();
    }
}

void ProcessButtonCommand() {
    switch(codeRead) {
        case 11100:
            Serial.printf("S: snooze\n");
            Snooze();
            break;
        case 12000:
            Serial.printf("A: no sound\n");
            toneAlarmActive = false;
            break;
        case 11200:
            Serial.printf("U: no sound or vibration\n");
            toneAlarmActive = false;
            vibrationAlarmActive = false;
            break;
        case 22200:
            Serial.printf("O: OTA Update\n");
            OTAUpdate();
            break;
        case 21220:
            Serial.printf("Y: Upload alarm data\n");
            UploadAlarmData();
            break;
    }
    ResetButtonRead();
}

void OTAUpdate() {

}

void UploadAlarmData() {

}

void Snooze() {
    ClearFeedback();

    snoozeStart = millis();
    snoozed = true;
}

void CheckUSB5v(unsigned long currentMillis) {
    usb5vState = digitalRead(usb5vPin);
    if (usb5vState == LOW) {
        delay(300);
        usb5vState = digitalRead(usb5vPin);
        if (usb5vState == LOW) {
           ByeBye();
           return;
        }
    }
}

void StartUp() {
    analogWrite(greenLedPin, 768); // inverted logic GPIO02
    delay(300);
    digitalWrite(stayOnPin, HIGH);
    digitalWrite(vibrationPin, HIGH);
    delay(500);
    digitalWrite(vibrationPin, LOW);
    analogWrite(greenLedPin, 1023); //
}

void ByeBye() {
    feedbackEnabled = false;

    for(byte i = 0; i<3; i++) {
     digitalWrite(vibrationPin, HIGH);
     analogWrite(redLedPin, 0);
     delay(100);
     digitalWrite(vibrationPin, LOW);
     analogWrite(redLedPin, 127);
     delay(100);
    }
    analogWrite(redLedPin, 0);
    Serial.println("Turn off");
    digitalWrite(stayOnPin, LOW);
    delay(5000);
}

void ResetButtonRead() {
    charsRead = 1;
    codeRead = 0;
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

void ClearFeedback() {
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
