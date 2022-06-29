#include <Adafruit_NeoPixel.h>
#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <bb_spi_lcd.h>

#include "clockPixie.h"
#include "clockFlags.h"

const char *ssid     = "WifiSSID";
const char *password = "WifiPassword";

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

#define PINSLED       21

#define CS1_PIN       1
#define CS2_PIN       2
#define CS3_PIN       3
#define CS4_PIN       4
#define CS5_PIN       5
#define CS6_PIN       6

#define RESET_PIN     33
#define DC_PIN        34
#define MOSI_PIN      35
#define SCK_PIN       36
#define LED_PIN       37
#define MISO_PIN      -1

// ST7735 width
#define DISPLAY_WIDTH 160
#define DISPLAY_HEIGHT 80

#define bufferLenght    800
#define buffercounts    32

Adafruit_NeoPixel pixel = Adafruit_NeoPixel(1, PINSLED, NEO_GRB + NEO_KHZ800);

uint8_t imgBuffer[bufferLenght];

uint8_t secondsCounter = 0;
uint8_t secondsStatus = 0;
uint8_t prevHours,prevMinutes;

SPILCD lcd[6];

void lcdDrawNumber(uint8_t Display, uint8_t Number){
  spilcdSetPosition(&lcd[Display],0,0, 80, 160, DRAW_TO_LCD);
  switch(Number){
    case 0:
      for(int k = 0; k < buffercounts; k++){
        for(int j = 0; j < bufferLenght; j++){
          imgBuffer[j] = zero_Pixie[j+(bufferLenght*k)];
        }
          spilcdWriteDataBlock(&lcd[Display],(uint8_t *)imgBuffer, sizeof(imgBuffer),  DRAW_TO_LCD | DRAW_WITH_DMA);
      }
    break;
    case 1:
      for(int k = 0; k < buffercounts; k++){
        for(int j = 0; j < bufferLenght; j++){
          imgBuffer[j] = one_Pixie[j+(bufferLenght*k)];
        }
          spilcdWriteDataBlock(&lcd[Display],(uint8_t *)imgBuffer, sizeof(imgBuffer),  DRAW_TO_LCD | DRAW_WITH_DMA);
      }
    break;
    case 2:
      for(int k = 0; k < buffercounts; k++){
        for(int j = 0; j < bufferLenght; j++){
          imgBuffer[j] = two_Pixie[j+(bufferLenght*k)];
        }
          spilcdWriteDataBlock(&lcd[Display],(uint8_t *)imgBuffer, sizeof(imgBuffer),  DRAW_TO_LCD | DRAW_WITH_DMA);
      }
    break;
    case 3:
      for(int k = 0; k < buffercounts; k++){
        for(int j = 0; j < bufferLenght; j++){
          imgBuffer[j] = three_Pixie[j+(bufferLenght*k)];
        }
          spilcdWriteDataBlock(&lcd[Display],(uint8_t *)imgBuffer, sizeof(imgBuffer),  DRAW_TO_LCD | DRAW_WITH_DMA);
      }
    break;
    case 4:
      for(int k = 0; k < buffercounts; k++){
        for(int j = 0; j < bufferLenght; j++){
          imgBuffer[j] = four_Pixie[j+(bufferLenght*k)];
        }
          spilcdWriteDataBlock(&lcd[Display],(uint8_t *)imgBuffer, sizeof(imgBuffer),  DRAW_TO_LCD | DRAW_WITH_DMA);
      }
    break;
    case 5:
      for(int k = 0; k < buffercounts; k++){
        for(int j = 0; j < bufferLenght; j++){
          imgBuffer[j] = five_Pixie[j+(bufferLenght*k)];
        }
          spilcdWriteDataBlock(&lcd[Display],(uint8_t *)imgBuffer, sizeof(imgBuffer),  DRAW_TO_LCD | DRAW_WITH_DMA);
      }
    break;
    case 6:
      for(int k = 0; k < buffercounts; k++){
        for(int j = 0; j < bufferLenght; j++){
          imgBuffer[j] = six_Pixie[j+(bufferLenght*k)];
        }
          spilcdWriteDataBlock(&lcd[Display],(uint8_t *)imgBuffer, sizeof(imgBuffer),  DRAW_TO_LCD | DRAW_WITH_DMA);
      }
    break;
    case 7:
      for(int k = 0; k < buffercounts; k++){
        for(int j = 0; j < bufferLenght; j++){
          imgBuffer[j] = seven_Pixie[j+(bufferLenght*k)];
        }
          spilcdWriteDataBlock(&lcd[Display],(uint8_t *)imgBuffer, sizeof(imgBuffer),  DRAW_TO_LCD | DRAW_WITH_DMA);
      }
    break;
    case 8:
      for(int k = 0; k < buffercounts; k++){
        for(int j = 0; j < bufferLenght; j++){
          imgBuffer[j] = eight_Pixie[j+(bufferLenght*k)];
        }
          spilcdWriteDataBlock(&lcd[Display],(uint8_t *)imgBuffer, sizeof(imgBuffer),  DRAW_TO_LCD | DRAW_WITH_DMA);
      }
    break;
    case 9:
      for(int k = 0; k < buffercounts; k++){
        for(int j = 0; j < bufferLenght; j++){

          imgBuffer[j] = nine_Pixie[j+(bufferLenght*k)];
        }
          spilcdWriteDataBlock(&lcd[Display],(uint8_t *)imgBuffer, sizeof(imgBuffer),  DRAW_TO_LCD | DRAW_WITH_DMA);
      }
    break;
    case 10:
      for(int k = 0; k < buffercounts; k++){
        for(int j = 0; j < bufferLenght; j++){
          imgBuffer[j] = colon_Pixie[j+(bufferLenght*k)];
        }
          spilcdWriteDataBlock(&lcd[Display],(uint8_t *)imgBuffer, sizeof(imgBuffer),  DRAW_TO_LCD | DRAW_WITH_DMA);
      }
    break;
    case 11:
      for(int k = 0; k < buffercounts; k++){
        for(int j = 0; j < bufferLenght; j++){
          imgBuffer[j] = space_Pixie[j+(bufferLenght*k)];
        }
          spilcdWriteDataBlock(&lcd[Display],(uint8_t *)imgBuffer, sizeof(imgBuffer),  DRAW_TO_LCD | DRAW_WITH_DMA);
      }
    break;
    case 12:
      for(int k = 0; k < buffercounts; k++){
        for(int j = 0; j < bufferLenght; j++){
          imgBuffer[j] = slash_Pixie[j+(bufferLenght*k)];
        }
          spilcdWriteDataBlock(&lcd[Display],(uint8_t *)imgBuffer, sizeof(imgBuffer),  DRAW_TO_LCD | DRAW_WITH_DMA);
      }
    break;
 }
}

