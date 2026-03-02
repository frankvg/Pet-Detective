/*  Pet Detective, by FAQware

    Using a time-of-flight sensor, it detects and records when a pet exits and returns.

    Last Build: 2-Mar-2026
    FAQware Copyright 2026

    Licensed under a Creative Commons Attribution-NonCommercial 4.0 International Public
    License
    https://creativecommons.org/licenses/by-nc/4.0/legalcode.txt

    The design uses 4 IR lasers and sensors to detect the motion of the pet across the four 
    sensors. The direction across the sensors determines which direction the pet has gone.  
    Sounds simple, but pets don't move as reliability as one hopes. The pet may come to the 
    door but not actually leave. They may leave or enter from the side angle. The sensors 
    cover 60-degrees and are spaced out in 15-degree increments perpendicular to the door. 
    The first beam is straight down (as the 3D printed case is designed to tilt the module 
    30 degrees).

    Our trend analysis of movement and with some weighting, gets fairly accurate. 
    We also auto-calibrate distance to the pet when first powered, and this is easily re-
    adjusted. You can go to the Sensor menu and see the distance (in mm) that each sensor 
    is detecting and recalibrate if needed. We also have a lockout period to avoid false 
    duplicates, currently set to 12 seconds.
    
    All user settings, including the distance calibration, are restored after a power
    failure. Logs are not saved. WiFi is not required but is used to set and maintain 
    accurate time. Your WiFi name and password can be entered and saved while using the
    Arduino IDE Serial monitor, at 115200 baud. When booted, if no name and password has
    been set, Pet Detector will explain and prompt to enter this data. If you elect to
    recompile the code, you can alternativly set your local WiFi SSID name and password 
    in the file “WiFi_SSID.h”. If WiFi is not used or detected, transtion status appears 
    without time information.

    A bonus with WiFi is you can use any local web browsers to see the transition status 
    by entering the IP address assigned to Pet Detector. There is an option to see all 32
    log entries along with trend values, which can be helpful if you need to further tune 
    the thresholds for your speciifc pet and pet door. IMPORTANT: This only works for
    your local network, and you must use http://  (Not https://).  Browsers often change
    the http to https and then you'll get a "Unable to connect" error.  The full URL
    is shown on the WiFi menu (two short button clicks).

    If you are getting incorrect in/out transition detections, it is usually due to the 
    sensor threshold (distance) set wrong. You can see the actual values in the Senor 
    menu and reset it to a new value. It uses 60% of the shortest distance of the four sensors. 
    It should be set when there is no pet in view of the sensor. The maximum sensor 
    range is limited 300 mm in software.
*/
const bool debug = false;  // false for normal operation
const char version[] = "1.4";


#include <WiFi.h>                       // external libraries
#include <time.h>
#include <NTPClient.h>
#include <EEPROM.h>
#include <Wire.h>
#include <SparkFun_VL53L5CX_Library.h>

#include "WiFI_SSID.h"                  // for your personal WiFi SSID and Password
#include "display.h"                    // various LCD display routines
#include "minimal_gif_decoder_flash.h"  // converts gif data to display on-screen
#include "routines.h"                   // EEPROM and other routines
#include "webpage.h"                    // web page - JavaScript and HTML

int  checkForButton(int pin, int wait);
void frameReady();
void frameInside();
void frameOutside();
void frameLog();
void frameWiFi();
void frameHours();
void frameGmtAdj();
void frameSensor();
void frameColor();
void frameAbout();
void frameError();
void frameWiFiReset();
void frameWiFiSetup();
void frameTrace();
void exitState();
void setState();
void setColorType();
int  InOutLogPtr(int newest);
int  percentInOut();
void showIndicator(int y, int barColor, bool* showing);
void hideIndicator(int y, bool* showing);
void buildIndicatorColors();
void processWebRequest();
void convertWiFiInfo();

String getQueryParameter(String& url, const String& paramName);


/*------------ Time of Flight VL Connections ---------------------------
Item  STEMcolor MyColor Ideaspark ESP32
      wire      wire    Pin       Pin
VCC   red       red     1 (3.3v)  3V3
GND   black     black   2         GND
SDA   blue      white   11        D21   (IC2 data)
SCL   yellow    yellow  14        D22   (IC2 clock)
*/

SparkFun_VL53L5CX myImager;
VL53L5CX_ResultsData measurementData;  // Result data class structure, 1356 byes of RAM
int imageResolution = 0;               //Used to pretty print output
int imageWidth = 0;                    //Used to pretty print output

bool sensorValid = false;
int sensorThreshold = 0;        // when booting, assumes furthest distance/2 is the threshold for pet detection
bool indicatorOn[4] = { 0 };    // this for the four side sensor indicators
int indicatorColor[4] = { 0 };  // outside to inside colors (built by buildIndicatorColors)
int sensorAllOn = false;

int tracePosition = 9;
bool showTrace = false;      // true when at least one threshold exceeded
bool endGroupTrace = false;  // after end of threshold indicators, display vertical line
int sensorColumn = 1;        // which of the 4 columns of sensor data to highlight in trace
int columns = 1;             // number of columns to process in Trace mode
int displayLine = 0;         // line to display on trace
int lockoutSeconds = 12;      // ignore events less than x seconds apart


//---------------- WiFi, Web Server & Time ------------------------------------------
int wifi_timeout = 0;
WiFiServer server(80);
String webColor = "00CCFF";    // color sent for web page status
bool webShowLog = false;       // show log in web page
String displayNowAction = "";  // text strings for web page
String displayCurrentActionHead = "";
String displayCurrentAction = "";
String displayPriorAction = "";
String displayPercentAction = "";
String displayTrendInside = "";
String displayTrendOutside = "";
bool fetchRequest = false;
bool skipPageUpdate = false;


