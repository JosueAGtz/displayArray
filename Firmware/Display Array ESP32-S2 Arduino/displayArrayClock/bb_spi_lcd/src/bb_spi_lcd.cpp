// LCD direct communication using the SPI interface
// Copyright (c) 2017 Larry Bank
// email: bitbank@pobox.com
// Project started 5/15/2017
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//

// The ILITEK LCD display controllers communicate through the SPI interface
// and two GPIO pins to control the RESET, and D/C (data/command)
// control lines. 
//#if defined(ADAFRUIT_PYBADGE_M4_EXPRESS)
//#define SPI SPI1
#include <SPI.h>
//#define SPI mySPI
// MISO, SCK, MOSI
//#endif

#if defined(__SAMD51__)
#define ARDUINO_SAMD_ZERO
#endif

#if defined( ARDUINO_SAMD_ZERO )
#include <SPI.h>
#include "wiring_private.h" // pinPeripheral() function
#include <Arduino.h>
#include <Adafruit_ZeroDMA.h>
#include "utility/dma.h"
#define HAS_DMA

void dma_callback(Adafruit_ZeroDMA *dma);

Adafruit_ZeroDMA myDMA;
ZeroDMAstatus    stat; // DMA status codes returned by some functions
DmacDescriptor *desc;

//SPIClass mySPI (&sercom0, 6, 8, 7, SPI_PAD_2_SCK_3, SERCOM_RX_PAD_1);

#ifdef SEEED_WIO_TERMINAL
SPIClass mySPI(
  &PERIPH_SPI3,         // -> Sercom peripheral
  PIN_SPI3_MISO,    // MISO pin (also digital pin 12)
  PIN_SPI3_SCK,     // SCK pin  (also digital pin 13)
  PIN_SPI3_MOSI,    // MOSI pin (also digital pin 11)
  PAD_SPI3_TX,  // TX pad (MOSI, SCK pads)
  PAD_SPI3_RX); // RX pad (MISO pad)
#else
SPIClass mySPI(
  &PERIPH_SPI,      // -> Sercom peripheral
  PIN_SPI_MISO,     // MISO pin (also digital pin 12)
  PIN_SPI_SCK,      // SCK pin  (also digital pin 13)
  PIN_SPI_MOSI,     // MOSI pin (also digital pin 11)
  PAD_SPI_TX,       // TX pad (MOSI, SCK pads)
  PAD_SPI_RX);      // RX pad (MISO pad)
#endif // WIO
#endif // ARDUINO_SAMD_ZERO

#ifdef _LINUX_
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <math.h>
#include <armbianio.h>
#define false 0
#define true 1
#define PROGMEM
#define memcpy_P memcpy
// convert wire library constants into ArmbianIO values
#define OUTPUT GPIO_OUT
#define INPUT GPIO_IN
#define INPUT_PULLUP GPIO_IN_PULLUP
#define HIGH 1
#define LOW 0
static int iHandle; // SPI handle
#else // Arduino
// Use the default (non DMA) SPI library for boards we don't currently support
#if !defined(HAL_ESP32_HAL_H_) && !defined(__SAMD51__) && !defined(ARDUINO_SAMD_ZERO) && !defined(ARDUINO_ARCH_RP2040)
#define mySPI SPI
#elif defined(ARDUINO_ARCH_RP2040)
MbedSPI *pSPI = new MbedSPI(4,7,6); // use GPIO numbers, not pin #s
#endif

#include <Arduino.h>
#include <SPI.h>
#endif // LINUX

#include <bb_spi_lcd.h>

#if defined(VSPI_HOST) && defined( HAL_ESP32_HAL_H_ ) && !defined( ARDUINO_ESP32C3_DEV )
#define ESP32_DMA
#define HAS_DMA
#endif

#ifdef ESP32_DMA
#include "driver/spi_master.h"
static spi_device_interface_config_t devcfg;
static spi_bus_config_t buscfg;
static int iStarted = 0; // indicates if the master driver has already been initialized
static void spi_post_transfer_callback(spi_transaction_t *t);
// ODROID-GO
//const gpio_num_t SPI_PIN_NUM_MISO = GPIO_NUM_19;
//const gpio_num_t SPI_PIN_NUM_MOSI = GPIO_NUM_23;
//const gpio_num_t SPI_PIN_NUM_CLK  = GPIO_NUM_18;
// M5StickC
//const gpio_num_t SPI_PIN_NUM_MISO = GPIO_NUM_19;
//const gpio_num_t SPI_PIN_NUM_MOSI = GPIO_NUM_15;
//const gpio_num_t SPI_PIN_NUM_CLK  = GPIO_NUM_13;
static spi_transaction_t trans[2];
static spi_device_handle_t spi;
//static TaskHandle_t xTaskToNotify = NULL;
// ESP32 has enough memory to spare 4K
DMA_ATTR uint8_t ucTXBuf[4096]="";
static unsigned char ucRXBuf[4096];
static int iTXBufSize = 4096; // max reasonable size
#else
static int iTXBufSize;
static unsigned char *ucTXBuf;
#ifdef __AVR__
static unsigned char ucRXBuf[512];
#else
static unsigned char ucRXBuf[2048]; // ucRXBuf[4096];
#endif // __AVR__
#endif // !ESP32
#define LCD_DELAY 0xff
#ifdef __AVR__
volatile uint8_t *outDC, *outCS; // port registers for fast I/O
uint8_t bitDC, bitCS; // bit mask for the chosen pins
#endif
#ifdef HAS_DMA
volatile bool transfer_is_done = true; // Done yet?
void spilcdWaitDMA(void);
void spilcdWriteDataDMA(SPILCD *pLCD, int iLen);
#endif
static void myPinWrite(int iPin, int iValue);
//static int32_t iSPISpeed;
//static int iCSPin, iDCPin, iResetPin, iLEDPin; // pin numbers for the GPIO control lines
//static int iScrollOffset; // current scroll amount
//static int iOrientation = LCD_ORIENTATION_0; // default to 'natural' orientation
//static int iLCDType, iLCDFlags;
//static int iOldX=-1, iOldY=-1, iOldCX, iOldCY;
//static int iWidth, iHeight;
//static int iCurrentWidth, iCurrentHeight; // reflects virtual size due to orientation
// User-provided callback for writing data/commands
//static DATACALLBACK pfnDataCallback = NULL;
//static RESETCALLBACK pfnResetCallback = NULL;
// For back buffer support
//static int iScreenPitch, iOffset, iMaxOffset;
//static int iSPIMode;
//static int iColStart, iRowStart, iMemoryX, iMemoryY; // display oddities with smaller LCDs
//static uint8_t *pBackBuffer = NULL;
//static int iWindowX, iWindowY, iCurrentX, iCurrentY;
//static int iWindowCX, iWindowCY;
static int bSetPosition = 0; // flag telling myspiWrite() to ignore data writes to memory
void spilcdWriteCommand(SPILCD *pLCD, unsigned char);
void spilcdWriteCommand16(SPILCD *pLCD, uint16_t us);
static void spilcdWriteData8(SPILCD *pLCD, unsigned char c);
static void spilcdWriteData16(SPILCD *pLCD, unsigned short us, int iFlags);
void spilcdSetPosition(SPILCD *pLCD, int x, int y, int w, int h, int iFlags);
int spilcdFill(SPILCD *pLCD, unsigned short usData, int iFlags);
const unsigned char ucE0_0[] PROGMEM = {0x0F, 0x31, 0x2B, 0x0C, 0x0E, 0x08, 0x4E, 0xF1, 0x37, 0x07, 0x10, 0x03, 0x0E, 0x09, 0x00};
const unsigned char ucE1_0[] PROGMEM = {0x00, 0x0E, 0x14, 0x03, 0x11, 0x07, 0x31, 0xC1, 0x48, 0x08, 0x0F, 0x0C, 0x31, 0x36, 0x0F};
const unsigned char ucE0_1[] PROGMEM = {0x1f, 0x1a, 0x18, 0x0a, 0x0f, 0x06, 0x45, 0x87, 0x32, 0x0a, 0x07, 0x02, 0x07, 0x05, 0x00};
const unsigned char ucE1_1[] PROGMEM = {0x00, 0x25, 0x27, 0x05, 0x10, 0x09, 0x3a, 0x78, 0x4d, 0x05, 0x18, 0x0d, 0x38, 0x3a, 0x1f};

// small (8x8) font
const uint8_t ucFont[]PROGMEM = {
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x06,0x5f,0x5f,0x06,0x00,0x00,
    0x00,0x07,0x07,0x00,0x07,0x07,0x00,0x00,0x14,0x7f,0x7f,0x14,0x7f,0x7f,0x14,0x00,
    0x24,0x2e,0x2a,0x6b,0x6b,0x3a,0x12,0x00,0x46,0x66,0x30,0x18,0x0c,0x66,0x62,0x00,
    0x30,0x7a,0x4f,0x5d,0x37,0x7a,0x48,0x00,0x00,0x04,0x07,0x03,0x00,0x00,0x00,0x00,
    0x00,0x1c,0x3e,0x63,0x41,0x00,0x00,0x00,0x00,0x41,0x63,0x3e,0x1c,0x00,0x00,0x00,
    0x08,0x2a,0x3e,0x1c,0x1c,0x3e,0x2a,0x08,0x00,0x08,0x08,0x3e,0x3e,0x08,0x08,0x00,
    0x00,0x00,0x80,0xe0,0x60,0x00,0x00,0x00,0x00,0x08,0x08,0x08,0x08,0x08,0x08,0x00,
    0x00,0x00,0x00,0x60,0x60,0x00,0x00,0x00,0x60,0x30,0x18,0x0c,0x06,0x03,0x01,0x00,
    0x3e,0x7f,0x59,0x4d,0x47,0x7f,0x3e,0x00,0x40,0x42,0x7f,0x7f,0x40,0x40,0x00,0x00,
    0x62,0x73,0x59,0x49,0x6f,0x66,0x00,0x00,0x22,0x63,0x49,0x49,0x7f,0x36,0x00,0x00,
    0x18,0x1c,0x16,0x53,0x7f,0x7f,0x50,0x00,0x27,0x67,0x45,0x45,0x7d,0x39,0x00,0x00,
    0x3c,0x7e,0x4b,0x49,0x79,0x30,0x00,0x00,0x03,0x03,0x71,0x79,0x0f,0x07,0x00,0x00,
    0x36,0x7f,0x49,0x49,0x7f,0x36,0x00,0x00,0x06,0x4f,0x49,0x69,0x3f,0x1e,0x00,0x00,
    0x00,0x00,0x00,0x66,0x66,0x00,0x00,0x00,0x00,0x00,0x80,0xe6,0x66,0x00,0x00,0x00,
    0x08,0x1c,0x36,0x63,0x41,0x00,0x00,0x00,0x00,0x14,0x14,0x14,0x14,0x14,0x14,0x00,
    0x00,0x41,0x63,0x36,0x1c,0x08,0x00,0x00,0x00,0x02,0x03,0x59,0x5d,0x07,0x02,0x00,
    0x3e,0x7f,0x41,0x5d,0x5d,0x5f,0x0e,0x00,0x7c,0x7e,0x13,0x13,0x7e,0x7c,0x00,0x00,
    0x41,0x7f,0x7f,0x49,0x49,0x7f,0x36,0x00,0x1c,0x3e,0x63,0x41,0x41,0x63,0x22,0x00,
    0x41,0x7f,0x7f,0x41,0x63,0x3e,0x1c,0x00,0x41,0x7f,0x7f,0x49,0x5d,0x41,0x63,0x00,
    0x41,0x7f,0x7f,0x49,0x1d,0x01,0x03,0x00,0x1c,0x3e,0x63,0x41,0x51,0x33,0x72,0x00,
    0x7f,0x7f,0x08,0x08,0x7f,0x7f,0x00,0x00,0x00,0x41,0x7f,0x7f,0x41,0x00,0x00,0x00,
    0x30,0x70,0x40,0x41,0x7f,0x3f,0x01,0x00,0x41,0x7f,0x7f,0x08,0x1c,0x77,0x63,0x00,
    0x41,0x7f,0x7f,0x41,0x40,0x60,0x70,0x00,0x7f,0x7f,0x0e,0x1c,0x0e,0x7f,0x7f,0x00,
    0x7f,0x7f,0x06,0x0c,0x18,0x7f,0x7f,0x00,0x1c,0x3e,0x63,0x41,0x63,0x3e,0x1c,0x00,
    0x41,0x7f,0x7f,0x49,0x09,0x0f,0x06,0x00,0x1e,0x3f,0x21,0x31,0x61,0x7f,0x5e,0x00,
    0x41,0x7f,0x7f,0x09,0x19,0x7f,0x66,0x00,0x26,0x6f,0x4d,0x49,0x59,0x73,0x32,0x00,
    0x03,0x41,0x7f,0x7f,0x41,0x03,0x00,0x00,0x7f,0x7f,0x40,0x40,0x7f,0x7f,0x00,0x00,
    0x1f,0x3f,0x60,0x60,0x3f,0x1f,0x00,0x00,0x3f,0x7f,0x60,0x30,0x60,0x7f,0x3f,0x00,
    0x63,0x77,0x1c,0x08,0x1c,0x77,0x63,0x00,0x07,0x4f,0x78,0x78,0x4f,0x07,0x00,0x00,
    0x47,0x63,0x71,0x59,0x4d,0x67,0x73,0x00,0x00,0x7f,0x7f,0x41,0x41,0x00,0x00,0x00,
    0x01,0x03,0x06,0x0c,0x18,0x30,0x60,0x00,0x00,0x41,0x41,0x7f,0x7f,0x00,0x00,0x00,
    0x08,0x0c,0x06,0x03,0x06,0x0c,0x08,0x00,0x80,0x80,0x80,0x80,0x80,0x80,0x80,0x80,
    0x00,0x00,0x03,0x07,0x04,0x00,0x00,0x00,0x20,0x74,0x54,0x54,0x3c,0x78,0x40,0x00,
    0x41,0x7f,0x3f,0x48,0x48,0x78,0x30,0x00,0x38,0x7c,0x44,0x44,0x6c,0x28,0x00,0x00,
    0x30,0x78,0x48,0x49,0x3f,0x7f,0x40,0x00,0x38,0x7c,0x54,0x54,0x5c,0x18,0x00,0x00,
    0x48,0x7e,0x7f,0x49,0x03,0x06,0x00,0x00,0x98,0xbc,0xa4,0xa4,0xf8,0x7c,0x04,0x00,
    0x41,0x7f,0x7f,0x08,0x04,0x7c,0x78,0x00,0x00,0x44,0x7d,0x7d,0x40,0x00,0x00,0x00,
    0x60,0xe0,0x80,0x84,0xfd,0x7d,0x00,0x00,0x41,0x7f,0x7f,0x10,0x38,0x6c,0x44,0x00,
    0x00,0x41,0x7f,0x7f,0x40,0x00,0x00,0x00,0x7c,0x7c,0x18,0x78,0x1c,0x7c,0x78,0x00,
    0x7c,0x78,0x04,0x04,0x7c,0x78,0x00,0x00,0x38,0x7c,0x44,0x44,0x7c,0x38,0x00,0x00,
    0x84,0xfc,0xf8,0xa4,0x24,0x3c,0x18,0x00,0x18,0x3c,0x24,0xa4,0xf8,0xfc,0x84,0x00,
    0x44,0x7c,0x78,0x4c,0x04,0x0c,0x18,0x00,0x48,0x5c,0x54,0x74,0x64,0x24,0x00,0x00,
    0x04,0x04,0x3e,0x7f,0x44,0x24,0x00,0x00,0x3c,0x7c,0x40,0x40,0x3c,0x7c,0x40,0x00,
    0x1c,0x3c,0x60,0x60,0x3c,0x1c,0x00,0x00,0x3c,0x7c,0x60,0x30,0x60,0x7c,0x3c,0x00,
    0x44,0x6c,0x38,0x10,0x38,0x6c,0x44,0x00,0x9c,0xbc,0xa0,0xa0,0xfc,0x7c,0x00,0x00,
    0x4c,0x64,0x74,0x5c,0x4c,0x64,0x00,0x00,0x08,0x08,0x3e,0x77,0x41,0x41,0x00,0x00,
    0x00,0x00,0x00,0x77,0x77,0x00,0x00,0x00,0x41,0x41,0x77,0x3e,0x08,0x08,0x00,0x00,
    0x02,0x03,0x01,0x03,0x02,0x03,0x01,0x00,0x70,0x78,0x4c,0x46,0x4c,0x78,0x70,0x00};
