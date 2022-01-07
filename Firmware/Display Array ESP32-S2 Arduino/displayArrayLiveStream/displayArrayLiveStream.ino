#include <Adafruit_NeoPixel.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include <YouTubeLiveStream.h>
#include <ArduinoJson.h>
#include <bb_spi_lcd.h>

#include "Youtubers.h"

struct YTChannelDetails
{
  const char *id;
  const char *name;
  bool live;
};

#define NUM_CHANNELS 6

YTChannelDetails channels[NUM_CHANNELS] = {

  {"UCezJOfu7OtqGzd5xrP3q6WA", "Brian Lough", false}, // https://www.youtube.com/channel/UCezJOfu7OtqGzd5xrP3q6WA
  {"UCUW49KGPezggFi0PGyDvcvg", "Zack Freedman", false}, // https://www.youtube.com/channel/UCUW49KGPezggFi0PGyDvcvg
  {"UCu94OHbBYVUXLYPh4NBu10w", "Unexpected Maker", false}, // https://www.youtube.com/channel/UCu94OHbBYVUXLYPh4NBu10w
  {"UCz_G0tNVpnFzwADHkoM5brA", "Dave Darko", false}, // https://www.youtube.com/channel/UCz_G0tNVpnFzwADHkoM5brA
  {"UCp_5PO66faM4dBFbFFBdPSQ", "Bitluni", false}, // https://www.youtube.com/channel/UCp_5PO66faM4dBFbFFBdPSQ
  {"UCv7UOhZ2XuPwm9SN5oJsCjA", "Intermit.Tech", false}, // https://www.youtube.com/channel/UCv7UOhZ2XuPwm9SN5oJsCjA
};

const char *ssid     = "SSID";
const char *password = "Password";

WiFiClientSecure client;
YouTubeLiveStream ytVideo(client, NULL); //"scrapeIsChannelLive" doesn't require a key.

unsigned long delayBetweenRequests = 5000; // Time between requests (5 seconds)
unsigned long requestDueTime;               //time when request due

int channelIndex = 0;
int listChange = false;

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
#define BLK_PIN       37
#define MISO_PIN      -1

// ST7735 width
#define DISPLAY_WIDTH 160
#define DISPLAY_HEIGHT 80

// Backlight PWM Control
#define BLK_CHANNEL_0     0     // use first channel of 16 channels (started from zero)
#define BLK_TIMER_13_BIT  13    // use 13 bit precission for BLK timer
#define BLK_BASE_FREQ     5000  // use 5000 Hz as a BLK base frequency

int brightness = 80;    // how bright the Backlight LED is

#define bufferLenght    800
#define buffercounts    32

Adafruit_NeoPixel pixel = Adafruit_NeoPixel(1, PINSLED, NEO_GRB + NEO_KHZ800);

uint8_t imgBuffer[bufferLenght];
SPILCD lcd[6];


// Arduino like analogWrite
// value has to be between 0 and valueMax
void ledcAnalogWrite(uint8_t channel, uint32_t value, uint32_t valueMax = 100) {
  // calculate duty, 8191 from 2 ^ 13 - 1
  uint32_t duty = (8191 / valueMax) * min(value, valueMax);

  // write duty to LEDC
  ledcWrite(channel, duty);
}

