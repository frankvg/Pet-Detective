/*  Pet Detective, by FAQware

LCD Display related includes, defines, variables and code

20-Jan-2026 (c) FAQware

*/

#include "ColorFormat.h"
// various display routines

#include <Adafruit_GFX.h>    // Importing the Adafruit_GFX library
#include <Adafruit_ST7789.h> // Import the Adafruit_ST7789 library
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSans18pt7b.h>
#include <Fonts/FreeMono9pt7b.h>
#include "GIF_images.h"     // cats, logos, indicators


//Define the size of the screen
#define LCD_HEIGHT  170     // after rotation!
#define LCD_WIDTH 320       // after rotation!
//Define the pins of the ESP32 connected to the LCD
#define LCD_MOSI 23         // SDA Pin on ESP32 D23
#define LCD_SCLK 18         // SCL Pin on ESP32 D18
#define LCD_CS   15         // Chip select control pin on ESP32 D15
#define LCD_DC    2         // Data Command control pin on ESP32 D2
#define LCD_RST   4         // Reset pin (could connect to RST pin) on ESP32 D4
#define LCD_BLK   32        // Black Light Pin on ESP32 D32
//Create the Adafruit_ST7789 object
Adafruit_ST7789 lcd = Adafruit_ST7789(LCD_CS, LCD_DC, LCD_RST);


void displayLeftClear(int font, int color, int x, int y, int width, const char *buf);
void displayLeft(int font, int color, int x, int y, const char *buf); 
void displayCenter(int font, int color, int x, int y, const char *buf);
void displayText(int color, int x, int y, const char *buf);
void displayChoice(int color, int x, int y, int val, const char *bufFalse, const char *bufTrue);
void displayInOutHeading(int width, const uint8_t *buf);
void displayHeading(const char *lastLine, const char *buf);
void displayRightClear(int font, int color, int x, int y, int width, const char *buf);
void ShowGif (int16_t x_loc, int16_t y_loc, const uint8_t *filename);
void displayZone();
void setTextColorLCD(int color565);

bool lcdDim = false;    // set to true for dimmer colors
int percentIncrease = 40;

String convertRgb565ToRgb888(int color565);

//------------------ Colors for LCD -------------------------------
int black       = 0;
int red         = lcd.color565(220, 0, 0);
int magenta     = lcd.color565(233, 0, 210);
int medmagenta  = lcd.color565(192, 0, 173);
int berry       = lcd.color565(84, 87, 179);  // blue-purple
int lilac       = lcd.color565(107, 111, 232);
int purple      = lcd.color565(128, 0, 255);
int babyblue    = lcd.color565(129, 174, 242);
int medblue     = lcd.color565(108, 148, 209);
int blue        = lcd.color565(0, 0, 255);
int cyan        = lcd.color565(0, 255, 255);
int green       = lcd.color565(0, 215, 36);
int medgreen    = lcd.color565(0, 178, 24);
int lime        = lcd.color565(128, 255, 0);
int yellow      = lcd.color565(240, 240, 0);
int amber       = lcd.color565(255, 180, 0);
int medamber    = lcd.color565(194, 143, 0);
int grey        = lcd.color565(140, 140, 140);
int darkgrey    = lcd.color565(25, 25, 25);
int white       = lcd.color565(255, 255, 255);

int insideColor = amber;
int insideColorMed = medamber;
int outsideColor = green;
int outsideColorMed = medgreen;
int colorType = 0;    // one of 6 color combinations

// psoitioning of items
int center  = 112;    // center point for most text when image on side (IN/OUT/READY)
int heading = center - 122/2 + 5;   // Secondary pet detective centering   
bool frameFirstTime = true;   // within each frame, what is static when firstTime


#define STARTUP      0
#define READY        1
#define OUTSIDE      2
#define INSIDE       3

#define LOG          4    // sequencial menu items
#define WIFI         5
#define SENSOR       6
#define HOURS        7
#define GMTADJ       8
#define COLOR        9
#define ABOUT       10
#define NOMORE      11

#define ERROR       12    // stand-alone functions
#define WIFI_RESET  13 
#define WIFI_SETUP  14 
#define TRACE       15

int state      = 0;
int priorState = 0;           //  used to skip repaint of some graphics when same as before
int lastState  = 0;
int frameCounter    = 0;      // within each frame, counter for refresh of part of display

// Display "Pet Detective" graphic and GIF image on right
void displayInOutHeading(int width, const uint8_t *buf) {
  frameFirstTime = false;
  if ((priorState == READY) || (priorState == INSIDE) || (priorState == OUTSIDE)) {
    // no need for Pet Detective graphic - only blackout other areas
    lcd.fillRect(0, 0, 10, 22, black);    // clear indicator area
    lcd.fillRect(0, 22, LCD_WIDTH-90, LCD_HEIGHT-22, black);    // clear only text area, not graphics
  } else {
    lcd.fillScreen(black);
    ShowGif(center-122/2, 2, Pet_Detective_Text_122x17_gif);
  }
  ShowGif(LCD_WIDTH-width, 0, buf);
}