int time_timeout = 0;
const char* ntpServer = "pool.ntp.org";  // Or pick another NTP server
long gmtOffset_sec = -8 * 60 * 60;       // Adjust for time zone offset from GMT
int daylightOffset_sec = 3600;           // Adjust for daylight saving time if applicable
WiFiUDP Udp;                             // A UDP instance to let us send and receive packets over UDP
NTPClient timeClient(Udp);
WiFiUDP ntpUDP;
time_t now;
tm timeinfo;
const char* monthsOfTheYear[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
const String zoneAbbreivations[] = { "HDT", "AKT", "PST", "MST", "CST", "EST", "AST" };  // only covering limited zones
int zone;
int zoneIndex;  // 0 to 13
unsigned long lastTime = 0;
int lastDay = 0;                        // used to not display unchanged date every second
unsigned long longTimeMilli = 0;        // if > 5 minutes in options, exit options

unsigned long currentMilli = 0;         // used to attempt accurate 100ms Loop timer
bool optionsStarted = false;
unsigned long optionsTimeout = 0;
int loopDurationMS = 100;               // planned loop duration, but is longer for many actions (i.e. graphics display update)

unsigned long lastMilli = 0;

#define logSize 32
time_t actionLog[logSize] = { 0 };      // type of entry (i.e. LOG_IN, etc.), a long on ESP32
int actionType[logSize]   = { 0 };      // array of action times in UTC time
int trendInside[logSize]  = { 0 };      // array of trend values used to determine if inside or out
int trendOutside[logSize] = { 0 };
struct tm* logTime;
int currentLogPtr = 0;                  // last log entry
int logPage = 0;                        // which page to display (4 entries per page)
#define LOG_UNUSED 0                    // types of log entries stored in actionLog
#define LOG_READY 1
#define LOG_IN 2
#define LOG_OUT 3
#define LOG_BUTTON 4                    // debug only

#define SENSOR_ACTIVE 1
#define SENSOR_INACTIVE 0
#define SENSOR_NOT_TRIPPED 0
#define SENSOR_TRIPPED 1
int lastSensorStateFront = SENSOR_INACTIVE;
int currentSensorStateFront = SENSOR_INACTIVE;
long activeSensorTimeFront = 0;
long inactiveSensorTimeFront = 0;
int sensorActionFront = SENSOR_NOT_TRIPPED;
int sensorThresholdCount = 0;
int maxSensorThreshold = 300;         // max value in mm

// Sensor Weighting Process
// This deals with the erratic values that can occur with pets not moving perfectly through the beams
// by tracking the 4 sensor values over the duration any sensors are triggered.
// bit 0 - Rear (outside side); bit 1 - Near Rear; bit 2 - Near Front; bit 3 - Front (inside side)
int sensorWeights[16] = { 0, 1, 0, 2, 0, 0, 0, 3, 7, 0, 0, 0, 6, 0, 5, 4 };
int sensorTrendInside = 0;
int sensorTrendOutside = 0;
#define sensorTrendShift 2      // This values adds weight to the trend outside coming in.  A zero value adds no weight.
                                // this might be different for other pet/door/positions. 
                                // With zero, we were getting too many "Now Outside" triggers when the pet had come in!
int eventsTraced = 0;
int sensorBitsLast = 0;
bool trendSet = false;
String trend = "";



// button press states
#define NONE 0
#define SHORT 1
#define LONG 2
#define VERYLONG 3
#define BUTTON0 0  // button pin
#define DOWN 0     // button position pressed
#define UP 1       // button position released
int lastButtonState = UP;
int currentButtonState = UP;
long pressedTime = 0;
long releasedTime = 0;

int buttonAction = NONE;
String debugLastAction = "";

// ======================= Run Once ==============================
void setup() {
  state = STARTUP;
  // control pins
  pinMode(BUTTON0, INPUT_PULLUP);  // lower button
  digitalRead(BUTTON0);            // read to maybe avoid false values later

  Serial.begin(115200);
  while (!Serial)
    ;

  lcd.init(LCD_HEIGHT, LCD_WIDTH);
  lcd.setRotation(1);  //The parameters are: 0, 1, 2, 3, representing the rotation of the screen 0°, 90°, 180°, 270°
  lcd.fillScreen(black);
  buildIndicatorColors();

  // STARTUP SCREEN
  Serial.println("");
  Serial.println("====== Pet Detective Started ======");

  // setup EEPROM and get prefrences
  initEEPROM();
  hour24 = EEPROM.read(EE_HOUR24);    // set to 1 if using 24 hour format
  colorType = EEPROM.read(EE_COLOR);  // set to color type
  setColorType();
  lcdDim = EEPROM.read(EE_DIM);  // set brightness
  sensorThreshold = (EEPROM.read(EE_THRESHOLD_HI) << 8) + EEPROM.read(EE_THRESHOLD_LO);
  int temp = static_cast<signed char>(EEPROM.read(EE_GMT_ADJUST));  // convert to signed value (0xFF = -1)
  if ((temp >= -10) && (temp <= -4)) {
    gmtOffset_sec = temp * 3600;  // valid
    temp = EEPROM.read(EE_DST_ADJUST);
    if ((temp == 0) || (temp == 1)) {
      daylightOffset_sec = temp * 3600;  // valid
    }
  }
  if ((ssid.length() == 0) && (password.length() == 0)) {   // if ssid/password not hard coded, get from EEPROM
    ssid = EEPROM.readString(EE_SSID);
    password = EEPROM.readString(EE_PASSWORD);
  }
  convertWiFiInfo();
  if (debug) Serial.println("EEPROM done");

  ShowGif(0, 30, Cat_Detective_Jacket_141x140_gif);  // position x, y, and raw data from GIF file
  ShowGif(155, 20, Pet_Detective_Text_155x62_gif);
  displayLeft(9, berry, 170, 120, "Sensor Init...");

  // Initialize sensor
  Wire.begin();            // This resets to 100kHz I2C
  Wire.setPins(21, 22);    // SDA, SCL assignments for EPS Dev Module
  Wire.setClock(1000000);  // Sensor has max I2C freq of 1MHz

  if (debug) Serial.println("Initializing sensor board");
  if (myImager.begin() == false) {
    displayLeftClear(9, red, 170, 120, LCD_WIDTH - 170, "Sensor not found");  // try power down/up to fix!
    Serial.println(F("Sensor not found"));
    sensorValid = false;
  } else {
    sensorValid = true;
  }

  myImager.setResolution(4 * 4);                // Enable 4x4
  myImager.setRangingFrequency(60);             // set 60Hz (default is 1Hz!)

  imageResolution = myImager.getResolution();   // Query sensor for current resolution - either 4x4 or 8x8
  imageWidth = sqrt(imageResolution);           // Calculate printing width

  myImager.startRanging();

  //Poll sensor for new data
  delay(100);
  if (sensorValid && sensorThreshold < 80) {    // if not saved in EEPROM, do a new calibration
    if (myImager.isDataReady() == true) {
      if (myImager.getRangingData(&measurementData)) {  // get data
        // with nothing in front of sensor, pick the nearest of four values
        int nearestVal = 2000;
        for (int i = 0; i < 4; i++) {
          if (measurementData.distance_mm[1 + i * 4] < nearestVal) {
            nearestVal = measurementData.distance_mm[1 + i * 4];
          }
        }
        sensorThreshold = nearestVal * 0.66;                  // set threshold to 2/3 distance
        if (sensorThreshold > maxSensorThreshold) sensorThreshold = maxSensorThreshold;  // 300 mm is max threshold
        if (sensorThreshold < 80) sensorThreshold = 80;       // 80 mm is min threshold
        displayLeftClear(9, berry, 170, 120, LCD_WIDTH - 170, "Sensor calibrated");
        EEPROM.write(EE_THRESHOLD_HI, (sensorThreshold >> 8) & 0xFF);
        EEPROM.write(EE_THRESHOLD_LO, sensorThreshold & 0xFF);
        EEPROM.commit();
      }
    }
  }
  if (sensorValid) {
    char tempChar[30];
    sprintf(tempChar, "Threshold %d mm", sensorThreshold);
    displayLeftClear(9, berry, 170, 120, LCD_WIDTH - 170, tempChar);
  }

  delay(2000);

  // access WiFi
  lcd.fillRect(170, 100, LCD_WIDTH - 170, LCD_HEIGHT - 110, black);
  if (password == ",") {
    displayLeft(9, berry, 170, 120, "WiFi Disabled");
    delay(2000);
    state = READY;
  } else if ((ssid == "") ||  (password == "")) {
    displayLeft(9, red, 170, 120, "WiFi not Set");
    state = WIFI_SETUP;
    delay(2000);
  } else {
    displayLeft(9, berry, 170, 120, "Connecting WiFi...");
    WiFi.begin(ssidChar, passwordChar);
    wifi_timeout = 0;
    time_timeout = 0;

    while (WiFi.status() != WL_CONNECTED) {
      if (debug) Serial.print(".");
      wifi_timeout++;
      lcd.setCursor(170 + wifi_timeout * 2, 150);
      setTextColorLCD(medgreen);  // Lime
      lcd.print(">");
      if (wifi_timeout == 35) WiFi.begin(ssidChar, passwordChar);   // try again 
      if (wifi_timeout > 70) break;  // 7 seconds (70 * 100ms)
      delay(100);
    }
    if (wifi_timeout < 71) {
      if (debug) Serial.println("\nWiFi connected.");
      if (debug) Serial.print("IP address: ");
      if (debug) Serial.println(WiFi.localIP());
      long rssi = WiFi.RSSI();
      if (debug) Serial.print("Signal strength (RSSI): ");
      if (debug) Serial.print(rssi);
      if (debug) Serial.println(" dBm");
      server.begin();  // start the web server on port 80

      // get time
      lcd.fillRect(170, 100, LCD_WIDTH - 170, LCD_HEIGHT - 110, black);
      displayLeft(9, berry, 170, 120, "Acquiring Time...");
      configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);  // set time from local server, default updates hourly
      setTextColorLCD(amber);
      while (!getLocalTime(&timeinfo)) {
        time_timeout++;                              // never gets here?
        lcd.setCursor(170 + time_timeout * 4, 150);  // continue indicator location from WiFi indicator
        lcd.print(">");
        lcd.setCursor(170 + time_timeout * 4 + 2, 150);
        lcd.print(">");
        if (time_timeout > 4) break;  // This is very slow when time is not acquired!  Maybe 4 seconds per loop
        delay(500);
      }
    }


    // Print the time to the serial monitor & display
    time(&now);
    localtime_r(&now, &timeinfo);
    if (timeinfo.tm_year > 124) {  // 1900+124 = 2024 (if year is 2024 or older, time is invalid)
      Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
      lcd.fillRect(170, 100, LCD_WIDTH - 170, LCD_HEIGHT - 110, black);
      lcd.setCursor(170, 120);
      setTextColorLCD(berry);  // blueish-purple
      lcd.println("Time Acquired");
      setTextColorLCD(lilac);  // blueish-purple
      lcd.setFont(&FreeSans12pt7b);
      lcd.setCursor(171, 150);
      String nowTime;
      int display_count = 4;
      while (display_count) {
        time(&now);
        localtime_r(&now, &timeinfo);
        nowTime = convertTime(timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
        lcd.fillRect(171, 121, LCD_WIDTH - 171, LCD_HEIGHT - 121, black);
        displayLeft(12, lilac, 171, 150, nowTime.c_str());
        display_count--;
        delay(1000);
        state = READY;
      }
    } else {
      // show failed status messge
      Serial.print("Failed time year: ");
      Serial.println(timeinfo.tm_year);
      state = ERROR;
    }
  }
  frameFirstTime = true;
  debugLastAction = "Startup";
  lastMilli = millis();
  if (debug) Serial.println("Startup done");
}


// ============================ Main Loop ===================================
void loop() {
  // check for button press and sensor changes every 100 mS

  // read the ButtonState of the button (Down=0, Up=1)
  buttonAction = NONE;
  currentButtonState = digitalRead(BUTTON0);
  if (lastButtonState != currentButtonState) {
    if (lastButtonState == UP && currentButtonState == DOWN)  // button is pressed
      pressedTime = millis();
    else if (lastButtonState == DOWN && currentButtonState == UP) {  // button is released
      releasedTime = millis();

      long pressDuration = releasedTime - pressedTime;
      lastState = state;
      if ((pressDuration < 700) && (pressDuration > 100)) {  // ignore very short presses
        buttonAction = SHORT;
        // debug
        debugLastAction = "Button short";
      }
      if (pressDuration >= 5000) {  // 5 seconds
        buttonAction = VERYLONG;
        debugLastAction = "Button very long";
      } else if (pressDuration >= 800) {
        buttonAction = LONG;
        debugLastAction = "Button long";
      }
    }
    lastButtonState = currentButtonState;
  }

  // check status change from sensor and update in/out status

  // process actions
  if (state == READY) {
    frameReady();
  } else if (state == INSIDE) {
    frameInside();
  } else if (state == OUTSIDE) {
    frameOutside();
  } else if (state == LOG) {      // each short press rotates through these frames
    frameLog();
  } else if (state == WIFI) {
    frameWiFi();
  } else if (state == SENSOR) {
    frameSensor();
  } else if (state == HOURS) {
    frameHours();
  } else if (state == GMTADJ) {
    frameGmtAdj();
  } else if (state == COLOR) {
    frameColor();
  } else if (state == ABOUT) {
    frameAbout();
  } else if (state == ERROR) {        // rest of these frames are special cases  
    frameError();
  } else if (state == WIFI_RESET) {
    frameWiFiReset();
  } else if (state == WIFI_SETUP) {
    frameWiFiSetup();
  } else if (state == TRACE) {
    frameTrace();
  }

if (state < LOG)  processWebRequest();       // if requested, display web page only if not in menus

  // try to wait the delay for each loop (unless code takes longer during a loop)
  currentMilli = millis();
  if ((currentMilli - lastMilli) < loopDurationMS) {
    delay(loopDurationMS - (currentMilli - lastMilli));
  } else if (debug) {
    Serial.print("Loop took ms: ");
    Serial.println(currentMilli - lastMilli);
    // no additional delay (ran out of time, as happens when larger graphics displayed)
  }
  lastMilli = millis();

  // if options take longer than 5 minutes, exit options
  if (optionsStarted && (optionsTimeout + 300000 < lastMilli)) {
    optionsStarted = false;
    state = NOMORE;
    exitState();
  }
}

// ============================ Functions ===================================

void frameReady() {
  if (frameFirstTime) {
    displayInOutHeading(89, Cat_Unknown_89x170_gif);
    displayNowAction = "Ready";
    displayCurrentAction = "Awaiting</br>Movement";
    webColor = convertRgb565ToRgb888(magenta);
    displayCenter(18, magenta, center, 58, displayNowAction.c_str());
    displayCenter(12, medmagenta, center, 90, "Awaiting");
    displayCenter(12, medmagenta, center, 112, "Movement");

    displayCenter(9, grey, center, 150, "Short press for options");
    for (int i = 0; i < 4; i++) {
      indicatorOn[i] = false;
    }
    sensorAllOn = false;

    // save current time to log
    time(&now);
    localtime_r(&now, &timeinfo);
    if (timeinfo.tm_year > 124) {  // 1900+124 = 2024 (if year is 2024 or older, time is invalid)
      currentLogPtr = (currentLogPtr + 1) % logSize;
      actionType[currentLogPtr] = LOG_READY;
      actionLog[currentLogPtr] = now;
      trendInside[currentLogPtr] = 0;
      trendOutside[currentLogPtr] = 0;
    }
  }
  setState();
}

void frameInside() {
  if (frameFirstTime) {
    displayInOutHeading(93, Cat_Inside_Sitting_93x170_gif);
    displayNowAction = "Now Inside";
    webColor = convertRgb565ToRgb888(insideColor);
    displayCenter(18, insideColor, center, 54, displayNowAction.c_str());
    for (int i = 0; i < 4; i++) {
      indicatorOn[i] = false;
    }
    sensorAllOn = false;

    // current entry should be LOG_IN
    int tempPtr = InOutLogPtr(0);  // get the newest in/out log ptr
    if (tempPtr != -1) {
      if (actionType[tempPtr] == LOG_IN) {  // should always be true!
        time_t raw_time = actionLog[tempPtr];
        struct tm* logTime = localtime(&raw_time);
        if (logTime != nullptr) {
          displayCurrentActionHead = "Returned at";
          displayCenter(9, grey, center, 74, displayCurrentActionHead.c_str());
          displayCurrentAction = convertTime(logTime->tm_hour, logTime->tm_min, -1);
          displayCenter(18, insideColorMed, center, 108, displayCurrentAction.c_str());
        }
      }

      int priorLogPtr = InOutLogPtr(1);  // get the next to newest in/out log ptr
      displayPriorAction = "";
      displayPercentAction = "";
      if (priorLogPtr != -1) {
        if (actionType[priorLogPtr] == LOG_OUT) {
          displayPriorAction = "Prior out at ";  // normal
        } else if (actionType[priorLogPtr] == LOG_IN) {
          displayPriorAction = "Prior in at ";  // unusal event, two in transitions in a row - maybe multiple pets or used another door to go out
        }
        if (displayPriorAction != "") {
          time_t prior_time = actionLog[priorLogPtr];
          struct tm* logTime = localtime(&prior_time);
          if (logTime != nullptr) {
            displayPriorAction += convertTime(logTime->tm_hour, logTime->tm_min, -1);
            displayCenter(9, grey, center, 130, displayPriorAction.c_str());
          }

          // scan log to get percentage time in vs. out
          int percentOutside = percentInOut();
          if (percentOutside != -1) {
            char tempText[30];
            sprintf(tempText, "Time outside %d%%", percentOutside);
            displayPercentAction = String(tempText);
            displayCenter(9, grey, center, 153, tempText);
          }
        }
      }
    }
  }
  setState();
}

void frameOutside() {
  if (frameFirstTime) {
    displayInOutHeading(123, Cat_Outside_Sitting_123x170_gif);
    displayNowAction = "Now Outside";
    webColor = convertRgb565ToRgb888(outsideColor);
    displayCenter(18, outsideColor, center, 54, displayNowAction.c_str());
    for (int i = 0; i < 4; i++) {
      indicatorOn[i] = false;
    }
    sensorAllOn = false;

    // current entry should be LOG_OUT
    int tempPtr = InOutLogPtr(0);  // get the newest in/out log ptr
    if (tempPtr != -1) {
      if (actionType[tempPtr] == LOG_OUT) {  // should always be true!
        time_t raw_time = actionLog[tempPtr];
        struct tm* logTime = localtime(&raw_time);
        if (logTime != nullptr) {
          displayCurrentActionHead = "Left at";
          displayCenter(9, grey, center, 74, displayCurrentActionHead.c_str());
          displayCurrentAction = convertTime(logTime->tm_hour, logTime->tm_min, -1);
          displayCenter(18, outsideColorMed, center, 108, displayCurrentAction.c_str());
        }
      }

      int priorLogPtr = InOutLogPtr(1);  // get the next to newest in/out log ptr
      displayPriorAction = "";
      displayPercentAction = "";
      if (priorLogPtr != -1) {
        if (actionType[priorLogPtr] == LOG_OUT) {
          displayPriorAction = "Prior out at ";  // unusal event, two out transistions in a row - maybe multiple pets or used another door
        } else if (actionType[priorLogPtr] == LOG_IN) {
          displayPriorAction = "Prior in at ";  // normal
        }
        if (displayPriorAction != "") {
          time_t prior_time = actionLog[priorLogPtr];
          struct tm* logTime = localtime(&prior_time);
          if (logTime != nullptr) {
            displayPriorAction += convertTime(logTime->tm_hour, logTime->tm_min, -1);
            displayCenter(9, grey, center, 130, displayPriorAction.c_str());
          }

          // scan log to get percentage time in vs. out
          int percentOutside = percentInOut();
          if (percentOutside != -1) {
            char tempText[30];
            sprintf(tempText, "Time outside %d%%", percentOutside);
            displayPercentAction = String(tempText);
            displayCenter(9, grey, center, 153, tempText);
          }
        }
      }
    }
  }
  setState();
}

// show log of in and out actions
void frameLog() {
  if (frameFirstTime) {
    displayNowAction = "Options";
    displayCurrentAction = "";
    displayPriorAction = "Sensor Inactive";
    displayPercentAction = "";
    webColor = convertRgb565ToRgb888(purple);
    WiFiServer server(80);

    optionsStarted = true;
    optionsTimeout = millis();
    if (debug && (actionType[currentLogPtr] != LOG_BUTTON)) {
      time(&now);
      localtime_r(&now, &timeinfo);
      if (timeinfo.tm_year > 124) {  // 1900+124 = 2024 (if year is 2024 or older, time is invalid)
        currentLogPtr = (currentLogPtr + 1) % logSize;
        actionType[currentLogPtr] = LOG_BUTTON;
        actionLog[currentLogPtr] = now;
      }
    }
    char tempPageNum[30], tempIndexText[20], tempChar[25];
    int tempIndexPtr = 0;
    displayHeading("Next", "Log");
    sprintf(tempPageNum, "Page %d of %d", logPage + 1, logSize / 4);
    displayCenter(9, purple, 135, 15, tempPageNum);

    displayLeft(9, grey, 0, 140, "Long press for another log page");
    bool timeNowValid = false;

    // find the newest time (largest number)
    time_t largestValue = 0;
    int newestIndex = 0;
    for (int j = 0; j < logSize; j++) {
      if ((actionType[j] != LOG_UNUSED) & (largestValue < actionLog[j])) {
        newestIndex = j;
        largestValue = actionLog[j];
      }
    }


    time(&now);
    localtime_r(&now, &timeinfo);
    if (timeinfo.tm_year > 124) {  // 1900+124 = 2024 (if year is 2024 or older, time is invalid)
      timeNowValid = true;
    }
    for (int i = 0; i < 4; i++) {
      tempIndexPtr = (newestIndex - i - logPage * 4);
      if (tempIndexPtr < 0) tempIndexPtr += logSize;

      sprintf(tempIndexText, "%d", i + logPage * 4 + 1);
      displayLeft(9, grey, 0, 50 + i * 20, tempIndexText);
      // convert to date and time
      if (actionType[tempIndexPtr] == 0) {
        displayLeft(9, purple, 25, 50 + i * 20, "Unused");
      } else {
        if (actionType[tempIndexPtr] == LOG_READY) {
          displayLeft(9, magenta, 25, 50 + i * 20, "Ready");
        } else if (actionType[tempIndexPtr] == LOG_IN) {
          displayLeft(9, insideColor, 25, 50 + i * 20, "Inside");
        } else if (actionType[tempIndexPtr] == LOG_OUT) {
          displayLeft(9, outsideColor, 25, 50 + i * 20, "Outside");
        } else if (actionType[tempIndexPtr] == LOG_BUTTON) {
          displayLeft(9, lilac, 25, 50 + i * 20, "Options");
        }
        if ((actionType[tempIndexPtr] == LOG_READY) || (actionType[tempIndexPtr] == LOG_IN) || (actionType[tempIndexPtr] == LOG_OUT) || (debug && (actionType[tempIndexPtr] == LOG_BUTTON))) {
          time_t raw_time = actionLog[tempIndexPtr];
          struct tm* logTime = localtime(&raw_time);
          if (logTime != nullptr) {
            String tempText = convertTime(logTime->tm_hour, logTime->tm_min, -1);
            displayRight(9, white, 168, 50 + i * 20, tempText.c_str());
            sprintf(tempChar, " %d-%s", logTime->tm_mday, monthsOfTheYear[logTime->tm_mon]);
            displayLeft(9, grey, 180, 50 + i * 20, tempChar);
            if (timeNowValid && (timeinfo.tm_mday == logTime->tm_mday) && (timeinfo.tm_mon == logTime->tm_mon) && (timeinfo.tm_year == logTime->tm_year)) {
              displayText(grey, -1, -1, " *");  // Append a today marker
              displayLeft(9, grey, 235, 160, "*Today");
            }
            // include the trend if in/out entry
            if ((actionType[tempIndexPtr] == LOG_IN) || (actionType[tempIndexPtr] == LOG_OUT)) {
              sprintf(tempChar, "%d", trendInside[tempIndexPtr]);
              displayTrendInside = tempChar;
              displayLeft(9, insideColorMed, 270, 50 + i * 20, tempChar);
              sprintf(tempChar, "%d", trendOutside[tempIndexPtr]);
              displayTrendOutside = tempChar;
              displayLeft(9, outsideColorMed, 290, 50 + i * 20, tempChar);
            }
          }
        }
      }
    }
  }
  if (buttonAction == LONG) {
    logPage = (logPage + 1) % (logSize / 4);
    frameFirstTime = true;  // display changed log page again
  } else if (buttonAction == SHORT) {
    logPage = 0;
    exitState();  // exit
  }
}

// when active, called every 100mS (loopDurationMS), option to reset and require WiFi
void frameWiFi() {
  int valAlign = 105;  // alignment of values from left edge
  if (frameFirstTime) {
    frameCounter = 5;  // set for 500 ms for info that needs updating
    displayHeading("Next", "WiFi");
    displayLeft(9, grey, 0, 60, "WiFi SSID");
    displayText(white, valAlign, 60, ssidChar);
    displayLeft(9, grey, 0, 80, "Mac addr");
    displayLeft(9, grey, 0, 100, "Web access");
    displayLeft(9, grey, 0, 120, "Signal level");
    displayLeft(9, grey, 0, 140, "Long press to reset WiFi");

    if (WiFi.status() != WL_CONNECTED) {
      frameCounter = -1;  // no updates
      if (ssid == "") displayLeft(9, red, valAlign, 60, "Not set");
      displayLeft(9, red, valAlign, 80, "WiFi not connected");
      displayLeft(9, red, valAlign, 100, "Unavailable");
      displayLeft(9, red, valAlign, 120, "Unavailable");
    } else {
      byte mac[6];
      WiFi.macAddress(mac);  // get 6 bytes of mac address
      char tempChar[30];
      sprintf(tempChar, "%02X:%02X:%02X:%02X:%02X:%02X", mac[5], mac[4], mac[3], mac[2], mac[1], mac[0]);
      displayText(white, valAlign, 80, tempChar);
      char ipCharBuffer[16];  // Buffer to hold the IP address string (e.g., "192.168.1.100\0")
      IPAddress myIP = WiFi.localIP();
      snprintf(ipCharBuffer, 30, "http://%d.%d.%d.%d", myIP[0], myIP[1], myIP[2], myIP[3]);
      displayLeft(9, white, valAlign, 100, ipCharBuffer);
    }
    frameFirstTime = false;
  }
  if (buttonAction == SHORT) {
    // long or short press exits WiFi to next frame
    exitState();
  } else if (buttonAction == LONG) {
    frameFirstTime = true;
    buttonAction = NONE;
    if ((password == ",") || (password == "")) {
      // password not set, so request user enter it
      state = WIFI_SETUP;
    } else {
      state = WIFI_RESET;
    }
    return;
  }

  if (frameCounter == 3) {  // only display every 500 ms
    // section for items updated every 500 ms until button pressed
    char tempChar[48];
    //lcd.fillRect(120, 85, LCD_WIDTH-120, 40, black);
    sprintf(tempChar, "%d dBm", WiFi.RSSI());
    displayLeftClear(9, white, valAlign, 120, LCD_WIDTH - valAlign, tempChar);
  }
  if (frameCounter > 0) {
    frameCounter--;
    if (frameCounter <= 0) frameCounter = 5;
  }
}

// reset WiFi
void frameWiFiReset() {
  if (frameFirstTime) {
    displayHeading("Exit", "WiFi Reset");
    displayLeft(9, grey, 0, 50,   "We use WiFi to get the current time.");
    displayLeft(9, white, 0, 70,  "Without WiFi, Pet Detective will still");
    displayLeft(9, white, 0, 90,  "detect in/out transitions, but without");
    displayLeft(9, white, 0, 110, "any time status.");
    displayLeft(9, grey, 0, 140,  "Long press to reset WiFi");
  }
  if (buttonAction == SHORT) {
    state = NOMORE;
    exitState();  // exit back to In/Out/Ready frames
  } else if (buttonAction == LONG) {
    if (password == ",") {
      // password not set, so request user enter password (likely can't get here)
      frameFirstTime = true;
      frameWiFiSetup();    
    } 
    lcd.fillRect(0, 36, LCD_WIDTH, LCD_HEIGHT - 36, black);
    displayLeft(12, grey, 0, 60, "Status");
    if (WiFi.status() == WL_CONNECTED) {  // is wifi connection active
      displayLeftClear(12, white, 100, 60, LCD_WIDTH - 100, "Disconnecting");
      // ditch the existing connection
      WiFi.disconnect();
      delay(1000);  // allow some time for the connection to be fully dropped, **important!**
      // check if the connection was really dropped
      if (WiFi.status() == WL_CONNECTED) {
        displayLeftClear(12, red, 100, 60, LCD_WIDTH - 100, "Disconnect failed");
        delay(2000);
        state = NOMORE;
        exitState();
      }
    }
    //
    displayLeftClear(12, grey, 100, 60, LCD_WIDTH - 100, "Connecting...");
    WiFi.begin(ssidChar, passwordChar);
    wifi_timeout = 0;
    time_timeout = 0;

    while (WiFi.status() != WL_CONNECTED) {
      wifi_timeout++;
      lcd.setCursor(100 + wifi_timeout * 2, 95);
      setTextColorLCD(medgreen);  // Lime
      lcd.print(">");
      if (wifi_timeout == 35) WiFi.begin(ssidChar, passwordChar);   // try again 
      if (wifi_timeout > 70) break;  // 7 seconds (70 * 100ms)
      delay(100);
    }
    if (wifi_timeout > 70) {
      displayLeftClear(12, red, 100, 60, LCD_WIDTH - 100, "Connection Failed");
      Serial.println("WiFi connection failed");
    } else {
      lcd.fillRect(100, 85, LCD_WIDTH - 100, 20, black);
      displayLeftClear(12, green, 100, 60, LCD_WIDTH - 100, "WiFi Connected");
      Serial.println("WiFi connected!");
      configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);  // set time from local server, default updates hourly
      server.begin();  // start the web server on port 80
      delay(2000);
      displayLeftClear(12, grey, 100, 60, LCD_WIDTH - 100, "Acquiring Time");
      setTextColorLCD(amber);
      while (!getLocalTime(&timeinfo)) {
        time_timeout++;
        lcd.setCursor(100 + time_timeout * 4, 95);
        lcd.print(">");
        lcd.setCursor(100 + time_timeout * 4 + 2, 95);
        lcd.print(">");
        if (time_timeout > 4) break;  // This is very slow when time fails to be acquired!  About 4 seconds per loop.
        delay(500);
      }
      if (time_timeout > 4) {
        displayLeftClear(12, red, 100, 60, LCD_WIDTH - 100, "Time Failed");
      } else {
        displayLeftClear(12, green, 100, 60, LCD_WIDTH - 100, "Time Acquired");
      }
    }
    state = NOMORE;
    delay(2000);
    exitState();  // exit
  }
}

