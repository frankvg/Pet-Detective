/*  Pet Detective, by FAQware

EEPROM and Time Routines

9-Feb-2026 (c) FAQware

*/


// preferences
#define EEPROM_SIZE 512
int hour24;                 // 0 = 12h, 1=24 hour
#define EE_HOUR24       3   // EEPROM address (start at 3)
#define EE_COLOR        4   // address for color type for Inside/Outside 
#define EE_THRESHOLD_HI 5   // saved threshold high byte  (combined zero indicates not yet saved)
#define EE_THRESHOLD_LO 6   // saved threshold low byte
#define EE_GMT_ADJUST   7   // Note - is a positive or negative number.  -8 for PST is 0xF8
#define EE_DST_ADJUST   8   // Set to 0 = DST off, or 1 DST on
#define EE_DIM          9   // Set to 0 = Full Brightness, 1 = Dimmer 

#define EE_SSID        20   // SSID is up to 32 characters
#define EE_PASSWORD    60   // SSID is up to 63 characters

void initEEPROM()
{
  if (debug) Serial.print("EEPROM bytes: ");
  if (debug) Serial.print(EEPROM_SIZE);
  EEPROM.begin(EEPROM_SIZE);      // use 512 bytes for preferences
  if ((EEPROM.read(0) == 0x55) && (EEPROM.read(1) == 0xAA) && (EEPROM.read(2) == 0xF0)) {
    if (debug) Serial.println(", signature valid");
    return;
  }
  EEPROM.write(0, 0x55);  // validation signature
  EEPROM.write(1, 0xAA);
  EEPROM.write(2, 0xF0);
  for (int i = 3 ; i < EEPROM_SIZE ; i++) {
    if (EEPROM.read(i) != 0) EEPROM.write(i, 0);
  }
  EEPROM.commit();
  if (debug) Serial.println(", initialized to zero");
}


// build a time string (if second = -1, ignores seconds)
String convertTime(int hour, int minute, int second) {
    String temp = "";
    String ampm = " am";
    if (hour24 == 0) { 
      if (hour >= 12) {
        ampm = " pm";
        if (hour > 12) {
          hour = hour - 12;
        }
      } else if (hour == 0) hour = 12;  // midnight
    } else if (hour < 10) temp += "0";
    temp += String(hour);
    temp += ":";
    if (minute < 10) temp += "0";
    temp += minute;
    if (second != -1) {
      temp += ":";
      if (second < 10) temp += "0";
      temp += second;
    }
    if (hour24 == 0) {
      temp += ampm;
    }
    return temp;
}

// get value of html GET parameter
String getQueryParameter(String& url, const String& paramName) {
    String key = paramName + "=";
    size_t startPos = url.indexOf(key);
    
    if (startPos == -1) return ""; // Parameter not found

    startPos += key.length();
    size_t endPos = url.indexOf('&', startPos);
    
    if (endPos == -1) {
        endPos = url.indexOf(' ', startPos);
       if (endPos == -1) {
          return url.substring(startPos); // Last parameter
        } else {
          return url.substring(startPos, endPos); // Last parameter, but skip junk after space
        }
    } else {
        return url.substring(startPos, endPos); // Middle parameter
    }
}