// Display Title and "Pet Detective" graphic, and bottom line action
// Lastline values: LINENEXT, LINEEND, or LINENONE
void displayHeading(const char *lastLine, const char *buf) {
  frameFirstTime = false;
  if ((priorState == READY) || (priorState == INSIDE) || (priorState == OUTSIDE) || (state == WIFI_SETUP) || (state == ERROR)) {
    lcd.fillScreen(black);
    ShowGif(LCD_WIDTH-124, 2, Pet_Detective_Text_122x17_gif);
  } else {
    // no need for Pet Detective graphic - only blackout other areas
    lcd.fillRect(0, 0, LCD_WIDTH-122, 26, black);    // clear only text area, not graphics
    lcd.fillRect(0, 26, LCD_WIDTH, LCD_HEIGHT-26, black);    // clear only text area, not graphics
  }
  displayLeft(18, purple, 2, 26, buf);

  //ShowGif(LCD_WIDTH/2-61, 0, Pet_Detective_Text_122x17_gif);
  //displayCenter(12, purple, LCD_WIDTH/2, 41, buf);

  if (strlen(lastLine) != 0) {
    displayLeft(9, grey, 0, 160, "Short press for ");
    lcd.print(lastLine);
  }

}


// Set one of three font sizes
void displaySetFont(int font) {
  if (font == 18) {
    lcd.setFont(&FreeSans18pt7b);
  } else if (font == 12) {
    lcd.setFont(&FreeSans12pt7b);
  } else if (font == 9) {
    lcd.setFont(&FreeSans9pt7b);
  } else {
    lcd.setFont(&FreeMono9pt7b);    // -9
  }
}

// Display text left aligned, but clear width first (untested for 18pt font)
void displayLeftClear(int font, int color, int x, int y, int width, const char *buf) {
  int16_t x1, y1;
  uint16_t w, h;
  uint16_t adj = 0;
  setTextColorLCD(color);
  displaySetFont(font);
  if (font > 9) adj = 5;
  lcd.getTextBounds(buf, 0, 0, &x1, &y1, &w, &h);
  lcd.fillRect(x, y-12-adj, width-1, h+3+adj, black);
  lcd.setCursor(x, y);
  lcd.print(buf);
}

// display text left aligned
void displayLeft(int font, int color, int x, int y, const char *buf) {
  displaySetFont(font);
  setTextColorLCD(color);
  lcd.setCursor(x, y);
  lcd.print(buf);
}

// display right aligned, but clear width first (x+width = rightmost pixel)
void displayRightClear(int font, int color, int x, int y, int width, const char *buf) {
  int16_t x1, y1;
  uint16_t w, h;
  displaySetFont(font);
  lcd.getTextBounds(buf, 0, 0, &x1, &y1, &w, &h);
  lcd.fillRect(x+3, y-12, width, h+3, black);
  lcd.setCursor(x+width-w, y);
  setTextColorLCD(color);
  lcd.print(buf);
}

// display right aligned (x = rightmost pixel)
void displayRight(int font, int color, int x, int y, const char *buf) {
  int16_t x1, y1;
  uint16_t w, h;
  displaySetFont(font);
  lcd.getTextBounds(buf, 0, 0, &x1, &y1, &w, &h);
  lcd.setCursor(x-w, y);
  setTextColorLCD(color);
  lcd.print(buf);
}

// display one of two strings based on value, clears area behind longest string first
void displayChoice(int color, int x, int y, int val, const char *bufFalse, const char *bufTrue) {
  setTextColorLCD(black);
  lcd.setCursor(x, y);
  lcd.print(bufFalse);        // paint black to erase
  lcd.setCursor(x, y);
  lcd.print(bufTrue);
  setTextColorLCD(color);
  lcd.setCursor(x, y);
  if (val == 0) {
    lcd.print(bufFalse);
  } else {
    lcd.print(bufTrue);
  }
}

// Display text with existing font. If x and y = -1, append to last postiion
void displayText(int color, int x, int y, const char *buf) {
  setTextColorLCD(color);
  if ((x != -1) && (y != -1)) {
    lcd.setCursor(x, y);
  }
  lcd.print(buf);
}


// Display text centered. Set x to where pixel should be centered
void displayCenter(int font, int color, int x, int y, const char *buf) {
  int16_t x1, y1, xleft;
  uint16_t w, h;
  displaySetFont(font);
  setTextColorLCD(color);
  lcd.getTextBounds(buf, 0, y, &x1, &y1, &w, &h); // Calculate bounds
  xleft = x - w/2;
  if (xleft < 0) xleft = 0;   // too large to fit, reset to left margin
  lcd.setCursor(xleft, y); // Set cursor for horizontal centering
  lcd.print(buf);
}

String convertRgb565ToRgb888(int color565) {
  uint8_t r = ((((color565 >> 11) & 0x1F) * 527) + 23) >> 6;
  uint8_t g = ((((color565 >> 5) & 0x3F) * 259) + 33) >> 6;
  uint8_t b = (((color565 & 0x1F) * 527) + 23) >> 6;
  char s[20];
  snprintf(s, 20, "%02X%02X%02X", r, g, b);
  return String(s);
}

void setTextColorLCD(int color565) {
  if (!lcdDim) {
    if (color565 == grey) {
      lcd.setTextColor(lcd.color565(210, 210, 210));
    } else {
      int r = (color565 >> 11) & 0x1F;
      int g = (color565 >> 5) & 0x3F;
      int b = color565 & 0x1F;
      r = std::min(31, r + (r * percentIncrease)/100);
      g = std::min(63, g + (g * (percentIncrease + 10))/100);  // adjust green a bit more
      b = std::min(31, b + (b * percentIncrease)/100);

      lcd.setTextColor((r << 11) | (g << 5) | b);
    }
  } else {
    lcd.setTextColor(color565);
  }
}