// get the WiFi SSID and Password via the IDE Serial Connection
void frameWiFiSetup() {
  if (frameFirstTime) {
    displayHeading("exit, run without WiFi", "WiFi Setup");
    displayLeft(9, white, 0, 50,  "WiFi is required to obtain the time. The");
    displayLeft(9, white, 0, 70,  "SSID & Password can be set using the");
    displayLeft(9, white, 0, 90,  "Arduino IDE Serial Monitor when Pet");
    displayLeft(9, white, 0, 110, "Detective connected to the PC via USB.");
    displayLeft(9, grey, 0, 140,  "Long press to enter data from IDE");
  }
  if (buttonAction == SHORT) {
    // short press goes to ready (no WiFi)
    password = ',';   // special password to ignore WiFi
    EEPROM.writeString(EE_PASSWORD, password);
    EEPROM.commit();
    state = NOMORE;
    exitState();
  } else if (buttonAction == LONG) {
    lcd.fillRect(0, 36, LCD_WIDTH, LCD_HEIGHT - 36, black);
    if (Serial) {   // Serial is always true on ESP32!
      Serial.println("WiFi Setup");
      displayLeft(9, white, 0, 50, "Waiting for entered WiFi info via IDE.");
      displayLeft(9, grey, 0, 70,  "If the Arduino IDE is not active or the");
      displayLeft(9, grey, 0, 90,  "Serial Monitor is not working, power");
      displayLeft(9, grey, 0, 110, "down to exit.");
      displayLeft(9, grey, 0, 160, "On Serial Monitor use: 115200 baud.");
      Serial.setTimeout(300000);  // wait 5 minutes before asking again
      // read the incoming string until a newline '\n' character is found
      while(1) {
        Serial.print("  Enter your WiFi SSID (Name) and press Enter: ");
        String inString = Serial.readStringUntil('\n');
        inString.trim(); 
        int inLen = inString.length();

        // check validity
        if ((inLen < 1) || (inLen > 32)) {
          Serial.println("\n    Invalid length - must be 1 to 32 characters.");
          continue;
        } 
        bool inStringBad = false;
        for (int i = 0; i < inLen; i++) {
          if (!(isAlphaNumeric(inString[i]) || (inString[i] == ' ')  || (inString[i] == '.') || (inString[i] == '-') || (inString[i] == '_'))) {
            inStringBad = true;
            break;
          }
        }
        if (inStringBad) {
            Serial.println("\n    Invalid characters. Use only A-Z, a-z, space, dash, underline, or period.");
            continue;
        }

        ssid = inString;
        Serial.println(ssid);
        break;
      }
      while (1) {
        Serial.print("  Enter your WiFi password and press Enter: ");
        String inString = Serial.readStringUntil('\n');
        inString.trim(); 
        int inLen = inString.length();
        if ((inLen < 8) || (inLen > 63)) {
          Serial.println("\n    Invalid length - must be 8 to 63 characters.");
          continue;
        } 
        password = inString;
        Serial.println(password);
        break;
      }
      convertWiFiInfo();
      // save values to EEPROM
      EEPROM.writeString(EE_SSID, ssid);
      EEPROM.writeString(EE_PASSWORD, password);
      EEPROM.commit();

      Serial.println("======= Proceeding to activate WiFi =======");
      // jump into WiFi Reset
      state = WIFI_RESET;
      frameFirstTime = false;
      buttonAction = LONG;
      frameWiFiReset();
    } else {
      // IDE not available!  (NOTE ESP32 will never get here, as Serial is always true!)
      lcd.fillRect(0, 36, LCD_WIDTH, LCD_HEIGHT - 36, black);
      displayLeft(9, red,   0, 50,  "IDE and Serial Monitor are inactive.");
      displayLeft(9, white, 0, 70,  "WiFi is disabled and no time info");
      displayLeft(9, white, 0, 90,  "is available. To enable WiFi and");
      displayLeft(9, white, 0, 110, "set the name and password, in the");
      displayLeft(9, white, 0, 130, "WiFi menu,do a WiFi reset.");
      delay(2000);
      password = ',';   // special password to ignore WiFi, but not saved in EEPROM
      state = NOMORE;
      exitState();
    }
  }
}

