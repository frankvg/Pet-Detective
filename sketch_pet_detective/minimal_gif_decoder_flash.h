/* Minimal GIF Decoder Flash Version - see http://www.technoblogy.com/show?45YI

   David Johnson-Davies - www.technoblogy.com - 15th December 2022
   Modified by FAQware 2026

   CC BY 4.0
   Licensed under a Creative Commons Attribution 4.0 International license:
   http://creativecommons.org/licenses/by/4.0/
*/

typedef struct {
  int16_t rest;
  uint8_t last;
} cell_t;

int16_t x_pos;
int16_t y_pos;
int16_t gif_width;
int16_t gif_height;

int debug_count = 1;

// Global - colour
int fore = 0xFFFF; // White

// Send a byte to the display

uint16_t Colour (int r, int g, int b) {
  // convert to single word RGB565 format
  if (!lcdDim) {
      r = std::min(255, r + (r * 20)/100);
      g = std::min(255, g + (g * 30)/100);  // adjust green a bit more
      b = std::min(255, b + (b * 20)/100);
  }
  return ((r & 0xf8)<<8 | (g & 0xfc)<<3 | b>>3);
}

// Plot point at x,y
void PlotPoint (int x, int y) {
  lcd.drawPixel(x + x_pos, LCD_HEIGHT - y + y_pos, fore);
/*
  if (debug_count < 5) {
    debug_count++;
    lcd.setFont(&FreeSans9pt7b);
    lcd.setTextColor(ST77XX_RED);
    lcd.setCursor(0, 20*debug_count);
    lcd.print("h: ");
    lcd.print(LCD_HEIGHT);
    lcd.print(", Y: ");
    lcd.print(y);
    lcd.print(", y_pos: ");
    lcd.print(y_pos);

  }
*/
}

// GIF Decoder **********************************************

cell_t Table[4096];
uint16_t ColourTable[256];

uint8_t Nbuf;
uint32_t Buf;
int Width, Block;
int Pixel = 0;
int Fileptr = 0;
const uint8_t *File;

uint8_t ReadByte () {
  return pgm_read_byte(&File[Fileptr++]);
}

int GetNBits (int n) {
  while (Nbuf < n) {
    if (Block == 0) Block = ReadByte();
    Buf = ((uint32_t)ReadByte() << Nbuf) | Buf;
    Block--; Nbuf = Nbuf + 8;
  }
  int result = ((1 << n) - 1) & Buf;
  Buf = Buf >> n; Nbuf = Nbuf - n;
  return result;
}

uint8_t FirstPixel (int c) {
  uint8_t last;
  do {
    last = Table[c].last;
    c = Table[c].rest;
  } while (c != -1);
  return last;
}

void PlotSequence (int c) {
  // Measure backtrack
  int i = 0, rest = c;
  while (rest != -1) {
    rest = Table[rest].rest;
    i++;
  }
  // Plot backwards
  Pixel = Pixel + i - 1;
  rest = c;
  while (rest != -1) {
    fore = ColourTable[Table[rest].last];
    PlotPoint (Pixel%Width, LCD_HEIGHT - Pixel/Width - 1);
    Pixel--;
    rest = Table[rest].rest;
  }
  Pixel = Pixel + i + 1;
}

void SkipNBytes (int n) {
  for (int i=0; i<n; i++) ReadByte();
}

boolean Power2 (int x) {
  return (x & (x - 1)) == 0;
}

void OpenFile (const uint8_t *file) {
  Fileptr = 0;
  File = file;
}

void CloseFile () {
}


int ReadInt () {
  return ReadByte() | ReadByte()<<8;
}

void Error (int err) {
  lcd.setTextColor(ST77XX_RED);
  lcd.setCursor(0, 0);
  lcd.print("GIF Error: ");
  if (err == 2) lcd.print("heading wrong.");
  else if (err == 3) lcd.print("transparancy not supported.");
  else if (err == 4) lcd.print("last byte not zero.");
  else lcd.print("unknown.");
  for(;;);  // lock up!
}

// display GIF at location on display from raw bytes of a GIF image
void ShowGif (int16_t x_loc, int16_t y_loc, const uint8_t *filename) {
  x_pos = x_loc;
  y_pos = y_loc;
  OpenFile(filename);
  const char *head = "GIF89a";
  for (int p=0; head[p]; p++) if (ReadByte() != head[p]) Error(2);
  gif_width = ReadInt();
  gif_height = ReadInt(); // Height
  uint8_t field = ReadByte();
  SkipNBytes(2); // background, and aspect
  uint8_t colbits = max(1 + (field & 7), 2);
  int colours = 1<<colbits;
  int clr = colours;
  int end = 1 + colours;
  int free = 1 + end;
  uint8_t bits = 1 + colbits;
  Width = gif_width;
  Pixel = 0;

  // Parse colour table
  for (int c = 0; c<colours; c++) {
    ColourTable[c] = Colour(ReadByte(), ReadByte(), ReadByte());
  }

  // Initialise table
  for (int c = 0; c<colours; c++) {
    Table[c].rest = -1; Table[c].last = c;
  }

  // Parse blocks
  do {
    uint8_t header = ReadByte();
    if (header == 0x2C) { // Image block
      SkipNBytes(8);
      if (ReadByte() != 0) Error(3); // Not interlaced/local
      SkipNBytes(1);
      Nbuf = 0;
      Buf = 0;
      Block = 0;
      boolean stop = false;
      int code = -1, last = -1;
      do {
        last = code;
        code = GetNBits(bits);
        if (code == clr) {
          free = 1 + end;
          bits = 1 + colbits;
          code = -1;
        } else if (code == end) {
          stop = true;
        } else if (last == -1) {
          PlotSequence(code);
        } else if (code < free) {
          Table[free].rest = last;
          Table[free].last = FirstPixel(code);
          PlotSequence(code);
          free++;
          if (Power2(free)) bits++;
        } else if (code == free) {
          Table[free].rest = last;
          Table[free].last = FirstPixel(last);
          PlotSequence(code);
          free++;
          if (Power2(free)) bits++;
        }
      } while(!stop);
      if (ReadByte() != 0) Error(4);
    } else if (header == 0x21) { // Extension block
      SkipNBytes(1); // GCE
      int length = ReadByte();
      SkipNBytes(1 + length);
    } else if (header == 0x3b) { // Terminating byte
      CloseFile();
      return;
    }
  } while (true);
}