void lcdDrawFlag(uint8_t Display, uint8_t Number){
  spilcdSetPosition(&lcd[Display],0,0, 80, 160, DRAW_TO_LCD);
  switch(Number){
    case 0:
      for(int k = 0; k < buffercounts; k++){
        for(int j = 0; j < bufferLenght; j++){
          imgBuffer[j] = mex_Flag[j+(bufferLenght*k)];
        }
          spilcdWriteDataBlock(&lcd[Display],(uint8_t *)imgBuffer, sizeof(imgBuffer),  DRAW_TO_LCD | DRAW_WITH_DMA);
      }
    break;
    case 1:
      for(int k = 0; k < buffercounts; k++){
        for(int j = 0; j < bufferLenght; j++){
          imgBuffer[j] = usa_Flag[j+(bufferLenght*k)];
        }
          spilcdWriteDataBlock(&lcd[Display],(uint8_t *)imgBuffer, sizeof(imgBuffer),  DRAW_TO_LCD | DRAW_WITH_DMA);
      }
    break;
    case 2:
      for(int k = 0; k < buffercounts; k++){
        for(int j = 0; j < bufferLenght; j++){
          imgBuffer[j] = gbr_Flag[j+(bufferLenght*k)];
        }
          spilcdWriteDataBlock(&lcd[Display],(uint8_t *)imgBuffer, sizeof(imgBuffer),  DRAW_TO_LCD | DRAW_WITH_DMA);
      }
    break;
 }
}