// update sensor data every 166 mS
void frameSensor() {
  char s[10] = "";
  if (frameFirstTime) {
    displayHeading("Next", "Sensor");
    displayLeft(9, grey, 0, 50, "Rear");
    displayLeft(9, grey, 0, 70, "Near Rear");
    displayLeft(9, grey, 0, 90, "Near Front");
    displayLeft(9, grey, 0, 110, "Front");

    displayLeft(9, grey, 190, 50, "Threshold");
    sprintf(s, "%d mm", sensorThreshold);
    displayRightClear(9, grey, 220, 70, 45, s);
    displayLeft(9, grey, 0, 140, "Long press to recalibrate threshold");
    frameCounter = 20;  // set for 500 ms for info that needs updating
  }
  if (buttonAction == LONG) {
    if (sensorValid) {
      if (myImager.isDataReady() == true) {
        if (myImager.getRangingData(&measurementData)) {  // get data
          // with nothing in front of sensor, pick the nearest of four values
          int nearestVal = 2000;
          for (int i = 0; i < 4; i++) {
            if (measurementData.distance_mm[1 + i * 4] < nearestVal) {
              nearestVal = measurementData.distance_mm[1 + i * 4];
            }
          }
          sensorThreshold = nearestVal * 0.66;                  //set threshold to 2/3 distance
          if (sensorThreshold > maxSensorThreshold) sensorThreshold = maxSensorThreshold;  // 300 mm is max threshold
          if (sensorThreshold < 80) sensorThreshold = 80;    // 80 mm is min threshold
          Serial.print("Sensor threshold: ");
          Serial.println(sensorThreshold);
          EEPROM.write(EE_THRESHOLD_HI, (sensorThreshold >> 8) & 0xFF);
          EEPROM.write(EE_THRESHOLD_LO, sensorThreshold & 0xFF);
          EEPROM.commit();

          sprintf(s, "%d mm", sensorThreshold);
          displayRightClear(9, white, 220, 70, 45, s);
          buttonAction = NONE;
          frameCounter = 20;  //force "white" for threshold for 2 seconds before returning to grey
        }
      }
    }
  } else if (buttonAction == VERYLONG) {  // hidden trace mode (5 second button press)
    state = TRACE;
    buttonAction = NONE;
    frameFirstTime = true;
    return;
  } else if (buttonAction == SHORT) {
    exitState();
  }

  // update range every 500 ms
  if (frameCounter % 5 == 0) {
    if (sensorValid) {
      if (myImager.isDataReady() == true) {
        if (myImager.getRangingData(&measurementData)) {  // get data
          int val;
          for (int i = 0; i < 4; i++) {
            val = measurementData.distance_mm[1 + i * 4];
            sprintf(s, "%d", val);
            if (val >= 1000) {
              // use monospaced font (-9) as right align code is not that accurate with other fonts
              displayRightClear(-9, amber, 120, 50 + i * 20, 40, "far");  // far out
            } else if (val > sensorThreshold) {
              displayRightClear(-9, grey, 120, 50 + i * 20, 40, s);  // beyond threshold
            } else {
              displayRightClear(-9, white, 120, 50 + i * 20, 40, s);  // within threshold
            }
          }
        }
      }
    } else {
      for (int i = 0; i < 4; i++) {
        displayRightClear(9, red, 120, 60 + i * 20, 45, "dead");
      }
    }
  }
  frameCounter--;
  if (frameCounter <= 0) {
    frameCounter = 20; 
    sprintf(s, "%d mm", sensorThreshold);
    displayRightClear(9, grey, 220, 70, 45, s);  // return to grey after 2 seconds
  }
}

