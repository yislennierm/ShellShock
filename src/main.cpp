#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <wm8960.h>
#include <TFT_eSPI.h>

//Radio headers
#include <WiFi.h>
#include <HTTPClient.h>
#include <esp_wifi.h>
#include "RadioStation.h"
#include "config.h"
// JPEG decoder library
#include <JPEGDecoder.h>





//Display PINS
#define TFT_WIDTH  135 // ST7789 240 x 240 and 240 x 320
#define TFT_HEIGHT 240

#define TFT_BL 4
#define TFT_MISO -1
#define TFT_MOSI 19
#define TFT_SCLK 18
#define TFT_CS   5  // Chip select control pin
#define TFT_DC    16  // Data Command control pin
#define TFT_RST   23  // Reset pin (could connect to RST pin)
//#define TFT_RST  -1  // Set TFT_RST to -1 if display RESET is connected to ESP32 board RST



//RADIO
#define DEBUG false
#define LOGSERVER      "192.168.1.4"
#define UDP_LOGGER
#include <SimpleUDPLogger.h>




WM8960 audio;
//TFT_eSPI display;
#define TFT_GREY 0x5AEB

TFT_eSPI display = TFT_eSPI();       // Invoke custom library




float sx = 0, sy = 1, mx = 1, my = 0, hx = -1, hy = 0;    // Saved H, M, S x & y multipliers
float sdeg=0, mdeg=0, hdeg=0;
uint16_t osx=120, osy=120, omx=120, omy=120, ohx=120, ohy=120;  // Saved H, M, S x & y coords
uint16_t x0=0, x1=0, yy0=0, yy1=0;
uint32_t targetTime = 0;                    // for next 1 second timeout

static uint8_t conv2d(const char* p); // Forward declaration needed for IDE 1.6.x
uint8_t hh=conv2d(__TIME__), mm=conv2d(__TIME__+3), ss=conv2d(__TIME__+6);  // Get H, M, S from compile time

bool initial = 1;


static uint8_t conv2d(const char* p) {
  uint8_t v = 0;
  if ('0' <= *p && *p <= '9')
    v = *p - '0';
  return 10 * v + *++p - '0';
}

void setup() {
  audio.begin();
  display.begin();
  display.setRotation(0);



  //tft.fillScreen(TFT_BLACK);
  //tft.fillScreen(TFT_RED);
  //tft.fillScreen(TFT_GREEN);
  //tft.fillScreen(TFT_BLUE);
  //tft.fillScreen(TFT_BLACK);
  display.fillScreen(TFT_GREY);
  
  display.setTextColor(TFT_WHITE, TFT_GREY);  // Adding a background colour erases previous text automatically
  
  // Draw clock face
  display.fillCircle(120, 120, 118, TFT_GREEN);
  display.fillCircle(120, 120, 110, TFT_BLACK);

  // Draw 12 lines
  for(int i = 0; i<360; i+= 30) {
    sx = cos((i-90)*0.0174532925);
    sy = sin((i-90)*0.0174532925);
    x0 = sx*114+120;
    yy0 = sy*114+120;
    x1 = sx*100+120;
    yy1 = sy*100+120;

    display.drawLine(x0, yy0, x1, yy1, TFT_GREEN);

  // put your setup code here, to run once:
  }
  // Draw 60 dots
  for(int i = 0; i<360; i+= 6) {
    sx = cos((i-90)*0.0174532925);
    sy = sin((i-90)*0.0174532925);
    x0 = sx*102+120;
    yy0 = sy*102+120;
    // Draw minute markers
    display.drawPixel(x0, yy0, TFT_WHITE);
    
    // Draw main quadrant dots
    if(i==0 || i==180) display.fillCircle(x0, yy0, 2, TFT_WHITE);
    if(i==90 || i==270) display.fillCircle(x0, yy0, 2, TFT_WHITE);
  }
   display.fillCircle(120, 121, 3, TFT_WHITE);

  // Draw text at position 120,260 using fonts 4
  // Only font numbers 2,4,6,7 are valid. Font 6 only contains characters [space] 0 1 2 3 4 5 6 7 8 9 : . - a p m
  // Font 7 is a 7 segment font and only contains characters [space] 0 1 2 3 4 5 6 7 8 9 : .
  display .drawCentreString("Time flies",120,260,4);

  targetTime = millis() + 1000; 
}

void loop() {
  if (targetTime < millis()) {
    targetTime += 1000;
    ss++;              // Advance second
    if (ss==60) {
      ss=0;
      mm++;            // Advance minute
      if(mm>59) {
        mm=0;
        hh++;          // Advance hour
        if (hh>23) {
          hh=0;
        }
      }
    }

    // Pre-compute hand degrees, x & y coords for a fast screen update
    sdeg = ss*6;                  // 0-59 -> 0-354
    mdeg = mm*6+sdeg*0.01666667;  // 0-59 -> 0-360 - includes seconds
    hdeg = hh*30+mdeg*0.0833333;  // 0-11 -> 0-360 - includes minutes and seconds
    hx = cos((hdeg-90)*0.0174532925);    
    hy = sin((hdeg-90)*0.0174532925);
    mx = cos((mdeg-90)*0.0174532925);    
    my = sin((mdeg-90)*0.0174532925);
    sx = cos((sdeg-90)*0.0174532925);    
    sy = sin((sdeg-90)*0.0174532925);

    if (ss==0 || initial) {
      initial = 0;
      // Erase hour and minute hand positions every minute
      display.drawLine(ohx, ohy, 120, 121, TFT_BLACK);
      ohx = hx*62+121;    
      ohy = hy*62+121;
      display.drawLine(omx, omy, 120, 121, TFT_BLACK);
      omx = mx*84+120;    
      omy = my*84+121;
    }

      // Redraw new hand positions, hour and minute hands not erased here to avoid flicker
      display.drawLine(osx, osy, 120, 121, TFT_BLACK);
      osx = sx*90+121;    
      osy = sy*90+121;
      display.drawLine(osx, osy, 120, 121, TFT_RED);
      display.drawLine(ohx, ohy, 120, 121, TFT_WHITE);
      display.drawLine(omx, omy, 120, 121, TFT_WHITE);
      display.drawLine(osx, osy, 120, 121, TFT_RED);

    display.fillCircle(120, 121, 3, TFT_RED);
  }// put your main code here, to run repeatedly:
}
