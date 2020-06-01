// THE SOCIAL DISTANCER
// Author: Néstor Daza - nestor@extensa.io
// http://gosocialdistancer.com/

#include "ESP8266WiFi.h"
#include "ESP8266httpUpdate.h"
#include "ESP8266HTTPClient.h"
#include "WiFiClientSecureBearSSL.h"
#include "FastRunningMedian.h"

#include "pitches.h"
#include "env.h"

extern "C" {
    #include "user_interface.h"
}
char message[100];

// Read
const byte readsSize = 15;
byte reads = 0;
FastRunningMedian<unsigned int,readsSize, 0> readsMedian;

// Alarm period
int alarmCheckInterval = 1000; // ms
unsigned long alarmCheckStarts = 0;
const int alarmOnDuration = 250; // high value to keep red LED on during testing
const int alarmStandByDuration = 250;

// Outputs
const int greenLedPin = 2; // inverted logic
const int yellowLedPin = 16;
const int redLedPin = 4;
const int vibrationPin = 15;
const int piezoPin = 5;
const int stayOnPin = 14;

const int analogWriteFrequency = 3500;
const int ledIntensity = 75; // 1 - 1023
const int onAnalogDutyCycle = 512;

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
byte highAlarmLevel = 58; // <------------------------ ALARM THRESHOLD
const byte powerLevel = 82; // <---------------------- POWER LEVEL (0-82)
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
byte pfCounter = 0;

//AP variables
const WLANChannel wifiChannel = WiFiChannels[11];
String ssid = "SDx";
byte mac[6];
String netName;

// Open WiFI connection
#define MAX_CONNECT_TIME 30000
const char* openWiFiSSID = "HotSusanaSpot";
const char* openWiFiPassword = "";

char msg[64];

// Snooze
bool snoozed = false;
const unsigned long snoozePeriod = 300000; // 5mins - 300000
unsigned long snoozeStart = 0;

byte operationMode = 1; // 1-SocialDistancer 2-OTA Update 3-Data Upload

const bool printMessage = true;
bool alarming = true;


const int scanningPeriod = 2000;
unsigned long lastScanStart = 0;

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
    WiFi.persistent(false);

    WiFi.macAddress(mac);
    netName = ssid + MacToString(mac);

    WiFi.enableAP(true);
    delay(200);
    WiFi.mode(WIFI_STA);

    Serial.println();
    Serial.println(WiFi.softAP(netName, "", wifiChannel.channelNumber, false) ? netName + " AP Ready" : "AP Failed!");
    
    delay(200);
    Serial.println("AP setup done");
    Serial.printf("SOCIAL DISTANCER READY v1.0.1 %s\n", ESP.getSdkVersion());
}

void loop() {
    unsigned long currentMillis = millis();

    //CheckUSB5v(currentMillis);

    unbounce = digitalRead(buttonPin);
    delay(25);
    reading = digitalRead(buttonPin);

    if (reading == unbounce) {
        if (reading == HIGH && previous == LOW) {
            analogWrite(yellowLedPin, 0);
            previous = HIGH;
            readingButton = false;
            lastReadEnd = currentMillis;
            CheckButton(currentMillis);
        }
        else if (reading == LOW && previous == HIGH) {
            analogWrite(yellowLedPin, ledIntensity);
            previous = LOW;
            readingButton = true;
            lastReadStart = currentMillis;
        }
    }

    if(readingButton)
        return;

    if (charsRead > 1) {
        if (currentMillis - lastReadEnd > endRead) {
            if(charsRead > 2) {
                ProcessButtonCommand(currentMillis);
            } else {
                ResetButtonRead();
            }
        }
        return;
    }

    switch(operationMode) {
        case 1:

            RunSocialDistancer(currentMillis);
        
            break;
        case 2:
            RunOTAUpdate(currentMillis);
            break;
        case 3:
            RunDataUpload(currentMillis);
            break;
        default:
            break;
    }
}