// Show sensor trace
void frameTrace() {
  int columnStart = 1;  // starting column (0-3)
  int columns = 1;      // number of sensor columns to trace (1-4)
  if (frameFirstTime) {
    displayHeading("Reset; Long to Exit", "Trace");
    lcd.fillRect(0, 47, 15, 32, grey);
    displayLeft(9, black, 1, 62, "R");
    displayLeft(9, black, 1, 77, "F");
    lcd.fillRect(0, 90, 15, 32, grey);
    displayLeft(9, black, 1, 105, "R");
    displayLeft(9, black, 1, 120, "F");
    optionsStarted = false;  // inhibit 5-minute option reset
    tracePosition = 18;      // horizontal cursor position
    showTrace = false;       // true when at least one threshold exceeded
    endGroupTrace = false;   // true while thresholds exceeded until no thresholds exceeded
    sensorColumn = 1;        // 0-3
    displayLine = 0;
    sensorTrendInside = 0;
    sensorTrendOutside = 0;
    eventsTraced = 0;
    sensorBitsLast = 0;
    trendSet = false;
    trend = "Unknown";
  }
  if (buttonAction == LONG) {
    exitState();
  } else if (buttonAction == SHORT) {
    buttonAction = NONE;
    frameFirstTime = true;
    return;
  }
  if (tracePosition >= LCD_WIDTH - 20) {
    displayLine++;  // continue on next line (2 lines max)
    tracePosition = 18;
  }
  if (sensorValid && (displayLine < 2)) {
    if (myImager.isDataReady() == true) {
      if (myImager.getRangingData(&measurementData)) {  // get data
        for (int j = columnStart; j < columns + 1; j++) {
          for (int i = 0; i < 4; i++) {
            if (measurementData.distance_mm[j + i * 4] <= sensorThreshold) {  // at least one 4x4 sensors sees something
              showTrace = true;
            }
          }
        }
        if (showTrace) {
          int sensorBits = 0;
          for (int j = columnStart; j < columns + 1; j++) {
            for (int i = 0; i < 4; i++) {
              if (measurementData.distance_mm[j + i * 4] <= sensorThreshold) {
                if (j == sensorColumn) {
                  lcd.fillRect(tracePosition + j, 50 + 7 * i + displayLine * 43, 2, 5, indicatorColor[i]);
                  sensorBits += 1 << 1 * i;
                } else {
                  lcd.fillRect(tracePosition + j, 50 + 7 * i + displayLine * 43, 2, 5, grey);
                }
              } else {
                lcd.fillRect(tracePosition + j, 50 + 7 * i + displayLine * 43, 2, 5, darkgrey);
              }
            }
            tracePosition += 3;
          }
          tracePosition += 5;
          showTrace = false;
          endGroupTrace = true;

          // collect weighting over time (first time only)
          if (eventsTraced == 0) {
            if (sensorBits != sensorBitsLast) {  // ignore duplicates in a row
              if (sensorBitsLast != 0) {         // not firsttime
                //Serial.print("SensorBits: ");
                //Serial.print(sensorBits);
                //Serial.print(", sensorWeight: ");
                //Serial.println(sensorWeights[sensorBits]);
                if (sensorWeights[sensorBits] > sensorWeights[sensorBitsLast]) {
                  sensorTrendInside++;
                } else {
                  sensorTrendOutside++;
                }
              }
              sensorBitsLast = sensorBits;
            }
          }

        } else if (endGroupTrace) {
          lcd.fillRect(tracePosition, 47 + displayLine * 43, 1, 30, purple);  // separator line
          tracePosition += 3;
          endGroupTrace = false;

          // display in/out status (SensorTrendShift values adds weight to outside coming in direction)
          if ((eventsTraced == 0) && ((sensorTrendInside + sensorTrendOutside) > sensorTrendShift)) {
            char s[90] = "";
            if ((sensorTrendInside + sensorTrendShift) >= sensorTrendOutside) {
              trend = "Inside";   // trend going inside from outside
            } else {
              trend = "Outside";  // trend going outside from inside
            }
            snprintf(s, 90, "In: %d; Out: %d; Trend: %s; Shift: %d", sensorTrendInside, sensorTrendOutside, trend.c_str(), sensorTrendShift);
            displayLeft(9, babyblue, 0, 140, s);
            eventsTraced++;
          }
        }
      }
    }
  }
}