void setup() {
  Serial.begin(115200);
  while (!Serial);

  pixel.begin();
  pixel.setBrightness(50);
  pixel.show(); // Initialize all pixels to 'off'

  pixel.setPixelColor(0, 0,255,255);
  pixel.show();
  
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  pinMode(RESET_PIN, OUTPUT);
  digitalWrite(RESET_PIN, HIGH);
  digitalWrite(RESET_PIN, LOW);
  delay(100);
  digitalWrite(RESET_PIN, HIGH);

  for (int i=0; i<6; i++) {
    spilcdInit(&lcd[i], LCD_ST7735S_B, FLAGS_INVERT | FLAGS_SWAP_RB, 40000000, CS1_PIN + i, DC_PIN, -1, LED_PIN, MISO_PIN, MOSI_PIN, SCK_PIN);
    spilcdSetOrientation(&lcd[i],  LCD_ORIENTATION_180);
    lcdDrawNumber(i,11);
  }
  digitalWrite(LED_PIN, HIGH);
  lcdDrawFlag(5,0);

  uint8_t wifiConectionAttmps = 0;
 
  WiFi.begin(ssid, password);

  Serial.print("Connecting to ");
  Serial.print(ssid);
  while ( WiFi.status() != WL_CONNECTED & wifiConectionAttmps < 20) {
    delay(500);Serial.print ( "." );
    wifiConectionAttmps++;
  }
  
  if( WiFi.status() == WL_CONNECTED){
    Serial.println( "OK." );
  }else{
    Serial.println( "ERROR." );
    Serial.println("Connection Failed!, please check credentials and try again.");
  } 
  
  timeClient.begin();
  timeClient.setTimeOffset(-18000);
  timeClient.update();

  prevHours = 60;
  prevMinutes = 60;

}

void loop() {

        if(timeClient.getHours() != prevHours){
            prevHours = timeClient.getHours();
    
            if(timeClient.getHours() < 10){
                lcdDrawNumber(0,0);
                lcdDrawNumber(1,timeClient.getHours());
            }else if (timeClient.getHours() < 20){
                lcdDrawNumber(0,1);
                lcdDrawNumber(1,timeClient.getHours()-10);
            }else{
                lcdDrawNumber(0,2);
                lcdDrawNumber(1,timeClient.getHours()-20);
            }
        }     

        if(timeClient.getMinutes() != prevMinutes){
            prevMinutes = timeClient.getMinutes();

            if(timeClient.getMinutes() < 10){
                lcdDrawNumber(3,0);
                lcdDrawNumber(4,timeClient.getMinutes());
            }else if (timeClient.getMinutes() < 20){
                lcdDrawNumber(3,1);
                lcdDrawNumber(4,timeClient.getMinutes()-10);
            }else if (timeClient.getMinutes() < 30){
                lcdDrawNumber(3,2);
                lcdDrawNumber(4,timeClient.getMinutes()-20);
            }else if (timeClient.getMinutes() < 40){
                lcdDrawNumber(3,3);
                lcdDrawNumber(4,timeClient.getMinutes()-30);
            }else if (timeClient.getMinutes() < 50){
                lcdDrawNumber(3,4);
                lcdDrawNumber(4,timeClient.getMinutes()-40);
            }else{
                lcdDrawNumber(3,5);
                lcdDrawNumber(4,timeClient.getMinutes()-50);
            }
        } 

       if((secondsCounter > 25) & (secondsStatus == 0)){
            lcdDrawNumber(2,10); 
            //lcdDrawNumber(6,10); 
            secondsStatus = 1;
            secondsCounter = 0;
        } else if((secondsCounter > 25) & (secondsStatus == 1)){
            lcdDrawNumber(2,11); 
            //lcdDrawNumber(6,11); 
            secondsStatus = 0;
            secondsCounter = 0;
        }

        delay(20);
        secondsCounter++;
}