void lcdDrawYoutuber(uint8_t Display, uint8_t Youtuber){
  spilcdSetPosition(&lcd[Display],0,0, 80, 160, DRAW_TO_LCD);
  switch(Youtuber){
    case 0:
      for(int k = 0; k < buffercounts; k++){
        for(int j = 0; j < bufferLenght; j++){
          imgBuffer[j] = BrianOff[j+(bufferLenght*k)];
        }
          spilcdWriteDataBlock(&lcd[Display],(uint8_t *)imgBuffer, sizeof(imgBuffer),  DRAW_TO_LCD | DRAW_WITH_DMA);
      }
    break;
    case 6:
      for(int k = 0; k < buffercounts; k++){
        for(int j = 0; j < bufferLenght; j++){
          imgBuffer[j] = BrianLive[j+(bufferLenght*k)];
        }
          spilcdWriteDataBlock(&lcd[Display],(uint8_t *)imgBuffer, sizeof(imgBuffer),  DRAW_TO_LCD | DRAW_WITH_DMA);
      }
    break;
    case 1:
      for(int k = 0; k < buffercounts; k++){
        for(int j = 0; j < bufferLenght; j++){
          imgBuffer[j] = ZackOff[j+(bufferLenght*k)];
        }
          spilcdWriteDataBlock(&lcd[Display],(uint8_t *)imgBuffer, sizeof(imgBuffer),  DRAW_TO_LCD | DRAW_WITH_DMA);
      }
    break;
    case 7:
      for(int k = 0; k < buffercounts; k++){
        for(int j = 0; j < bufferLenght; j++){
          imgBuffer[j] = ZackLive[j+(bufferLenght*k)];
        }
          spilcdWriteDataBlock(&lcd[Display],(uint8_t *)imgBuffer, sizeof(imgBuffer),  DRAW_TO_LCD | DRAW_WITH_DMA);
      }
    break;
    case 2:
      for(int k = 0; k < buffercounts; k++){
        for(int j = 0; j < bufferLenght; j++){
          imgBuffer[j] = UnexpOff[j+(bufferLenght*k)];
        }
          spilcdWriteDataBlock(&lcd[Display],(uint8_t *)imgBuffer, sizeof(imgBuffer),  DRAW_TO_LCD | DRAW_WITH_DMA);
      }
    break;
    case 8:
      for(int k = 0; k < buffercounts; k++){
        for(int j = 0; j < bufferLenght; j++){
          imgBuffer[j] = UnexpLive[j+(bufferLenght*k)];
        }
          spilcdWriteDataBlock(&lcd[Display],(uint8_t *)imgBuffer, sizeof(imgBuffer),  DRAW_TO_LCD | DRAW_WITH_DMA);
      }
    break;
    case 3:
      for(int k = 0; k < buffercounts; k++){
        for(int j = 0; j < bufferLenght; j++){
          imgBuffer[j] = DaveOff[j+(bufferLenght*k)];
        }
          spilcdWriteDataBlock(&lcd[Display],(uint8_t *)imgBuffer, sizeof(imgBuffer),  DRAW_TO_LCD | DRAW_WITH_DMA);
      }
    break;
    case 9:
      for(int k = 0; k < buffercounts; k++){
        for(int j = 0; j < bufferLenght; j++){
          imgBuffer[j] = DaveLive[j+(bufferLenght*k)];
        }
          spilcdWriteDataBlock(&lcd[Display],(uint8_t *)imgBuffer, sizeof(imgBuffer),  DRAW_TO_LCD | DRAW_WITH_DMA);
      }
    break;
    case 4:
      for(int k = 0; k < buffercounts; k++){
        for(int j = 0; j < bufferLenght; j++){
          imgBuffer[j] = BitluniOff[j+(bufferLenght*k)];
        }
          spilcdWriteDataBlock(&lcd[Display],(uint8_t *)imgBuffer, sizeof(imgBuffer),  DRAW_TO_LCD | DRAW_WITH_DMA);
      }
    break;
    case 10:
      for(int k = 0; k < buffercounts; k++){
        for(int j = 0; j < bufferLenght; j++){
          imgBuffer[j] = BitluniLive[j+(bufferLenght*k)];
        }
          spilcdWriteDataBlock(&lcd[Display],(uint8_t *)imgBuffer, sizeof(imgBuffer),  DRAW_TO_LCD | DRAW_WITH_DMA);
      }
    break;
    case 5:
      for(int k = 0; k < buffercounts; k++){
        for(int j = 0; j < bufferLenght; j++){
          imgBuffer[j] = IntermitOff[j+(bufferLenght*k)];
        }
          spilcdWriteDataBlock(&lcd[Display],(uint8_t *)imgBuffer, sizeof(imgBuffer),  DRAW_TO_LCD | DRAW_WITH_DMA);
      }
    break;
    case 11:
      for(int k = 0; k < buffercounts; k++){
        for(int j = 0; j < bufferLenght; j++){
          imgBuffer[j] = IntermitLive[j+(bufferLenght*k)];
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

  pixel.setPixelColor(0, 255,0,0);
  pixel.show();

  ledcSetup(BLK_CHANNEL_0, BLK_BASE_FREQ, BLK_TIMER_13_BIT);
  ledcAttachPin(BLK_PIN, BLK_CHANNEL_0);
  
  pinMode(RESET_PIN, OUTPUT);
  digitalWrite(RESET_PIN, HIGH);
  digitalWrite(RESET_PIN, LOW);
  delay(100);
  digitalWrite(RESET_PIN, HIGH);

  for (int i=0; i<6; i++) {
    spilcdInit(&lcd[i], LCD_ST7735S_B, FLAGS_INVERT | FLAGS_SWAP_RB, 40000000, CS1_PIN + i, DC_PIN, -1, BLK_PIN, MISO_PIN, MOSI_PIN, SCK_PIN);
    spilcdSetOrientation(&lcd[i],  LCD_ORIENTATION_180); 
  }
  lcdDrawYoutuber(0,0);
  lcdDrawYoutuber(1,1);
  lcdDrawYoutuber(2,2);
  lcdDrawYoutuber(3,3);
  lcdDrawYoutuber(4,4);
  lcdDrawYoutuber(5,5);

  ledcAnalogWrite(BLK_CHANNEL_0, brightness);

  uint8_t wifiConectionAttmps = 0;
 
  WiFi.begin(ssid, password);

  Serial.print("Connecting to ");
  Serial.print(ssid);
  while ( WiFi.status() != WL_CONNECTED & wifiConectionAttmps < 25) {
    delay(500);Serial.print ( "." );
    wifiConectionAttmps++;
  }
  
  if( WiFi.status() == WL_CONNECTED){
    Serial.println( "OK." );
    Serial.print("IP address: ");
    IPAddress ip = WiFi.localIP();
    Serial.println(ip);
    
    pixel.setPixelColor(0, 0,255,0);
    pixel.show();
  }else{
    Serial.println( "ERROR." );
    Serial.println("Connection Failed!, please check credentials and try again.");
    pixel.setPixelColor(0, 0,0,255);
    pixel.show();
  } 

  client.setInsecure();
}

void loop() {

  if (millis() > requestDueTime)
  {
    if (ytVideo.scrapeIsChannelLive(channels[channelIndex].id)) {
      if (!channels[channelIndex].live)
      {
        channels[channelIndex].live = true;
        listChange = true;
      }
      Serial.print(channels[channelIndex].name);
      Serial.println(" is live");

      lcdDrawYoutuber(channelIndex,channelIndex+6);
      
      requestDueTime = millis() + delayBetweenRequests;
    } else {
      if (channels[channelIndex].live)
      {
        channels[channelIndex].live = false;
        listChange = true;
      }
      Serial.print(channels[channelIndex].name);
      Serial.println(" is NOT live");

      lcdDrawYoutuber(channelIndex,channelIndex);
      
      requestDueTime = millis() + 5000;
    }
    channelIndex++;
    if (channelIndex >= NUM_CHANNELS)
    {
      channelIndex = 0;
    }
  }

  if (listChange) {
    listChange = false;
    Serial.println("-----------------");
    Serial.println("    Live Now");
    Serial.println("-----------------");
    for (int i = 0; i < NUM_CHANNELS; i++) {
      if (channels[i].live) {
        Serial.println(channels[i].name);
      }
    }
    Serial.println("-----------------");
  }
  
}