void frameHours() {
  if (frameFirstTime) {
    displayHeading("Next", "Hours");
    displayLeft(9, grey, 0, 140, "Long press to toggle 12/24-hour mode");
    displayLeft(12, grey, 0, 80, "Hours mode");
    displayChoice(white, 140, 80, hour24, "12", "24");
    lastDay = -1;
  }
  if (buttonAction == LONG) {
    if (hour24 == 0) hour24 = 1;
    else hour24 = 0;
    displayChoice(white, 140, 80, hour24, "12", "24");

    EEPROM.write(EE_HOUR24, hour24);
    EEPROM.commit();
  } else if (buttonAction == SHORT) {
    exitState();  // exit
  }
}

void frameGmtAdj() {
  if (frameFirstTime) {
    displayHeading("Next", "Time Zone");
    frameFirstTime = true;
    displayLeft(9, grey, 0, 140, "Long press for next zone");
    displayLeft(12, grey, 0, 60, "Zone");
    displayLeft(12, grey, 0, 90, "Time");
    displayLeft(12, grey, 0, 120, "Date");
    lastDay = -1;
  }
  if ((buttonAction == LONG) || frameFirstTime) {
    if (frameFirstTime) {
      // current zone index, 0 to 13
      frameFirstTime = false;
      zoneIndex = (gmtOffset_sec / 3600 + 10) * 2 + daylightOffset_sec / 3600;
    } else {
      // select next index to rotate through the 14 zone/DST combination options
      zoneIndex = (zoneIndex + 1) % 14;
    }

    gmtOffset_sec = ((zoneIndex / 2) - 10) * 3600;
    daylightOffset_sec = (zoneIndex % 2) * 3600;

    // display time zone and dst info
    char buff[30];
    String zoneText = "";
    if ((zoneIndex >= 0) && (zoneIndex <= 13)) {
      zoneText = zoneAbbreivations[zoneIndex / 2];
    }
    snprintf(buff, 30, "%s (%d)", zoneText.c_str(), (zoneIndex / 2) - 10);
    displayLeftClear(12, white, 100, 60, LCD_WIDTH - 100, buff);
    int dst = daylightOffset_sec / 3600;
    if (dst == 1) {
      lcd.print(" w/DST");
    }
    if (buttonAction == LONG) {
      // set new time, and store adjusment preferences
      configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);  // set time from local server, default updates hourly
      getLocalTime(&timeinfo);

      EEPROM.write(EE_GMT_ADJUST, (zoneIndex / 2) - 10);
      EEPROM.write(EE_DST_ADJUST, dst);
      EEPROM.commit();
    }
  } else if (buttonAction == SHORT) {
    lastTime = 0;
    exitState();  // exit
  }
  //display time
  time(&now);
  if (lastTime != now) {
    // update when time changes (every second)
    lastTime = now;
    localtime_r(&now, &timeinfo);
    if (timeinfo.tm_year > 124) {  // 1900+124 = 2024 (if year is 2024 or older, time is invalid)
      String tempText = convertTime(timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
      displayLeftClear(12, grey, 100, 90, LCD_WIDTH - 100, tempText.c_str());
      if (timeinfo.tm_mday != lastDay) {
        char tempChar[30];
        snprintf(tempChar, 30, " %d-%s-%d", timeinfo.tm_mday, monthsOfTheYear[timeinfo.tm_mon], timeinfo.tm_year + 1900);
        displayLeftClear(12, grey, 98, 120, LCD_WIDTH - 100, tempChar);
        lastDay = timeinfo.tm_mday;
      }
    }
  }
}


void frameColor() {
  if (frameFirstTime) {
    displayHeading("Next", "Color");
    displayLeft(9, grey, 0, 140, "Long press to toggle color styles");
    displayLeft(12, outsideColor, 0, 65, "Outside Color");
    displayLeft(12, insideColor, 0, 95, "Inside Color");
  }
  if (buttonAction == LONG) {
    colorType++;
    if (colorType > 5) colorType = 0;
    setColorType();
    displayLeft(12, outsideColor, 0, 65, "Outside Color");
    displayLeft(12, insideColor, 0, 95, "Inside Color");
    buildIndicatorColors();  // build array of indicator colors
    EEPROM.write(EE_COLOR, colorType);
    EEPROM.commit();
  } else if (buttonAction == SHORT) {
    exitState();  // exit
  } else if (buttonAction == VERYLONG) {
    lcdDim = !lcdDim;   // toggle brightness level and redisplay 
    EEPROM.write(EE_DIM, lcdDim);
    EEPROM.commit();
    Serial.print("lcdDim: ");
    Serial.println(lcdDim);
    
    displayHeading("Next", "Color");
    if (lcdDim) {
      displayLeft(9, white, 0, 120, "Brightness level: 7");
    } else {
      displayLeft(9, white, 0, 120, "Brightness level: 10");
    }
    displayLeft(9, grey, 0, 140, "Long press to toggle color styles");
    displayLeft(12, outsideColor, 0, 65, "Outside Color");
    displayLeft(12, insideColor, 0, 95, "Inside Color");
    ShowGif(LCD_WIDTH-124, 2, Pet_Detective_Text_122x17_gif);
  
  }
}

void frameAbout() {
  if (frameFirstTime) {
    displayHeading("End", "About");
    displayLeft(9, berry, 0, 60, "Company");
    displayLeft(9, berry, 100, 60, "FAQware Copyright 2026");
    displayLeft(9, berry, 0, 80, "Location");
    displayLeft(9, berry, 100, 80, "Mountain View, CA");
    displayLeft(9, berry, 0, 100, "Version");
    displayLeft(9, berry, 100, 100, version);
    displayLeft(9, berry, 150, 100, __DATE__);
    displayLeft(9, grey, 0, 140, "10-second press to set factory defaults");
  }

  if (buttonAction == VERYLONG) {
    EEPROM.write(EE_HOUR24, 0);
    EEPROM.write(EE_COLOR, 0);
    EEPROM.write(EE_THRESHOLD_HI, 0);  // this forces an auto-calibrate on the next boot
    EEPROM.write(EE_THRESHOLD_LO, 0);
    EEPROM.write(EE_GMT_ADJUST, 0xF8);  // Time zone -8
    EEPROM.write(EE_DST_ADJUST, 1);     // DST on
    EEPROM.write(EE_DIM, 0);            // full brightness
    for (int i = 0; i < logSize; i++) {
      actionType[i] = 0;
      actionLog[i] = 0;
    }
    EEPROM.writeString(EE_SSID, "");
    EEPROM.writeString(EE_PASSWORD, "");
    EEPROM.commit();

    currentLogPtr = 0;
    displayLeftClear(9, white, 0, 140, LCD_WIDTH, "Reset complete, rebooting...");
    delay(2000);
    ESP.restart();
    ssid = "";       // clear WiFi ssid & password
    password = "";
    convertWiFiInfo();
  } else if (buttonAction == SHORT) {
    exitState();  // exit
    optionsStarted = false;
  }
}