// AVR MCUs have very little memory; save 6K of FLASH by stretching the 'normal'
// font instead of using this large font
#ifndef __AVR__
const uint8_t ucBigFont[]PROGMEM = {
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0xfc,0xfc,0xff,0xff,0xff,0xff,0xfc,0xfc,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x3f,0x3f,0x3f,0x3f,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x0f,0x0f,0x0f,0x0f,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0xc0,0xc0,0xc0,0xc0,0x00,0x00,0x00,0x00,0xc0,0xc0,0xc0,0xc0,0x00,0x00,
    0x00,0x00,0x0f,0x0f,0x3f,0x3f,0x00,0x00,0x00,0x00,0x3f,0x3f,0x0f,0x0f,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0xc0,0xc0,0xfc,0xfc,0xfc,0xfc,0xc0,0xc0,0xfc,0xfc,0xfc,0xfc,0xc0,0xc0,0x00,0x00,
    0xc0,0xc0,0xff,0xff,0xff,0xff,0xc0,0xc0,0xff,0xff,0xff,0xff,0xc0,0xc0,0x00,0x00,
    0x00,0x00,0x0f,0x0f,0x0f,0x0f,0x00,0x00,0x0f,0x0f,0x0f,0x0f,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0xf0,0xf0,0xf0,0xf0,0x00,0x00,0x00,0x00,0x00,0x00,
    0xfc,0xfc,0xff,0xff,0x03,0x03,0x03,0x03,0x03,0x03,0x0f,0x0f,0x3c,0x3c,0x00,0x00,
    0xf0,0xf0,0xc3,0xc3,0x03,0x03,0x03,0x03,0x03,0x03,0xff,0xff,0xfc,0xfc,0x00,0x00,
    0x00,0x00,0x03,0x03,0x03,0x03,0x3f,0x3f,0x3f,0x3f,0x03,0x03,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0xf0,0xf0,0xf0,0xf0,0x00,0x00,0x00,0x00,0x00,0x00,0xc0,0xc0,0xf0,0xf0,0x00,0x00,
    0x00,0x00,0xc0,0xc0,0xf0,0xf0,0x3c,0x3c,0x0f,0x0f,0x03,0x03,0x00,0x00,0x00,0x00,
    0x0f,0x0f,0x03,0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x0f,0x0f,0x0f,0x0f,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x3c,0x3c,0xff,0xff,0xc3,0xc3,0xff,0xff,0x3c,0x3c,0x00,0x00,0x00,0x00,
    0xfc,0xfc,0xff,0xff,0x03,0x03,0x0f,0x0f,0xfc,0xfc,0xff,0xff,0x03,0x03,0x00,0x00,
    0x03,0x03,0x0f,0x0f,0x0c,0x0c,0x0c,0x0c,0x03,0x03,0x0f,0x0f,0x0c,0x0c,0x00,0x00,
    0x00,0x00,0x00,0x00,0xc0,0xc0,0xc0,0xc0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x30,0x30,0x3f,0x3f,0x0f,0x0f,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0xf0,0xf0,0xfc,0xfc,0x0f,0x0f,0x03,0x03,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0xff,0xff,0xff,0xff,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x03,0x03,0x0f,0x0f,0x0c,0x0c,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x03,0x03,0x0f,0x0f,0xfc,0xfc,0xf0,0xf0,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xff,0xff,0xff,0xff,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x0c,0x0c,0x0f,0x0f,0x03,0x03,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0xc0,0xc0,0xc0,0xc0,0x00,0x00,0x00,0x00,0xc0,0xc0,0xc0,0xc0,0x00,0x00,
    0x0c,0x0c,0xcc,0xcc,0xff,0xff,0x3f,0x3f,0x3f,0x3f,0xff,0xff,0xcc,0xcc,0x0c,0x0c,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0xc0,0xc0,0xc0,0xc0,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x0c,0x0c,0x0c,0x0c,0xff,0xff,0xff,0xff,0x0c,0x0c,0x0c,0x0c,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0xc0,0xc0,0xc0,0xc0,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x30,0x30,0x3f,0x3f,0x0f,0x0f,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x0c,0x0c,0x0c,0x0c,0x0c,0x0c,0x0c,0x0c,0x0c,0x0c,0x0c,0x0c,0x0c,0x0c,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x0f,0x0f,0x0f,0x0f,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xc0,0xc0,0xf0,0xf0,0x00,0x00,
    0x00,0x00,0xc0,0xc0,0xf0,0xf0,0x3c,0x3c,0x0f,0x0f,0x03,0x03,0x00,0x00,0x00,0x00,
    0x0f,0x0f,0x03,0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0xfc,0xfc,0xff,0xff,0x03,0x03,0x03,0x03,0xc3,0xc3,0xff,0xff,0xfc,0xfc,0x00,0x00,
    0xff,0xff,0xff,0xff,0x30,0x30,0x0f,0x0f,0x00,0x00,0xff,0xff,0xff,0xff,0x00,0x00,
    0x03,0x03,0x0f,0x0f,0x0c,0x0c,0x0c,0x0c,0x0c,0x0c,0x0f,0x0f,0x03,0x03,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x30,0x30,0x3c,0x3c,0xff,0xff,0xff,0xff,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0xff,0xff,0xff,0xff,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x0c,0x0c,0x0c,0x0c,0x0f,0x0f,0x0f,0x0f,0x0c,0x0c,0x0c,0x0c,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x0c,0x0c,0x0f,0x0f,0x03,0x03,0x03,0x03,0xc3,0xc3,0xff,0xff,0x3c,0x3c,0x00,0x00,
    0xc0,0xc0,0xf0,0xf0,0x3c,0x3c,0x0f,0x0f,0x03,0x03,0x00,0x00,0x00,0x00,0x00,0x00,
    0x0f,0x0f,0x0f,0x0f,0x0c,0x0c,0x0c,0x0c,0x0c,0x0c,0x0f,0x0f,0x0f,0x0f,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x0c,0x0c,0x0f,0x0f,0x03,0x03,0x03,0x03,0x03,0x03,0xff,0xff,0xfc,0xfc,0x00,0x00,
    0x00,0x00,0x00,0x00,0x03,0x03,0x03,0x03,0x03,0x03,0xff,0xff,0xfc,0xfc,0x00,0x00,
    0x03,0x03,0x0f,0x0f,0x0c,0x0c,0x0c,0x0c,0x0c,0x0c,0x0f,0x0f,0x03,0x03,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0xc0,0xc0,0xf0,0xf0,0x3c,0x3c,0xff,0xff,0xff,0xff,0x00,0x00,0x00,0x00,
    0x0f,0x0f,0x0f,0x0f,0x0c,0x0c,0x0c,0x0c,0xff,0xff,0xff,0xff,0x0c,0x0c,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x0c,0x0c,0x0f,0x0f,0x0f,0x0f,0x0c,0x0c,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0xff,0xff,0xff,0xff,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x00,0x00,
    0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x0f,0x0f,0xff,0xff,0xfc,0xfc,0x00,0x00,
    0x03,0x03,0x0f,0x0f,0x0c,0x0c,0x0c,0x0c,0x0c,0x0c,0x0f,0x0f,0x03,0x03,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0xf0,0xf0,0xfc,0xfc,0x0f,0x0f,0x03,0x03,0x03,0x03,0x00,0x00,0x00,0x00,0x00,0x00,
    0xff,0xff,0xff,0xff,0x03,0x03,0x03,0x03,0x03,0x03,0xff,0xff,0xfc,0xfc,0x00,0x00,
    0x03,0x03,0x0f,0x0f,0x0c,0x0c,0x0c,0x0c,0x0c,0x0c,0x0f,0x0f,0x03,0x03,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x0f,0x0f,0x0f,0x0f,0x03,0x03,0x03,0x03,0x03,0x03,0xff,0xff,0xff,0xff,0x00,0x00,
    0x00,0x00,0x00,0x00,0xf0,0xf0,0xfc,0xfc,0x0f,0x0f,0x03,0x03,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x0f,0x0f,0x0f,0x0f,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0xfc,0xfc,0xff,0xff,0x03,0x03,0x03,0x03,0x03,0x03,0xff,0xff,0xfc,0xfc,0x00,0x00,
    0xfc,0xfc,0xff,0xff,0x03,0x03,0x03,0x03,0x03,0x03,0xff,0xff,0xfc,0xfc,0x00,0x00,
    0x03,0x03,0x0f,0x0f,0x0c,0x0c,0x0c,0x0c,0x0c,0x0c,0x0f,0x0f,0x03,0x03,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0xfc,0xfc,0xff,0xff,0x03,0x03,0x03,0x03,0x03,0x03,0xff,0xff,0xfc,0xfc,0x00,0x00,
    0x00,0x00,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0xff,0xff,0xff,0xff,0x00,0x00,
    0x00,0x00,0x0c,0x0c,0x0c,0x0c,0x0c,0x0c,0x0f,0x0f,0x03,0x03,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0xf0,0xf0,0xf0,0xf0,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0xc0,0xc0,0xc0,0xc0,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x03,0x03,0x03,0x03,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0xf0,0xf0,0xf0,0xf0,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0xc0,0xc0,0xc0,0xc0,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x0c,0x0c,0x0f,0x0f,0x03,0x03,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0xc0,0xc0,0xf0,0xf0,0x3c,0x3c,0x0c,0x0c,0x00,0x00,
    0x00,0x00,0x0c,0x0c,0x3f,0x3f,0xf3,0xf3,0xc0,0xc0,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x03,0x03,0x0f,0x0f,0x0c,0x0c,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0xc3,0xc3,0xc3,0xc3,0xc3,0xc3,0xc3,0xc3,0xc3,0xc3,0xc3,0xc3,0xc3,0xc3,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x0c,0x0c,0x3c,0x3c,0xf0,0xf0,0xc0,0xc0,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0xc0,0xc0,0xf3,0xf3,0x3f,0x3f,0x0c,0x0c,0x00,0x00,
    0x00,0x00,0x0c,0x0c,0x0f,0x0f,0x03,0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x3c,0x3c,0x3f,0x3f,0x03,0x03,0x03,0x03,0xc3,0xc3,0xff,0xff,0x3c,0x3c,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x3f,0x3f,0x3f,0x3f,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x0f,0x0f,0x0f,0x0f,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0xf0,0xf0,0xfc,0xfc,0x0c,0x0c,0x0c,0x0c,0x0c,0x0c,0xfc,0xfc,0xf0,0xf0,0x00,0x00,
    0xff,0xff,0xff,0xff,0x00,0x00,0xff,0xff,0xff,0xff,0xff,0xff,0x3f,0x3f,0x00,0x00,
    0x03,0x03,0x0f,0x0f,0x0c,0x0c,0x0c,0x0c,0x0c,0x0c,0x0c,0x0c,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0xc0,0xc0,0xf0,0xf0,0x3c,0x3c,0x0f,0x0f,0x3c,0x3c,0xf0,0xf0,0xc0,0xc0,0x00,0x00,
    0xff,0xff,0xff,0xff,0x0c,0x0c,0x0c,0x0c,0x0c,0x0c,0xff,0xff,0xff,0xff,0x00,0x00,
    0x0f,0x0f,0x0f,0x0f,0x00,0x00,0x00,0x00,0x00,0x00,0x0f,0x0f,0x0f,0x0f,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x03,0x03,0xff,0xff,0xff,0xff,0x03,0x03,0x03,0x03,0xff,0xff,0xfc,0xfc,0x00,0x00,
    0x00,0x00,0xff,0xff,0xff,0xff,0x03,0x03,0x03,0x03,0xff,0xff,0xfc,0xfc,0x00,0x00,
    0x0c,0x0c,0x0f,0x0f,0x0f,0x0f,0x0c,0x0c,0x0c,0x0c,0x0f,0x0f,0x03,0x03,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0xf0,0xf0,0xfc,0xfc,0x0f,0x0f,0x03,0x03,0x03,0x03,0x0f,0x0f,0x3c,0x3c,0x00,0x00,
    0xff,0xff,0xff,0xff,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xc0,0xc0,0x00,0x00,
    0x00,0x00,0x03,0x03,0x0f,0x0f,0x0c,0x0c,0x0c,0x0c,0x0f,0x0f,0x03,0x03,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x03,0x03,0xff,0xff,0xff,0xff,0x03,0x03,0x0f,0x0f,0xfc,0xfc,0xf0,0xf0,0x00,0x00,
    0x00,0x00,0xff,0xff,0xff,0xff,0x00,0x00,0x00,0x00,0xff,0xff,0xff,0xff,0x00,0x00,
    0x0c,0x0c,0x0f,0x0f,0x0f,0x0f,0x0c,0x0c,0x0f,0x0f,0x03,0x03,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x03,0x03,0xff,0xff,0xff,0xff,0x03,0x03,0xc3,0xc3,0x0f,0x0f,0x3f,0x3f,0x00,0x00,
    0x00,0x00,0xff,0xff,0xff,0xff,0x03,0x03,0x0f,0x0f,0x00,0x00,0xc0,0xc0,0x00,0x00,
    0x0c,0x0c,0x0f,0x0f,0x0f,0x0f,0x0c,0x0c,0x0c,0x0c,0x0f,0x0f,0x0f,0x0f,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x03,0x03,0xff,0xff,0xff,0xff,0x03,0x03,0xc3,0xc3,0x0f,0x0f,0x3f,0x3f,0x00,0x00,
    0x00,0x00,0xff,0xff,0xff,0xff,0x03,0x03,0x0f,0x0f,0x00,0x00,0x00,0x00,0x00,0x00,
    0x0c,0x0c,0x0f,0x0f,0x0f,0x0f,0x0c,0x0c,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0xf0,0xf0,0xfc,0xfc,0x0f,0x0f,0x03,0x03,0x03,0x03,0x0f,0x0f,0x3c,0x3c,0x00,0x00,
    0xff,0xff,0xff,0xff,0x00,0x00,0x0c,0x0c,0x0c,0x0c,0xfc,0xfc,0xfc,0xfc,0x00,0x00,
    0x00,0x00,0x03,0x03,0x0f,0x0f,0x0c,0x0c,0x0c,0x0c,0x03,0x03,0x0f,0x0f,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0xff,0xff,0xff,0xff,0x00,0x00,0x00,0x00,0x00,0x00,0xff,0xff,0xff,0xff,0x00,0x00,
    0xff,0xff,0xff,0xff,0x03,0x03,0x03,0x03,0x03,0x03,0xff,0xff,0xff,0xff,0x00,0x00,
    0x0f,0x0f,0x0f,0x0f,0x00,0x00,0x00,0x00,0x00,0x00,0x0f,0x0f,0x0f,0x0f,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x03,0x03,0xff,0xff,0xff,0xff,0x03,0x03,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0xff,0xff,0xff,0xff,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x0c,0x0c,0x0f,0x0f,0x0f,0x0f,0x0c,0x0c,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x03,0x03,0xff,0xff,0xff,0xff,0x03,0x03,0x00,0x00,
    0xf0,0xf0,0xf0,0xf0,0x00,0x00,0x00,0x00,0xff,0xff,0xff,0xff,0x00,0x00,0x00,0x00,
    0x03,0x03,0x0f,0x0f,0x0c,0x0c,0x0c,0x0c,0x0f,0x0f,0x03,0x03,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x03,0x03,0xff,0xff,0xff,0xff,0x00,0x00,0xf0,0xf0,0xff,0xff,0x0f,0x0f,0x00,0x00,
    0x00,0x00,0xff,0xff,0xff,0xff,0x0f,0x0f,0x3f,0x3f,0xf0,0xf0,0xc0,0xc0,0x00,0x00,
    0x0c,0x0c,0x0f,0x0f,0x0f,0x0f,0x00,0x00,0x00,0x00,0x0f,0x0f,0x0f,0x0f,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x03,0x03,0xff,0xff,0xff,0xff,0x03,0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0xff,0xff,0xff,0xff,0x00,0x00,0x00,0x00,0x00,0x00,0xc0,0xc0,0x00,0x00,
    0x0c,0x0c,0x0f,0x0f,0x0f,0x0f,0x0c,0x0c,0x0c,0x0c,0x0f,0x0f,0x0f,0x0f,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0xff,0xff,0xff,0xff,0xfc,0xfc,0xf0,0xf0,0xfc,0xfc,0xff,0xff,0xff,0xff,0x00,0x00,
    0xff,0xff,0xff,0xff,0x00,0x00,0x03,0x03,0x00,0x00,0xff,0xff,0xff,0xff,0x00,0x00,
    0x0f,0x0f,0x0f,0x0f,0x00,0x00,0x00,0x00,0x00,0x00,0x0f,0x0f,0x0f,0x0f,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0xff,0xff,0xff,0xff,0xfc,0xfc,0xf0,0xf0,0xc0,0xc0,0xff,0xff,0xff,0xff,0x00,0x00,
    0xff,0xff,0xff,0xff,0x00,0x00,0x03,0x03,0x0f,0x0f,0xff,0xff,0xff,0xff,0x00,0x00,
    0x0f,0x0f,0x0f,0x0f,0x00,0x00,0x00,0x00,0x00,0x00,0x0f,0x0f,0x0f,0x0f,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0xf0,0xf0,0xfc,0xfc,0x0f,0x0f,0x03,0x03,0x0f,0x0f,0xfc,0xfc,0xf0,0xf0,0x00,0x00,
    0xff,0xff,0xff,0xff,0x00,0x00,0x00,0x00,0x00,0x00,0xff,0xff,0xff,0xff,0x00,0x00,
    0x00,0x00,0x03,0x03,0x0f,0x0f,0x0c,0x0c,0x0f,0x0f,0x03,0x03,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x03,0x03,0xff,0xff,0xff,0xff,0x03,0x03,0x03,0x03,0xff,0xff,0xfc,0xfc,0x00,0x00,
    0x00,0x00,0xff,0xff,0xff,0xff,0x03,0x03,0x03,0x03,0x03,0x03,0x00,0x00,0x00,0x00,
    0x0c,0x0c,0x0f,0x0f,0x0f,0x0f,0x0c,0x0c,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0xfc,0xfc,0xff,0xff,0x03,0x03,0x03,0x03,0x03,0x03,0xff,0xff,0xfc,0xfc,0x00,0x00,
    0xff,0xff,0xff,0xff,0x00,0x00,0xc0,0xc0,0x00,0x00,0xff,0xff,0xff,0xff,0x00,0x00,
    0x03,0x03,0x0f,0x0f,0x0c,0x0c,0x0f,0x0f,0xff,0xff,0xff,0xff,0xc3,0xc3,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x03,0x03,0xff,0xff,0xff,0xff,0x03,0x03,0x03,0x03,0xff,0xff,0xfc,0xfc,0x00,0x00,
    0x00,0x00,0xff,0xff,0xff,0xff,0x03,0x03,0x0f,0x0f,0xff,0xff,0xf0,0xf0,0x00,0x00,
    0x0c,0x0c,0x0f,0x0f,0x0f,0x0f,0x00,0x00,0x00,0x00,0x0f,0x0f,0x0f,0x0f,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x3c,0x3c,0xff,0xff,0xc3,0xc3,0x03,0x03,0x03,0x03,0x3f,0x3f,0x3c,0x3c,0x00,0x00,
    0xc0,0xc0,0xc0,0xc0,0x03,0x03,0x03,0x03,0x0f,0x0f,0xfc,0xfc,0xf0,0xf0,0x00,0x00,
    0x03,0x03,0x0f,0x0f,0x0c,0x0c,0x0c,0x0c,0x0c,0x0c,0x0f,0x0f,0x03,0x03,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x3f,0x3f,0x0f,0x0f,0xff,0xff,0xff,0xff,0x0f,0x0f,0x3f,0x3f,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0xff,0xff,0xff,0xff,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x0c,0x0c,0x0f,0x0f,0x0f,0x0f,0x0c,0x0c,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0xff,0xff,0xff,0xff,0x00,0x00,0x00,0x00,0x00,0x00,0xff,0xff,0xff,0xff,0x00,0x00,
    0xff,0xff,0xff,0xff,0x00,0x00,0x00,0x00,0x00,0x00,0xff,0xff,0xff,0xff,0x00,0x00,
    0x03,0x03,0x0f,0x0f,0x0c,0x0c,0x0c,0x0c,0x0c,0x0c,0x0f,0x0f,0x03,0x03,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0xff,0xff,0xff,0xff,0x00,0x00,0x00,0x00,0x00,0x00,0xff,0xff,0xff,0xff,0x00,0x00,
    0x3f,0x3f,0xff,0xff,0xc0,0xc0,0x00,0x00,0xc0,0xc0,0xff,0xff,0x3f,0x3f,0x00,0x00,
    0x00,0x00,0x00,0x00,0x03,0x03,0x0f,0x0f,0x03,0x03,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0xff,0xff,0xff,0xff,0x00,0x00,0x00,0x00,0x00,0x00,0xff,0xff,0xff,0xff,0x00,0x00,
    0xff,0xff,0xff,0xff,0xc0,0xc0,0xfc,0xfc,0xc0,0xc0,0xff,0xff,0xff,0xff,0x00,0x00,
    0x00,0x00,0x0f,0x0f,0x0f,0x0f,0x00,0x00,0x0f,0x0f,0x0f,0x0f,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x0f,0x0f,0xff,0xff,0xf0,0xf0,0x00,0x00,0xf0,0xf0,0xff,0xff,0x0f,0x0f,0x00,0x00,
    0x00,0x00,0xf0,0xf0,0xff,0xff,0x0f,0x0f,0xff,0xff,0xf0,0xf0,0x00,0x00,0x00,0x00,
    0x0f,0x0f,0x0f,0x0f,0x00,0x00,0x00,0x00,0x00,0x00,0x0f,0x0f,0x0f,0x0f,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0xff,0xff,0xff,0xff,0x00,0x00,0x00,0x00,0xff,0xff,0xff,0xff,0x00,0x00,
    0x00,0x00,0x00,0x00,0x03,0x03,0xff,0xff,0xff,0xff,0x03,0x03,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x0c,0x0c,0x0f,0x0f,0x0f,0x0f,0x0c,0x0c,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x3f,0x3f,0x0f,0x0f,0x03,0x03,0x03,0x03,0xc3,0xc3,0xff,0xff,0x3f,0x3f,0x00,0x00,
    0xc0,0xc0,0xf0,0xf0,0x3c,0x3c,0x0f,0x0f,0x03,0x03,0x00,0x00,0xc0,0xc0,0x00,0x00,
    0x0f,0x0f,0x0f,0x0f,0x0c,0x0c,0x0c,0x0c,0x0c,0x0c,0x0f,0x0f,0x0f,0x0f,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0xff,0xff,0xff,0xff,0x03,0x03,0x03,0x03,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0xff,0xff,0xff,0xff,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x0f,0x0f,0x0f,0x0f,0x0c,0x0c,0x0c,0x0c,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0xfc,0xfc,0xf0,0xf0,0xc0,0xc0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x03,0x03,0x0f,0x0f,0x3f,0x3f,0xfc,0xfc,0xf0,0xf0,0xc0,0xc0,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x03,0x03,0x0f,0x0f,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x03,0x03,0x03,0x03,0xff,0xff,0xff,0xff,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xff,0xff,0xff,0xff,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x0c,0x0c,0x0c,0x0c,0x0f,0x0f,0x0f,0x0f,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0xc0,0xc0,0xf0,0xf0,0xc0,0xc0,0x00,0x00,0x00,0x00,0x00,0x00,
    0x0c,0x0c,0x0f,0x0f,0x03,0x03,0x00,0x00,0x03,0x03,0x0f,0x0f,0x0c,0x0c,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0xc0,0xc0,0xc0,0xc0,0xc0,0xc0,0xc0,0xc0,0xc0,0xc0,0xc0,0xc0,0xc0,0xc0,0xc0,0xc0,
    0x00,0x00,0x00,0x00,0xf0,0xf0,0xf0,0xf0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x03,0x03,0x03,0x03,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0xc0,0xc0,0xc0,0xc0,0xc0,0xc0,0xc0,0xc0,0x00,0x00,0x00,0x00,0x00,0x00,
    0xf0,0xf0,0xfc,0xfc,0x0c,0x0c,0x0c,0x0c,0xff,0xff,0xff,0xff,0x00,0x00,0x00,0x00,
    0x03,0x03,0x0f,0x0f,0x0c,0x0c,0x0c,0x0c,0x03,0x03,0x0f,0x0f,0x0c,0x0c,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x03,0x03,0xff,0xff,0xff,0xff,0xc0,0xc0,0xc0,0xc0,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0xff,0xff,0xff,0xff,0x00,0x00,0x03,0x03,0xff,0xff,0xfc,0xfc,0x00,0x00,
    0x0c,0x0c,0x0f,0x0f,0x03,0x03,0x0c,0x0c,0x0c,0x0c,0x0f,0x0f,0x03,0x03,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0xc0,0xc0,0xc0,0xc0,0xc0,0xc0,0xc0,0xc0,0xc0,0xc0,0x00,0x00,0x00,0x00,
    0xff,0xff,0xff,0xff,0x00,0x00,0x00,0x00,0x00,0x00,0x03,0x03,0x03,0x03,0x00,0x00,
    0x03,0x03,0x0f,0x0f,0x0c,0x0c,0x0c,0x0c,0x0c,0x0c,0x0f,0x0f,0x03,0x03,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0xc0,0xc0,0xc3,0xc3,0xff,0xff,0xff,0xff,0x00,0x00,0x00,0x00,
    0xfc,0xfc,0xff,0xff,0x03,0x03,0x00,0x00,0xff,0xff,0xff,0xff,0x00,0x00,0x00,0x00,
    0x03,0x03,0x0f,0x0f,0x0c,0x0c,0x0c,0x0c,0x03,0x03,0x0f,0x0f,0x0c,0x0c,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0xc0,0xc0,0xc0,0xc0,0xc0,0xc0,0xc0,0xc0,0xc0,0xc0,0x00,0x00,0x00,0x00,
    0xff,0xff,0xff,0xff,0x0c,0x0c,0x0c,0x0c,0x0c,0x0c,0x0f,0x0f,0x0f,0x0f,0x00,0x00,
    0x03,0x03,0x0f,0x0f,0x0c,0x0c,0x0c,0x0c,0x0c,0x0c,0x0f,0x0f,0x03,0x03,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0xfc,0xfc,0xff,0xff,0x03,0x03,0x0f,0x0f,0x3c,0x3c,0x00,0x00,0x00,0x00,
    0x03,0x03,0xff,0xff,0xff,0xff,0x03,0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x0c,0x0c,0x0f,0x0f,0x0f,0x0f,0x0c,0x0c,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0xc0,0xc0,0xc0,0xc0,0xc0,0xc0,0x00,0x00,0xc0,0xc0,0xc0,0xc0,0x00,0x00,
    0xff,0xff,0xff,0xff,0x00,0x00,0x00,0x00,0xff,0xff,0xff,0xff,0x00,0x00,0x00,0x00,
    0xc3,0xc3,0xcf,0xcf,0x0c,0x0c,0x0c,0x0c,0xff,0xff,0xff,0xff,0x00,0x00,0x00,0x00,
    0x00,0x00,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x00,0x00,0x00,0x00,0x00,0x00,
    0x03,0x03,0xff,0xff,0xff,0xff,0x00,0x00,0xc0,0xc0,0xc0,0xc0,0x00,0x00,0x00,0x00,
    0x00,0x00,0xff,0xff,0xff,0xff,0x03,0x03,0x00,0x00,0xff,0xff,0xff,0xff,0x00,0x00,
    0x0c,0x0c,0x0f,0x0f,0x0f,0x0f,0x00,0x00,0x00,0x00,0x0f,0x0f,0x0f,0x0f,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0xc0,0xc0,0xcf,0xcf,0xcf,0xcf,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0xff,0xff,0xff,0xff,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x0c,0x0c,0x0f,0x0f,0x0f,0x0f,0x0c,0x0c,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xc0,0xc0,0xcf,0xcf,0xcf,0xcf,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xff,0xff,0xff,0xff,0x00,0x00,
    0x00,0x00,0xf0,0xf0,0xf0,0xf0,0x00,0x00,0x00,0x00,0xff,0xff,0xff,0xff,0x00,0x00,
    0x00,0x00,0x00,0x00,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x00,0x00,0x00,0x00,
    0x03,0x03,0xff,0xff,0xff,0xff,0x00,0x00,0x00,0x00,0xc0,0xc0,0xc0,0xc0,0x00,0x00,
    0x00,0x00,0xff,0xff,0xff,0xff,0x3c,0x3c,0xff,0xff,0xc3,0xc3,0x00,0x00,0x00,0x00,
    0x0c,0x0c,0x0f,0x0f,0x0f,0x0f,0x00,0x00,0x00,0x00,0x0f,0x0f,0x0f,0x0f,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x03,0x03,0xff,0xff,0xff,0xff,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0xff,0xff,0xff,0xff,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x0c,0x0c,0x0f,0x0f,0x0f,0x0f,0x0c,0x0c,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0xc0,0xc0,0xc0,0xc0,0xc0,0xc0,0x00,0x00,0xc0,0xc0,0xc0,0xc0,0x00,0x00,0x00,0x00,
    0xff,0xff,0xff,0xff,0x03,0x03,0xff,0xff,0x03,0x03,0xff,0xff,0xff,0xff,0x00,0x00,
    0x0f,0x0f,0x0f,0x0f,0x00,0x00,0x0f,0x0f,0x00,0x00,0x0f,0x0f,0x0f,0x0f,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0xc0,0xc0,0xc0,0xc0,0x00,0x00,0xc0,0xc0,0xc0,0xc0,0xc0,0xc0,0x00,0x00,0x00,0x00,
    0x00,0x00,0xff,0xff,0xff,0xff,0x00,0x00,0x00,0x00,0xff,0xff,0xff,0xff,0x00,0x00,
    0x00,0x00,0x0f,0x0f,0x0f,0x0f,0x00,0x00,0x00,0x00,0x0f,0x0f,0x0f,0x0f,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0xc0,0xc0,0xc0,0xc0,0xc0,0xc0,0xc0,0xc0,0xc0,0xc0,0x00,0x00,0x00,0x00,
    0xff,0xff,0xff,0xff,0x00,0x00,0x00,0x00,0x00,0x00,0xff,0xff,0xff,0xff,0x00,0x00,
    0x03,0x03,0x0f,0x0f,0x0c,0x0c,0x0c,0x0c,0x0c,0x0c,0x0f,0x0f,0x03,0x03,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0xc0,0xc0,0xc0,0xc0,0x00,0x00,0xc0,0xc0,0xc0,0xc0,0xc0,0xc0,0x00,0x00,0x00,0x00,
    0x00,0x00,0xff,0xff,0xff,0xff,0x00,0x00,0x00,0x00,0xff,0xff,0xff,0xff,0x00,0x00,
    0x00,0x00,0xff,0xff,0xff,0xff,0x0c,0x0c,0x0c,0x0c,0x0f,0x0f,0x03,0x03,0x00,0x00,
    0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0xc0,0xc0,0xc0,0xc0,0xc0,0xc0,0x00,0x00,0xc0,0xc0,0xc0,0xc0,0x00,0x00,
    0xff,0xff,0xff,0xff,0x00,0x00,0x00,0x00,0xff,0xff,0xff,0xff,0x00,0x00,0x00,0x00,
    0x03,0x03,0x0f,0x0f,0x0c,0x0c,0x0c,0x0c,0xff,0xff,0xff,0xff,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x00,0x00,
    0xc0,0xc0,0xc0,0xc0,0x00,0x00,0xc0,0xc0,0xc0,0xc0,0xc0,0xc0,0x00,0x00,0x00,0x00,
    0x00,0x00,0xff,0xff,0xff,0xff,0x03,0x03,0x00,0x00,0x03,0x03,0x0f,0x0f,0x00,0x00,
    0x0c,0x0c,0x0f,0x0f,0x0f,0x0f,0x0c,0x0c,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0xc0,0xc0,0xc0,0xc0,0xc0,0xc0,0xc0,0xc0,0xc0,0xc0,0x00,0x00,0x00,0x00,
    0x03,0x03,0x0f,0x0f,0x3c,0x3c,0x30,0x30,0xf0,0xf0,0xc3,0xc3,0x03,0x03,0x00,0x00,
    0x03,0x03,0x0f,0x0f,0x0c,0x0c,0x0c,0x0c,0x0c,0x0c,0x0f,0x0f,0x03,0x03,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0xc0,0xc0,0xc0,0xc0,0xfc,0xfc,0xff,0xff,0xc0,0xc0,0xc0,0xc0,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0xff,0xff,0xff,0xff,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x03,0x03,0x0f,0x0f,0x0c,0x0c,0x0f,0x0f,0x03,0x03,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0xc0,0xc0,0xc0,0xc0,0x00,0x00,0x00,0x00,0xc0,0xc0,0xc0,0xc0,0x00,0x00,0x00,0x00,
    0xff,0xff,0xff,0xff,0x00,0x00,0x00,0x00,0xff,0xff,0xff,0xff,0x00,0x00,0x00,0x00,
    0x03,0x03,0x0f,0x0f,0x0c,0x0c,0x0c,0x0c,0x03,0x03,0x0f,0x0f,0x0c,0x0c,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0xc0,0xc0,0xc0,0xc0,0x00,0x00,0x00,0x00,0xc0,0xc0,0xc0,0xc0,0x00,0x00,
    0x00,0x00,0xff,0xff,0xff,0xff,0x00,0x00,0x00,0x00,0xff,0xff,0xff,0xff,0x00,0x00,
    0x00,0x00,0x00,0x00,0x03,0x03,0x0f,0x0f,0x0f,0x0f,0x03,0x03,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0xc0,0xc0,0xc0,0xc0,0x00,0x00,0x00,0x00,0x00,0x00,0xc0,0xc0,0xc0,0xc0,0x00,0x00,
    0xff,0xff,0xff,0xff,0x00,0x00,0xf0,0xf0,0x00,0x00,0xff,0xff,0xff,0xff,0x00,0x00,
    0x03,0x03,0x0f,0x0f,0x0f,0x0f,0x03,0x03,0x0f,0x0f,0x0f,0x0f,0x03,0x03,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0xc0,0xc0,0xc0,0xc0,0x00,0x00,0x00,0x00,0x00,0x00,0xc0,0xc0,0xc0,0xc0,0x00,0x00,
    0x00,0x00,0x03,0x03,0xff,0xff,0xfc,0xfc,0xff,0xff,0x03,0x03,0x00,0x00,0x00,0x00,
    0x0c,0x0c,0x0f,0x0f,0x03,0x03,0x00,0x00,0x03,0x03,0x0f,0x0f,0x0c,0x0c,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0xc0,0xc0,0xc0,0xc0,0x00,0x00,0x00,0x00,0x00,0x00,0xc0,0xc0,0xc0,0xc0,0x00,0x00,
    0xff,0xff,0xff,0xff,0x00,0x00,0x00,0x00,0x00,0x00,0xff,0xff,0xff,0xff,0x00,0x00,
    0x03,0x03,0x0f,0x0f,0x0c,0x0c,0x0c,0x0c,0xcc,0xcc,0xff,0xff,0x3f,0x3f,0x00,0x00,
    0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x00,0x00,0x00,0x00,0x00,0x00,
    0xc0,0xc0,0xc0,0xc0,0xc0,0xc0,0xc0,0xc0,0xc0,0xc0,0xc0,0xc0,0xc0,0xc0,0x00,0x00,
    0x03,0x03,0xc3,0xc3,0xf0,0xf0,0x3c,0x3c,0x0f,0x0f,0x03,0x03,0x00,0x00,0x00,0x00,
    0x0f,0x0f,0x0f,0x0f,0x0c,0x0c,0x0c,0x0c,0x0c,0x0c,0x0f,0x0f,0x0f,0x0f,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0xfc,0xfc,0xff,0xff,0x03,0x03,0x03,0x03,0x00,0x00,
    0x00,0x00,0x03,0x03,0x03,0x03,0xff,0xff,0xfc,0xfc,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x03,0x03,0x0f,0x0f,0x0c,0x0c,0x0c,0x0c,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0xff,0xff,0xff,0xff,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0xfc,0xfc,0xfc,0xfc,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x0f,0x0f,0x0f,0x0f,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x03,0x03,0x03,0x03,0xff,0xff,0xfc,0xfc,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0xfc,0xfc,0xff,0xff,0x03,0x03,0x03,0x03,0x00,0x00,
    0x00,0x00,0x0c,0x0c,0x0c,0x0c,0x0f,0x0f,0x03,0x03,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x0c,0x0c,0x0f,0x0f,0x03,0x03,0x0f,0x0f,0x0c,0x0c,0x0f,0x0f,0x03,0x03,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0xc0,0xc0,0xf0,0xf0,0xc0,0xc0,0x00,0x00,0x00,0x00,0x00,0x00,
    0xfc,0xfc,0xff,0xff,0x03,0x03,0x00,0x00,0x03,0x03,0xff,0xff,0xfc,0xfc,0x00,0x00,
    0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
//
// Table of sine values for 0-360 degrees expressed as a signed 16-bit value
// from -32768 (-1) to 32767 (1)
//
int16_t i16SineTable[] = {0,572, 1144, 1715, 2286, 2856, 3425, 3993, 4560, 5126,  // 0-9
        5690,  6252, 6813, 7371, 7927, 8481, 9032, 9580, 10126, 10668, // 10-19
        11207,  11743, 12275, 12803, 13328, 13848, 14365, 14876, 15384, 15886,// 20-29
        16384,  16877, 17364, 17847, 18324, 18795, 19261, 19720, 20174, 20622,// 30-39
        21063,  21498, 21926, 22348, 22763, 23170, 23571, 23965, 24351, 24730,// 40-49
        25102,  25466, 25822, 26170, 26510, 26842, 27166, 27482, 27789, 28088,// 50-59
        28378,  28660, 28932, 29197, 29452, 29698, 29935, 30163, 30382, 30592,// 60-69
        30792,  30983, 31164, 31336, 31499, 31651, 31795, 31928, 32052, 32166,// 70-79
        32270,  32365, 32440, 32524, 32599, 32643, 32688, 32723, 32748, 32763,// 80-89
        32767,  32763, 32748, 32723, 32688, 32643, 32588, 32524, 32449, 32365,// 90-99
        32270,  32166, 32052, 31928, 31795, 31651, 31499, 31336, 31164, 30983,// 100-109
        30792,  30592, 30382, 30163, 29935, 29698, 29452, 29197, 28932, 28660,// 110-119
        28378,  28088, 27789, 27482, 27166, 26842, 26510, 26170, 25822, 25466,// 120-129
        25102,  24730, 24351, 23965, 23571, 23170, 22763, 22348, 21926, 21498,// 130-139
        21063,  20622, 20174, 19720, 19261, 18795, 18324, 17847, 17364, 16877,// 140-149
        16384,  15886, 15384, 14876, 14365, 13848, 13328, 12803, 12275, 11743,// 150-159
        11207,  10668, 10126, 9580, 9032, 8481, 7927, 7371, 6813, 6252,// 160-169
        5690,  5126, 4560, 3993, 3425, 2856, 2286, 1715, 1144, 572,//  170-179
        0,  -572, -1144, -1715, -2286, -2856, -3425, -3993, -4560, -5126,// 180-189
        -5690,  -6252, -6813, -7371, -7927, -8481, -9032, -9580, -10126, -10668,// 190-199
        -11207,  -11743, -12275, -12803, -13328, -13848, -14365, -14876, -15384, -15886,// 200-209
        -16384,  -16877, -17364, -17847, -18324, -18795, -19261, -19720, -20174, -20622,// 210-219
        -21063,  -21498, -21926, -22348, -22763, -23170, -23571, -23965, -24351, -24730, // 220-229
        -25102,  -25466, -25822, -26170, -26510, -26842, -27166, -27482, -27789, -28088, // 230-239
        -28378,  -28660, -28932, -29196, -29452, -29698, -29935, -30163, -30382, -30592, // 240-249
        -30792,  -30983, -31164, -31336, -31499, -31651, -31795, -31928, -32052, -32166, // 250-259
        -32270,  -32365, -32449, -32524, -32588, -32643, -32688, -32723, -32748, -32763, // 260-269
        -32768,  -32763, -32748, -32723, -32688, -32643, -32588, -32524, -32449, -32365, // 270-279
        -32270,  -32166, -32052, -31928, -31795, -31651, -31499, -31336, -31164, -30983, // 280-289
        -30792,  -30592, -30382, -30163, -29935, -29698, -29452, -29196, -28932, -28660, // 290-299
        -28378,  -28088, -27789, -27482, -27166, -26842, -26510, -26170, -25822, -25466, // 300-309
        -25102,  -24730, -24351, -23965, -23571, -23170, -22763, -22348, -21926, -21498, // 310-319
        -21063,  -20622, -20174, -19720, -19261, -18795, -18234, -17847, -17364, -16877, // 320-329
        -16384,  -15886, -15384, -14876, -14365, -13848, -13328, -12803, -12275, -11743, // 330-339
        -11207,  -10668, -10126, -9580, -9032, -8481, -7927, -7371, -6813, -6252,// 340-349
        -5690,  -5126, -4560, -3993, -3425, -2856, -2286, -1715, -1144, -572, // 350-359
// an extra 90 degrees to simulate the cosine function
        0,572,  1144, 1715, 2286, 2856, 3425, 3993, 4560, 5126,// 0-9
        5690,  6252, 6813, 7371, 7927, 8481, 9032, 9580, 10126, 10668,// 10-19
        11207,  11743, 12275, 12803, 13328, 13848, 14365, 14876, 15384, 15886,// 20-29
        16384,  16877, 17364, 17847, 18324, 18795, 19261, 19720, 20174, 20622,// 30-39
        21063,  21498, 21926, 22348, 22763, 23170, 23571, 23965, 24351, 24730,// 40-49
        25102,  25466, 25822, 26170, 26510, 26842, 27166, 27482, 27789, 28088,// 50-59
        28378,  28660, 28932, 29197, 29452, 29698, 29935, 30163, 30382, 30592,// 60-69
        30792,  30983, 31164, 31336, 31499, 31651, 31795, 31928, 32052, 32166,// 70-79
    32270,  32365, 32440, 32524, 32599, 32643, 32688, 32723, 32748, 32763}; // 80-89

#endif // !__AVR__

// 5x7 font (in 6x8 cell)
const uint8_t ucSmallFont[]PROGMEM = {
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x06,0x5f,0x06,0x00,0x00,0x07,0x03,0x00,
    0x07,0x03,0x00,0x24,0x7e,0x24,0x7e,0x24,0x00,0x24,0x2b,0x6a,0x12,0x00,0x00,0x63,
    0x13,0x08,0x64,0x63,0x00,0x36,0x49,0x56,0x20,0x50,0x00,0x00,0x07,0x03,0x00,0x00,
    0x00,0x00,0x3e,0x41,0x00,0x00,0x00,0x00,0x41,0x3e,0x00,0x00,0x00,0x08,0x3e,0x1c,
    0x3e,0x08,0x00,0x08,0x08,0x3e,0x08,0x08,0x00,0x00,0xe0,0x60,0x00,0x00,0x00,0x08,
    0x08,0x08,0x08,0x08,0x00,0x00,0x60,0x60,0x00,0x00,0x00,0x20,0x10,0x08,0x04,0x02,
    0x00,0x3e,0x51,0x49,0x45,0x3e,0x00,0x00,0x42,0x7f,0x40,0x00,0x00,0x62,0x51,0x49,
    0x49,0x46,0x00,0x22,0x49,0x49,0x49,0x36,0x00,0x18,0x14,0x12,0x7f,0x10,0x00,0x2f,
    0x49,0x49,0x49,0x31,0x00,0x3c,0x4a,0x49,0x49,0x30,0x00,0x01,0x71,0x09,0x05,0x03,
    0x00,0x36,0x49,0x49,0x49,0x36,0x00,0x06,0x49,0x49,0x29,0x1e,0x00,0x00,0x6c,0x6c,
    0x00,0x00,0x00,0x00,0xec,0x6c,0x00,0x00,0x00,0x08,0x14,0x22,0x41,0x00,0x00,0x24,
    0x24,0x24,0x24,0x24,0x00,0x00,0x41,0x22,0x14,0x08,0x00,0x02,0x01,0x59,0x09,0x06,
    0x00,0x3e,0x41,0x5d,0x55,0x1e,0x00,0x7e,0x11,0x11,0x11,0x7e,0x00,0x7f,0x49,0x49,
    0x49,0x36,0x00,0x3e,0x41,0x41,0x41,0x22,0x00,0x7f,0x41,0x41,0x41,0x3e,0x00,0x7f,
    0x49,0x49,0x49,0x41,0x00,0x7f,0x09,0x09,0x09,0x01,0x00,0x3e,0x41,0x49,0x49,0x7a,
    0x00,0x7f,0x08,0x08,0x08,0x7f,0x00,0x00,0x41,0x7f,0x41,0x00,0x00,0x30,0x40,0x40,
    0x40,0x3f,0x00,0x7f,0x08,0x14,0x22,0x41,0x00,0x7f,0x40,0x40,0x40,0x40,0x00,0x7f,
    0x02,0x04,0x02,0x7f,0x00,0x7f,0x02,0x04,0x08,0x7f,0x00,0x3e,0x41,0x41,0x41,0x3e,
    0x00,0x7f,0x09,0x09,0x09,0x06,0x00,0x3e,0x41,0x51,0x21,0x5e,0x00,0x7f,0x09,0x09,
    0x19,0x66,0x00,0x26,0x49,0x49,0x49,0x32,0x00,0x01,0x01,0x7f,0x01,0x01,0x00,0x3f,
    0x40,0x40,0x40,0x3f,0x00,0x1f,0x20,0x40,0x20,0x1f,0x00,0x3f,0x40,0x3c,0x40,0x3f,
    0x00,0x63,0x14,0x08,0x14,0x63,0x00,0x07,0x08,0x70,0x08,0x07,0x00,0x71,0x49,0x45,
    0x43,0x00,0x00,0x00,0x7f,0x41,0x41,0x00,0x00,0x02,0x04,0x08,0x10,0x20,0x00,0x00,
    0x41,0x41,0x7f,0x00,0x00,0x04,0x02,0x01,0x02,0x04,0x00,0x80,0x80,0x80,0x80,0x80,
    0x00,0x00,0x03,0x07,0x00,0x00,0x00,0x20,0x54,0x54,0x54,0x78,0x00,0x7f,0x44,0x44,
    0x44,0x38,0x00,0x38,0x44,0x44,0x44,0x28,0x00,0x38,0x44,0x44,0x44,0x7f,0x00,0x38,
    0x54,0x54,0x54,0x08,0x00,0x08,0x7e,0x09,0x09,0x00,0x00,0x18,0xa4,0xa4,0xa4,0x7c,
    0x00,0x7f,0x04,0x04,0x78,0x00,0x00,0x00,0x00,0x7d,0x40,0x00,0x00,0x40,0x80,0x84,
    0x7d,0x00,0x00,0x7f,0x10,0x28,0x44,0x00,0x00,0x00,0x00,0x7f,0x40,0x00,0x00,0x7c,
    0x04,0x18,0x04,0x78,0x00,0x7c,0x04,0x04,0x78,0x00,0x00,0x38,0x44,0x44,0x44,0x38,
    0x00,0xfc,0x44,0x44,0x44,0x38,0x00,0x38,0x44,0x44,0x44,0xfc,0x00,0x44,0x78,0x44,
    0x04,0x08,0x00,0x08,0x54,0x54,0x54,0x20,0x00,0x04,0x3e,0x44,0x24,0x00,0x00,0x3c,
    0x40,0x20,0x7c,0x00,0x00,0x1c,0x20,0x40,0x20,0x1c,0x00,0x3c,0x60,0x30,0x60,0x3c,
    0x00,0x6c,0x10,0x10,0x6c,0x00,0x00,0x9c,0xa0,0x60,0x3c,0x00,0x00,0x64,0x54,0x54,
    0x4c,0x00,0x00,0x08,0x3e,0x41,0x41,0x00,0x00,0x00,0x00,0x77,0x00,0x00,0x00,0x00,
    0x41,0x41,0x3e,0x08,0x00,0x02,0x01,0x02,0x01,0x00,0x00,0x3c,0x26,0x23,0x26,0x3c};

// wrapper/adapter functions to make the code work on Linux
#ifdef _LINUX_
static int digitalRead(int iPin)
{
  return AIOReadGPIO(iPin);
} /* digitalRead() */

static void digitalWrite(int iPin, int iState)
{
   AIOWriteGPIO(iPin, iState);
} /* digitalWrite() */

static void pinMode(int iPin, int iMode)
{
   AIOAddGPIO(iPin, iMode);
} /* pinMode() */

static void delayMicroseconds(int iMS)
{
  usleep(iMS);
} /* delayMicroseconds() */

static uint8_t pgm_read_byte(uint8_t *ptr)
{
  return *ptr;
}
static int16_t pgm_read_word(uint8_t *ptr)
{
  return ptr[0] + (ptr[1]<<8);
}
#endif // _LINUX_
//
// Provide a small temporary buffer for use by the graphics functions
//
void spilcdSetTXBuffer(uint8_t *pBuf, int iSize)
{
#ifndef ESP32_DMA
  ucTXBuf = pBuf;
  iTXBufSize = iSize;
#endif
} /* spilcdSetTXBuffer() */

// Sets the D/C pin to data or command mode
void spilcdSetMode(SPILCD *pLCD, int iMode)
{
#ifdef __AVR__
    if (iMode == MODE_DATA)
       *outDC |= bitDC;
    else
       *outDC &= ~bitDC;
#else
	myPinWrite(pLCD->iDCPin, iMode == MODE_DATA);
#endif
#ifdef HAL_ESP32_HAL_H_
	delayMicroseconds(1); // some systems are so fast that it needs to be delayed
#endif
} /* spilcdSetMode() */

// List of command/parameters to initialize the ST7789 LCD
const unsigned char uc240x240InitList[]PROGMEM = {
    1, 0x13, // partial mode off
    1, 0x21, // display inversion off
    2, 0x36,0x08,    // memory access 0xc0 for 180 degree flipped
    2, 0x3a,0x55,    // pixel format; 5=RGB565
    3, 0x37,0x00,0x00, //
    6, 0xb2,0x0c,0x0c,0x00,0x33,0x33, // Porch control
    2, 0xb7,0x35,    // gate control
    2, 0xbb,0x1a,    // VCOM
    2, 0xc0,0x2c,    // LCM
    2, 0xc2,0x01,    // VDV & VRH command enable
    2, 0xc3,0x0b,    // VRH set
    2, 0xc4,0x20,    // VDV set
    2, 0xc6,0x0f,    // FR control 2
    3, 0xd0, 0xa4, 0xa1,     // Power control 1
    15, 0xe0, 0x00,0x19,0x1e,0x0a,0x09,0x15,0x3d,0x44,0x51,0x12,0x03,
        0x00,0x3f,0x3f,     // gamma 1
    15, 0xe1, 0x00,0x18,0x1e,0x0a,0x09,0x25,0x3f,0x43,0x52,0x33,0x03,
        0x00,0x3f,0x3f,        // gamma 2
    1, 0x29,    // display on
    0
};
// List of command/parameters to initialize the ili9341 display
const unsigned char uc240InitList[]PROGMEM = {
        4, 0xEF, 0x03, 0x80, 0x02,
        4, 0xCF, 0x00, 0XC1, 0X30,
        5, 0xED, 0x64, 0x03, 0X12, 0X81,
        4, 0xE8, 0x85, 0x00, 0x78,
        6, 0xCB, 0x39, 0x2C, 0x00, 0x34, 0x02,
        2, 0xF7, 0x20,
        3, 0xEA, 0x00, 0x00,
        2, 0xc0, 0x23, // Power control
        2, 0xc1, 0x10, // Power control
        3, 0xc5, 0x3e, 0x28, // VCM control
        2, 0xc7, 0x86, // VCM control2
        2, 0x36, 0x48, // Memory Access Control
        1, 0x20,        // non inverted
        2, 0x3a, 0x55,
        3, 0xb1, 0x00, 0x18,
        4, 0xb6, 0x08, 0x82, 0x27, // Display Function Control
        2, 0xF2, 0x00, // Gamma Function Disable
        2, 0x26, 0x01, // Gamma curve selected
        16, 0xe0, 0x0F, 0x31, 0x2B, 0x0C, 0x0E, 0x08,
                0x4E, 0xF1, 0x37, 0x07, 0x10, 0x03, 0x0E, 0x09, 0x00, // Set Gamma
        16, 0xe1, 0x00, 0x0E, 0x14, 0x03, 0x11, 0x07,
                0x31, 0xC1, 0x48, 0x08, 0x0F, 0x0C, 0x31, 0x36, 0x0F, // Set Gamma
        3, 0xb1, 0x00, 0x10, // FrameRate Control 119Hz
        0
};
// Init sequence for the SSD1331 OLED display
// Courtesy of Adafruit's SSD1331 library
const unsigned char ucSSD1331InitList[] PROGMEM = {
    1, 0xae,    // display off
    1, 0xa0,    // set remap
    1, 0x72,    // RGB 0x76 == BGR
    2, 0xa1, 0x00,  // set start line
    2, 0xa2, 0x00,  // set display offset
    1, 0xa4,    // normal display
    2, 0xa8, 0x3f,  // set multiplex 1/64 duty
    2, 0xad, 0x8e, // set master configuration
    2, 0xb0, 0x0b, // disable power save
    2, 0xb1, 0x31, // phase period adjustment
    2, 0xb3, 0xf0, // clock divider
    2, 0x8a, 0x64, // precharge a
    2, 0x8b, 0x78, // precharge b
    2, 0x8c, 0x64, // precharge c
    2, 0xbb, 0x3a, // set precharge level
    2, 0xbe, 0x3e, // set vcomh
    2, 0x87, 0x06, // master current control
    2, 0x81, 0x91, // contrast for color a
    2, 0x82, 0x50, // contrast for color b
    2, 0x83, 0x7D, // contrast for color c
    1, 0xAF, // display on, normal
    0,0
};
// List of command/parameters to initialize the SSD1351 OLED display
const unsigned char ucOLEDInitList[] PROGMEM = {
	2, 0xfd, 0x12, // unlock the controller
	2, 0xfd, 0xb1, // unlock the command
	1, 0xae,	// display off
	2, 0xb3, 0xf1,  // clock divider
	2, 0xca, 0x7f,	// mux ratio
	2, 0xa0, 0x74,	// set remap
	3, 0x15, 0x00, 0x7f,	// set column
	3, 0x75, 0x00, 0x7f,	// set row
	2, 0xb5, 0x00,	// set GPIO state
	2, 0xab, 0x01,	// function select (internal diode drop)
	2, 0xb1, 0x32,	// precharge
	2, 0xbe, 0x05,	// vcomh
	1, 0xa6,	// set normal display mode
	4, 0xc1, 0xc8, 0x80, 0xc8, // contrast ABC
	2, 0xc7, 0x0f,	// contrast master
	4, 0xb4, 0xa0,0xb5,0x55,	// set VSL
	2, 0xb6, 0x01,	// precharge 2
	1, 0xaf,	// display ON
	0};
// List of command/parameters for the SSD1283A display
const unsigned char uc132InitList[]PROGMEM = {
    3, 0x10, 0x2F,0x8E,
    3, 0x11, 0x00,0x0C,
    3, 0x07, 0x00,0x21,
    3, 0x28, 0x00,0x06,
    3, 0x28, 0x00,0x05,
    3, 0x27, 0x05,0x7F,
    3, 0x29, 0x89,0xA1,
    3, 0x00, 0x00,0x01,
    LCD_DELAY, 100,
    3, 0x29, 0x80,0xB0,
    LCD_DELAY, 30,
    3, 0x29, 0xFF,0xFE,
    3, 0x07, 0x02,0x23,
    LCD_DELAY, 30,
    3, 0x07, 0x02,0x33,
    3, 0x01, 0x21,0x83,
    3, 0x03, 0x68,0x30,
    3, 0x2F, 0xFF,0xFF,
    3, 0x2C, 0x80,0x00,
    3, 0x27, 0x05,0x70,
    3, 0x02, 0x03,0x00,
    3, 0x0B, 0x58,0x0C,
    3, 0x12, 0x06,0x09,
    3, 0x13, 0x31,0x00,
    0
};

// List of command/parameters to initialize the ili9342 display
const unsigned char uc320InitList[]PROGMEM = {
        2, 0xc0, 0x23, // Power control
        2, 0xc1, 0x10, // Power control
        3, 0xc5, 0x3e, 0x28, // VCM control
        2, 0xc7, 0x86, // VCM control2
        2, 0x36, 0x08, // Memory Access Control (flip x/y/bgr/rgb)
        2, 0x3a, 0x55,
	1, 0x21,	// inverted display off
//        2, 0x26, 0x01, // Gamma curve selected
//        16, 0xe0, 0x0F, 0x31, 0x2B, 0x0C, 0x0E, 0x08,
//                0x4E, 0xF1, 0x37, 0x07, 0x10, 0x03, 0x0E, 0x09, 0x00, // Set Gamma
//        16, 0xe1, 0x00, 0x0E, 0x14, 0x03, 0x11, 0x07,
//                0x31, 0xC1, 0x48, 0x08, 0x0F, 0x0C, 0x31, 0x36, 0x0F, // Set Gamma
//        3, 0xb1, 0x00, 0x10, // FrameRate Control 119Hz
        0
};

// List of command/parameters to initialize the st7735s display
const unsigned char uc80InitList[]PROGMEM = {
//        4, 0xb1, 0x01, 0x2c, 0x2d,    // frame rate control
//        4, 0xb2, 0x01, 0x2c, 0x2d,    // frame rate control (idle mode)
//        7, 0xb3, 0x01, 0x2c, 0x2d, 0x01, 0x2c, 0x2d, // frctrl - partial mode
//        2, 0xb4, 0x07,    // non-inverted
//        4, 0xc0, 0xa2, 0x02, 0x84,    // power control
//        2, 0xc1, 0xc5,     // pwr ctrl2
//        2, 0xc2, 0x0a, 0x00, // pwr ctrl3
//        3, 0xc3, 0x8a, 0x2a, // pwr ctrl4
//        3, 0xc4, 0x8a, 0xee, // pwr ctrl5
//        2, 0xc5, 0x0e,        // vm ctrl1
    2, 0x3a, 0x05,    // pixel format RGB565
    2, 0x36, 0xc0, // MADCTL
    5, 0x2a, 0x00, 0x02, 0x00, 0x7f+0x02, // column address start
    5, 0x2b, 0x00, 0x01, 0x00, 0x9f+0x01, // row address start
    17, 0xe0, 0x09, 0x16, 0x09,0x20,
    0x21,0x1b,0x13,0x19,
    0x17,0x15,0x1e,0x2b,
    0x04,0x05,0x02,0x0e, // gamma sequence
    17, 0xe1, 0x0b,0x14,0x08,0x1e,
    0x22,0x1d,0x18,0x1e,
    0x1b,0x1a,0x24,0x2b,
    0x06,0x06,0x02,0x0f,
    1, 0x21,    // display inversion on
    0
};
// List of command/parameters to initialize the ILI9225 176x220 LCD
const unsigned char ucILI9225InitList[] PROGMEM = {
    //************* Start Initial Sequence **********//
    // These are 16-bit commands and data
    // Set SS bit and direction output from S528 to S1
    2, 0x00, 0x10, 0x00, 0x00, // Set SAP, DSTB, STB
    2, 0x00, 0x11, 0x00, 0x00, // Set APON,PON,AON,VCI1EN,VC
    2, 0x00, 0x12, 0x00, 0x00, // Set BT,DC1,DC2,DC3
    2, 0x00, 0x13, 0x00, 0x00, // Set GVDD
    2, 0x00, 0x14, 0x00, 0x00, // Set VCOMH/VCOML voltage
    LCD_DELAY, 40,
    // Power on sequence
    2, 0x00, 0x11, 0x00, 0x18, // Set APON,PON,AON,VCI1EN,VC
    2, 0x00, 0x12, 0x61, 0x21, // Set BT,DC1,DC2,DC3
    2, 0x00, 0x13, 0x00, 0x6f, // Set GVDD
    2, 0x00, 0x14, 0x49, 0x5f, // Set VCOMH/VCOML voltage
    2, 0x00, 0x10, 0x08, 0x00, // Set SAP, DSTB, STB
    LCD_DELAY, 10,
    2, 0x00, 0x11, 0x10, 0x3B, // Set APON,PON,AON,VCI1EN,VC
    LCD_DELAY, 50,
    2, 0x00, 0x01, 0x01, 0x1C, // set display line number and direction
    2, 0x00, 0x02, 0x01, 0x00, // set 1 line inversion
    2, 0x00, 0x03, 0x10, 0x30, // set GRAM write direction and BGR=1
    2, 0x00, 0x07, 0x00, 0x00, // display off
    2, 0x00, 0x08, 0x08, 0x08,  // set back porch and front porch
    2, 0x00, 0x0b, 0x11, 0x00, // set the clocks number per line
    2, 0x00, 0x0C, 0x00, 0x00, // interface control - CPU
    2, 0x00, 0x0F, 0x0d, 0x01, // Set frame rate (OSC control)
    2, 0x00, 0x15, 0x00, 0x20, // VCI recycling
    2, 0x00, 0x20, 0x00, 0x00, // Set GRAM horizontal address
    2, 0x00, 0x21, 0x00, 0x00, // Set GRAM vertical address
    //------------------------ Set GRAM area --------------------------------//
    2, 0x00, 0x30, 0x00, 0x00,
    2, 0x00, 0x31, 0x00, 0xDB,
    2, 0x00, 0x32, 0x00, 0x00,
    2, 0x00, 0x33, 0x00, 0x00,
    2, 0x00, 0x34, 0x00, 0xDB,
    2, 0x00, 0x35, 0x00, 0x00,
    2, 0x00, 0x36, 0x00, 0xAF,
    2, 0x00, 0x37, 0x00, 0x00,
    2, 0x00, 0x38, 0x00, 0xDB,
    2, 0x00, 0x39, 0x00, 0x00,
// set GAMMA curve
    2, 0x00, 0x50, 0x00, 0x00,
    2, 0x00, 0x51, 0x08, 0x08,
    2, 0x00, 0x52, 0x08, 0x0A,
    2, 0x00, 0x53, 0x00, 0x0A,
    2, 0x00, 0x54, 0x0A, 0x08,
    2, 0x00, 0x55, 0x08, 0x08,
    2, 0x00, 0x56, 0x00, 0x00,
    2, 0x00, 0x57, 0x0A, 0x00,
    2, 0x00, 0x58, 0x07, 0x10,
    2, 0x00, 0x59, 0x07, 0x10,
    2, 0x00, 0x07, 0x00, 0x12, // display ctrl
    LCD_DELAY, 50,
    2, 0x00, 0x07, 0x10, 0x17, // display on
    0
};
// List of command/parameters to initialize the st7735r display
const unsigned char uc128InitList[]PROGMEM = {
//	4, 0xb1, 0x01, 0x2c, 0x2d,	// frame rate control
//	4, 0xb2, 0x01, 0x2c, 0x2d,	// frame rate control (idle mode)
//	7, 0xb3, 0x01, 0x2c, 0x2d, 0x01, 0x2c, 0x2d, // frctrl - partial mode
//	2, 0xb4, 0x07,	// non-inverted
//	4, 0xc0, 0x82, 0x02, 0x84,	// power control
//	2, 0xc1, 0xc5, 	// pwr ctrl2
//	2, 0xc2, 0x0a, 0x00, // pwr ctrl3
//	3, 0xc3, 0x8a, 0x2a, // pwr ctrl4
//	3, 0xc4, 0x8a, 0xee, // pwr ctrl5
//	2, 0xc5, 0x0e,		// pwr ctrl
//	1, 0x20,	// display inversion off
	2, 0x3a, 0x55,	// pixel format RGB565
	2, 0x36, 0xc0, // MADCTL
	17, 0xe0, 0x09, 0x16, 0x09,0x20,
		0x21,0x1b,0x13,0x19,
		0x17,0x15,0x1e,0x2b,
		0x04,0x05,0x02,0x0e, // gamma sequence
	17, 0xe1, 0x0b,0x14,0x08,0x1e,
		0x22,0x1d,0x18,0x1e,
		0x1b,0x1a,0x24,0x2b,
		0x06,0x06,0x02,0x0f,
	0
};
// List of command/parameters to initialize the ILI9486 display
const unsigned char ucILI9486InitList[] PROGMEM = {
   2, 0x01, 0x00,
   LCD_DELAY, 50,
   2, 0x28, 0x00,
   3, 0xc0, 0xd, 0xd,
   3, 0xc1, 0x43, 0x00,
   2, 0xc2, 0x00,
   3, 0xc5, 0x00, 0x48,
   4, 0xb6, 0x00, 0x22, 0x3b,
   16, 0xe0,0x0f,0x24,0x1c,0x0a,0x0f,0x08,0x43,0x88,0x32,0x0f,0x10,0x06,0x0f,0x07,0x00,
   16, 0xe1,0x0f,0x38,0x30,0x09,0x0f,0x0f,0x4e,0x77,0x3c,0x07,0x10,0x05,0x23,0x1b,0x00,
   1, 0x20,
   2, 0x36,0x0a,
   2, 0x3a,0x55,
   1, 0x11,
   LCD_DELAY, 150,
   1, 0x29,
   LCD_DELAY, 25,
   0
};
// List of command/parameters to initialize the hx8357 display
const unsigned char uc480InitList[]PROGMEM = {
	2, 0x3a, 0x55,
	2, 0xc2, 0x44,
	5, 0xc5, 0x00, 0x00, 0x00, 0x00,
	16, 0xe0, 0x0f, 0x1f, 0x1c, 0x0c, 0x0f, 0x08, 0x48, 0x98, 0x37,
		0x0a,0x13, 0x04, 0x11, 0x0d, 0x00,
	16, 0xe1, 0x0f, 0x32, 0x2e, 0x0b, 0x0d, 0x05, 0x47, 0x75, 0x37,
		0x06, 0x10, 0x03, 0x24, 0x20, 0x00,
	16, 0xe2, 0x0f, 0x32, 0x2e, 0x0b, 0x0d, 0x05, 0x47, 0x75, 0x37,
		0x06, 0x10, 0x03, 0x24, 0x20, 0x00,
	2, 0x36, 0x48,
	0	
};

//
// 16-bit memset
//
void memset16(uint16_t *pDest, uint16_t usPattern, int iCount)
{
    while (iCount--)
        *pDest++ = usPattern;
}

//
// Returns true if DMA is currently busy
//
int spilcdIsDMABusy(void)
{
#ifdef HAS_DMA
    return !transfer_is_done;
#endif
    return 0;
} /* spilcdIsDMABusy() */
//
// Send the data by bit-banging the GPIO ports
//
void SPI_BitBang(SPILCD *pLCD, uint8_t *pData, int iLen, int iMode)
{
    int iMOSI, iCLK; // local vars to speed access
    uint8_t c, j;
    
    iMOSI = pLCD->iMOSIPin;
    iCLK = pLCD->iCLKPin;
    if (pLCD->iCSPin != -1)
        myPinWrite(pLCD->iCSPin, 0);
    if (iMode == MODE_COMMAND)
        myPinWrite(pLCD->iDCPin, 0);
    while (iLen)
    {
        c = *pData++;
        if (c == 0 || c == 0xff) // quicker for all bits equal
        {
            digitalWrite(iMOSI, c);
            for (j=0; j<8; j++)
            {
                myPinWrite(iCLK, HIGH);
                delayMicroseconds(0);
                myPinWrite(iCLK, LOW);
            }
        }
        else
        {
            for (j=0; j<8; j++)
            {
                myPinWrite(iMOSI, c & 0x80);
                myPinWrite(iCLK, HIGH);
                c <<= 1;
                delayMicroseconds(0);
                myPinWrite(iCLK, LOW);
            }
        }
        iLen--;
    }
    if (pLCD->iCSPin != -1)
        myPinWrite(pLCD->iCSPin, 1);
    if (iMode == MODE_COMMAND) // restore it to MODE_DATA before leaving
        myPinWrite(pLCD->iDCPin, 1);
} /* SPI_BitBang() */
//
// Wrapper function for writing to SPI
//
static void myspiWrite(SPILCD *pLCD, unsigned char *pBuf, int iLen, int iMode, int iFlags)
{
    if (iMode == MODE_DATA && pLCD->pBackBuffer != NULL && !bSetPosition && iFlags & DRAW_TO_RAM) // write it to the back buffer
    {
        uint16_t *s, *d;
        int j, iOff, iStrip, iMaxX, iMaxY, i;
        iMaxX = pLCD->iWindowX + pLCD->iWindowCX;
        iMaxY = pLCD->iWindowY + pLCD->iWindowCY;
        iOff = 0;
        i = iLen/2;
        while (i > 0)
        {
            iStrip = iMaxX - pLCD->iCurrentX; // max pixels that can be written in one shot
            if (iStrip > i)
                iStrip = i;
            s = (uint16_t *)&pBuf[iOff];
            d = (uint16_t *)&pLCD->pBackBuffer[pLCD->iOffset];
            if (pLCD->iOffset > pLCD->iMaxOffset || (pLCD->iOffset+iStrip*2) > pLCD->iMaxOffset)
            { // going to write past the end of memory, don't even try
                i = iStrip = 0;
            }
            for (j=0; j<iStrip; j++) // memcpy could be much slower for small runs
            {
                *d++ = *s++;
            }
            pLCD->iOffset += iStrip*2; iOff += iStrip*2;
            i -= iStrip;
            pLCD->iCurrentX += iStrip;
            if (pLCD->iCurrentX >= iMaxX) // need to wrap around to the next line
            {
                pLCD->iCurrentX = pLCD->iWindowX;
                pLCD->iCurrentY++;
                if (pLCD->iCurrentY >= iMaxY)
                    pLCD->iCurrentY = pLCD->iWindowY;
                pLCD->iOffset = (pLCD->iCurrentWidth * 2 * pLCD->iCurrentY) + (pLCD->iCurrentX * 2);
            }
        }
    }
    if (!(iFlags & DRAW_TO_LCD))
        return; // don't write it to the display
    if (pLCD->pfnDataCallback != NULL)
    {
       (*pLCD->pfnDataCallback)(pBuf, iLen, iMode);
       return;
    }
    if (pLCD->iLCDFlags & FLAGS_BITBANG)
    {
        SPI_BitBang(pLCD, pBuf, iLen, iMode);
        return;
    }
    if (pLCD->iCSPin != -1)
    {
#ifdef __AVR__
      *outCS &= ~bitCS;
#else
        myPinWrite(pLCD->iCSPin, 0);
#endif // __AVR__
    }
    if (iMode == MODE_COMMAND)
    {
#ifdef HAS_DMA
        spilcdWaitDMA(); // wait for any previous transaction to finish
#endif
        spilcdSetMode(pLCD, MODE_COMMAND);
    }
#ifdef ESP32_DMA
    // don't use DMA
    if (iMode == MODE_COMMAND || !(iFlags & DRAW_WITH_DMA))
    {
        esp_err_t ret;
        static spi_transaction_t t;
        memset(&t, 0, sizeof(t));       //Zero out the transaction
        t.length=iLen*8;  // length in bits
        t.tx_buffer=pBuf;
        t.user=(void*)iMode;
    // Queue the transaction
//    ret = spi_device_queue_trans(spi, &t, portMAX_DELAY);
        ret=spi_device_polling_transmit(spi, &t);  //Transmit!
        assert(ret==ESP_OK);            //Should have had no issues.
        if (iMode == MODE_COMMAND) // restore D/C pin to DATA
            spilcdSetMode(pLCD, MODE_DATA);
        if (pLCD->iCSPin != -1)
           myPinWrite(pLCD->iCSPin, 1);
        return;
    }
    // wait for it to complete
//    spi_transaction_t *rtrans;
//    spi_device_get_trans_result(spi, &rtrans, portMAX_DELAY);
#endif
#ifdef HAS_DMA
    if (iMode == MODE_DATA && (iFlags & DRAW_WITH_DMA)) // only pixels will get DMA treatment
    {
        spilcdWaitDMA(); // wait for any previous transaction to finish
        if (pBuf != ucTXBuf) // for DMA, we must use the one output buffer
            memcpy(ucTXBuf, pBuf, iLen);
        spilcdWriteDataDMA(pLCD, iLen);
 //       myPinWrite(iCSPin, 1);
        return;
    }
#endif // HAS_DMA
        
// No DMA requested or available, fall through to here
#ifdef _LINUX_
    AIOWriteSPI(iHandle, pBuf, iLen);
#else
#ifdef ARDUINO_ARCH_RP2040
    pSPI->beginTransaction(SPISettings(pLCD->iSPISpeed, MSBFIRST, pLCD->iSPIMode));
#else
    mySPI.beginTransaction(SPISettings(pLCD->iSPISpeed, MSBFIRST, pLCD->iSPIMode));
#endif
#ifdef HAL_ESP32_HAL_H_
    SPI.transferBytes(pBuf, ucRXBuf, iLen);
#else
#ifdef ARDUINO_ARCH_RP2040
    pSPI->transfer(pBuf, iLen);
#else
    mySPI.transfer(pBuf, iLen);
#endif // RP2040
#endif
#ifdef ARDUINO_ARCH_RP2040
    pSPI->endTransaction();
#else
    mySPI.endTransaction();
#endif
#endif // _LINUX_
    if (iMode == MODE_COMMAND) // restore D/C pin to DATA
        spilcdSetMode(pLCD, MODE_DATA);
    if (pLCD->iCSPin != -1)
    {
#ifdef __AVR__
       *outCS |= bitCS;
#else
       myPinWrite(pLCD->iCSPin, 1);
#endif
    }
} /* myspiWrite() */

//
// Public wrapper function to write data to the display
//
void spilcdWriteDataBlock(SPILCD *pLCD, uint8_t *pData, int iLen, int iFlags)
{
  myspiWrite(pLCD, pData, iLen, MODE_DATA, iFlags);
} /* spilcdWriteDataBlock() */
//
// spilcdWritePixelsMasked
//
void spilcdWritePixelsMasked(SPILCD *pLCD, int x, int y, uint8_t *pData, uint8_t *pMask, int iCount, int iFlags)
{
    int i, pix_count, bit_count;
    uint8_t c, *s, *sPixels, *pEnd;
    s = pMask; sPixels = pData;
    pEnd = &pMask[(iCount+7)>>3];
    i = 0;
    bit_count = 8;
    c = *s++; // get first byte
    while (i<iCount && s < pEnd) {
        // Count the number of consecutive pixels to skip
        pix_count = 0;
        while (bit_count && (c & 0x80) == 0) { // count 0's
            if (c == 0) { // quick count remaining 0 bits
                pix_count += bit_count;
                bit_count = 0;
                if (s < pEnd) {
                    bit_count = 8;
                    c = *s++;
                }
                continue;
            }
            pix_count++;
            bit_count--;
            c <<= 1;
            if (bit_count == 0 && s < pEnd) {
                bit_count = 8;
                c = *s++;
            }
        }
        // we've hit the first 1 bit, skip the source pixels we've counted
        i += pix_count;
        sPixels += pix_count*2; // skip RGB565 pixels
        // Count the number of consecutive pixels to draw
        pix_count = 0;
        while (bit_count && (c & 0x80) == 0x80) { // count 1's
            if (c == 0xff) {
                pix_count += 8;
                bit_count = 0;
                if (s < pEnd) {
                    c = *s++;
                    bit_count = 8;
                }
                continue;
            }
            pix_count++;
            bit_count--;
            c <<= 1;
            if (bit_count == 0 && s < pEnd) {
                bit_count = 8;
                c = *s++;
            }
        }
        if (pix_count) {
            spilcdSetPosition(pLCD, x+i, y, pix_count, 1, iFlags);
            spilcdWriteDataBlock(pLCD, sPixels, pix_count*2, iFlags);
        }
        i += pix_count;
        sPixels += pix_count*2;
    } // while counting pixels
} /* spilcdWritePixelsMasked() */
//
// Wrapper function to control a GPIO line
//
static void myPinWrite(int iPin, int iValue)
{
	digitalWrite(iPin, (iValue) ? HIGH: LOW);
} /* myPinWrite() */

//
// Choose the gamma curve between 2 choices (0/1)
// ILI9341 only
//
int spilcdSetGamma(SPILCD *pLCD, int iMode)
{
int i;
unsigned char *sE0, *sE1;

	if (iMode < 0 || iMode > 1 || pLCD->iLCDType != LCD_ILI9341)
		return 1;
	if (iMode == 0)
	{
		sE0 = (unsigned char *)ucE0_0;
		sE1 = (unsigned char *)ucE1_0;
	}
	else
	{
		sE0 = (unsigned char *)ucE0_1;
		sE1 = (unsigned char *)ucE1_1;
	}
	spilcdWriteCommand(pLCD, 0xe0);
	for(i=0; i<16; i++)
	{
		spilcdWriteData8(pLCD, pgm_read_byte(sE0++));
	}
	spilcdWriteCommand(pLCD, 0xe1);
	for(i=0; i<16; i++)
	{
		spilcdWriteData8(pLCD, pgm_read_byte(sE1++));
	}

	return 0;
} /* spilcdSetGamma() */
//
// Configure a GPIO pin for input
// Returns 0 if successful, -1 if unavailable
// all input pins are assumed to use internal pullup resistors
// and are connected to ground when pressed
//
int spilcdConfigurePin(int iPin)
{
        if (iPin == -1) // invalid
                return -1;
        pinMode(iPin, INPUT_PULLUP);
        return 0;
} /* spilcdConfigurePin() */
// Read from a GPIO pin
int spilcdReadPin(int iPin)
{
   if (iPin == -1)
      return -1;
   return (digitalRead(iPin) == HIGH);
} /* spilcdReadPin() */
//
// Give bb_spi_lcd two callback functions to talk to the LCD
// useful when not using SPI or providing an optimized interface
//
void spilcdSetCallbacks(SPILCD *pLCD, RESETCALLBACK pfnReset, DATACALLBACK pfnData)
{
    pLCD->pfnDataCallback = pfnData;
    pLCD->pfnResetCallback = pfnReset;
}
//
// Initialize the LCD controller and clear the display
// LED pin is optional - pass as -1 to disable
//
int spilcdInit(SPILCD *pLCD, int iType, int iFlags, int32_t iSPIFreq, int iCS, int iDC, int iReset, int iLED, int iMISOPin, int iMOSIPin, int iCLKPin)
{
unsigned char *s, *d;
int i, iCount;
   
    pLCD->iColStart = pLCD->iRowStart = pLCD->iMemoryX = pLCD->iMemoryY = 0;
    pLCD->iOrientation = 0;
    pLCD->iLCDType = iType;
    pLCD->iLCDFlags = iFlags;
  if (pLCD->pfnResetCallback != NULL)
  {
     (*pLCD->pfnResetCallback)();
     goto start_of_init;
  }
#ifndef HAL_ESP32_HAL_H_
    (void)iMISOPin;
#endif
#ifdef __AVR__
{ // setup fast I/O
  uint8_t port;
    port = digitalPinToPort(iDC);
    outDC = portOutputRegister(port);
    bitDC = digitalPinToBitMask(iDC);
    if (iCS != -1) {
      port = digitalPinToPort(iCS);
      outCS = portOutputRegister(port);
      bitCS = digitalPinToBitMask(iCS);
    }
}
#endif

    pLCD->iLEDPin = -1; // assume it's not defined
	if (iType <= LCD_INVALID || iType >= LCD_VALID_MAX)
	{
#ifndef _LINUX_
		Serial.println("Unsupported display type\n");
#endif // _LINUX_
		return -1;
	}
#ifndef _LINUX_
    pLCD->iSPIMode = SPI_MODE0; //(iType == LCD_ST7789_NOCS || iType == LCD_ST7789_135) ? SPI_MODE3 : SPI_MODE0;
#endif
    pLCD->iSPISpeed = iSPIFreq;
	pLCD->iScrollOffset = 0; // current hardware scroll register value

    pLCD->iDCPin = iDC;
    pLCD->iCSPin = iCS;
    pLCD->iResetPin = iReset;
    pLCD->iLEDPin = iLED;
	if (pLCD->iDCPin == -1)
	{
#ifndef _LINUX_
		Serial.println("One or more invalid GPIO pin numbers\n");
#endif
		return -1;
	}
    if (iFlags & FLAGS_BITBANG)
    {
        pinMode(pLCD->iMOSIPin, OUTPUT);
        pinMode(pLCD->iCLKPin, OUTPUT);
        goto skip_spi_init;
    }
#ifdef ESP32_DMA
    if (!iStarted) {
    esp_err_t ret;
    
    memset(&buscfg, 0, sizeof(buscfg));
    buscfg.miso_io_num = iMISOPin;
    buscfg.mosi_io_num = iMOSIPin;
    buscfg.sclk_io_num = iCLKPin;
    buscfg.max_transfer_sz=240*9*2;
    buscfg.quadwp_io_num=-1;
    buscfg.quadhd_io_num=-1;
    //Initialize the SPI bus
    ret=spi_bus_initialize(VSPI_HOST, &buscfg, 1);
    assert(ret==ESP_OK);
    
    memset(&devcfg, 0, sizeof(devcfg));
    devcfg.clock_speed_hz = iSPIFreq;
    devcfg.mode = pLCD->iSPIMode;                         //SPI mode 0 or 3
    devcfg.spics_io_num = -1;               //CS pin, set to -1 to disable since we handle it outside of the master driver
    devcfg.queue_size = 2;                          //We want to be able to queue 2 transactions at a time
//    devcfg.pre_cb = spi_pre_transfer_callback;  //Specify pre-transfer callback to handle D/C line
    devcfg.post_cb = spi_post_transfer_callback;
    devcfg.flags = SPI_DEVICE_NO_DUMMY; // allow speeds > 26Mhz
//    devcfg.flags = SPI_DEVICE_HALFDUPLEX; // this disables SD card access
    //Attach the LCD to the SPI bus
    ret=spi_bus_add_device(VSPI_HOST, &devcfg, &spi);
    assert(ret==ESP_OK);
    memset(&trans[0], 0, sizeof(spi_transaction_t));
        iStarted = 1; // don't re-initialize this code
    }
#else
#if defined( HAL_ESP32_HAL_H_ ) || defined( RISCV )
if (iMISOPin != iMOSIPin)
    SPI.begin(iCLKPin, iMISOPin, iMOSIPin, iCS);
else
    SPI.begin();
#else
#ifdef _LINUX_
    iHandle = AIOOpenSPI(0, iSPIFreq); // DEBUG - open SPI channel 0 
#else
#ifdef ARDUINO_ARCH_RP2040
  pSPI->begin();
#else
  mySPI.begin(); // simple Arduino init (e.g. AVR)
#endif
#ifdef ARDUINO_SAMD_ZERO

  myDMA.setTrigger(mySPI.getDMAC_ID_TX());
  myDMA.setAction(DMA_TRIGGER_ACTON_BEAT);

  stat = myDMA.allocate();
  desc = myDMA.addDescriptor(
    ucTXBuf,                    // move data from here
    (void *)(mySPI.getDataRegister()),
    100,                      // this many...
    DMA_BEAT_SIZE_BYTE,               // bytes/hword/words
    true,                             // increment source addr?
    false);                           // increment dest addr?

  myDMA.setCallback(dma_callback);
#endif // ARDUINO_SAMD_ZERO

#endif // _LINUX_
#endif
#endif
//
// Start here if bit bang enabled
//
    skip_spi_init:
    if (pLCD->iCSPin != -1)
    {
        pinMode(pLCD->iCSPin, OUTPUT);
        myPinWrite(pLCD->iCSPin, HIGH);
    }
	pinMode(pLCD->iDCPin, OUTPUT);
	if (pLCD->iLEDPin != -1)
    {
		pinMode(pLCD->iLEDPin, OUTPUT);
        myPinWrite(pLCD->iLEDPin, 1); // turn on the backlight
    }
	if (pLCD->iResetPin != -1)
	{
        pinMode(pLCD->iResetPin, OUTPUT);
		myPinWrite(pLCD->iResetPin, 1);
		delayMicroseconds(60000);
		myPinWrite(pLCD->iResetPin, 0); // reset the controller
		delayMicroseconds(60000);
		myPinWrite(pLCD->iResetPin, 1);
		delayMicroseconds(60000);
	}
	if (pLCD->iLCDType != LCD_SSD1351 && pLCD->iLCDType != LCD_SSD1331) // no backlight and no soft reset on OLED
	{
        spilcdWriteCommand(pLCD, 0x01); // software reset
        delayMicroseconds(60000);

        spilcdWriteCommand(pLCD, 0x11);
        delayMicroseconds(60000);
        delayMicroseconds(60000);
	}
start_of_init:
    d = &ucRXBuf[128]; // point to middle otherwise full duplex SPI will overwrite our data
	if (pLCD->iLCDType == LCD_ST7789 || pLCD->iLCDType == LCD_ST7789_240 || pLCD->iLCDType == LCD_ST7789_135 || pLCD->iLCDType == LCD_ST7789_NOCS)
	{
        uint8_t iBGR = (pLCD->iLCDFlags & FLAGS_SWAP_RB) ? 8:0;
		s = (unsigned char *)&uc240x240InitList[0];
        memcpy_P(d, s, sizeof(uc240x240InitList));
        s = d;
        s[6] = 0x00 + iBGR;
        pLCD->iCurrentWidth = pLCD->iWidth = 240;
        pLCD->iCurrentHeight = pLCD->iHeight = 320;
		if (pLCD->iLCDType == LCD_ST7789_240 || pLCD->iLCDType == LCD_ST7789_NOCS)
		{
            pLCD->iCurrentWidth = pLCD->iWidth = 240;
            pLCD->iCurrentHeight = pLCD->iHeight = 240;
		}
		else if (pLCD->iLCDType == LCD_ST7789_135)
		{
            pLCD->iCurrentWidth = pLCD->iWidth = 135;
            pLCD->iCurrentHeight = pLCD->iHeight = 240;
            pLCD->iColStart = pLCD->iMemoryX = 52;
            pLCD->iRowStart = pLCD->iMemoryY = 40;
		}
        pLCD->iLCDType = LCD_ST7789; // treat them the same from here on
	} // ST7789
    else if (pLCD->iLCDType == LCD_SSD1331)
    {
        s = (unsigned char *)ucSSD1331InitList;
        memcpy_P(d, ucSSD1331InitList, sizeof(ucSSD1331InitList));
        s = d;

        pLCD->iCurrentWidth = pLCD->iWidth = 96;
        pLCD->iCurrentHeight = pLCD->iHeight = 64;

        if (pLCD->iLCDFlags & FLAGS_SWAP_RB)
        { // copy to RAM to modify it
            s[6] = 0x76;
        }
    }
	else if (pLCD->iLCDType == LCD_SSD1351)
	{
		s = (unsigned char *)ucOLEDInitList; // do the commands manually
                memcpy_P(d, s, sizeof(ucOLEDInitList));
        pLCD->iCurrentWidth = pLCD->iWidth = 128;
        pLCD->iCurrentHeight = pLCD->iHeight = 128;
	}
    // Send the commands/parameters to initialize the LCD controller
	else if (pLCD->iLCDType == LCD_ILI9341)
	{  // copy to RAM to modify
        s = (unsigned char *)uc240InitList;
        memcpy_P(d, s, sizeof(uc240InitList));
        s = d;
        if (pLCD->iLCDFlags & FLAGS_INVERT)
            s[52] = 0x21; // invert pixels
        else
            s[52] = 0x20; // non-inverted
        pLCD->iCurrentWidth = pLCD->iWidth = 240;
        pLCD->iCurrentHeight = pLCD->iHeight = 320;
	}
    else if (pLCD->iLCDType == LCD_SSD1283A)
    {
        s = (unsigned char *)uc132InitList;
        memcpy_P(d, s, sizeof(uc132InitList));
        pLCD->iCurrentWidth = pLCD->iWidth = 132;
        pLCD->iCurrentHeight = pLCD->iHeight = 132;
    }
	else if (pLCD->iLCDType == LCD_ILI9342)
	{
		s = (unsigned char *)uc320InitList;
        memcpy_P(d, s, sizeof(uc320InitList));
        s = d;
        pLCD->iCurrentWidth = pLCD->iWidth = 320;
        pLCD->iCurrentHeight = pLCD->iHeight = 240;
	}
	else if (pLCD->iLCDType == LCD_HX8357)
	{
        spilcdWriteCommand(pLCD, 0xb0);
        spilcdWriteData16(pLCD, 0x00FF, DRAW_TO_LCD);
        spilcdWriteData16(pLCD, 0x0001, DRAW_TO_LCD);
        delayMicroseconds(60000);

        s = (unsigned char *)uc480InitList;
        memcpy_P(d, s, sizeof(uc480InitList));
        s = d;
        pLCD->iCurrentWidth = pLCD->iWidth = 320;
        pLCD->iCurrentHeight = pLCD->iHeight = 480;
	}
        else if (pLCD->iLCDType == LCD_ILI9486)
        {
            uint8_t ucBGRFlags;
            s = (unsigned char *)ucILI9486InitList;
            memcpy_P(d, s, sizeof(ucILI9486InitList));
            s = d;
            ucBGRFlags = 0xa; // normal direction, RGB color order
            if (pLCD->iLCDFlags & FLAGS_INVERT)
               d[63] = 0x21; // invert display command
            if (pLCD->iLCDFlags & FLAGS_SWAP_RB)
            {
                ucBGRFlags |= 8;
               d[66] = ucBGRFlags;
            }
            pLCD->iCurrentWidth = pLCD->iWidth = 320;
            pLCD->iCurrentHeight = pLCD->iHeight = 480;
        }
    else if (pLCD->iLCDType == LCD_ILI9225)
    {
        uint8_t iBGR = 0x10;
        if (pLCD->iLCDFlags & FLAGS_SWAP_RB)
            iBGR = 0;
        s = (unsigned char *)ucILI9225InitList;
        memcpy_P(d, s, sizeof(ucILI9225InitList));
        s = d;
//        if (iFlags & FLAGS_INVERT)
//           s[55] = 0x21; // invert on
//        else
//           s[55] = 0x20; // invert off
        s[74] = iBGR;
        pLCD->iCurrentWidth = pLCD->iWidth = 176;
        pLCD->iCurrentHeight = pLCD->iHeight = 220;
    }
    else if (pLCD->iLCDType == LCD_ST7735S || pLCD->iLCDType == LCD_ST7735S_B)
    {
        uint8_t iBGR = 0;
        if (pLCD->iLCDFlags & FLAGS_SWAP_RB)
            iBGR = 8;
        s = (unsigned char *)uc80InitList;
        memcpy_P(d, s, sizeof(uc80InitList));
        s = d;
        if (pLCD->iLCDFlags & FLAGS_INVERT)
           s[55] = 0x21; // invert on
        else
           s[55] = 0x20; // invert off
        s[5] = 0x00 + iBGR; // normal orientation
        pLCD->iCurrentWidth = pLCD->iWidth = 80;
        pLCD->iCurrentHeight = pLCD->iHeight = 160;
        if (pLCD->iLCDType == LCD_ST7735S_B)
        {
            pLCD->iLCDType = LCD_ST7735S; // the rest is the same
            pLCD->iColStart = pLCD->iMemoryX = 26; // x offset of visible area
            pLCD->iRowStart = pLCD->iMemoryY = 1;
        }
        else
        {
            pLCD->iColStart = pLCD->iMemoryX = 24;
        }
    }
	else // ST7735R
	{
		s = (unsigned char *)uc128InitList;
                memcpy_P(d, s, sizeof(uc128InitList));
                s = d;
        pLCD->iCurrentWidth = pLCD->iWidth = 128;
        pLCD->iCurrentHeight = pLCD->iHeight = 160;
	}

	iCount = 1;
    bSetPosition = 1; // don't let the data writes affect RAM
    s = d; // start of RAM copy of our data
	while (iCount)
	{
		iCount = *s++;
		if (iCount != 0)
		{
               unsigned char uc;
               if (iCount == LCD_DELAY)
               {
                 uc = *s++;
                 delay(uc);
               }
               else
               {
                   uc = *s++;
                 if (pLCD->iLCDType == LCD_ILI9225) // 16-bit commands and data
                 {
                     uint8_t uc2;
                     uc2 = *s++;
                     spilcdWriteCommand16(pLCD, (uc << 8) | uc2);
                     for (i=0; i<iCount-1; i++)
                     {
                        uc = *s++;
                         uc2 = *s++;
                        spilcdWriteData16(pLCD, (uc << 8) | uc2, DRAW_TO_LCD);
                     } // for i
                 } else {
                     spilcdWriteCommand(pLCD, uc);
                     for (i=0; i<iCount-1; i++)
                     {
                        uc = *s++;
                     // hackhackhack
                     // the ssd1331 is kind of like the ssd1306 in that it expects the parameters
                     // to each command to have the DC pin LOW while being sent
                        if (pLCD->iLCDType != LCD_SSD1331)
                            spilcdWriteData8(pLCD, uc);
                        else
                            spilcdWriteCommand(pLCD, uc);
                     } // for i
                 }
              }
          }
	  }
        bSetPosition = 0;
	if (pLCD->iLCDType != LCD_SSD1351 && pLCD->iLCDType != LCD_SSD1331)
	{
		spilcdWriteCommand(pLCD, 0x11); // sleep out
		delayMicroseconds(60000);
		spilcdWriteCommand(pLCD, 0x29); // Display ON
		delayMicroseconds(10000);
	}
//	spilcdFill(0, 1); // erase memory
	spilcdScrollReset(pLCD);
   
	return 0;

} /* spilcdInit() */

//
// Reset the scroll position to 0
//
void spilcdScrollReset(SPILCD *pLCD)
{
	pLCD->iScrollOffset = 0;
	if (pLCD->iLCDType == LCD_SSD1351)
	{
		spilcdWriteCommand(pLCD, 0xa1); // set scroll start line
		spilcdWriteData8(pLCD, 0x00);
		spilcdWriteCommand(pLCD, 0xa2); // display offset
		spilcdWriteData8(pLCD, 0x00);
		return;
	}
    else if (pLCD->iLCDType == LCD_SSD1331)
    {
        spilcdWriteCommand(pLCD, 0xa1);
        spilcdWriteCommand(pLCD, 0x00);
        spilcdWriteCommand(pLCD, 0xa2);
        spilcdWriteCommand(pLCD, 0x00);
        return;
    }
	spilcdWriteCommand(pLCD, 0x37); // scroll start address
	spilcdWriteData16(pLCD, 0, DRAW_TO_LCD);
	if (pLCD->iLCDType == LCD_HX8357)
	{
		spilcdWriteData16(pLCD, 0, DRAW_TO_LCD);
	}
} /* spilcdScrollReset() */

//
// Scroll the screen N lines vertically (positive or negative)
// The value given represents a delta which affects the current scroll offset
// If iFillColor != -1, the newly exposed lines will be filled with that color
//
void spilcdScroll(SPILCD *pLCD, int iLines, int iFillColor)
{
	pLCD->iScrollOffset = (pLCD->iScrollOffset + iLines) % pLCD->iHeight;
	if (pLCD->iLCDType == LCD_SSD1351)
	{
		spilcdWriteCommand(pLCD, 0xa1); // set scroll start line
		spilcdWriteData8(pLCD, pLCD->iScrollOffset);
		return;
	}
    else if (pLCD->iLCDType == LCD_SSD1331)
    {
        spilcdWriteCommand(pLCD, 0xa1);
        spilcdWriteCommand(pLCD, pLCD->iScrollOffset);
        return;
    }
	else
	{
		spilcdWriteCommand(pLCD, 0x37); // Vertical scrolling start address
		if (pLCD->iLCDType == LCD_ILI9341 || pLCD->iLCDType == LCD_ILI9342 || pLCD->iLCDType == LCD_ST7735R || pLCD->iLCDType == LCD_ST7789 || pLCD->iLCDType == LCD_ST7789_135 || pLCD->iLCDType == LCD_ST7735S)
		{
			spilcdWriteData16(pLCD, pLCD->iScrollOffset, DRAW_TO_LCD);
		}
		else
		{
			spilcdWriteData16(pLCD, pLCD->iScrollOffset >> 8, DRAW_TO_LCD);
			spilcdWriteData16(pLCD, pLCD->iScrollOffset & -1, DRAW_TO_LCD);
		}
	}
	if (iFillColor != -1) // fill the exposed lines
	{
	int i, iStart;
	uint16_t *usTemp = (uint16_t *)ucRXBuf;
	uint32_t *d;
	uint32_t u32Fill;
		// quickly prepare a full line's worth of the color
		u32Fill = (iFillColor >> 8) | ((iFillColor & -1) << 8);
		u32Fill |= (u32Fill << 16);
		d = (uint32_t *)&usTemp[0];
		for (i=0; i<pLCD->iWidth/2; i++)
			*d++ = u32Fill;
		if (iLines < 0)
		{
			iStart = 0;
			iLines = 0 - iLines;
		}
		else
			iStart = pLCD->iHeight - iLines;
        spilcdSetPosition(pLCD, 0, iStart, pLCD->iWidth, iLines, DRAW_TO_LCD);
		for (i=0; i<iLines; i++)
		{
			myspiWrite(pLCD, (unsigned char *)usTemp, pLCD->iWidth*2, MODE_DATA, DRAW_TO_LCD);
		}
	}

} /* spilcdScroll() */
//
// Draw a 24x24 RGB565 tile scaled to 40x40
// The main purpose of this function is for GameBoy emulation
// Since the original display is 160x144, this function allows it to be
// stretched 166% larger (266x240). Not a perfect fit for 320x240, but better
// Each group of 3x3 pixels becomes a group of 5x5 pixels by averaging the pixels
//
// +-+-+-+ becomes +----+----+----+----+----+
// |A|B|C|         |A   |ab  |B   |bc  |C   |
// +-+-+-+         +----+----+----+----+----+
// |D|E|F|         |ad  |abde|be  |becf|cf  |
// +-+-+-+         +----+----+----+----+----+
// |G|H|I|         |D   |de  |E   |ef  |F   |
// +-+-+-+         +----+----+----+----+----+
//                 |dg  |dgeh|eh  |ehfi|fi  |
//                 +----+----+----+----+----+
//                 |G   |gh  |H   |hi  |I   |
//                 +----+----+----+----+----+
//
// The x/y coordinates will be scaled as well
//
int spilcdDraw53Tile(SPILCD *pLCD, int x, int y, int cx, int cy, unsigned char *pTile, int iPitch, int iFlags)
{
    int i, j, iPitch16;
    uint16_t *s, *d;
    uint16_t u32A, u32B, u32C, u32D, u32E, u32F;
    uint16_t t1, t2, u32ab, u32bc, u32de, u32ef, u32ad, u32be, u32cf;
    uint16_t u32Magic = 0xf7de;
    
    // scale coordinates for stretching
    x = (x * 5)/3;
    y = (y * 5)/3;
    iPitch16 = iPitch/2;
    if (cx < 24 || cy < 24)
        memset(ucTXBuf, 0, 40*40*2);
    for (j=0; j<cy/3; j++) // 8 blocks of 3 lines
    {
        s = (uint16_t *)&pTile[j*3*iPitch];
        d = (uint16_t *)&ucTXBuf[j*40*5*2];
        for (i=0; i<cx-2; i+=3) // source columns (3 at a time)
        {
            u32A = s[i];
            u32B = s[i+1];
            u32C = s[i+2];
            u32D = s[i+iPitch16];
            u32E = s[i+iPitch16+1];
            u32F = s[i+iPitch16 + 2];
            u32bc = u32ab = (u32B & u32Magic) >> 1;
            u32ab += ((u32A & u32Magic) >> 1);
            u32bc += (u32C & u32Magic) >> 1;
            u32de = u32ef = ((u32E & u32Magic) >> 1);
            u32de += ((u32D & u32Magic) >> 1);
            u32ef += ((u32F & u32Magic) >> 1);
            u32ad = ((u32A & u32Magic) >> 1) + ((u32D & u32Magic) >> 1);
            u32be = ((u32B & u32Magic) >> 1) + ((u32E & u32Magic) >> 1);
            u32cf = ((u32C & u32Magic) >> 1) + ((u32F & u32Magic) >> 1);
            // first row
            d[0] = __builtin_bswap16(u32A); // swap byte order
            d[1] = __builtin_bswap16(u32ab);
            d[2] = __builtin_bswap16(u32B);
            d[3] = __builtin_bswap16(u32bc);
            d[4] = __builtin_bswap16(u32C);
            // second row
            t1 = ((u32ab & u32Magic) >> 1) + ((u32de & u32Magic) >> 1);
            t2 = ((u32be & u32Magic) >> 1) + ((u32cf & u32Magic) >> 1);
            d[40] = __builtin_bswap16(u32ad);
            d[41] = __builtin_bswap16(t1);
            d[42] = __builtin_bswap16(u32be);
            d[43] = __builtin_bswap16(t2);
            d[44] = __builtin_bswap16(u32cf);
            // third row
            d[80] = __builtin_bswap16(u32D);
            d[81] = __builtin_bswap16(u32de);
            d[82] = __builtin_bswap16(u32E);
            d[83] = __builtin_bswap16(u32ef);
            d[84] = __builtin_bswap16(u32F);
            // fourth row
            u32A = s[i+iPitch16*2];
            u32B = s[i+iPitch16*2 + 1];
            u32C = s[i+iPitch16*2 + 2];
            u32bc = u32ab = (u32B & u32Magic) >> 1;
            u32ab += ((u32A & u32Magic) >> 1);
            u32bc += (u32C & u32Magic) >> 1;
            u32ad = ((u32A & u32Magic) >> 1) + ((u32D & u32Magic) >> 1);
            u32be = ((u32B & u32Magic) >> 1) + ((u32E & u32Magic) >> 1);
            u32cf = ((u32C & u32Magic) >> 1) + ((u32F & u32Magic) >> 1);
            t1 = ((u32ab & u32Magic) >> 1) + ((u32de & u32Magic) >> 1);
            t2 = ((u32be & u32Magic) >> 1) + ((u32cf & u32Magic) >> 1);
            d[120] = __builtin_bswap16(u32ad);
            d[121] = __builtin_bswap16(t1);
            d[122] = __builtin_bswap16(u32be);
            d[123] = __builtin_bswap16(t2);
            d[124] = __builtin_bswap16(u32cf);
            // fifth row
            d[160] = __builtin_bswap16(u32A);
            d[161] = __builtin_bswap16(u32ab);
            d[162] = __builtin_bswap16(u32B);
            d[163] = __builtin_bswap16(u32bc);
            d[164] = __builtin_bswap16(u32C);
            d += 5;
        } // for i
    } // for j
    spilcdSetPosition(pLCD, x, y, 40, 40, iFlags);
    myspiWrite(pLCD, ucTXBuf, 40*40*2, MODE_DATA, iFlags);
    return 0;
} /* spilcdDraw53Tile() */
//
// Draw a NxN RGB565 tile
// This reverses the pixel byte order and sets a memory "window"
// of pixels so that the write can occur in one shot
//
int spilcdDrawTile(SPILCD *pLCD, int x, int y, int iTileWidth, int iTileHeight, unsigned char *pTile, int iPitch, int iFlags)
{
    int i, j;
    uint32_t ul32;
    unsigned char *s, *d;
    
    if (iTileWidth*iTileHeight > 2048)
        return -1; // tile must fit in 4k SPI block size
    
        uint16_t *s16, *d16;
        // First convert to big-endian order
        d16 = (uint16_t *)ucRXBuf;
        for (j=0; j<iTileHeight; j++)
        {
            s16 = (uint16_t*)&pTile[j*iPitch];
            for (i=0; i<iTileWidth; i++)
            {
                *d16++ = __builtin_bswap16(*s16++);
            } // for i;
        } // for j
        spilcdSetPosition(pLCD, x, y, iTileWidth, iTileHeight, iFlags);
        myspiWrite(pLCD, ucRXBuf, iTileWidth*iTileHeight*2, MODE_DATA, iFlags);
    return 0;
} /* spilcdDrawTile() */

//
// Draw a 16x16 tile as 16x14 (with pixel averaging)
// This is for drawing 160x144 video games onto a 160x128 display
// It is assumed that the display is set to LANDSCAPE orientation
//
int spilcdDrawSmallTile(SPILCD *pLCD, int x, int y, unsigned char *pTile, int iPitch, int iFlags)
{
    unsigned char ucTemp[448];
    int i, j, iPitch32;
    uint16_t *d;
    uint32_t *s;
    uint32_t u32A, u32B, u32a, u32b, u32C, u32D;
    uint32_t u32Magic = 0xf7def7de;
    uint32_t u32Mask = 0xffff;
    
    // scale y coordinate for shrinking
    y = (y * 7)/8;
    iPitch32 = iPitch/4;
    for (j=0; j<16; j+=2) // 16 source lines (2 at a time)
    {
        s = (uint32_t *)&pTile[j * 2];
        d = (uint16_t *)&ucTemp[j*28];
        for (i=0; i<16; i+=2) // 16 source columns (2 at a time)
        {
            u32A = s[(15-i)*iPitch32]; // read A+C
            u32B = s[(14-i)*iPitch32]; // read B+D
            u32C = u32A >> 16;
            u32D = u32B >> 16;
            u32A &= u32Mask;
            u32B &= u32Mask;
            if (i == 0 || i == 8) // pixel average a pair
            {
                u32a = (u32A & u32Magic) >> 1;
                u32a += ((u32B & u32Magic) >> 1);
                u32b = (u32C & u32Magic) >> 1;
                u32b += ((u32D & u32Magic) >> 1);
                d[0] = __builtin_bswap16(u32a);
                d[14] = __builtin_bswap16(u32b);
                d++;
            }
            else
            {
                d[0] = __builtin_bswap16(u32A);
                d[1] = __builtin_bswap16(u32B);
                d[14] = __builtin_bswap16(u32C);
                d[15] = __builtin_bswap16(u32D);
                d += 2;
            }
        } // for i
    } // for j
    spilcdSetPosition(pLCD, x, y, 16, 14, iFlags);
    myspiWrite(pLCD, ucTemp, 448, MODE_DATA, iFlags);
    return 0;
} /* spilcdDrawSmallTile() */

//
// Draw a 16x16 RGB656 tile with select rows/columns removed
// the mask contains 1 bit for every column/row that should be drawn
// For example, to skip the first 2 columns, the mask value would be 0xfffc
//
int spilcdDrawMaskedTile(SPILCD *pLCD, int x, int y, unsigned char *pTile, int iPitch, int iColMask, int iRowMask, int iFlags)
{
    unsigned char ucTemp[512]; // fix the byte order first to write it more quickly
    int i, j;
    unsigned char *s, *d;
    int iNumCols, iNumRows, iTotalSize;
    
    iNumCols = __builtin_popcount(iColMask);
    iNumRows = __builtin_popcount(iRowMask);
    iTotalSize = iNumCols * iNumRows * 2;
    
        uint16_t *s16 = (uint16_t *)s;
        uint16_t u16, *d16 = (uint16_t *)d;
        int iMask;
        
        // First convert to big-endian order
        d16 = (uint16_t *)ucTemp;
        for (j=0; j<16; j++)
        {
            if ((iRowMask & (1<<j)) == 0) continue; // skip row
            s16 = (uint16_t *)&pTile[j*iPitch];
            iMask = iColMask;
            for (i=0; i<16; i++)
            {
                u16 = *s16++;
                if (iMask & 1)
                {
                    *d16++ = __builtin_bswap16(u16);
                }
                iMask >>= 1;
            } // for i;
        } // for j
        spilcdSetPosition(pLCD, x, y, iNumCols, iNumRows, iFlags);
        myspiWrite(pLCD, ucTemp, iTotalSize, MODE_DATA, iFlags);
    return 0;
} /* spilcdDrawMaskedTile() */

//
// Draw a 16x16 tile as 16x13 (with priority to non-black pixels)
// This is for drawing a 224x288 image onto a 320x240 display in landscape
//
int spilcdDrawRetroTile(SPILCD *pLCD, int x, int y, unsigned char *pTile, int iPitch, int iFlags)
{
    unsigned char ucTemp[416];
    int i, j, iPitch16;
    uint16_t *s, *d, u16A, u16B;
    
    // scale y coordinate for shrinking
    y = (y * 13)/16;
    iPitch16 = iPitch/2;
    for (j=0; j<16; j++) // 16 destination columns
    {
        s = (uint16_t *)&pTile[j * 2];
        d = (uint16_t *)&ucTemp[j*26];
        for (i=0; i<16; i++) // 13 actual source rows
        {
            if (i == 0 || i == 5 || i == 10) // combined pixels
            {
                u16A = s[(15-i)*iPitch16];
                u16B = s[(14-i)*iPitch16];
                if (u16A == 0)
                    *d++ = __builtin_bswap16(u16B);
                else
                    *d++ = __builtin_bswap16(u16A);
                i++; // advance count since we merged 2 lines
            }
            else // just copy
            {
                *d++ = __builtin_bswap16(s[(15-i)*iPitch16]);
            }
        } // for i
    } // for j
    spilcdSetPosition(pLCD, x, y, 16, 13, iFlags);
    myspiWrite(pLCD, ucTemp, 416, MODE_DATA, iFlags);
    return 0;
    
} /* spilcdDrawRetroTile() */
//
// Draw a NxN RGB565 tile
// This reverses the pixel byte order and sets a memory "window"
// of pixels so that the write can occur in one shot
// Scales the tile by 150% (for GameBoy/GameGear)
//
int spilcdDrawTile150(SPILCD *pLCD, int x, int y, int iTileWidth, int iTileHeight, unsigned char *pTile, int iPitch, int iFlags)
{
    int i, j, iPitch32, iLocalPitch;
    uint32_t ul32A, ul32B, ul32Avg, ul32Avg2;
    uint16_t u16Avg, u16Avg2;
    uint32_t u32Magic = 0xf7def7de;
    uint16_t u16Magic = 0xf7de;
    uint16_t *d16;
    uint32_t *s32;
    
    if (iTileWidth*iTileHeight > 1365)
        return -1; // tile must fit in 4k SPI block size
    
    iPitch32 = iPitch / 4;
    iLocalPitch = (iTileWidth * 3)/2; // offset to next output line
    d16 = (uint16_t *)ucRXBuf;
    for (j=0; j<iTileHeight; j+=2)
    {
        s32 = (uint32_t*)&pTile[j*iPitch];
        for (i=0; i<iTileWidth; i+=2) // turn 2x2 pixels into 3x3
        {
            ul32A = s32[0];
            ul32B = s32[iPitch32]; // get 2x2 pixels
            // top row
            ul32Avg = ((ul32A & u32Magic) >> 1);
            ul32Avg2 = ((ul32B & u32Magic) >> 1);
            u16Avg = (uint16_t)(ul32Avg + (ul32Avg >> 16)); // average the 2 pixels
            d16[0] = __builtin_bswap16((uint16_t)ul32A); // first pixel
            d16[1] = __builtin_bswap16(u16Avg); // middle (new) pixel
            d16[2] = __builtin_bswap16((uint16_t)(ul32A >> 16)); // 3rd pixel
            u16Avg2 = (uint16_t)(ul32Avg2 + (ul32Avg2 >> 16)); // bottom line averaged pixel
            d16[iLocalPitch] = __builtin_bswap16((uint16_t)(ul32Avg + ul32Avg2)); // vertical average
            d16[iLocalPitch+2] = __builtin_bswap16((uint16_t)((ul32Avg + ul32Avg2)>>16)); // vertical average
            d16[iLocalPitch*2] = __builtin_bswap16((uint16_t)ul32B); // last line 1st
            d16[iLocalPitch*2+1] = __builtin_bswap16(u16Avg2); // middle pixel
            d16[iLocalPitch*2+2] = __builtin_bswap16((uint16_t)(ul32B >> 16)); // 3rd pixel
            u16Avg = (u16Avg & u16Magic) >> 1;
            u16Avg2 = (u16Avg2 & u16Magic) >> 1;
            d16[iLocalPitch+1] = __builtin_bswap16(u16Avg + u16Avg2); // middle pixel
            d16 += 3;
            s32 += 1;
        } // for i;
        d16 += iLocalPitch*2; // skip lines we already output
    } // for j
    spilcdSetPosition(pLCD, (x*3)/2, (y*3)/2, (iTileWidth*3)/2, (iTileHeight*3)/2, iFlags);
    myspiWrite(pLCD, ucRXBuf, (iTileWidth*iTileHeight*9)/2, MODE_DATA, iFlags);
    return 0;
} /* spilcdDrawTile150() */

//
// Draw a 1-bpp pattern with the given color and translucency
// 1 bits are drawn as color, 0 are transparent
// The translucency value can range from 1 (barely visible) to 32 (fully opaque)
// If there is a backbuffer, the bitmap is draw only into memory
// If there is no backbuffer, the bitmap is drawn on the screen with a black background color
//
void spilcdDrawPattern(SPILCD *pLCD, uint8_t *pPattern, int iSrcPitch, int iDestX, int iDestY, int iCX, int iCY, uint16_t usColor, int iTranslucency)
{
    int x, y;
    uint8_t *s, uc, ucMask;
    uint16_t us, *d;
    uint32_t ulMask = 0x07e0f81f; // this allows all 3 values to be multipled at once
    uint32_t ulSrcClr, ulDestClr, ulDestTrans;
    
    ulDestTrans = 32-iTranslucency; // inverted to combine src+dest
    ulSrcClr = (usColor & 0xf81f) | ((uint32_t)(usColor & 0x06e0) << 16); // shift green to upper 16-bits
    ulSrcClr *= iTranslucency; // prepare for color blending
    if (iDestX+iCX > pLCD->iCurrentWidth) // trim to fit on display
        iCX = (pLCD->iCurrentWidth - iDestX);
    if (iDestY+iCY > pLCD->iCurrentHeight)
        iCY = (pLCD->iCurrentHeight - iDestY);
    if (pPattern == NULL || iDestX < 0 || iDestY < 0 || iCX <=0 || iCY <= 0 || iTranslucency < 1 || iTranslucency > 32)
        return;
    if (pLCD->pBackBuffer == NULL) // no back buffer, draw opaque colors
    {
      uint16_t u16Clr;
      u16Clr = (usColor >> 8) | (usColor << 8); // swap low/high bytes
      spilcdSetPosition(pLCD, iDestX, iDestY, iCX, iCY, DRAW_TO_LCD);
      for (y=0; y<iCY; y++)
      {
        s = &pPattern[y * iSrcPitch];
        ucMask = uc = 0;
        d = (uint16_t *)&ucTXBuf[0];
        for (x=0; x<iCX; x++)
        {
            ucMask >>= 1;
            if (ucMask == 0)
            {
                ucMask = 0x80;
                uc = *s++;
            }
            if (uc & ucMask) // active pixel
               *d++ = u16Clr;
            else
               *d++ = 0;
        } // for x
        myspiWrite(pLCD, ucTXBuf, iCX*2, MODE_DATA, DRAW_TO_LCD);
      } // for y
      return;
    }
    for (y=0; y<iCY; y++)
    {
        int iDelta;
        iDelta = 1;
        d = (uint16_t *)&pLCD->pBackBuffer[((iDestY+y)*pLCD->iScreenPitch) + (iDestX*2)];
        s = &pPattern[y * iSrcPitch];
        ucMask = uc = 0;
        for (x=0; x<iCX; x++)
        {
            ucMask >>= 1;
            if (ucMask == 0)
            {
                ucMask = 0x80;
                uc = *s++;
            }
            if (uc & ucMask) // active pixel
            {
                us = d[0]; // read destination pixel
                us = (us >> 8) | (us << 8); // fix the byte order
                // The fast way to combine 2 RGB565 colors
                ulDestClr = (us & 0xf81f) | ((uint32_t)(us & 0x06e0) << 16);
                ulDestClr = (ulDestClr * ulDestTrans);
                ulDestClr += ulSrcClr; // combine old and new colors
                ulDestClr = (ulDestClr >> 5) & ulMask; // done!
                ulDestClr = (ulDestClr >> 16) | (ulDestClr); // move green back into place
                us = (uint16_t)ulDestClr;
                us = (us >> 8) | (us << 8); // swap bytes for LCD
                d[0] = us;
            }
            d += iDelta;
        } // for x
    } // for y
    
}
void spilcdRectangle(SPILCD *pLCD, int x, int y, int w, int h, unsigned short usColor1, unsigned short usColor2, int bFill, int iFlags)
{
unsigned short *usTemp = (unsigned short *)ucRXBuf;
int i, ty, th, iStart;
uint16_t usColor;
    
	// check bounds
	if (x < 0 || x >= pLCD->iCurrentWidth || x+w > pLCD->iCurrentWidth)
		return; // out of bounds
	if (y < 0 || y >= pLCD->iCurrentHeight || y+h > pLCD->iCurrentHeight)
		return;

	ty = y;
	th = h;
	if (bFill)
	{
        int32_t iDR, iDG, iDB; // current colors and deltas
        int32_t iRAcc, iGAcc, iBAcc;
        uint16_t usRInc, usGInc, usBInc;
        iRAcc = iGAcc = iBAcc = 0; // color fraction accumulators
        iDB = (int32_t)(usColor2 & 0x1f) - (int32_t)(usColor1 & 0x1f); // color deltas
        usBInc = (iDB < 0) ? 0xffff : 0x0001;
        iDB = abs(iDB);
        iDR = (int32_t)(usColor2 >> 11) - (int32_t)(usColor1 >> 11);
        usRInc = (iDR < 0) ? 0xf800 : 0x0800;
        iDR = abs(iDR);
        iDG = (int32_t)((usColor2 & 0x06e0) >> 5) - (int32_t)((usColor1 & 0x06e0) >> 5);
        usGInc = (iDG < 0) ? 0xffe0 : 0x0020;
        iDG = abs(iDG);
        iDB = (iDB << 16) / th;
        iDR = (iDR << 16) / th;
        iDG = (iDG << 16) / th;
        spilcdSetPosition(pLCD, x, y, w, h, iFlags);
//	        if (((ty + iScrollOffset) % iHeight) > iHeight-th) // need to write in 2 parts since it won't wrap
//		{
//          	iStart = (iHeight - ((ty+iScrollOffset) % iHeight));
//			for (i=0; i<iStart; i++)
//           		myspiWrite((unsigned char *)usTemp, iStart*iPerLine, MODE_DATA, bRender); // first N lines
//			if (iCurrentWidth == iWidth)
//				spilcdSetPosition(x, y+iStart, w, h-iStart, bRender);
//			else
//				spilcdSetPosition(x+iStart, y, w-iStart, h, bRender);
//			for (i=0; i<th-iStart; i++)
 //         		myspiWrite((unsigned char *)usTemp, iPerLine, MODE_DATA, bRender);
//       		 }
//        	else // can write in one shot
//        	{
			for (i=0; i<h; i++)
            {
                usColor = (usColor1 >> 8) | (usColor1 << 8); // swap byte order
                memset16((uint16_t*)usTemp, usColor, w);
                myspiWrite(pLCD, (unsigned char *)usTemp, w*2, MODE_DATA, iFlags);
                // Update the color components
                iRAcc += iDR;
                if (iRAcc >= 0x10000) // time to increment
                {
                    usColor1 += usRInc;
                    iRAcc -= 0x10000;
                }
                iGAcc += iDG;
                if (iGAcc >= 0x10000) // time to increment
                {
                    usColor1 += usGInc;
                    iGAcc -= 0x10000;
                }
                iBAcc += iDB;
                if (iBAcc >= 0x10000) // time to increment
                {
                    usColor1 += usBInc;
                    iBAcc -= 0x10000;
                }
            }
//        	}
	}
	else // outline
	{
        usColor = (usColor1 >> 8) | (usColor1 << 8); // swap byte order
		// draw top/bottom
		spilcdSetPosition(pLCD, x, y, w, 1, iFlags);
        memset16((uint16_t*)usTemp, usColor, w);
        myspiWrite(pLCD, (unsigned char *)usTemp, w*2, MODE_DATA, iFlags);
		spilcdSetPosition(pLCD, x, y + h-1, w, 1, iFlags);
        memset16((uint16_t*)usTemp, usColor, w);
		myspiWrite(pLCD, (unsigned char *)usTemp, w*2, MODE_DATA, iFlags);
		// draw left/right
		if (((ty + pLCD->iScrollOffset) % pLCD->iHeight) > pLCD->iHeight-th)
		{
			iStart = (pLCD->iHeight - ((ty+pLCD->iScrollOffset) % pLCD->iHeight));
			spilcdSetPosition(pLCD, x, y, 1, iStart, iFlags);
            memset16((uint16_t*)usTemp, usColor, iStart);
			myspiWrite(pLCD, (unsigned char *)usTemp, iStart*2, MODE_DATA, iFlags);
			spilcdSetPosition(pLCD, x+w-1, y, 1, iStart, iFlags);
            memset16((uint16_t*)usTemp, usColor, iStart);
			myspiWrite(pLCD, (unsigned char *)usTemp, iStart*2, MODE_DATA, iFlags);
			// second half
			spilcdSetPosition(pLCD, x,y+iStart, 1, h-iStart, iFlags);
            memset16((uint16_t*)usTemp, usColor, h-iStart);
			myspiWrite(pLCD, (unsigned char *)usTemp, (h-iStart)*2, MODE_DATA, iFlags);
			spilcdSetPosition(pLCD, x+w-1, y+iStart, 1, h-iStart, iFlags);
            memset16((uint16_t*)usTemp, usColor, h-iStart);
			myspiWrite(pLCD, (unsigned char *)usTemp, (h-iStart)*2, MODE_DATA, iFlags);
		}
		else // can do it in 1 shot
		{
			spilcdSetPosition(pLCD, x, y, 1, h, iFlags);
            memset16((uint16_t*)usTemp, usColor, h);
			myspiWrite(pLCD, (unsigned char *)usTemp, h*2, MODE_DATA, iFlags);
			spilcdSetPosition(pLCD, x + w-1, y, 1, h, iFlags);
            memset16((uint16_t*)usTemp, usColor, h);
			myspiWrite(pLCD, (unsigned char *)usTemp, h*2, MODE_DATA, iFlags);
		}
	} // outline
} /* spilcdRectangle() */

//
// Show part or all of the back buffer on the display
// Used after delayed rendering of graphics
//
void spilcdShowBuffer(SPILCD *pLCD, int iStartX, int iStartY, int cx, int cy, int iFlags)
{
    int y;
    uint8_t *s;
    
    if (pLCD->pBackBuffer == NULL)
        return; // nothing to do
    if (iStartX + cx > pLCD->iCurrentWidth || iStartY + cy > pLCD->iCurrentHeight || iStartX < 0 || iStartY < 0)
        return; // invalid area
    spilcdSetPosition(pLCD, iStartX, iStartY, cx, cy, iFlags);
    bSetPosition = 1;
    for (y=iStartY; y<iStartY+cy; y++)
    {
        s = &pLCD->pBackBuffer[(y * pLCD->iCurrentWidth * 2) + iStartX*2];
        myspiWrite(pLCD, s, cx * 2, MODE_DATA, iFlags);
    }
    bSetPosition = 0;
} /* spilcdShowBuffer() */

//
// Sends a command to turn off the LCD display
// Turns off the backlight LED
// Closes the SPI file handle
//
void spilcdShutdown(SPILCD *pLCD)
{
	if (pLCD->iLCDType == LCD_SSD1351 || pLCD->iLCDType == LCD_SSD1331)
		spilcdWriteCommand(pLCD, 0xae); // Display Off
	else
		spilcdWriteCommand(pLCD, 0x28); // Display OFF
	if (pLCD->iLEDPin != -1)
		myPinWrite(pLCD->iLEDPin, 0); // turn off the backlight
    spilcdFreeBackbuffer(pLCD);
#ifdef _LINUX_
	AIOCloseSPI(iHandle);
	AIORemoveGPIO(pLCD->iDCPin);
	AIORemoveGPIO(pLCD->iResetPin);
	AIORemoveGPIO(pLCD->iLEDPin);
#endif // _LINUX_
} /* spilcdShutdown() */

//
// Send a command byte to the LCD controller
// In SPI 8-bit mode, the D/C line must be set
// high during the write
//
void spilcdWriteCommand(SPILCD *pLCD, unsigned char c)
{
unsigned char buf[2];

	buf[0] = c;
	myspiWrite(pLCD, buf, 1, MODE_COMMAND, DRAW_TO_LCD);
} /* spilcdWriteCommand() */

void spilcdWriteCommand16(SPILCD *pLCD, uint16_t us)
{
unsigned char buf[2];

    buf[0] = (uint8_t)(us >> 8);
    buf[1] = (uint8_t)us;
    myspiWrite(pLCD, buf, 2, MODE_COMMAND, DRAW_TO_LCD);
} /* spilcdWriteCommand() */

//
// Write a single byte of data
//
static void spilcdWriteData8(SPILCD *pLCD, unsigned char c)
{
unsigned char buf[2];

	buf[0] = c;
    myspiWrite(pLCD, buf, 1, MODE_DATA, DRAW_TO_LCD);

} /* spilcdWriteData8() */

//
// Write 16-bits of data
// The ILI9341 receives data in big-endian order
// (MSB first)
//
static void spilcdWriteData16(SPILCD *pLCD, unsigned short us, int iFlags)
{
unsigned char buf[2];

    buf[0] = (unsigned char)(us >> 8);
    buf[1] = (unsigned char)us;
    myspiWrite(pLCD, buf, 2, MODE_DATA, iFlags);

} /* spilcdWriteData16() */

//
// Position the "cursor" to the given
// row and column. The width and height of the memory
// 'window' must be specified as well. The controller
// allows more efficient writing of small blocks (e.g. tiles)
// by bounding the writes within a small area and automatically
// wrapping the address when reaching the end of the window
// on the curent row
//
void spilcdSetPosition(SPILCD *pLCD, int x, int y, int w, int h, int iFlags)
{
unsigned char ucBuf[8];

    pLCD->iWindowX = pLCD->iCurrentX = x; pLCD->iWindowY = pLCD->iCurrentY = y;
    pLCD->iWindowCX = w; pLCD->iWindowCY = h;
    pLCD->iOffset = (pLCD->iCurrentWidth * 2 * y) + (x * 2);

    if (!(iFlags & DRAW_TO_LCD)) return; // nothing to do
    bSetPosition = 1; // flag to let myspiWrite know to ignore data writes
    y = (y + pLCD->iScrollOffset) % pLCD->iHeight; // scroll offset affects writing position

    if (pLCD->iLCDType == LCD_ILI9225) // 16-bit commands and data
    { // The display flipping bits don't change the address information, just
        // the horizontal and vertical inc/dec, so we must place the starting
        // address correctly for the 4 different orientations
        int xs=0, xb=0, xe=0, ys=0, yb= 0,ye=0;
        switch (pLCD->iOrientation)
        {
            case LCD_ORIENTATION_0:
                xs = xb = x; xe = (x + w - 1);
                ys = yb = y; ye = (y + h - 1);
                break;
            case LCD_ORIENTATION_90:
                yb = ys = x;
                ye = x + w - 1;
                xb = pLCD->iCurrentHeight - y - h;
                xe = xs = pLCD->iCurrentHeight - 1 - y;
                break;
            case LCD_ORIENTATION_180:
                xb = (pLCD->iCurrentWidth - x - w);
                xe = xs = (pLCD->iCurrentWidth - 1 - x);
                yb = (pLCD->iCurrentHeight - y - h);
                ye = ys = (pLCD->iCurrentHeight - 1 - y);
                break;
            case LCD_ORIENTATION_270:
                ye = ys = pLCD->iCurrentWidth - 1 - x;
                yb = pLCD->iCurrentWidth - x - w;
                xb = xs = y;
                xe = y + h - 1;
                break;
        }
        spilcdWriteCommand16(pLCD, 0x36); // Horizontal Address end
        spilcdWriteData16(pLCD, xe, DRAW_TO_LCD);
        spilcdWriteCommand16(pLCD, 0x37); // Horizontal Address start
        spilcdWriteData16(pLCD, xb, DRAW_TO_LCD);
        spilcdWriteCommand16(pLCD, 0x38); // Vertical Address end
        spilcdWriteData16(pLCD, ye, DRAW_TO_LCD);
        spilcdWriteCommand16(pLCD, 0x39); // Vertical Address start
        spilcdWriteData16(pLCD, yb, DRAW_TO_LCD);

        spilcdWriteCommand16(pLCD, 0x20); // Horizontal RAM address set
        spilcdWriteData16(pLCD, xs, DRAW_TO_LCD);
        spilcdWriteCommand16(pLCD, 0x21); // Vertical RAM address set
        spilcdWriteData16(pLCD, ys, DRAW_TO_LCD);

        spilcdWriteCommand16(pLCD, 0x22); // write to RAM
        bSetPosition = 0;
        return;
    }
	if (pLCD->iLCDType == LCD_SSD1351) // OLED has very different commands
	{
		spilcdWriteCommand(pLCD, 0x15); // set column
		ucBuf[0] = x;
		ucBuf[1] = x + w - 1;
		myspiWrite(pLCD, ucBuf, 2, MODE_DATA, iFlags);
		spilcdWriteCommand(pLCD, 0x75); // set row
		ucBuf[0] = y;
		ucBuf[1] = y + h - 1;
		myspiWrite(pLCD, ucBuf, 2, MODE_DATA, iFlags);
		spilcdWriteCommand(pLCD, 0x5c); // write RAM
		bSetPosition = 0;
		return;
	}
        else if (pLCD->iLCDType == LCD_SSD1283A) // so does the SSD1283A
        {
            switch (pLCD->iOrientation) {
                case LCD_ORIENTATION_0:
                    spilcdWriteCommand(pLCD, 0x44); // set col
                    ucBuf[0] = x + w + 1;
                    ucBuf[1] = x + 2;
                    myspiWrite(pLCD, ucBuf, 2, MODE_DATA, iFlags);
                    spilcdWriteCommand(pLCD, 0x45); // set row
                    ucBuf[0] = y + h + 1;
                    ucBuf[1] = y + 2;
                    myspiWrite(pLCD, ucBuf, 2, MODE_DATA, iFlags);
                    spilcdWriteCommand(pLCD, 0x21); // set col+row
                    ucBuf[0] = y+2;
                    ucBuf[1] = x+2;
                    myspiWrite(pLCD, ucBuf, 2, MODE_DATA, iFlags);
                    break;
                case LCD_ORIENTATION_90:
                    spilcdWriteCommand(pLCD, 0x44); // set col
                    ucBuf[0] = y + h;
                    ucBuf[1] = y + 1;
                    myspiWrite(pLCD, ucBuf, 2, MODE_DATA, iFlags);
                    spilcdWriteCommand(pLCD, 0x45); // set row
                    ucBuf[0] = x + w -1;
                    ucBuf[1] = x;
                    myspiWrite(pLCD, ucBuf, 2, MODE_DATA, iFlags);
                    spilcdWriteCommand(pLCD, 0x21); // set col+row
                    ucBuf[0] = x+1;
                    ucBuf[1] = y+1;
                    myspiWrite(pLCD, ucBuf, 2, MODE_DATA, iFlags);
                    break;
                case LCD_ORIENTATION_180:
                    spilcdWriteCommand(pLCD, 0x44);
                    ucBuf[0] = pLCD->iCurrentWidth - x - 1;
                    ucBuf[1] = pLCD->iCurrentWidth - (x+w);
                    myspiWrite(pLCD, ucBuf, 2, MODE_DATA, iFlags);
                    spilcdWriteCommand(pLCD, 0x45);
                    ucBuf[0] = pLCD->iCurrentHeight - y - 1;
                    ucBuf[1] = pLCD->iCurrentHeight - (y+h) - 1;
                    myspiWrite(pLCD, ucBuf, 2, MODE_DATA, iFlags);
                    spilcdWriteCommand(pLCD, 0x21);
                    ucBuf[0] = pLCD->iCurrentHeight - y - 1;
                    ucBuf[1] = pLCD->iCurrentWidth - x - 1;
                    myspiWrite(pLCD, ucBuf, 2, MODE_DATA, iFlags);
                  break;
                case LCD_ORIENTATION_270:
                    spilcdWriteCommand(pLCD, 0x44);
                    ucBuf[0] = pLCD->iCurrentHeight - (y + h) - 1;
                    ucBuf[1] = pLCD->iCurrentHeight - y - 1;
                    myspiWrite(pLCD, ucBuf, 2, MODE_DATA, iFlags);
                    spilcdWriteCommand(pLCD, 0x45);
                    ucBuf[0] = x + w - 1;
                    ucBuf[1] = x;
                    myspiWrite(pLCD, ucBuf, 2, MODE_DATA, iFlags);
                    spilcdWriteCommand(pLCD, 0x21);
                    ucBuf[0] = x + 2;
                    ucBuf[1] = y + 1;
                    myspiWrite(pLCD, ucBuf, 2, MODE_DATA, iFlags);
                  break;
            } // switch on orientation
            spilcdWriteCommand(pLCD, 0x22); // write RAM
            bSetPosition = 0;
            return;
        }
    else if (pLCD->iLCDType == LCD_SSD1331)
    {
        spilcdWriteCommand(pLCD, 0x15);
        ucBuf[0] = x;
        ucBuf[1] = x + w - 1;
        myspiWrite(pLCD, ucBuf, 2, MODE_COMMAND, iFlags);

        spilcdWriteCommand(pLCD, 0x75);
        ucBuf[0] = y;
        ucBuf[1] = y + h - 1;
        myspiWrite(pLCD, ucBuf, 2, MODE_COMMAND, iFlags);

        bSetPosition = 0;
        return;
    }
    if (x != pLCD->iOldX || w != pLCD->iOldCX)
    {
        pLCD->iOldX = x; pLCD->iOldCX = w;
	spilcdWriteCommand(pLCD, 0x2a); // set column address
	if (pLCD->iLCDType == LCD_ILI9341 || pLCD->iLCDType == LCD_ILI9342 || pLCD->iLCDType == LCD_ST7735R || pLCD->iLCDType == LCD_ST7789 || pLCD->iLCDType == LCD_ST7735S || pLCD->iLCDType == LCD_ILI9486)
	{
		x += pLCD->iMemoryX;
		ucBuf[0] = (unsigned char)(x >> 8);
		ucBuf[1] = (unsigned char)x;
		x = x + w - 1;
//		if ((x-iMemoryX) > iWidth-1) x = iMemoryX + iWidth-1;
		ucBuf[2] = (unsigned char)(x >> 8);
		ucBuf[3] = (unsigned char)x; 
		myspiWrite(pLCD, ucBuf, 4, MODE_DATA, iFlags);
	}
	else
	{
// combine coordinates into 1 write to save time
		ucBuf[0] = 0;
 		ucBuf[1] = (unsigned char)(x >> 8); // MSB first
		ucBuf[2] = 0;
		ucBuf[3] = (unsigned char)x;
		x = x + w -1;
		if (x > pLCD->iWidth-1) x = pLCD->iWidth-1;
		ucBuf[4] = 0;
		ucBuf[5] = (unsigned char)(x >> 8);
		ucBuf[6] = 0;
		ucBuf[7] = (unsigned char)x;
		myspiWrite(pLCD, ucBuf, 8, MODE_DATA, iFlags);
	}
    } // if X changed
    if (y != pLCD->iOldY || h != pLCD->iOldCY)
    {
        pLCD->iOldY = y; pLCD->iOldCY = h;
	spilcdWriteCommand(pLCD, 0x2b); // set row address
	if (pLCD->iLCDType == LCD_ILI9341 || pLCD->iLCDType == LCD_ILI9342 || pLCD->iLCDType == LCD_ST7735R || pLCD->iLCDType == LCD_ST7735S || pLCD->iLCDType == LCD_ST7789 || pLCD->iLCDType == LCD_ILI9486)
	{
                if (pLCD->iCurrentHeight == 135 && pLCD->iOrientation == LCD_ORIENTATION_90)
                   pLCD->iMemoryY+= 1; // ST7789 240x135 rotated 90 is off by 1
		y += pLCD->iMemoryY;
		ucBuf[0] = (unsigned char)(y >> 8);
		ucBuf[1] = (unsigned char)y;
		y = y + h;
		if ((y-pLCD->iMemoryY) > pLCD->iCurrentHeight-1) y = pLCD->iMemoryY + pLCD->iCurrentHeight;
		ucBuf[2] = (unsigned char)(y >> 8);
		ucBuf[3] = (unsigned char)y;
		myspiWrite(pLCD, ucBuf, 4, MODE_DATA, iFlags);
                if (pLCD->iCurrentHeight == 135 && pLCD->iOrientation == LCD_ORIENTATION_90)
                   pLCD->iMemoryY -=1; // ST7789 240x135 rotated 90 is off by 1

	}
	else
	{
// combine coordinates into 1 write to save time
		ucBuf[0] = 0;
		ucBuf[1] = (unsigned char)(y >> 8); // MSB first
		ucBuf[2] = 0;
		ucBuf[3] = (unsigned char)y;
		y = y + h - 1;
		if (y > pLCD->iHeight-1) y = pLCD->iHeight-1;
		ucBuf[4] = 0;
		ucBuf[5] = (unsigned char)(y >> 8);
		ucBuf[6] = 0;
		ucBuf[7] = (unsigned char)y;
		myspiWrite(pLCD, ucBuf, 8, MODE_DATA, iFlags);
	}
    } // if Y changed
	spilcdWriteCommand(pLCD, 0x2c); // write memory begin
//	spilcdWriteCommand(0x3c); // write memory continue
    bSetPosition = 0;
} /* spilcdSetPosition() */

//
// Draw an individual RGB565 pixel
//
int spilcdSetPixel(SPILCD *pLCD, int x, int y, unsigned short usColor, int iFlags)
{
	spilcdSetPosition(pLCD, x, y, 1, 1, iFlags);
	spilcdWriteData16(pLCD, usColor, iFlags);
	return 0;
} /* spilcdSetPixel() */

uint8_t * spilcdGetDMABuffer(void)
{
  return (uint8_t *)ucTXBuf;
}

#ifdef ESP32_DMA
//This function is called (in irq context!) just before a transmission starts. It will
//set the D/C line to the value indicated in the user field.
//static void spi_pre_transfer_callback(spi_transaction_t *t)
//{
//    int iMode=(int)t->user;
//    spilcdSetMode(iMode);
//}
static void spi_post_transfer_callback(spi_transaction_t *t)
{
    SPILCD *pLCD = (SPILCD*)t;
    transfer_is_done = true;
    if (pLCD->iCSPin != -1)
        myPinWrite(pLCD->iCSPin, 1);
}
#endif

#ifdef ARDUINO_SAMD_ZERO
// Callback for end-of-DMA-transfer
void dma_callback(Adafruit_ZeroDMA *dma) {
    SPILCD *pLCD = (SPILCD*)dma; // not needed
  transfer_is_done = true;
    if (pLCD->iCSPin != -1)
        myPinWrite(pLCD->iCSPin, 1);
}
#endif // ARDUINO_SAMD_ZERO

// wait for previous transaction to complete
void spilcdWaitDMA(void)
{
#ifdef HAS_DMA
    while (!transfer_is_done);
#ifdef ARDUINO_SAMD_ZERO
    mySPI.endTransaction();
#endif // ARDUINO_SAMD_ZERO
#endif // HAS_DMA
}
// Queue a new transaction for the SPI DMA
void spilcdWriteDataDMA(SPILCD *pLCD, int iLen)
{
#ifdef ESP32_DMA
esp_err_t ret;

    trans[0].tx_buffer = ucTXBuf;
    trans[0].length = iLen * 8; // Length in bits
    trans[0].rxlength = 0; // defaults to the same length as tx length
    
    // Queue the transaction
//    ret = spi_device_polling_transmit(spi, &t);
    ret = spi_device_queue_trans(spi, &trans[0], portMAX_DELAY);
    transfer_is_done = false;
    assert (ret==ESP_OK);
//    iFirst = 0;
    return;
#endif // ESP32_DMA
#ifdef ARDUINO_SAMD_ZERO
  myDMA.changeDescriptor(
    desc,
    ucTXBuf,     // src
    NULL, // dest
    iLen);                      // this many...
  mySPI.beginTransaction(SPISettings(pLCD->iSPISpeed, MSBFIRST, pLCD->iSPIMode));
  transfer_is_done = false;
  myDMA.startJob();
  return;
#endif
} /* spilcdWriteDataDMA() */

//
// Smooth expanded text
//
//  A    --\ 1 2
//C P B  --/ 3 4
//  D
// 1=P; 2=P; 3=P; 4=P;
// IF C==A AND C!=D AND A!=B => 1=A
// IF A==B AND A!=C AND B!=D => 2=B
// IF B==D AND B!=A AND D!=C => 4=D
// IF D==C AND D!=B AND C!=A => 3=C
void SmoothImg(uint16_t *pSrc, uint16_t *pDest, int iSrcPitch, int iDestPitch, int iWidth, int iHeight)
{
int x, y;
unsigned short *s,*s2;
uint32_t *d,*d2;
unsigned short A, B, C, D, P;
uint32_t ulPixel, ulPixel2;

// copy the edge pixels as-is
// top+bottom lines first
s = pSrc;
s2 = &pSrc[(iHeight-1)*iSrcPitch];
d = (uint32_t *)pDest;
d2 = (uint32_t *)&pDest[(iHeight-1)*2*iDestPitch];
for (x=0; x<iWidth; x++)
{     
      ulPixel = *s++;
      ulPixel2 = *s2++;
      ulPixel |= (ulPixel << 16); // simply double it
      ulPixel2 |= (ulPixel2 << 16);
      d[0] = ulPixel; 
      d[iDestPitch/2] = ulPixel;
      d2[0] = ulPixel2;
      d2[iDestPitch/2] = ulPixel2;
      d++; d2++;
}
for (y=1; y<iHeight-1; y++)
  {
  s = &pSrc[y * iSrcPitch];
  d = (uint32_t *)&pDest[(y * 2 * iDestPitch)];
// first pixel is just stretched
  ulPixel = *s++;
  ulPixel |= (ulPixel << 16);
  d[0] = ulPixel;
  d[iDestPitch/2] = ulPixel;
  d++;
  for (x=1; x<iWidth-1; x++)
     {
     A = s[-iSrcPitch];
     C = s[-1];
     P = s[0];
     B = s[1];
     D = s[iSrcPitch];
     if (C==A && C!=D && A!=B)
        ulPixel = A;
     else
        ulPixel = P;
     if (A==B && A!=C && B!=D)
        ulPixel |= (B << 16);
     else
        ulPixel |= (P << 16);
     d[0] = ulPixel;
     if (D==C && D!=B && C!=A)
        ulPixel = C;
     else
        ulPixel = P;
     if (B==D && B!=A && D!=C)
        ulPixel |= (D << 16);
     else
        ulPixel |= (P << 16);
     d[iDestPitch/2] = ulPixel;
     d++;
     s++;
     } // for x
// last pixel is just stretched
  ulPixel = s[0];
  ulPixel |= (ulPixel << 16);
  d[0] = ulPixel;
  d[iDestPitch/2] = ulPixel;
  } // for y
} /* SmoothImg() */

//
// Draw a string in a proportional font you supply
//
int spilcdWriteStringCustom(SPILCD *pLCD, GFXfont *pFont, int x, int y, char *szMsg, uint16_t usFGColor, uint16_t usBGColor, int bBlank, int iFlags)
{
int i, j, k, iLen, dx, dy, cx, cy, c, iBitOff;
int tx, ty;
uint8_t *s, bits, uc;
GFXfont font;
GFXglyph glyph, *pGlyph;
#define TEMP_BUF_SIZE 64
#define TEMP_HIGHWATER (TEMP_BUF_SIZE-8)
uint16_t *d, u16Temp[TEMP_BUF_SIZE];

   if (pFont == NULL || x < 0)
      return -1;
   // in case of running on AVR, get copy of data from FLASH
   memcpy_P(&font, pFont, sizeof(font));
   pGlyph = &glyph;
   usFGColor = (usFGColor >> 8) | (usFGColor << 8); // swap h/l bytes
   usBGColor = (usBGColor >> 8) | (usBGColor << 8);

   i = 0;
   while (szMsg[i] && x < pLCD->iCurrentWidth)
   {
      c = szMsg[i++];
      if (c < font.first || c > font.last) // undefined character
         continue; // skip it
      c -= font.first; // first char of font defined
      memcpy_P(&glyph, &font.glyph[c], sizeof(glyph));
      // set up the destination window (rectangle) on the display
      dx = x + pGlyph->xOffset; // offset from character UL to start drawing
      dy = y + pGlyph->yOffset;
      cx = pGlyph->width;
      cy = pGlyph->height;
      iBitOff = 0; // bitmap offset (in bits)
      if (dy + cy > pLCD->iCurrentHeight)
         cy = pLCD->iCurrentHeight - dy;
      else if (dy < 0) {
         cy += dy;
         iBitOff += (pGlyph->width * (-dy));
         dy = 0;
      }
      s = font.bitmap + pGlyph->bitmapOffset; // start of bitmap data
      // Bitmap drawing loop. Image is MSB first and each pixel is packed next
      // to the next (continuing on to the next character line)
      bits = uc = 0; // bits left in this font byte

      if (bBlank) { // erase the areas around the char to not leave old bits
         int miny, maxy;
         c = '0' - font.first;
         miny = y + (int8_t)pgm_read_byte(&font.glyph[c].yOffset);
         c = 'y' - font.first;
         maxy = y + (int8_t)pgm_read_byte(&font.glyph[c].yOffset) + pgm_read_byte(&font.glyph[c].height);
         spilcdSetPosition(pLCD, x, miny, pGlyph->xAdvance, maxy-miny, iFlags);
            // blank out area above character
            for (ty=miny; ty<y+pGlyph->yOffset; ty++) {
               for (tx=0; tx>pGlyph->xAdvance; tx++)
                  u16Temp[tx] = usBGColor;
               myspiWrite(pLCD, (uint8_t *)u16Temp, pGlyph->xAdvance*sizeof(uint16_t), MODE_DATA, iFlags);
            } // for ty
            // character area (with possible padding on L+R)
            for (ty=0; ty<pGlyph->height; ty++) {
               d = &u16Temp[0];
               for (tx=0; tx<pGlyph->xOffset; tx++) { // left padding
                  *d++ = usBGColor;
               }
            // character bitmap (center area)
               for (tx=0; tx<pGlyph->width; tx++) {
                  if (bits == 0) { // need more data
                     uc = pgm_read_byte(&s[iBitOff>>3]);
                     bits = 8;
                     iBitOff += bits;
                  }
                  *d++ = (uc & 0x80) ? usFGColor : usBGColor;
                  bits--;
                  uc <<= 1;
               } // for tx
               // right padding
               k = pGlyph->xAdvance - (int)(d - u16Temp); // remaining amount
               for (tx=0; tx<k; tx++)
               *d++ = usBGColor;
               myspiWrite(pLCD, (uint8_t *)u16Temp, pGlyph->xAdvance*sizeof(uint16_t), MODE_DATA, iFlags);
            } // for ty
            // padding below the current character
            ty = y + pGlyph->yOffset + pGlyph->height;
            for (; ty < maxy; ty++) {
               for (tx=0; tx<pGlyph->xAdvance; tx++)
                  u16Temp[tx] = usBGColor;
               myspiWrite(pLCD, (uint8_t *)u16Temp, pGlyph->xAdvance*sizeof(uint16_t), MODE_DATA, iFlags);
            } // for ty
      } else { // just draw the current character box
         spilcdSetPosition(pLCD, dx, dy, cx, cy, iFlags);
            iLen = cx * cy; // total pixels to draw
            d = &u16Temp[0]; // point to start of output buffer
            for (j=0; j<iLen; j++) {
               if (uc == 0) { // need to read more font data
                  j += bits;
                  while (bits > 0) {
                     *d++ = usBGColor; // draw any remaining 0 bits
                     bits--;
                  }
                  uc = pgm_read_byte(&s[iBitOff>>3]); // get more font bitmap data
                  bits = 8 - (iBitOff & 7); // we might not be on a byte boundary
                  iBitOff += bits; // because of a clipped line
                  uc <<= (8-bits);
                  k = (int)(d-u16Temp); // number of words in output buffer
                  if (k >= TEMP_HIGHWATER) { // time to write it
                     myspiWrite(pLCD, (uint8_t *)u16Temp, k*sizeof(uint16_t), MODE_DATA, iFlags);
                     d = &u16Temp[0];
                  }
               } // if we ran out of bits
               *d++ = (uc & 0x80) ? usFGColor : usBGColor;
               bits--; // next bit
               uc <<= 1;
            } // for j
            k = (int)(d-u16Temp);
            if (k) // write any remaining data
               myspiWrite(pLCD, (uint8_t *)u16Temp, k*sizeof(uint16_t), MODE_DATA, iFlags);
      } // quicker drawing
      x += pGlyph->xAdvance; // width of this character
   } // while drawing characters
   return 0;

} /* spilcdWriteStringCustom() */
//
// Get the width of text in a custom font
//
void spilcdGetStringBox(GFXfont *pFont, char *szMsg, int *width, int *top, int *bottom)
{
int cx = 0;
int c, i = 0;
GFXfont font;
GFXglyph glyph, *pGlyph;
int miny, maxy;

   if (pFont == NULL)
      return;
   // in case of running on AVR, get copy of data from FLASH
   memcpy_P(&font, pFont, sizeof(font));
   pGlyph = &glyph;
   if (width == NULL || top == NULL || bottom == NULL || pFont == NULL || szMsg == NULL) return; // bad pointers
   miny = 100; maxy = 0;
   while (szMsg[i]) {
      c = szMsg[i++];
      if (c < font.first || c > font.last) // undefined character
         continue; // skip it
      c -= font.first; // first char of font defined
      memcpy_P(&glyph, &font.glyph[c], sizeof(glyph));
      cx += pGlyph->xAdvance;
      if (pGlyph->yOffset < miny) miny = pGlyph->yOffset;
      if (pGlyph->height+pGlyph->yOffset > maxy) maxy = pGlyph->height+pGlyph->yOffset;
   }
   *width = cx;
   *top = miny;
   *bottom = maxy;
} /* spilcdGetStringBox() */

#ifndef __AVR__
//
// Draw a string of small (8x8) text as quickly as possible
// by writing it to the LCD in a single SPI write
// The string must be 32 characters or shorter
//
int spilcdWriteStringFast(SPILCD *pLCD, int x, int y, char *szMsg, unsigned short usFGColor, unsigned short usBGColor, int iFontSize, int iFlags)
{
int i, j, k, iLen;
int iStride;
uint8_t *s;
uint16_t usFG = (usFGColor >> 8) | ((usFGColor & -1)<< 8);
uint16_t usBG = (usBGColor >> 8) | ((usBGColor & -1)<< 8);
uint16_t *usD;
int cx;
uint8_t *pFont;

    if (iFontSize != FONT_6x8 && iFontSize != FONT_8x8)
        return -1; // invalid size
    cx = (iFontSize == FONT_8x8) ? 8:6;
    pFont = (iFontSize == FONT_8x8) ? (uint8_t *)ucFont : (uint8_t *)ucSmallFont;
    iLen = strlen(szMsg);
	if (iLen <=0) return -1; // can't use this function

    if ((cx*iLen) + x > pLCD->iCurrentWidth) iLen = (pLCD->iCurrentWidth - x)/cx; // can't display it all
    if (iLen > 32) iLen = 32;
    iStride = iLen * cx*2;
    for (i=0; i<iLen; i++)
    {
        s = &pFont[((unsigned char)szMsg[i]-32) * cx];
        uint8_t ucMask = 1;
        for (k=0; k<8; k++) // for each scanline
        {
            usD = (unsigned short *)&ucRXBuf[(k*iStride) + (i * cx*2)];
            for (j=0; j<cx; j++)
            {
                if (s[j] & ucMask)
                    *usD++ = usFG;
                else
                    *usD++ = usBG;
            } // for j
            ucMask <<= 1;
        } // for k
    } // for i
    // write the data in one shot
    spilcdSetPosition(pLCD, x, y, cx*iLen, 8, iFlags);
    myspiWrite(pLCD, ucRXBuf, iLen*cx*16, MODE_DATA, iFlags);
	return 0;
} /* spilcdWriteStringFast() */
#endif // !__AVR__

//
// Draw a string of small (8x8) or large (16x32) characters
// At the given col+row
//
int spilcdWriteString(SPILCD *pLCD, int x, int y, char *szMsg, int usFGColor, int usBGColor, int iFontSize, int iFlags)
{
int i, j, k, iLen;
#ifndef __AVR__
int l;
#endif
unsigned char *s;
unsigned short usFG = (usFGColor >> 8) | (usFGColor << 8);
unsigned short usBG = (usBGColor >> 8) | (usBGColor << 8);
uint16_t usPitch = pLCD->iScreenPitch/2;


	iLen = strlen(szMsg);
    if (usBGColor == -1)
        iFlags = DRAW_TO_RAM; // transparent text doesn't get written to the display
#ifndef __AVR__
	if (iFontSize == FONT_16x32) // draw 16x32 font
	{
		if (iLen*16 + x > pLCD->iCurrentWidth) iLen = (pLCD->iCurrentWidth - x) / 16;
		if (iLen < 0) return -1;
		for (i=0; i<iLen; i++)
		{
			uint16_t *usD, *usTemp = (uint16_t *)ucRXBuf;
			s = (uint8_t *)&ucBigFont[((unsigned char)szMsg[i]-32)*64];
			usD = &usTemp[0];
            if (usBGColor == -1) // transparent text is not rendered to the display
               iFlags = DRAW_TO_RAM;
            spilcdSetPosition(pLCD, x+(i*16), y,16,32, iFlags);
            for (l=0; l<4; l++) // 4 sets of 8 rows
            {
                uint8_t ucMask = 1;
                for (k=0; k<8; k++) // for each scanline
                { // left half
                    if (usBGColor == -1) // transparent text
                    {
                        uint16_t *d = (uint16_t *)&pLCD->pBackBuffer[pLCD->iOffset + ((l*8+k)*pLCD->iScreenPitch)];
                        for (j=0; j<16; j++)
                        {
                            if (s[j] & ucMask)
                                *d = usFG;
                            d++;
                        } // for j
                    }
                    else
                    {
                        for (j=0; j<16; j++)
                        {
                            if (s[j] & ucMask)
                                *usD++ = usFG;
                            else
                                *usD++ = usBG;
                        } // for j
                    }
                    ucMask <<= 1;
                } // for each scanline
                s += 16;
            } // for each set of 8 scanlines
        if (usBGColor != -1) // don't write anything if we're doing transparent text
            myspiWrite(pLCD, (unsigned char *)usTemp, 1024, MODE_DATA, iFlags);
		} // for each character
	}
#endif // !__AVR__
    if (iFontSize == FONT_8x8 || iFontSize == FONT_6x8) // draw the 6x8 or 8x8 font
	{
		uint16_t *usD, *usTemp = (uint16_t *)ucRXBuf;
        int cx;
        uint8_t *pFont;

        cx = (iFontSize == FONT_8x8) ? 8:6;
        pFont = (iFontSize == FONT_8x8) ? (uint8_t *)ucFont : (uint8_t *)ucSmallFont;
		if ((cx*iLen) + x > pLCD->iCurrentWidth) iLen = (pLCD->iCurrentWidth - x)/cx; // can't display it all
		if (iLen < 0)return -1;

		for (i=0; i<iLen; i++)
		{
			s = &pFont[((unsigned char)szMsg[i]-32) * cx];
			usD = &usTemp[0];
            spilcdSetPosition(pLCD, x+(i*cx), y, cx, 8, iFlags);
            uint8_t ucMask = 1;
            for (k=0; k<8; k++) // for each scanline
            {
                if (usBGColor == -1) // transparent text
                {
                    usD = (uint16_t *)&pLCD->pBackBuffer[pLCD->iOffset + (k * pLCD->iScreenPitch)];
                    for (j=0; j<cx; j++)
                    {
                        if (pgm_read_byte(&s[j]) & ucMask)
                            *usD = usFG;
                        usD++;
                    } // for j
                }
                else // regular text
                {
                    for (j=0; j<cx; j++)
                    {
                        if (pgm_read_byte(&s[j]) & ucMask)
                            *usD++ = usFG;
                        else
                            *usD++ = usBG;
                    } // for j
                }
                ucMask <<= 1;
            } // for k
    // write the data in one shot
        if (usBGColor != -1) // don't write anything if we're doing transparent text
            myspiWrite(pLCD, (unsigned char *)usTemp, cx*16, MODE_DATA, iFlags);
		}	
	} // 6x8 and 8x8
    if (iFontSize == FONT_12x16) // 6x8 stretched to 12x16 (with smoothing)
    {
        uint16_t *usD, *usTemp = (uint16_t *)ucRXBuf;
        
        if ((12*iLen) + x > pLCD->iCurrentWidth) iLen = (pLCD->iCurrentWidth - x)/12; // can't display it all
        if (iLen < 0)return -1;
        
        for (i=0; i<iLen; i++)
        {
            s = (uint8_t *)&ucSmallFont[((unsigned char)szMsg[i]-32) * 6];
            usD = &usTemp[0];
            spilcdSetPosition(pLCD, x+(i*12), y, 12, 16, iFlags);
            uint8_t ucMask = 1;
            if (usBGColor != -1) // start with all BG color
            {
               for (k=0; k<12*16; k++)
                  usD[k] = usBG;
            }
            for (k=0; k<8; k++) // for each scanline
            {
                if (usBGColor == -1) // transparent text
                {
                    uint8_t c0, c1;
                    usD = (uint16_t *)&pLCD->pBackBuffer[pLCD->iOffset + (k*2*pLCD->iScreenPitch)];
                    for (j=0; j<6; j++)
                    {
                        c0 = pgm_read_byte(&s[j]);
                        if (c0 & ucMask)
                            usD[0] = usD[1] = usD[usPitch] = usD[usPitch+1] = usFG;
                        // test for smoothing diagonals
                        if (k < 7 && j < 5) {
                           uint8_t ucMask2 = ucMask << 1;
                           c1 = pgm_read_byte(&s[j+1]);
                           if ((c0 & ucMask) && (~c1 & ucMask) && (~c0 & ucMask2) && (c1 & ucMask2)) // first diagonal condition
                               usD[usPitch+2] = usD[2*usPitch+1] = usFG;
                           else if ((~c0 & ucMask) && (c1 & ucMask) && (c0 & ucMask2) && (~c1 & ucMask2))
                               usD[usPitch+1] = usD[2*usPitch+2] = usFG;
                        } // if not on last row and last col
                        usD += 2;
                    } // for j
                }
                else // regular text drawing
                {
                    uint8_t c0, c1;
                    for (j=0; j<6; j++)
                    {
                        c0 = pgm_read_byte(&s[j]);
                        if (c0 & ucMask)
                           usD[0] = usD[1] = usD[12] = usD[13] = usFG;
                        // test for smoothing diagonals
                        if (k < 7 && j < 5) {
                           uint8_t ucMask2 = ucMask << 1;
                           c1 = pgm_read_byte(&s[j+1]);
                           if ((c0 & ucMask) && (~c1 & ucMask) && (~c0 & ucMask2) && (c1 & ucMask2)) // first diagonal condition
                               usD[14] = usD[25] = usFG;
                           else if ((~c0 & ucMask) && (c1 & ucMask) && (c0 & ucMask2) && (~c1 & ucMask2))
                               usD[13] = usD[26] = usFG;
                        } // if not on last row and last col
                        usD+=2;
                    } // for j
                }
                usD += 12; // skip the extra line
                ucMask <<= 1;
            } // for k
        // write the data in one shot
        if (usBGColor != -1) // don't write anything if we're doing transparent text
            myspiWrite(pLCD, (unsigned char *)&usTemp[0], 12*16*2, MODE_DATA, iFlags);
        }
    } // FONT_12x16
    if (iFontSize == FONT_16x16) // 8x8 stretched to 16x16
    {
        uint16_t *usD, *usTemp = (uint16_t *)ucRXBuf;
        
        if ((16*iLen) + x > pLCD->iCurrentWidth) iLen = (pLCD->iCurrentWidth - x)/16; // can't display it all
        if (iLen < 0)return -1;
        
        for (i=0; i<iLen; i++)
        {
            s = (uint8_t *)&ucFont[((unsigned char)szMsg[i]-32) * 8];
            usD = &usTemp[0];
            spilcdSetPosition(pLCD, x+(i*16), y, 16, 16, iFlags);
            uint8_t ucMask = 1;
            if (usBGColor != -1) // start with all BG color
            {
               for (k=0; k<256; k++)
                  usD[k] = usBG;
            }
            for (k=0; k<8; k++) // for each scanline
            {
                if (usBGColor == -1) // transparent text
                {
                    usD = (uint16_t *)&pLCD->pBackBuffer[pLCD->iOffset + (k*2*pLCD->iScreenPitch)];
                    for (j=0; j<8; j++)
                    {
                        if (pgm_read_byte(&s[j]) & ucMask)
                            usD[0] = usD[1] = usD[usPitch] = usD[usPitch+1] = usFG;
                        usD += 2;
                    } // for j
                }
                else // regular text drawing
                {
                    uint8_t c0;
                    for (j=0; j<8; j++)
                    {
                        c0 = pgm_read_byte(&s[j]);
                        if (c0 & ucMask)
                        usD[0] = usD[1] = usD[16] = usD[17] = usFG;
                        usD+=2;
                    } // for j
                }
                usD += 16; // skip the next line
                ucMask <<= 1;
            } // for k
        // write the data in one shot
        if (usBGColor != -1) // don't write anything if we're doing transparent text
            myspiWrite(pLCD, (unsigned char *)&usTemp[0], 512, MODE_DATA, iFlags);
        }
    } // FONT_16x16
	return 0;
} /* spilcdWriteString() */
//
// For drawing ellipses, a circle is drawn and the x and y pixels are scaled by a 16-bit integer fraction
// This function draws a single pixel and scales its position based on the x/y fraction of the ellipse
//
void DrawScaledPixel(SPILCD *pLCD, int32_t iCX, int32_t iCY, int32_t x, int32_t y, int32_t iXFrac, int32_t iYFrac, unsigned short usColor, int iFlags)
{
    uint8_t ucBuf[2];
    if (iXFrac != 0x10000) x = (x * iXFrac) >> 16;
    if (iYFrac != 0x10000) y = (y * iYFrac) >> 16;
    x += iCX; y += iCY;
    if (x < 0 || x >= pLCD->iCurrentWidth || y < 0 || y >= pLCD->iCurrentHeight)
        return; // off the screen
    ucBuf[0] = (uint8_t)(usColor >> 8);
    ucBuf[1] = (uint8_t)usColor;
    spilcdSetPosition(pLCD, x, y, 1, 1, iFlags);
    myspiWrite(pLCD, ucBuf, 2, MODE_DATA, iFlags);
} /* DrawScaledPixel() */
        
void DrawScaledLine(SPILCD *pLCD, int32_t iCX, int32_t iCY, int32_t x, int32_t y, int32_t iXFrac, int32_t iYFrac, uint16_t *pBuf, int iFlags)
{
    int32_t iLen, x2;
    if (iXFrac != 0x10000) x = (x * iXFrac) >> 16;
    if (iYFrac != 0x10000) y = (y * iYFrac) >> 16;
    iLen = x * 2;
    x = iCX - x; y += iCY;
    x2 = x + iLen;
    if (y < 0 || y >= pLCD->iCurrentHeight)
        return; // completely off the screen
    if (x < 0) x = 0;
    if (x2 >= pLCD->iCurrentWidth) x2 = pLCD->iCurrentWidth-1;
    iLen = x2 - x + 1; // new length
    spilcdSetPosition(pLCD, x, y, iLen, 1, iFlags);
#ifdef ESP32_DMA 
    myspiWrite(pLCD, (uint8_t*)pBuf, iLen*2, MODE_DATA, iFlags);
#else
    // need to refresh the output data each time
    {
    int i;
    unsigned short us = pBuf[0];
      for (i=1; i<iLen; i++)
        pBuf[i] = us;
    }
    myspiWrite(pLCD, (uint8_t*)&pBuf[1], iLen*2, MODE_DATA, iFlags);
#endif
} /* DrawScaledLine() */
//
// Draw the 8 pixels around the Bresenham circle
// (scaled to make an ellipse)
//
void BresenhamCircle(SPILCD *pLCD, int32_t iCX, int32_t iCY, int32_t x, int32_t y, int32_t iXFrac, int32_t iYFrac, uint16_t iColor, uint16_t *pFill, int iFlags)
{
    if (pFill != NULL) // draw a filled ellipse
    {
        static int prev_y = -1;
        // for a filled ellipse, draw 4 lines instead of 8 pixels
        DrawScaledLine(pLCD, iCX, iCY, y, x, iXFrac, iYFrac, pFill, iFlags);
        DrawScaledLine(pLCD, iCX, iCY, y, -x, iXFrac, iYFrac, pFill, iFlags);
        if (y != prev_y) {
            DrawScaledLine(pLCD, iCX, iCY, x, y, iXFrac, iYFrac, pFill, iFlags);
            DrawScaledLine(pLCD, iCX, iCY, x, -y, iXFrac, iYFrac, pFill, iFlags);
            prev_y = y;
        }
    }
    else // draw 8 pixels around the edges
    {
        DrawScaledPixel(pLCD, iCX, iCY, x, y, iXFrac, iYFrac, iColor, iFlags);
        DrawScaledPixel(pLCD, iCX, iCY, -x, y, iXFrac, iYFrac, iColor, iFlags);
        DrawScaledPixel(pLCD, iCX, iCY, x, -y, iXFrac, iYFrac, iColor, iFlags);
        DrawScaledPixel(pLCD, iCX, iCY, -x, -y, iXFrac, iYFrac, iColor, iFlags);
        DrawScaledPixel(pLCD, iCX, iCY, y, x, iXFrac, iYFrac, iColor, iFlags);
        DrawScaledPixel(pLCD, iCX, iCY, -y, x, iXFrac, iYFrac, iColor, iFlags);
        DrawScaledPixel(pLCD, iCX, iCY, y, -x, iXFrac, iYFrac, iColor, iFlags);
        DrawScaledPixel(pLCD, iCX, iCY, -y, -x, iXFrac, iYFrac, iColor, iFlags);
    }
} /* BresenhamCircle() */

void spilcdEllipse(SPILCD *pLCD, int32_t iCenterX, int32_t iCenterY, int32_t iRadiusX, int32_t iRadiusY, unsigned short usColor, int bFilled, int iFlags)
{
    int32_t iRadius, iXFrac, iYFrac;
    int32_t iDelta, x, y;
    uint16_t us, *pus, *usTemp = (uint16_t *)ucRXBuf; // up to 320 pixels wide
    
    if (iRadiusX > iRadiusY) // use X as the primary radius
    {
        iRadius = iRadiusX;
        iXFrac = 65536;
        iYFrac = (iRadiusY * 65536) / iRadiusX;
    }
    else
    {
        iRadius = iRadiusY;
        iXFrac = (iRadiusX * 65536) / iRadiusY;
        iYFrac = 65536;
    }
    // set up a buffer with the widest possible run of pixels to dump in 1 shot
    if (bFilled)
    {
        us = (usColor >> 8) | (usColor << 8); // swap byte order
        y = iRadius*2;
        if (y > 320) y = 320; // max size
#ifdef ESP32_DMA
        for (x=0; x<y; x++)
        {
            usTemp[x] = us;
        }
#else
	usTemp[0] = us; // otherwise just set the first one to the color
#endif
        pus = usTemp;
    }
    else
    {
        pus = NULL;
    }
    iDelta = 3 - (2 * iRadius);
    x = 0; y = iRadius;
    while (x < y)
    {
        BresenhamCircle(pLCD, iCenterX, iCenterY, x, y, iXFrac, iYFrac, usColor, pus, iFlags);
        x++;
        if (iDelta < 0)
        {
            iDelta += (4*x) + 6;
        }
        else
        {
            iDelta += 4 * (x-y) + 10;
            y--;
        }
    }

} /* spilcdEllipse() */

//
// Set the (software) orientation of the display
// The hardware is permanently oriented in 240x320 portrait mode
// The library can draw characters/tiles rotated 90
// degrees if set into landscape mode
//
int spilcdSetOrientation(SPILCD *pLCD, int iOrient)
{
int bX=0, bY=0, bV=0;

    pLCD->iOrientation = iOrient;
    // Make sure next setPos() resets both x and y
    pLCD->iOldX=-1; pLCD->iOldY=-1; pLCD->iOldCX=-1; pLCD->iOldCY = -1;
   switch(iOrient)
   {
     case LCD_ORIENTATION_0:
           bX = bY = bV = 0;
           pLCD->iMemoryX = pLCD->iColStart;
           pLCD->iMemoryY = pLCD->iRowStart;
           pLCD->iCurrentHeight = pLCD->iHeight;
           pLCD->iCurrentWidth = pLCD->iWidth;
        break;
     case LCD_ORIENTATION_90:
        bX = bV = 1;
        bY = 0;
           pLCD->iMemoryX = pLCD->iRowStart;
           pLCD->iMemoryY = pLCD->iColStart;
           pLCD->iCurrentHeight = pLCD->iWidth;
           pLCD->iCurrentWidth = pLCD->iHeight;
        break;
     case LCD_ORIENTATION_180:
        bX = bY = 1;
        bV = 0;
        if (pLCD->iColStart != 0)
            pLCD->iMemoryX = pLCD->iColStart;// - 1;
           pLCD->iMemoryY = pLCD->iRowStart;
           pLCD->iCurrentHeight = pLCD->iHeight;
           pLCD->iCurrentWidth = pLCD->iWidth;
        break;
     case LCD_ORIENTATION_270:
        bY = bV = 1;
        bX = 0;
           pLCD->iMemoryX = pLCD->iRowStart;
           pLCD->iMemoryY = pLCD->iColStart;
           pLCD->iCurrentHeight = pLCD->iWidth;
           pLCD->iCurrentWidth = pLCD->iHeight;
        break;
   }
   pLCD->iScreenPitch = pLCD->iCurrentWidth * 2;
    if (pLCD->iLCDType == LCD_ST7789 && pLCD->iHeight == 240 && pLCD->iWidth == 240) {
        // special issue with memory offsets in certain orientations
        if (pLCD->iOrientation == LCD_ORIENTATION_180) {
            pLCD->iMemoryX = 0; pLCD->iMemoryY = 80;
        } else if (pLCD->iOrientation == LCD_ORIENTATION_270) {
            pLCD->iMemoryX = 80; pLCD->iMemoryY = 0;
        } else {
            pLCD->iMemoryX = pLCD->iMemoryY = 0;
        }
    }
   if (pLCD->iLCDType == LCD_ILI9486 || pLCD->iLCDType == LCD_ST7789_NOCS || pLCD->iLCDType == LCD_ST7789_135 || pLCD->iLCDType == LCD_ST7789 || pLCD->iLCDType == LCD_ST7735R || pLCD->iLCDType == LCD_ST7735S || pLCD->iLCDType == LCD_ST7735S_B || pLCD->iLCDType == LCD_ILI9341 || pLCD->iLCDType == LCD_ILI9342)
   {
      uint8_t uc = 0;
       if (pLCD->iLCDType == LCD_ILI9342) // x is reversed
           bX = ~bX;
      if (bY) uc |= 0x80;
      if (bX) uc |= 0x40;
      if (bV) uc |= 0x20;
       if (pLCD->iLCDType == LCD_ILI9341 || pLCD->iLCDType == LCD_ILI9342) {
          uc |= 8; // R/B inverted from other LCDs
           uc ^= 0x40; // x is inverted too
       }
      if (pLCD->iLCDFlags & FLAGS_SWAP_RB)
          uc ^= 0x8;
      spilcdWriteCommand(pLCD, 0x36); // MADCTL
      spilcdWriteData8(pLCD, uc);
   }
    if (pLCD->iLCDType == LCD_SSD1283A) {
        uint16_t u1=0, u3=0;
        switch (pLCD->iOrientation)
        {
            case LCD_ORIENTATION_0:
                u1 = 0x2183;
                u3 = 0x6830;
                break;
            case LCD_ORIENTATION_90:
                u1 = 0x2283;
                u3 = 0x6838;
                break;
            case LCD_ORIENTATION_180:
                u1 = 0x2183;
                u3 = 0x6800;
                break;
            case LCD_ORIENTATION_270:
                u1 = 0x2183;
                u3 = 0x6828;
                break;
        }
        spilcdWriteCommand(pLCD, 0x1); // output control
        spilcdWriteData8(pLCD, (uint8_t)(u1 >> 8));
        spilcdWriteData8(pLCD, (uint8_t)u1);
        spilcdWriteCommand(pLCD, 0x3); // Entry Mode
        spilcdWriteData8(pLCD, (uint8_t)(u3 >> 8));
        spilcdWriteData8(pLCD, (uint8_t)u3);
    }
    if (pLCD->iLCDType == LCD_ILI9225)
    {
        uint16_t usVal = 0x1000; // starting Entry Mode (0x03) bits
        if (bX == bY) // for 0 and 180, invert the bits
        {
            bX = !bX;
            bY = !bY;
        }
        usVal |= (bV << 3);
        usVal |= (bY << 4);
        usVal |= (bX << 5);
        if (pLCD->iLCDFlags & FLAGS_SWAP_RB)
            usVal ^= 0x1000;
        spilcdWriteCommand16(pLCD, 0x03);
        spilcdWriteData16(pLCD, usVal, DRAW_TO_LCD);
    }

    return 0;
} /* spilcdSetOrientation() */
//
// Fill the frame buffer with a single color
//
int spilcdFill(SPILCD *pLCD, unsigned short usData, int iFlags)
{
int i, cx, tx, x, y;
uint16_t *u16Temp = (uint16_t *)ucRXBuf;

    // make sure we're in landscape mode to use the correct coordinates
    spilcdScrollReset(pLCD);
    spilcdSetPosition(pLCD, 0,0,pLCD->iCurrentWidth,pLCD->iCurrentHeight, iFlags);
    usData = (usData >> 8) | (usData << 8); // swap hi/lo byte for LCD
    // fit within our temp buffer
    cx = 1; tx = pLCD->iCurrentWidth;
    if (pLCD->iCurrentWidth > 160)
    {
       cx = 2; tx = pLCD->iCurrentWidth/2;
    }
    for (y=0; y<pLCD->iCurrentHeight; y++)
    {
       for (x=0; x<cx; x++)
       {
// have to do this every time because the buffer gets overrun (no half-duplex mode in Arduino SPI library)
            for (i=0; i<tx; i++)
                u16Temp[i] = usData;
            myspiWrite(pLCD, (uint8_t *)u16Temp, tx*2, MODE_DATA, iFlags); // fill with data byte
       } // for x
    } // for y
    return 0;
} /* spilcdFill() */

//
// Draw a line between 2 points using Bresenham's algorithm
// An optimized version of the algorithm where each continuous run of pixels is written in a
// single shot to reduce the total number of SPI transactions. Perfectly vertical or horizontal
// lines are the most extreme version of this condition and will write the data in a single
// operation.
//
void spilcdDrawLine(SPILCD *pLCD, int x1, int y1, int x2, int y2, unsigned short usColor, int iFlags)
{
    int temp;
    int dx = x2 - x1;
    int dy = y2 - y1;
    int error;
    int xinc, yinc;
    int iLen, x, y;
#ifndef ESP32_DMA
    int i;
#endif
    uint16_t *usTemp = (uint16_t *)ucRXBuf, us;

    if (x1 < 0 || x2 < 0 || y1 < 0 || y2 < 0 || x1 >= pLCD->iCurrentWidth || x2 >= pLCD->iCurrentWidth || y1 >= pLCD->iCurrentHeight || y2 >= pLCD->iCurrentHeight)
        return;
    us = (usColor >> 8) | (usColor << 8); // byte swap for LCD byte order

    if(abs(dx) > abs(dy)) {
        // X major case
        if(x2 < x1) {
            dx = -dx;
            temp = x1;
            x1 = x2;
            x2 = temp;
            temp = y1;
            y1 = y2;
            y2 = temp;
        }
#ifdef ESP32_DMA
        for (x=0; x<dx+1; x++) // prepare color data for max length line
            usTemp[x] = us;
#endif
//        spilcdSetPosition(x1, y1, dx+1, 1); // set the starting position in both X and Y
        y = y1;
        dy = (y2 - y1);
        error = dx >> 1;
        yinc = 1;
        if (dy < 0)
        {
            dy = -dy;
            yinc = -1;
        }
        for(x = x1; x1 <= x2; x1++) {
            error -= dy;
            if (error < 0) // y needs to change, write existing pixels
            {
                error += dx;
		iLen = (x1-x+1);
                spilcdSetPosition(pLCD, x, y, iLen, 1, iFlags);
#ifndef ESP32_DMA
	        for (i=0; i<iLen; i++) // prepare color data for max length line
                   usTemp[i] = us;
#endif
                myspiWrite(pLCD, (uint8_t*)usTemp, iLen*2, MODE_DATA, iFlags); // write the row we changed
                y += yinc;
//                spilcdSetPosY(y, 1); // update the y position only
                x = x1+1; // we've already written the pixel at x1
            }
        } // for x1
        if (x != x1) // some data needs to be written
        {
	    iLen = (x1-x+1);
#ifndef ESP32_DMA
            for (temp=0; temp<iLen; temp++) // prepare color data for max length line
               usTemp[temp] = us;
#endif
            spilcdSetPosition(pLCD, x, y, iLen, 1, iFlags);
            myspiWrite(pLCD, (uint8_t*)usTemp, iLen*2, MODE_DATA, iFlags); // write the row we changed
        }
    }
    else {
        // Y major case
        if(y1 > y2) {
            dy = -dy;
            temp = x1;
            x1 = x2;
            x2 = temp;
            temp = y1;
            y1 = y2;
            y2 = temp;
        }
#ifdef ESP32_DMA
        for (x=0; x<dy+1; x++) // prepare color data for max length line
            usTemp[x] = us;
#endif
//        spilcdSetPosition(x1, y1, 1, dy+1); // set the starting position in both X and Y
        dx = (x2 - x1);
        error = dy >> 1;
        xinc = 1;
        if (dx < 0)
        {
            dx = -dx;
            xinc = -1;
        }
        x = x1;
        for(y = y1; y1 <= y2; y1++) {
            error -= dx;
            if (error < 0) { // x needs to change, write any pixels we traversed
                error += dy;
                iLen = y1-y+1;
#ifndef ESP32_DMA
      		for (i=0; i<iLen; i++) // prepare color data for max length line
       		    usTemp[i] = us;
#endif
                spilcdSetPosition(pLCD, x, y, 1, iLen, iFlags);
                myspiWrite(pLCD, (uint8_t*)usTemp, iLen*2, MODE_DATA, iFlags); // write the row we changed
                x += xinc;
//                spilcdSetPosX(x, 1); // update the x position only
                y = y1+1; // we've already written the pixel at y1
            }
        } // for y
        if (y != y1) // write the last byte we modified if it changed
        {
	    iLen = y1-y+1;
#ifndef ESP32_DMA
            for (i=0; i<iLen; i++) // prepare color data for max length line
               usTemp[i] = us;
#endif
            spilcdSetPosition(pLCD, x, y, 1, iLen, iFlags);
            myspiWrite(pLCD, (uint8_t*)usTemp, iLen*2, MODE_DATA, iFlags); // write the row we changed
        }
    } // y major case
} /* spilcdDrawLine() */
//
// Decompress one line of 8-bit RLE data
//
unsigned char * DecodeRLE8(unsigned char *s, int iWidth, uint16_t *d, uint16_t *usPalette)
{
unsigned char c, ucRepeat, ucCount, ucColor=0;
long l;

   ucRepeat = 0;
   ucCount = 0;

   while (iWidth > 0)
   {
      if (ucCount) // some non-repeating bytes to deal with
      {  
         while (ucCount && iWidth > 0)
         {  
            ucCount--;
            iWidth--; 
            ucColor = *s++;
            *d++ = usPalette[ucColor];
         } 
         l = (long)s;
         if (l & 1) s++; // compressed data pointer must always be even
      }
      if (ucRepeat == 0 && iWidth > 0) // get a new repeat code or command byte
      {
         ucRepeat = *s++;
         if (ucRepeat == 0) // command code
         {
            c = *s++;
            switch (c)
            {
               case 0: // end of line
                 break; // we already deal with this
               case 1: // end of bitmap
                 break; // we already deal with this
               case 2: // move
                 c = *s++; // debug - delta X
                 d += c; iWidth -= c;
                 c = *s++; // debug - delta Y
                 break;
               default: // uncompressed data
                 ucCount = c;
                 break;
            } // switch on command byte
         }
         else
         {
            ucColor = *s++; // get the new colors
         }     
      }
      while (ucRepeat && iWidth > 0)
      {
         ucRepeat--;
         *d++ = usPalette[ucColor];
         iWidth--;
      } // while decoding the current line
   } // while pixels on the current line to draw
   return s;
} /* DecodeRLE8() */

//
// Decompress one line of 4-bit RLE data
//
unsigned char * DecodeRLE4(unsigned char *s, int iWidth, uint16_t *d, uint16_t *usPalette)
{
unsigned char c, ucOdd=0, ucRepeat, ucCount, ucColor, uc1=0, uc2=0;
long l;

   ucRepeat = 0;
   ucCount = 0;

   while (iWidth > 0)
   {
      if (ucCount) // some non-repeating bytes to deal with
      {
         while (ucCount && iWidth > 0)
         {
            ucCount--;
            iWidth--;
            ucColor = *s++;
            uc1 = ucColor >> 4; uc2 = ucColor & 0xf;
            *d++ = usPalette[uc1];
            if (ucCount && iWidth)
            {
               *d++ = usPalette[uc2];
               ucCount--;
               iWidth--;
            }
         }
         l = (long)s;
         if (l & 1) s++; // compressed data pointer must always be even
      }
      if (ucRepeat == 0 && iWidth > 0) // get a new repeat code or command byte
      {
         ucRepeat = *s++;
         if (ucRepeat == 0) // command code
         {
            c = *s++;
            switch (c)
            {
               case 0: // end of line
                 break; // we already deal with this
               case 1: // end of bitmap
                 break; // we already deal with this
               case 2: // move
                 c = *s++; // debug - delta X
                 d += c; iWidth -= c;
                 c = *s++; // debug - delta Y
                 break;
               default: // uncompressed data
                 ucCount = c;
                 break;
            } // switch on command byte
         }
         else
         {
            ucOdd = 0; // start on an even source pixel
            ucColor = *s++; // get the new colors
            uc1 = ucColor >> 4; uc2 = ucColor & 0xf;
         }     
      }
      while (ucRepeat && iWidth > 0)
      {
         ucRepeat--;
         *d++ = (ucOdd) ? usPalette[uc2] : usPalette[uc1]; 
         ucOdd = !ucOdd;
         iWidth--;
      } // while decoding the current line
   } // while pixels on the current line to draw
   return s;
} /* DecodeRLE4() */

//
// Draw a 4, 8 or 16-bit Windows uncompressed bitmap onto the display
// Pass the pointer to the beginning of the BMP file
// Optionally stretch to 2x size
// Optimized for drawing to the backbuffer. The transparent color index is only used
// when drawinng to the back buffer. Set it to -1 to disable
// returns -1 for error, 0 for success
//
int spilcdDrawBMP(SPILCD *pLCD, uint8_t *pBMP, int iDestX, int iDestY, int bStretch, int iTransparent, int iFlags)
{
    int iOffBits, iPitch;
    uint16_t usPalette[256];
    uint8_t *pCompressed;
    uint8_t ucCompression;
    int16_t cx, cy, bpp, y; // offset to bitmap data
    int j, x;
    uint16_t *pus, us, *d, *usTemp = (uint16_t *)ucRXBuf; // process a line at a time
    uint8_t bFlipped = false;
    
    if (pBMP[0] != 'B' || pBMP[1] != 'M') // must start with 'BM'
        return -1; // not a BMP file
    cx = pBMP[18] | pBMP[19]<<8;
    cy = pBMP[22] | pBMP[23]<<8;
    ucCompression = pBMP[30]; // 0 = uncompressed, 1/2/4 = RLE compressed
    if (ucCompression > 4) // unsupported feature
        return -1;
    if (cy > 0) // BMP is flipped vertically (typical)
        bFlipped = true;
    else
        cy = -cy;
    bpp = pBMP[28] | pBMP[29]<<8;
    if (bpp != 16 && bpp != 4 && bpp != 8) // must be 4/8/16 bits per pixel
        return -1;
    if (iDestX + cx > pLCD->iCurrentWidth || iDestX < 0 || cx < 0)
        return -1; // invalid
    if (iDestY + cy > pLCD->iCurrentHeight || iDestY < 0 || cy < 0)
        return -1;
    if (iTransparent != -1) // transparent drawing can only happen on the back buffer
        iFlags = DRAW_TO_RAM;
    iOffBits = pBMP[10] | pBMP[11]<<8;
    iPitch = (cx * bpp) >> 3; // bytes per line
    iPitch = (iPitch + 3) & 0xfffc; // must be dword aligned
    // Get the palette as RGB565 values (if there is one)
    if (bpp == 4 || bpp == 8)
    {
        uint16_t r, g, b, us;
        int iOff, iColors;
        iColors = pBMP[46]; // colors used BMP field
        if (iColors == 0 || iColors > (1<<bpp))
            iColors = (1 << bpp); // full palette
        iOff = iOffBits - (4 * iColors); // start of color palette
        for (x=0; x<iColors; x++)
        {
            b = pBMP[iOff++];
            g = pBMP[iOff++];
            r = pBMP[iOff++];
            iOff++; // skip extra byte
            r >>= 3;
            us = (r  << 11);
            g >>= 2;
            us |= (g << 5);
            us |= (b >> 3);
            usPalette[x] = (us >> 8) | (us << 8); // swap byte order for writing to the display
        }
    }
    if (ucCompression) // need to do it differently for RLE compressed
    { 
    uint16_t *d = (uint16_t *)ucRXBuf;
    int y, iStartY, iEndY, iDeltaY;
   
       pCompressed = &pBMP[iOffBits]; // start of compressed data
       if (bFlipped)
       {  
          iStartY = iDestY + cy - 1;
          iEndY = iDestY - 1;
          iDeltaY = -1;
       }
       else
       {  
          iStartY = iDestY;
          iEndY = iDestY + cy;
          iDeltaY = 1;
       }
       for (y=iStartY; y!= iEndY; y += iDeltaY)
       {  
          spilcdSetPosition(pLCD, iDestX, y, cx, 1, iFlags);
          if (bpp == 4)
             pCompressed = DecodeRLE4(pCompressed, cx, d, usPalette);
          else
             pCompressed = DecodeRLE8(pCompressed, cx, d, usPalette);
          spilcdWriteDataBlock(pLCD, (uint8_t *)d, cx*2, iFlags);
       } 
       return 0;
    } // RLE compressed

    if (bFlipped)
    {
        iOffBits += (cy-1) * iPitch; // start from bottom
        iPitch = -iPitch;
    }

        if (bStretch)
        {
            spilcdSetPosition(pLCD, iDestX, iDestY, cx*2, cy*2, iFlags);
            for (y=0; y<cy; y++)
            {
                pus = (uint16_t *)&pBMP[iOffBits + (y * iPitch)]; // source line
                for (j=0; j<2; j++) // for systems without half-duplex, we need to prepare the data for each write
                {
                    if (iFlags & DRAW_TO_LCD)
                        d = usTemp;
                    else
                        d = (uint16_t *)&pLCD->pBackBuffer[pLCD->iOffset + (y*pLCD->iScreenPitch)];
                    if (bpp == 16)
                    {
                        if (iTransparent == -1) // no transparency
                        {
                            for (x=0; x<cx; x++)
                            {
                                us = pus[x];
                                d[0] = d[1] = (us >> 8) | (us << 8); // swap byte order
                                d += 2;
                            } // for x
                        }
                        else
                        {
                            for (x=0; x<cx; x++)
                            {
                                us = pus[x];
                                if (us != (uint16_t)iTransparent)
                                    d[0] = d[1] = (us >> 8) | (us << 8); // swap byte order
                                d += 2;
                            } // for x
                        }
                    }
                    else if (bpp == 8)
                    {
                        uint8_t *s = (uint8_t *)pus;
                        if (iTransparent == -1) // no transparency
                        {
                            for (x=0; x<cx; x++)
                            {
                                d[0] = d[1] = usPalette[*s++];
                                d += 2;
                            }
                        }
                        else
                        {
                            for (x=0; x<cx; x++)
                            {
                                uint8_t uc = *s++;
                                if (uc != (uint8_t)iTransparent)
                                    d[0] = d[1] = usPalette[uc];
                                d += 2;
                            }
                        }
                    }
                    else // 4 bpp
                    {
                        uint8_t uc, *s = (uint8_t *)pus;
                        if (iTransparent == -1) // no transparency
                        {
                            for (x=0; x<cx; x+=2)
                            {
                                uc = *s++;
                                d[0] = d[1] = usPalette[uc >> 4];
                                d[2] = d[3] = usPalette[uc & 0xf];
                                d += 4;
                            }
                        }
                        else
                        {
                            for (x=0; x<cx; x+=2)
                            {
                                uc = *s++;
                                if ((uc >> 4) != (uint8_t)iTransparent)
                                    d[0] = d[1] = usPalette[uc >> 4];
                                if ((uc & 0xf) != (uint8_t)iTransparent)
                                    d[2] = d[3] = usPalette[uc & 0xf];
                                d += 4;
                            }
                        }
                    }
                    if (iFlags & DRAW_TO_LCD)
                        spilcdWriteDataBlock(pLCD, (uint8_t *)usTemp, cx*4, iFlags); // write the same line twice
                } // for j
            } // for y
        } // 2:1
        else // 1:1
        {
            spilcdSetPosition(pLCD, iDestX, iDestY, cx, cy, iFlags);
            for (y=0; y<cy; y++)
            {
                pus = (uint16_t *)&pBMP[iOffBits + (y * iPitch)]; // source line
                if (bpp == 16)
                {
                    if (iFlags & DRAW_TO_LCD)
                        d = usTemp;
                    else
                        d = (uint16_t *)&pLCD->pBackBuffer[pLCD->iOffset + (y * pLCD->iScreenPitch)];
                    if (iTransparent == -1) // no transparency
                    {
                        for (x=0; x<cx; x++)
                        {
                           us = *pus++;
                           *d++ = (us >> 8) | (us << 8); // swap byte order
                        }
                    }
                    else // skip transparent pixels
                    {
                        for (x=0; x<cx; x++)
                        {
                            us = *pus++;
                            if (us != (uint16_t)iTransparent)
                             d[0] = (us >> 8) | (us << 8); // swap byte order
                            d++;
                        }
                    }
                }
                else if (bpp == 8)
                {
                    uint8_t uc, *s = (uint8_t *)pus;
                    if (iFlags & DRAW_TO_LCD)
                        d = usTemp;
                    else
                        d = (uint16_t *)&pLCD->pBackBuffer[pLCD->iOffset + (y*pLCD->iScreenPitch)];
                    if (iTransparent == -1) // no transparency
                    {
                        for (x=0; x<cx; x++)
                        {
                            *d++ = usPalette[*s++];
                        }
                    }
                    else
                    {
                        for (x=0; x<cx; x++)
                        {
                            uc = *s++;
                            if (uc != iTransparent)
                                d[0] = usPalette[*s++];
                            d++;
                        }
                    }
                }
                else // 4 bpp
                {
                    uint8_t uc, *s = (uint8_t *)pus;
                    if (iFlags & DRAW_TO_LCD)
                        d = usTemp;
                    else // write to the correct spot directly to save time
                        d = (uint16_t *)&pLCD->pBackBuffer[pLCD->iOffset + (y*pLCD->iScreenPitch)];
                    if (iTransparent == -1) // no transparency
                    {
                        for (x=0; x<cx; x+=2)
                        {
                            uc = *s++;
                            *d++ = usPalette[uc >> 4];
                            *d++ = usPalette[uc & 0xf];
                        }
                    }
                    else // check transparent color
                    {
                        for (x=0; x<cx; x+=2)
                        {
                            uc = *s++;
                            if ((uc >> 4) != iTransparent)
                               d[0] = usPalette[uc >> 4];
                            if ((uc & 0xf) != iTransparent)
                               d[1] = usPalette[uc & 0xf];
                            d += 2;
                        }
                    }
                }
                if (iFlags & DRAW_TO_LCD)
                    spilcdWriteDataBlock(pLCD, (uint8_t *)usTemp, cx*2, iFlags);
            } // for y
        } // 1:1
    return 0;
} /* spilcdDrawBMP() */

#ifndef __AVR__
//
// Returns the current backbuffer address
//
uint16_t * spilcdGetBuffer(SPILCD *pLCD)
{
    return (uint16_t *)pLCD->pBackBuffer;
}
//
// Allocate the back buffer for delayed rendering operations
// returns -1 for failure, 0 for success
//
int spilcdAllocBackbuffer(SPILCD *pLCD)
{
    if (pLCD->pBackBuffer != NULL) // already allocated
        return -1;
    pLCD->iScreenPitch = pLCD->iCurrentWidth * 2;
    pLCD->pBackBuffer = (uint8_t *)malloc(pLCD->iScreenPitch * pLCD->iCurrentHeight);
    if (pLCD->pBackBuffer == NULL) // no memory
        return -1;
    memset(pLCD->pBackBuffer, 0, pLCD->iScreenPitch * pLCD->iCurrentHeight);
    pLCD->iOffset = 0; // starting offset
    pLCD->iMaxOffset = pLCD->iScreenPitch * pLCD->iCurrentHeight; // can't write past this point
    pLCD->iWindowX = pLCD->iWindowY = 0; // current window = whole display
    pLCD->iWindowCX = pLCD->iCurrentWidth;
    pLCD->iWindowCY = pLCD->iCurrentHeight;
    return 0;
}
//
// Free the back buffer
//
void spilcdFreeBackbuffer(SPILCD *pLCD)
{
    if (pLCD->pBackBuffer)
    {
        free(pLCD->pBackBuffer);
        pLCD->pBackBuffer = NULL;
    }
}
//
// Rotate a 1-bpp mask image around a given center point
// valid angles are 0-359
//
void spilcdRotateBitmap(uint8_t *pSrc, uint8_t *pDest, int iBpp, int iWidth, int iHeight, int iPitch, int iCenterX, int iCenterY, int iAngle)
{
int32_t i, x, y;
int16_t pre_sin[512], pre_cos[512], *pSin, *pCos;
int32_t tx, ty, sa, ca;
uint8_t *s, *d, uc, ucMask;
uint16_t *uss, *usd;

    if (pSrc == NULL || pDest == NULL || iWidth < 1 || iHeight < 1 || iPitch < 1 || iAngle < 0 || iAngle > 359 || iCenterX < 0 || iCenterX >= iWidth || iCenterY < 0 || iCenterY >= iHeight || (iBpp != 1 && iBpp != 16))
        return;
    // since we're rotating from dest back to source, reverse the angle
    iAngle = 360 - iAngle;
    if (iAngle == 360) // just copy src to dest
    {
        memcpy(pDest, pSrc, iHeight * iPitch);
        return;
    }
    // Create a quicker lookup table for sin/cos pre-multiplied at the given angle
    sa = (int32_t)i16SineTable[iAngle]; // sine of given angle
    ca = (int32_t)i16SineTable[iAngle+90]; // cosine of given angle
    for (i=-256; i<256; i++) // create the pre-calc tables
    {
        pre_sin[i+256] = (sa * i) >> 15; // sin * x
        pre_cos[i+256] = (ca * i) >> 15;
    }
    pSin = &pre_sin[256]; pCos = &pre_cos[256]; // point to 0 points in tables
    for (y=0; y<iHeight; y++)
    {
        int16_t siny = pSin[y-iCenterY];
        int16_t cosy = pCos[y-iCenterY];
        d = &pDest[y * iPitch];
        usd = (uint16_t *)d;
        ucMask = 0x80;
        uc = 0;
        for (x=0; x<iWidth; x++)
        {
            // Rotate from the destination pixel back to the source to not have gaps
            // x' = cos*x - sin*y, y' = sin*x + cos*y
            tx = iCenterX + pCos[x-iCenterX] - siny;
            ty = iCenterY + pSin[x-iCenterX] + cosy;
            if (iBpp == 1)
            {
                if (tx > 0 && ty > 0 && tx < iWidth && ty < iHeight) // check source pixel
                {
                    s = &pSrc[(ty*iPitch)+(tx>>3)];
                    if (s[0] & (0x80 >> (tx & 7)))
                        uc |= ucMask; // set destination pixel
                }
                ucMask >>= 1;
                if (ucMask == 0) // write the byte into the destination bitmap
                {
                    ucMask = 0x80;
                    *d++ = uc;
                    uc = 0;
                }
            }
            else // 16-bpp
            {
                if (tx > 0 && ty > 0 && tx < iWidth && ty < iHeight) // check source pixel
                {
                    uss = (uint16_t *)&pSrc[(ty*iPitch)+(tx*2)];
                    *usd++ = uss[0]; // copy the pixel
                }
            }
        }
        if (iBpp == 1 && ucMask != 0x80) // store partial byte
            *d++ = uc;
    } // for y
} /* spilcdRotateMask() */
#endif // !__AVR__