void RunSocialDistancer(unsigned long currentMillis) {
    system_phy_set_max_tpw(powerLevel);

    if (snoozed) {
        if (currentMillis - snoozeStart < snoozePeriod) {
            return;
        } else {
            snoozed = false;
        }
    }

    int n = WiFi.scanNetworks(false, false, wifiChannel.channelNumber); // sync, not hidden

    bool deviceFound = false;
    String deviceSSID;
    int readPowerPercentage;
    int rssi;

    for (int i = 0; i < n; i++) {
        deviceSSID = WiFi.SSID(i);
        rssi = WiFi.RSSI(i);

        readPowerPercentage = 100 - abs(rssi);        
        readsMedian.addValue(readPowerPercentage);
        
        if (deviceSSID.substring(0,3) == ssid && readsMedian.getMedian() >= highAlarmLevel)
        {
            deviceFound = true;
        }
    }

    if(currentMillis - lastScanStart > scanningPeriod) {
        lastScanStart = currentMillis;
        
        //readsMedian.addValue(readPowerPercentage);
        if (printMessage) Serial.printf("Power: %d  Median: %d\n", rssi, readsMedian.getMedian());
    }  

    if (deviceFound && !alarming) {
        analogWrite(redLedPin, ledIntensity );
        analogWrite(greenLedPin, 1023); // inverted logic GPIO02
        if (printMessage) Serial.printf("Alert from %s\n", deviceSSID.c_str());

        alarming = true;
    }

    if (!deviceFound && alarming) {
        analogWrite(redLedPin, LOW );
        analogWrite(greenLedPin, 1023 - ledIntensity); // inverted logic GPIO02
        if (printMessage) Serial.println("No alert");

        alarming = false;
    }

    WiFi.scanDelete();
}

void RunOTAUpdate(unsigned long currentMillis) {

    if (ConnectToWifi()) {

        Serial.println("Running OTA - http://www.stomperapp.com/sd/SocialDistancerWiFi.ino.nodemcu.bin");
        t_httpUpdate_return ret = ESPhttpUpdate.update("stomperapp.com", 80, "/sd/SocialDistancerWiFi.ino.nodemcu.bin");
        switch (ret) {
            case HTTP_UPDATE_FAILED:
                Serial.printf("HTTP_UPDATE_FAILED Error (%d): %s\n", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
                break;
            case HTTP_UPDATE_NO_UPDATES:
                Serial.println("HTTP_UPDATE_NO_UPDATES");
                break;
            case HTTP_UPDATE_OK:
                Serial.println("HTTP_UPDATE_OK");
                ESP.restart();
                break;
        }
    }
    ByeBye();
}

void RunDataUpload(unsigned long currentMillis) {

}

///////////////////////////////////////////////////////////////////////////////////

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

    if (ledState == HIGH && currentMillis - lastPeriodOnStart >= onPeriod) {

        ledState = LOW;
        ClearFeedback(); // stop all feedback;
        lastPeriodOffStart = currentMillis;

    } else if (ledState == LOW && currentMillis - lastPeriodOffStart >=  standByPeriod) {

        ledState = HIGH;
        lastPeriodOnStart = currentMillis;
        if (feedbackEnabled) {
            if (currentLed == greenLedPin) {
                analogWrite(currentLed, 1023 - ledIntensity); // inverted logic GPIO02
            } else {
                analogWrite(currentLed, ledIntensity );
            }
            if(givePhysicalFeedback) {
                PlayPhysicalFeedback();
            }
        }
    }
}

void PlayPhysicalFeedback() {
    pfCounter ++;

    switch (pfCounter) {
        case 1:
        case 10:
            if (toneAlarmActive) {
                analogWrite(piezoPin, onAnalogDutyCycle);
            }
            break;
        case 2:
        case 11:
            if (vibrationAlarmActive) {
                digitalWrite(vibrationPin, HIGH);
            }
            break;
        case 20:
            pfCounter = 0;
            break;
    }
}