void frameError() {
  if (frameFirstTime) {
    Serial.println("Failed to obtain time");
    displayHeading("exit", "Error");

    if (wifi_timeout > 80) {
      displayCenter(12, red, LCD_WIDTH / 2, 55, "WiFi Failure");
      displayCenter(12, grey, LCD_WIDTH / 2, 88, "Connect failed for SSID:");
      displayCenter(12, grey, LCD_WIDTH / 2, 118, ssidChar);
    } else {
      displayCenter(12, red, LCD_WIDTH / 2, 55, "Time Failure");
      displayCenter(12, grey, LCD_WIDTH / 2, 88, "Unable to get time");
      displayCenter(12, grey, LCD_WIDTH / 2, 118, "from NTP server.");
    }
    displayLeft(9, white, 0, 140, "Operations can continue without time.");
  } else if (buttonAction == SHORT) {
    exitState();  // exit
  }
}

#define LOG_UNUSED 0  // types of log entries stored in actionLog
#define LOG_READY 1
#define LOG_IN 2
#define LOG_OUT 3
#define LOG_BUTTON 4  // debug only


// exit from last option frame and set state based on newest status in/out/ready
void exitState() {
  priorState = state;
  state++;  // walk through option frames
  buttonAction = NONE;
  frameFirstTime = true;
  if (state >= NOMORE) {
    state = READY;        // occurs if no entries in log (for example after a reset)
    int tempPtr = currentLogPtr;
    // find newest
    for (int i = 0; i < logSize; i++) {
      if ((actionType[tempPtr] == LOG_UNUSED) || (actionType[tempPtr] == LOG_READY)) break;  // use Ready state
      if (actionType[tempPtr] == LOG_IN) {
        state = INSIDE;
        break;
      } else if (actionType[tempPtr] == LOG_OUT) {
        state = OUTSIDE;
        break;
      }
      tempPtr = (tempPtr - 1) % logSize;
    }
  }
}

// gets here after every Ready/In/Out frame cycle
void setState() {
  // set state if button pressed for various options
  if (buttonAction == SHORT) {
    debugLastAction = "button setstate";
    priorState = state;
    state = LOG;  // first option frame to start with
    buttonAction = NONE;
    frameFirstTime = true;
  } else {
    //Poll sensor for new data
    if (myImager.isDataReady() == true) {
      if (myImager.getRangingData(&measurementData)) {  //Read distance data into array
        // The ST library returns the data transposed from zone mapping shown in datasheet
        // Pretty-print data with increasing y, decreasing x to reflect reality
        // while data is 4x4, we are only interested in 4 values
        int sensorBits = 0;
        for (int y = 0; y < 4; y++) {
          //Serial.print("\t");
          //Serial.print(measurementData.distance_mm[1 + imageWidth*y]);
          if (measurementData.distance_mm[1 + imageWidth * y] <= sensorThreshold) {
            showIndicator(y * 40 + 2, indicatorColor[y], &indicatorOn[y]);  // if indicator off, turn on
            sensorBits += 1 << 1 * y;
          } else {                                       // did not exceed threshold
            hideIndicator(y * 40 + 2, &indicatorOn[y]);  // if indicator off, turn on
          }
        }
        if (sensorBits != sensorBitsLast) {  // ignore duplicates in a row
          if (sensorBitsLast != 0) {         // not firsttime
            if (sensorWeights[sensorBits] > sensorWeights[sensorBitsLast]) {
              sensorTrendInside++;
            } else {
              sensorTrendOutside++;
            }
          }
          sensorBitsLast = sensorBits;
        }
        if (sensorBits == 0) {  // done collecting trend
          time(&now);
          // only identify trend if more than 6 seconds since last in/out
          if ((actionLog[currentLogPtr]+lockoutSeconds <= now) || 
             ((actionType[currentLogPtr] != LOG_OUT) && (actionType[currentLogPtr] != LOG_IN))) { 
            if ((sensorTrendInside + sensorTrendOutside) > sensorTrendShift) {
              // we have some trend data
              priorState = state;
              if (sensorTrendInside+sensorTrendShift >= sensorTrendOutside) {
                state = INSIDE;
              } else {
                state = OUTSIDE;
              }
              // save current time to log
              localtime_r(&now, &timeinfo);
              if (timeinfo.tm_year > 124) {  // 1900+124 = 2024 (if year is 2024 or older, time is invalid)
                currentLogPtr = (currentLogPtr + 1) % logSize;
                if (state == OUTSIDE) {
                  actionType[currentLogPtr] = LOG_OUT;
                } else {
                  actionType[currentLogPtr] = LOG_IN;
                }
                actionLog[currentLogPtr] = now;
                trendInside[currentLogPtr] = sensorTrendInside;
                trendOutside[currentLogPtr] = sensorTrendOutside;
              }
              frameFirstTime = true;
            }
          }
          sensorTrendInside = 0;
          sensorTrendOutside = 0;
          sensorBitsLast = 0;
        }
      }
    }
    //Serial.println();
  }
}


void setColorType() {
  // four colors per type for outsideColor, outsideColorMed, insideColor, insideColorMed
  int colorCombos[] = { green, medgreen, amber, medamber,
                        amber, medamber, babyblue, medblue,
                        babyblue, medblue, green, medgreen,
                        green, medgreen, babyblue, medblue,
                        amber, medamber, green, medgreen,
                        babyblue, medblue, amber, medamber };

  outsideColor = colorCombos[colorType * 4];
  outsideColorMed = colorCombos[colorType * 4 + 1];
  insideColor = colorCombos[colorType * 4 + 2];
  insideColorMed = colorCombos[colorType * 4 + 3];
}


// return the ptr to the newest log entry that is IN or OUT
//    set newest to 0 for newest, 1 for next to newest In/Out
//    return -1 if none found
int InOutLogPtr(int newest) {
  int tempPtr = currentLogPtr;
  int i;
  for (i = 0; i < logSize; i++) {
    if (actionType[tempPtr] == LOG_UNUSED) {
      return -1;  // no entry found
    }
    if ((actionType[tempPtr] == LOG_IN) || (actionType[tempPtr] == LOG_OUT)) {
      if (newest == 0) break;
      newest = 0;  // skip this first one found and look for the 2nd newest
    }
    tempPtr = (tempPtr - 1) % logSize;
  }
  if (i >= logSize) return -1;    // no match found
  return tempPtr;
}


// scan the log to get the % of the time inside vs. outside
// value -1 indicates not enough log entries to get the percentage
int percentInOut() {
  int newestIndex = InOutLogPtr(0);

  if (newestIndex != -1) {
    long timeInside = 0;
    long timeOutside = 0;
    long priorTime = actionLog[newestIndex];
    int priorType = actionType[newestIndex];
    int tempIndexPtr;
    // squence through newest to oldest log entries (skipping the current entry)
    for (int i = 1; i < logSize; i++) {
      tempIndexPtr = newestIndex - i;
      if (tempIndexPtr < 0) tempIndexPtr += logSize;
      if (tempIndexPtr == newestIndex) break;  // no more entries to search
      if (actionLog[tempIndexPtr] == 0) break;
      if ((actionType[tempIndexPtr] == LOG_OUT) && (priorType == LOG_IN)) {
        timeOutside += actionLog[tempIndexPtr] - priorTime;  // time outside added
      } else if ((actionType[tempIndexPtr] == LOG_IN) && (priorType == LOG_OUT)) {
        timeInside += actionLog[tempIndexPtr] - priorTime;  // time inside added
      } else if ((actionType[tempIndexPtr] == LOG_IN) && (priorType == LOG_IN)) {
        timeInside += actionLog[tempIndexPtr] - priorTime;  // time inside added (mulitple pets or let out from another door)
      } else if ((actionType[tempIndexPtr] == LOG_OUT) && (priorType == LOG_OUT)) {
        timeOutside += actionLog[tempIndexPtr] - priorTime;  // time outside added (mulitple pets or let in from another door)
      }
      priorTime = actionLog[tempIndexPtr];
      priorType = actionType[tempIndexPtr];
    }
    if ((timeInside != 0) && (timeOutside != 0)) {
      return (int)((timeOutside * 100) / (timeInside + timeOutside));
    }
  }
  return -1;
}

// show the left-side indicator when sensor detects distance is less than threshold
//    but only do it once
void showIndicator(int y, int barColor, bool* showing) {
  if (!*showing) {  // show indicator only once while active
    *showing = true;
    lcd.fillRoundRect(0, y, 10, 30, 5, barColor);
  }
}

// if showing on, clear the area and set showing off
void hideIndicator(int y, bool* showing) {
  if (*showing) {  // clear indicator only once
    *showing = false;
    lcd.fillRoundRect(0, y, 10, 30, 5, black);
  }
}


