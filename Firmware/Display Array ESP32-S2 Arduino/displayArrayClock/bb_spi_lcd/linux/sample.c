//
// SPI LCD test program
// Written by Larry Bank
// demo written for the Waveshare 1.3" 240x240 IPS LCD "Hat"
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "bb_spi_lcd.h"
#include <armbianio.h>

// Pin definitions for Adafruit PiTFT HAT
// GPIO 25 = Pin 22
#define DC_PIN 22
// GPIO 27 = Pin 13
#define RESET_PIN 13
// GPIO 8 = Pin 24
#define CS_PIN 24
// GPIO 24 = Pin 18
#define LED_PIN 18
//#define LCD_TYPE LCD_ILI9341

// Pin definitions for Waveshare 1.4" 240x240 ST7789 HAT
#define LCD_TYPE LCD_ST7789_240
SPILCD lcd;
static uint8_t ucBuffer[4096];

int main(int argc, char *argv[])
{
int i;

	i = AIOInitBoard("Raspberry Pi");
	if (i == 0) // problem
	{
		printf("Error in AIOInit(); check if this board is supported\n");
		return 0;
	}
// int spilcdInit(int iLCDType, int bFlipRGB, int bInvert, int bFlipped, int32_t iSPIFreq, int iCSPin, int iDCPin, int iResetPin, int iLEDPin, int iMISOPin, int iMOSIPin, int iCLKPin);
	i = spilcdInit(&lcd, LCD_TYPE, FLAGS_NONE, 31250000, CS_PIN, DC_PIN, RESET_PIN, LED_PIN, -1,-1,-1);
	if (i == 0)
	{
		spilcdSetTXBuffer(ucBuffer, 4096);
		spilcdSetOrientation(&lcd, LCD_ORIENTATION_90);
		spilcdFill(&lcd, 0xff, DRAW_TO_LCD);
		for (i=0; i<30; i++)
		spilcdWriteString(&lcd, 0,i*8,(char *)"Hello World!", 0x1f,0,FONT_8x8, DRAW_TO_LCD);
		printf("Successfully initialized bb_spi_lcd library\n");
		printf("Press ENTER to quit\n");
		getchar();
		spilcdShutdown(&lcd);
	}
	else
	{
		printf("Unable to initialize the spi_lcd library\n");
	}
   return 0;
} /* main() */
