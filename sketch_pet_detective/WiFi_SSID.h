/*  Pet Detective, by FAQware

WiFi SSID/Password Values

You can hard code the SSID and Password here.

If SSID and password are empty, the last saved values in EEPROM are used.  If EEPROM values are empty, 
during the boot process it will ask if you want to enter the WiFi information. This is done through
the IDE when connected via USB to your PC

If running without WiFi, a comma is put in the password field so it will not ask again on later boots.  
This 'bad" comma password is also saved in EEPROM so it will not continue to ask again. Only a factory
reset (in the About menu) will clear this.

*/
String ssid = "";         // up to 32 characters max
String password = "";     // up to 63 characters max

char ssidChar[33];
char passwordChar[64];

void convertWiFiInfo() {
    ssid.toCharArray(ssidChar, ssid.length() + 1);
    password.toCharArray(passwordChar, password.length() + 1);
}