// Create four colors from outside and inside color for the sensor indicators
void buildIndicatorColors() {
  indicatorColor[0] = outsideColor;
  indicatorColor[3] = insideColor;
  // convert RGB565 to 256-bit colors
  uint8_t oRed = ((outsideColor >> 11) & 0x1F) * (255 / 31.0f);
  uint8_t oGreen = ((outsideColor >> 5) & 0x3F) * (255 / 63.0f);
  uint8_t oBlue = (outsideColor & 0x1F) * (255 / 31.0f);
  uint8_t iRed = ((insideColor >> 11) & 0x1F) * (255 / 31.0f);
  uint8_t iGreen = ((insideColor >> 5) & 0x3F) * (255 / 63.0f);
  uint8_t iBlue = (insideColor & 0x1F) * (255 / 31.0f);
  // create two new colors betweeen outside and inside
  uint8_t nRed = oRed * 0.66f + iRed * 0.33f;
  uint8_t nGreen = oGreen * 0.66f + iGreen * 0.33f;
  uint8_t nBlue = oBlue * 0.66f + iBlue * 0.33f;
  indicatorColor[1] = lcd.color565(nRed, nGreen, nBlue);
  nRed = oRed * 0.33f + iRed * 0.66f;
  nGreen = oGreen * 0.33f + iGreen * 0.66f;
  nBlue = oBlue * 0.33f + iBlue * 0.66f;
  indicatorColor[2] = lcd.color565(nRed, nGreen, nBlue);
}


// check if the local network wants the current status (using the IP address)
void processWebRequest() {
  fetchRequest = false;                 // true = webpage request is from JavaScript fetch, otherwise normal page reply
  skipPageUpdate = false;               // set true if a page refresh is required (in response to fetch)
  String tempText;
  char tempChar[25];

  WiFiClient client = server.accept();  // listen for incoming clients
  if (client) {                         // if you get a client,
    Serial.println("new client");       // print a message out the serial port
    String currentLine = "";            // make a String to hold incoming data from the client
    while (client.connected()) {        // loop while the client's connected
      if (client.available()) {         // if there's bytes to read from the client,
        char c = client.read();         // read a byte, then
        Serial.write(c);                // print it out the serial monitor
        if (c == '\n') {                // if the byte is a newline character
          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {
            //HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
            if (fetchRequest) {
              if (skipPageUpdate) {
                client.println("HTTP/1.1 204 No Content");  // no need to update web page, nothing has changed!
                if (debug) Serial.println("No web page update! (204)");
              } else {
                client.println("HTTP/1.1 205 Reset Content");  // data changed so have webpage request new page update
                if (debug) Serial.println("Web page update! (205)");
              }
            } else {
              client.println("HTTP/1.1 200 OK");
              //client.print("ETag: \"");
              //client.print(currentLogPtr);    
              //client.println("\"");
              client.println("Content-type:text/html");
              client.println();
              // the content of the HTTP response follows the header:
              client.print(webPageTop);
              sprintf(tempChar, "%d.%d", currentLogPtr, timeinfo.tm_mday);
              tempText = tempChar;
              //Serial.println(tempText);
              client.print(tempText);// insert log ptr & day into JavaScript for cached updates
              client.print(webPageVersion);
              client.print(webColor);
              client.print(webPageMiddle);
              String lineDisplay;
              if (displayNowAction == "") {
                lineDisplay = "<div class=\"stat-now\">Inactive</div>";
              } else {
                lineDisplay = "<div class=\"stat-now\">" + displayNowAction + "</div>";
              }
              client.print(lineDisplay);
              if (displayCurrentAction != "") {
                lineDisplay = "<div class=\"stat-grey\">" + displayCurrentActionHead + "</div>";
                client.print(lineDisplay);
                lineDisplay = "<div class=\"stat-at\">" + displayCurrentAction + "</div>";
              } else {
                lineDisplay = "<div class=\"stat-grey\">Awaiting User Action</div>";
              }
              client.print(lineDisplay);
              if (displayPriorAction != "") {
                lineDisplay = "<div class=\"stat-grey\">" + displayPriorAction + "</div>";
                client.print(lineDisplay);
              }
              if (displayPercentAction != "") {
                lineDisplay = "<div class=\"stat-grey\">" + displayPercentAction + "</div>";
                client.print(lineDisplay);
              }
              client.print(webPageBottom1);

              if (webShowLog) {
                client.print("<a href=\"?log=n\">Hide Log</a><br>");
              } else {
                client.print("<a href=\"?log=y\">Show Log</a><br>");
              }

              // dislay log data on web page
              if (webShowLog) {
                client.print("<br><div class=\"stat-box log-box\"><div class=\"stat\">Log</div>");

                int tempIndexPtr = 0;
                time_t largestValue = 0;
                int newestIndex = 0;
                for (int j = 0; j < logSize; j++) {
                  if ((actionType[j] != LOG_UNUSED) & (largestValue < actionLog[j])) {
                    newestIndex = j;
                    largestValue = actionLog[j];
                  }
                }

                String inColor = convertRgb565ToRgb888(insideColor);
                String outColor = convertRgb565ToRgb888(outsideColor);
                String readyColor = convertRgb565ToRgb888(magenta);
                client.print("<table><tr><th colspan=\"3\"></th><th colspan=\"2\">Trend</th></tr>");
                client.print("<tr><th>Type</th><th colspan=\"2\">Occurred at</th><th>In</th><th>Out</th></tr>");
                String logType;
                for (int i = 0; i < logSize; i++) {
                  tempIndexPtr = (newestIndex - i);
                  if (tempIndexPtr < 0) tempIndexPtr += logSize;
                  // convert to date and time
                  logType = "";
                  if (actionType[tempIndexPtr] == LOG_READY) logType =  "<span style=\"color: #" + readyColor + "\">Ready</span>";
                  else if (actionType[tempIndexPtr] == LOG_IN) logType = "<span style=\"color: #" + inColor + "\">Inside</span>";
                  else if (actionType[tempIndexPtr] == LOG_OUT) logType = "<span style=\"color: #" + outColor + "\">Outside</span>";
                  else if (actionType[tempIndexPtr] == LOG_BUTTON) logType = "Options";
                  else continue;    // ignore unused entries
                  client.print("<tr><td style=\"text-align: left;\">" + logType + "</td>");

                  if ((actionType[tempIndexPtr] != 0) && ((actionType[tempIndexPtr] == LOG_READY) || (actionType[tempIndexPtr] == LOG_IN) || (actionType[tempIndexPtr] == LOG_OUT) || (debug && (actionType[tempIndexPtr] == LOG_BUTTON)))) {
                    time_t raw_time = actionLog[tempIndexPtr];
                    struct tm* logTime = localtime(&raw_time);
                    if (logTime != nullptr) {
                      tempText = convertTime(logTime->tm_hour, logTime->tm_min, logTime->tm_sec);
                      client.print("<td style=\"text-align: right;\">" + tempText + "</td>");
                      if ((logTime->tm_mday == timeinfo.tm_mday) && (logTime->tm_mon == timeinfo.tm_mon)) {
                        tempText = "Today";
                      } else {
                        sprintf(tempChar, " %d-%s ", logTime->tm_mday, monthsOfTheYear[logTime->tm_mon]);
                        tempText = tempChar;
                      }
                      client.print("<td>" + tempText + "</td>");
                      // include the trend if in/out entry
                      if ((actionType[tempIndexPtr] == LOG_IN) || (actionType[tempIndexPtr] == LOG_OUT)) {
                        sprintf(tempChar, "%d", trendInside[tempIndexPtr]);
                        tempText = tempChar;
                        client.print("<td width=\"30\">" + tempText + "</td>");
                        sprintf(tempChar, "%d", trendOutside[tempIndexPtr]);
                        tempText = tempChar;
                        client.print("<td width=\"30\">" + tempText + "</td>");
                      } else {
                        client.print("<td colspan=\"2\"></td>");
                      }
                    } else {
                      client.print("<td colspan=\"4\"></td>");
                    }
                  } else {
                    client.print("<td colspan=\"4\"></td>");
                  }
                  client.print("</tr>");
                }
                client.print("</table>");
                
                sprintf(tempChar, "%d", sensorThreshold);
                tempText = tempChar;
                client.print("<br>Sensor Threshold: " + tempText);

                sprintf(tempChar, "%d", sensorTrendShift);
                tempText = tempChar;
                client.print("<br>Sensor Trend Shift: " + tempText);

                sprintf(tempChar, "%d", lockoutSeconds);
                tempText = tempChar;
                client.print("<br>Lockout Seconds: " + tempText  + "</div>");
              }
              client.print(webPageBottom2);
            }
            // The HTTP response ends with another blank line:
            client.println();
            // break out of the while loop:
            break;
          } else {  // if you got a newline, then clear currentLine:
            currentLine = "";
          }
        } else if (c != '\r') {  // if you got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
        } else if (currentLine.startsWith("GET /")) {
          String getValue = getQueryParameter(currentLine, "log");
          if (getValue != "") {
            if (getValue == "y") {
              webShowLog = true;
            } else {
              webShowLog = false;
            } 
          }
          getValue = getQueryParameter(currentLine, "ver");
          if (getValue != "") {
            fetchRequest = true;
            sprintf(tempChar, "%d.%d", currentLogPtr, timeinfo.tm_mday);
            String tempString = String(tempChar);
            if (getValue == tempString) {
              // no changes since last web request, so skip update!
              skipPageUpdate = true;
            }
          }
        }        
      }
    }
    // close the connection:
    client.stop();
    if (debug) Serial.println("client disconnected");
  }
}