void CheckButton(unsigned long currentMillis) {

    int elapsedTime = lastReadEnd - lastReadStart;
    if (elapsedTime <= shortPush) {
        if (printMessage) Serial.printf("short push\n");
        codeRead += (1 *  pow(10, 5 - charsRead));
    } else if (elapsedTime >= turnOffPush) {
        ByeBye();
        return;
    } else {
        if (printMessage) Serial.printf("long push\n");
        codeRead += (2 *  pow(10, 5 - charsRead));
    }
    charsRead++;

    if(charsRead > 5) {
        ProcessButtonCommand(currentMillis);
    }
}

void ProcessButtonCommand(unsigned long currentMillis) {

    if (printMessage) Serial.printf("processing button... ");
    switch(codeRead) {
        case 11100:
            if (printMessage) Serial.printf("S: snooze\n");
            Snooze(currentMillis);
            break;
        case 12000:
            if (printMessage) Serial.printf("A: no sound\n");
            toneAlarmActive = false;
            break;
        case 11200:
            if (printMessage) Serial.printf("U: no sound or vibration\n");
            toneAlarmActive = false;
            vibrationAlarmActive = false;
            break;
        case 22200:
            if (printMessage) Serial.printf("O: OTA Update\n");
            operationMode = 2;
            break;
        case 21111:
            if (printMessage) Serial.printf("6: Send SMS\n");
            SendSMS();
            break;
        case 11221:
            if (printMessage) Serial.printf("Y: Upload alarm data\n");
            UploadAlarmData(currentMillis);
            break;
    }
    ResetButtonRead();
}

void UploadAlarmData(unsigned long currentMillis) {

}

void Snooze(unsigned long currentMillis) {
    ClearFeedback();
    snoozeStart = currentMillis;
    snoozed = true;
}

///////////////////////////////////////////////////////////////////////////////////

bool ConnectToWifi() {
    bool connected = false;
    /* Clear previous modes. */
    WiFi.softAPdisconnect();
    WiFi.disconnect();
    WiFi.mode(WIFI_STA);

    delay(shortPush);

    if (ConnectToAP(openWiFiSSID)) {
        return true;
    }
    else {

        int n = WiFi.scanNetworks(); // sync, hidden

        for (int i = 0; i < n; i++) {
            connected = ConnectToAP(WiFi.SSID(i).c_str());
            if (connected) {
                return true;
            }
        }
    }

    Serial.println("Couldn't connect to any WiFi");
    return false;
}

bool ConnectToAP(char const* ApSSID){

    if (printMessage) Serial.printf("Trying to connect to %s\n", ApSSID);
    WiFi.begin(ApSSID);
    unsigned short tries = 0;
    while (WiFi.status() != WL_CONNECTED && tries < 3) {
        if (printMessage) Serial.printf("...\n");
        tries++;
        delay(longPush * tries);
    }
    if(WiFi.status() == WL_CONNECTED) {
        Serial.printf("Connected to %s\n", ApSSID);
        Serial.println(WiFi.localIP());
        return true;
    }
    return false;
}

void SendSMS() {

    if (ConnectToWifi()) {
        char *host = "secure.smsgateway.ca";
        //String url = "secure.smsgateway.ca/SendSMS.aspx?CellNumber=15145854452&AccountKey=" + String(SMS_ACCOUNT_KEY) + "&MessageBody=" + ESP.getSdkVersion();
        String url = "secure.smsgateway.ca/SendSMS.aspx?CellNumber=15145854452&AccountKey=" + String(SMS_ACCOUNT_KEY) + "&MessageBody=dalerojodale";

        Serial.printf("Using ESP object: %s\n Sending to: %s\n", ESP.getSdkVersion(), url.c_str());
        int retCode = httpsGET(host, fingerprint, url);
        if (retCode==HTTP_CODE_OK) {
            Serial.println("[-] GET succeeded.");
            PlayFeedbackSequence(2);
        } else {
            Serial.printf("[!] failed: %d\n", retCode);
        }
    }

    ByeBye();
}

