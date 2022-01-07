#include <AnimatedGIF.h>
#include <bb_spi_lcd.h>
#include "rotating_160_480.h"
#include "pattern_160_480.h"


AnimatedGIF gif;
SPILCD lcd[6];
#define DISPLAY_WIDTH 160
#define DISPLAY_HEIGHT 480
static uint8_t ucImage[DISPLAY_WIDTH * DISPLAY_HEIGHT]; // holds fully decoded image

#define BUTTON_A 10
#define BUTTON_B 11
#define BUTTON_C 12
#define BUTTON_D 13

#define CLK_PIN 36
#define MISO_PIN 37
#define MOSI_PIN 35
#define LCD_CS 1
#define LCD_DC 34
#define LCD_LED 37
#define LCD_RESET 33
static uint8_t ucTXBuf[4096];

void GIFDraw(GIFDRAW *pDraw)
{
    uint8_t *s, *d;
    uint16_t *pus, *usPalette, usTemp[160];
    int i, x, y;
    static int iNewLCD, iCurrentLCD = -1;

    usPalette = pDraw->pPalette;
    s = pDraw->pPixels;
    d = &ucImage[pDraw->iX + (pDraw->iY + pDraw->y) * DISPLAY_WIDTH];
    if (pDraw->ucDisposalMethod == 2) // restore to background color
    {
      for (x=0; x<pDraw->iWidth; x++)
      {
        if (s[x] == pDraw->ucTransparent)
           d[x] = pDraw->ucBackground;
      }
      pDraw->ucHasTransparency = 0;
    }
    // Apply the new pixels to the main image
    if (pDraw->ucHasTransparency) // if transparency used
    {
      const uint8_t ucTransparent = pDraw->ucTransparent;
      for (x=0; x<pDraw->iWidth; x++)
      {
        if (s[x] != ucTransparent)
          d[x] = s[x]; // draw non-transparent pixels into the image buffer
      }
    }
    else // no transparency, just copy them all
    {
      memcpy(d, pDraw->pPixels, pDraw->iWidth);
    }
#ifdef HORIZONTAL
    if (pDraw->y == pDraw->iHeight-1) { // last line, draw it to the LCD(s)
      for (i=0; i<6; i++) { // draw each display 1 at a time
        spilcdSetPosition(&lcd[i], 0, 0, 80, 160, DRAW_TO_LCD); // set the entire LCD as the area
        // Translate the 8-bit pixels through the RGB565 palette (already byte reversed)
        for (y=0; y<160; y+=4) { // send 4 lines at a time
          s = &ucImage[(y * DISPLAY_WIDTH) + (i * 80)];        
          for (x=0; x<80; x++) {
            usTemp[x] = usPalette[s[x]];
            usTemp[x+80] = usPalette[s[x+DISPLAY_WIDTH]];
            usTemp[x+160] = usPalette[s[x+DISPLAY_WIDTH*2]];
            usTemp[x+240] = usPalette[s[x+DISPLAY_WIDTH*3]];
          }
          spilcdWriteDataBlock(&lcd[i], (uint8_t *)usTemp, 320*2, DRAW_TO_LCD | DRAW_WITH_DMA);
        } // for y
      } // for i (each display)
    } // if we hit the last line of the frame
#else
// vertical orientation
   y = pDraw->iY + pDraw->y;
   if (pDraw->y == 0) { // set the memory pointer to the right display, and the right line
      int h, StartY;
      iCurrentLCD = y / 80;
      StartY = y % 80;
      h = 80 - StartY; // number of lines until the end of the display
      spilcdSetPosition(&lcd[iCurrentLCD], 0, StartY, 160, h, DRAW_TO_LCD); // set the required area
   } else {    
      iNewLCD = y/80;
      if (iNewLCD != iCurrentLCD) { // need to switch to the next LCD
        iCurrentLCD = iNewLCD;
        spilcdSetPosition(&lcd[iCurrentLCD], 0, 0, 160, 80, DRAW_TO_LCD); // set the entire LCD as the area
      }
   }
   s = &ucImage[(y * DISPLAY_WIDTH)];        
   for (x=0; x<DISPLAY_WIDTH; x++) {
      usTemp[x] = usPalette[s[x]];
   }
   spilcdWriteDataBlock(&lcd[iCurrentLCD], (uint8_t *)usTemp, DISPLAY_WIDTH*2, DRAW_TO_LCD | DRAW_WITH_DMA); // write with DMA
#endif
} /* GIFDraw() */

const uint16_t usColors[] = {0xffff, 0xf81f, 0xf800, 0xffe0, 0x1f, 0x7e0};
void setup() {
  int i;

  Serial.begin(115200);
  delay(500);
  spilcdSetTXBuffer(ucTXBuf, sizeof(ucTXBuf));
  // Set the buttons as inputs
  for (int i=BUTTON_A; i<=BUTTON_D; i++) {
    pinMode(i, INPUT);
  }
  // All reset lines are tied together, so do them all at once
  pinMode(LCD_RESET, OUTPUT);
  digitalWrite(LCD_RESET, HIGH);
  digitalWrite(LCD_RESET, LOW);
  delay(100);
  digitalWrite(LCD_RESET, HIGH);
  
  for (int i=0; i<6; i++) {
     spilcdInit(&lcd[i], LCD_ST7735S_B, FLAGS_INVERT | FLAGS_SWAP_RB, 40000000, LCD_CS+i, LCD_DC, -1, LCD_LED, MISO_PIN, MOSI_PIN, CLK_PIN);
     spilcdFill(&lcd[i], usColors[i], DRAW_TO_LCD);
     spilcdSetOrientation(&lcd[i], LCD_ORIENTATION_90);
  }
  delay(500);
  gif.begin(GIF_PALETTE_RGB565_BE); // big endian pixels
}

void loop() {
  int rc;
//  long lTime;
  int iGIF = 1;
  while (1) {
//  rc = gif.open((uint8_t *)pattern_480_160, sizeof(pattern_480_160), GIFDraw);
//  rc = gif.open((uint8_t *)rotating_pattern_480_160, sizeof(rotating_pattern_480_160), GIFDraw);
  if (iGIF == 0)
    rc = gif.open((uint8_t *)rotating_160_480, sizeof(rotating_160_480), GIFDraw);
  else
    rc = gif.open((uint8_t *)pattern_160_480, sizeof(pattern_160_480), GIFDraw);
  if (rc) {
      Serial.printf("image specs: (%d x %d)\n", gif.getCanvasWidth(), gif.getCanvasHeight());
//      lTime = micros();
        while (rc && digitalRead(BUTTON_A) == HIGH) {
          rc = gif.playFrame(false, NULL);
        }
//      lTime = micros() - lTime;
      gif.close();
      if (digitalRead(BUTTON_A) == LOW) { // switch images
        iGIF ^= 1;
      }
//      Serial.printf("Total display time for all frames = %d us\n", (int)lTime);
  } else {
    char szTemp[64];
    sprintf(szTemp, "rc = %d", gif.getLastError());
    spilcdWriteString(&lcd[0], 0,0,(char *)szTemp, 0xf800, 0xffff, FONT_12x16, DRAW_TO_LCD);
  }
  } // while (1)
} /* loop() */