int httpsGET(char const* host, char const* fingerprint, String url) {

    std::unique_ptr<BearSSL::WiFiClientSecure>client(new BearSSL::WiFiClientSecure);
    client->setInsecure();

    HTTPClient https;

    Serial.print("[HTTPS] begin...\n");
    if (https.begin(*client, url)) {  // HTTPS

      Serial.print("[HTTPS] GET...\n");
      // start connection and send HTTP header
      int httpCode = https.GET();

      // httpCode will be negative on error
      if (httpCode > 0) {
        // HTTP header has been send and Server response header has been handled
        Serial.printf("[HTTPS] GET... code: %d\n", httpCode);

        // file found at server
        if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
          String payload = https.getString();
          Serial.println(payload);
        }
      } else {
        Serial.printf("[HTTPS] GET... failed, error: %s\n", https.errorToString(httpCode).c_str());
      }

      https.end();
    } else {
      Serial.printf("[HTTPS] Unable to connect\n");
    }
/*

    WiFiClientSecure client;
    client.setTimeout(15000);

    Serial.printf("Connecting to: %s\n", host);

    bool connected = false;
    byte retry = 0;
    while (!connected && retry < 30) {
        connected = client.connect(host, 443);
        Serial.print(".");
        retry++;
    }
    if(!connected) {
        Serial.println("HTTPS connection failed");
        return -1;
    }

    // Send GET
    client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" +
               "User-Agent: ESP8266\r\n" +
               "Connection: close\r\n\r\n");

    Serial.println("GET sent");
    while (client.connected()) {
        String line = client.readStringUntil('\n');
        if (line.startsWith("HTTP/1.1")) {
            // Get HTTP return code
            return line.substring(9,12).toInt();
        }
    }

    return -1;
    */
}

///////////////////////////////////////////////////////////////////////////////////
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
    PlayFeedbackSequence(2);
    digitalWrite(stayOnPin, HIGH);
    delay(shortPush);
}

void PlayFeedbackSequence(byte sequence) {
    switch(sequence) {
        case 1: // ByeBye
            for(byte i = 0; i<3; i++) {
                digitalWrite(vibrationPin, HIGH);
                analogWrite(redLedPin, 0);
                delay(100);
                digitalWrite(vibrationPin, LOW);
                analogWrite(redLedPin, 127);
                delay(100);
                analogWrite(redLedPin, 0);
            }
            break;
        case 2: // StartUp
            for (int i=0; i<4 ; i++) {
                analogWrite(redLedPin, ledIntensity);
                delay(shortPush);
                analogWrite(redLedPin, 0);
                delay(shortPush);
                analogWrite(greenLedPin, 1023 - ledIntensity); // inverted logic GPIO02
                delay(shortPush);
                analogWrite(greenLedPin, 1023); // inverted logic GPIO02
            }
            digitalWrite(vibrationPin, HIGH);
            delay(shortPush);
            digitalWrite(vibrationPin, LOW);
            break;
        case 3: // OTA
            for (byte i=0; i<3 ; i++) {
                analogWrite(yellowLedPin, ledIntensity);
                delay(shortPush);
                analogWrite(yellowLedPin, 0);
                analogWrite(redLedPin, ledIntensity);
                delay(shortPush);
                analogWrite(redLedPin, 0);
            }
            break;
    }
}

void ByeBye() {
    feedbackEnabled = false;
    operationMode = 0;

    PlayFeedbackSequence(1);

    Serial.println("Turn off");
    digitalWrite(stayOnPin, LOW);
    delay(5000);
}

void ResetButtonRead() {
    if (printMessage) Serial.printf("resetting button after %d\n", codeRead);
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
        for (int i=0; i<batterySamplingSize; i++) {
            average += batteryReadings[i];
        }
        average /= batterySamplingSize;

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

    analogWrite(piezoPin, 0);
    digitalWrite(vibrationPin, LOW);
}

String MacToString(const unsigned char* mac) {
    char buf[20];
    snprintf(buf, sizeof(buf), "-%02x%02x%02x%02x%02x%02x",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return String(buf);
}
