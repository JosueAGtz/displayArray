#include <Preferences.h>
#include <bb_spi_lcd.h>
#include <PNGdec.h>
#include <string.h>
#include <SPIFFS.h>
//#include <SD.h>

#include <WiFi.h>
#include <WiFiUdp.h>
#include <NTPClient.h>

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

Preferences preferences;

const char *ssid     = "YourWifi_SSID";
const char *password = "YourPassword";

#define NUM_DISPLAYS       6      // 8 or 6
#define DISPLAY_HEIGHT     80     // 135 or 80  
#define DISPLAY_WIDTH      160    // 240 or 160

#define SYSTEM        0           // system Theme 
#define SYSCLK        1
#define CLKTHEME1     2           // clock Theme 1
#define CLKTHEME2     3           // clock Theme 2

#define CLOCKSCREEN   0
#define MENUSCREEN    1
#define TIMEOSCREEN   2
#define BRTSCREEN     3
#define THMSCREEN     4
#define SETTHMSCREEN  5

#define SYSOK         0
#define SYSERROR      1
#define NOWIFI        2

#define LEFTENTER     3
#define RIGHTBACK     4
#define UPDTTIME      5
#define TIMEOFFS      6
#define SETBRT        7
#define LOADTHM       8
#define UPDTTIMEU     9  
#define TIMEOFFSU     10
#define SETBRTU       11
#define LOADTHMU      12

#define BRTSCROLL     14
#define TIMEOSCROLL   15
#define THMSCROLL     16
#define SAVEBACK      17 
#define MERGEBACK     18
#define SETTHM1       19
#define SETTHM2       20  
#define TIMEUPDATED   21
#define CONNECTNG     22

#define COLON         10
#define DASH          11
#define SPACE         12
#define PERCENT       13
#define AM            14
#define PM            15
#define DEGREES       16
#define ALARM         17
#define BRT           18

#define RST_PIN       7
#define DC_PIN        8
#define TYPEDISP      9
#define ABUTTON       10
#define BBUTTON       11
#define CBUTTON       12
#define DBUTTON       13
#define BLK_PIN       14
#define AMP_PIN       16
#define DAC_PIN       17
#define LED_PIN       18
#define VBAT_PIN      19
#define VUSB_PIN      20
#define CD_PIN        33
#define CS_PIN        34
#define MOSI_PIN      35
#define SCK_PIN       36
#define MISO_PIN      37

#define BLK_CHANNEL        0      // use first channel of 16 channels (started from zero)
#define BLK_TIMER_13_BIT   13     // use 13 bit precission for LEDC timer
#define BLK_BASE_FREQ      36000  // use 36000 Hz as a LEDC base frequency

SPILCD lcd[NUM_DISPLAYS];
PNG png;

String selectedTheme;
char themeAddress[32];
char *themeDirectory[] = {"/clockTheme1/","/clockTheme2/","/clockTheme3/","/clockTheme4/","/clockTheme5/","/clockTheme6/",
                       "/clockTheme7/","/clockTheme8/","/clockTheme9/","/clockTheme10/","/clockTheme11/","/clockTheme12/",
                       "/clockTheme13/","/clockTheme14/"};

uint8_t brightnessTable[] = {1,3,5,10,15,20,30,40,50,60};     // How bright the LED 
uint8_t brightnessPcnt[] = {10,20,30,40,50,60,70,80,90,100};  // How bright the LED is in %
uint8_t brightness = 6;
uint8_t tmpBrightness = 0;

uint8_t tmpClockTheme = 0;
uint8_t clockTheme1 = 8;
uint8_t clockTheme2 = 11;

uint8_t timeOffsetSign;
uint8_t timeOffsetHrs;
int timeOffset = -5;                      // Default timeOffset for Mexico City
int tmpTimeOffset = 0;

unsigned long requestDueTime;             // Time when request due 
uint8_t currentScreen = CLOCKSCREEN;
uint8_t wifiConectionAttmps = 0;
uint8_t currentDisplay = 0;
uint8_t prevMenuSelection = 1;
uint8_t menuSelection = 0;
uint8_t secondStatus = 0;
uint8_t prevHours = 60;
uint8_t prevMinutes = 60;
uint8_t prevSeconds = 60;

// Function to set the backlight brightness with PWM
void setBacklight(uint8_t channel, uint32_t value, uint32_t valueMax = 100) {
  uint32_t duty = (8191 / valueMax) * min(value, valueMax);    // calculate duty, 8191 from 2 ^ 13 - 1
  ledcWrite(channel, duty);                                    // write duty to LEDC
}
// Functions to access a file on the SPIFFS
File myfile;
void * myOpen(const char *filename, int32_t *size) {
  myfile = SPIFFS.open(filename); // SPIFFS
  *size = myfile.size();
  return &myfile;
}
void myClose(void *handle) {
  if (myfile) myfile.close();
}
int32_t myRead(PNGFILE *handle, uint8_t *buffer, int32_t length) {
  if (!myfile) return 0;
  return myfile.read(buffer, length);
}
int32_t mySeek(PNGFILE *handle, int32_t position) {
  if (!myfile) return 0;
  return myfile.seek(position);
}
void PNGDraw(PNGDRAW *pDraw) {    // Function to draw pixels to the display
  unsigned short usPixels[80];
    png.getLineAsRGB565(pDraw, usPixels, PNG_RGB565_BIG_ENDIAN, 0xffffffff);
    spilcdWriteDataBlock(&lcd[currentDisplay], (uint8_t *)usPixels, sizeof(usPixels),  DRAW_TO_LCD | DRAW_WITH_DMA);
}

void drawDisplay(uint8_t display, uint8_t pngImage, uint8_t themeSet) {
  int rc;
  currentDisplay = display - 1;
  if (digitalRead(TYPEDISP))
    spilcdSetPosition(&lcd[currentDisplay], 27, 40, DISPLAY_HEIGHT, DISPLAY_WIDTH, DRAW_TO_LCD);
  else
    spilcdSetPosition(&lcd[currentDisplay], 0, 0, DISPLAY_HEIGHT, DISPLAY_WIDTH, DRAW_TO_LCD);
  switch (pngImage) {
    case 0:
      if(themeSet == SYSTEM)
        rc = png.open("/systemTheme/ok.png", myOpen, myClose, myRead, mySeek, PNGDraw);
      else if(themeSet == SYSCLK)
        rc = png.open("/systemTheme/zero.png", myOpen, myClose, myRead, mySeek, PNGDraw);
      else if (themeSet > SYSCLK){
        if(themeSet == CLKTHEME1)
          selectedTheme = String(themeDirectory[clockTheme1]) + "zero.png";
        else if (themeSet == CLKTHEME2)
          selectedTheme = String(themeDirectory[clockTheme2]) + "zero.png";
        selectedTheme.toCharArray(themeAddress,selectedTheme.length()+1);
        rc = png.open(themeAddress, myOpen, myClose, myRead, mySeek, PNGDraw);
      } 
    break;
    case 1:
      if(themeSet == SYSTEM)
        rc = png.open("/systemTheme/error.png", myOpen, myClose, myRead, mySeek, PNGDraw);
      else if(themeSet == SYSCLK)
        rc = png.open("/systemTheme/one.png", myOpen, myClose, myRead, mySeek, PNGDraw);
      else if (themeSet > SYSCLK){
        if(themeSet == CLKTHEME1)
          selectedTheme = String(themeDirectory[clockTheme1]) + "one.png";
        else if (themeSet == CLKTHEME2)
          selectedTheme = String(themeDirectory[clockTheme2]) + "one.png";
        selectedTheme.toCharArray(themeAddress,selectedTheme.length()+1);
        rc = png.open(themeAddress, myOpen, myClose, myRead, mySeek, PNGDraw);
      } 
    break;
    case 2:
      if(themeSet == SYSTEM)
        rc = png.open("/systemTheme/noWifi.png", myOpen, myClose, myRead, mySeek, PNGDraw);
      else if(themeSet == SYSCLK)
        rc = png.open("/systemTheme/two.png", myOpen, myClose, myRead, mySeek, PNGDraw);
      else if (themeSet > SYSCLK){
        if(themeSet == CLKTHEME1)
          selectedTheme = String(themeDirectory[clockTheme1]) + "two.png";
        else if (themeSet == CLKTHEME2)
          selectedTheme = String(themeDirectory[clockTheme2]) + "two.png";
        selectedTheme.toCharArray(themeAddress,selectedTheme.length()+1);
        rc = png.open(themeAddress, myOpen, myClose, myRead, mySeek, PNGDraw);
      } 
    break;
    case 3:
      if(themeSet == SYSTEM)
        rc = png.open("/systemTheme/leftEnter.png", myOpen, myClose, myRead, mySeek, PNGDraw);
      else if(themeSet == SYSCLK)
        rc = png.open("/systemTheme/three.png", myOpen, myClose, myRead, mySeek, PNGDraw);
      else if (themeSet > SYSCLK){
        if(themeSet == CLKTHEME1)
          selectedTheme = String(themeDirectory[clockTheme1]) + "three.png";
        else if (themeSet == CLKTHEME2)
          selectedTheme = String(themeDirectory[clockTheme2]) + "three.png";
        selectedTheme.toCharArray(themeAddress,selectedTheme.length()+1);
        rc = png.open(themeAddress, myOpen, myClose, myRead, mySeek, PNGDraw);
      } 
    break;
    case 4:
      if(themeSet == SYSTEM)
        rc = png.open("/systemTheme/rightBack.png", myOpen, myClose, myRead, mySeek, PNGDraw);
      else if(themeSet == SYSCLK)
        rc = png.open("/systemTheme/four.png", myOpen, myClose, myRead, mySeek, PNGDraw);
      else if (themeSet > SYSCLK){
        if(themeSet == CLKTHEME1)
          selectedTheme = String(themeDirectory[clockTheme1]) + "four.png";
        else if (themeSet == CLKTHEME2)
          selectedTheme = String(themeDirectory[clockTheme2]) + "four.png";
        selectedTheme.toCharArray(themeAddress,selectedTheme.length()+1);
        rc = png.open(themeAddress, myOpen, myClose, myRead, mySeek, PNGDraw);
      } 
    break;
    case 5:
      if(themeSet == SYSTEM)
        rc = png.open("/systemTheme/updateTime.png", myOpen, myClose, myRead, mySeek, PNGDraw);
      else if(themeSet == SYSCLK)
        rc = png.open("/systemTheme/five.png", myOpen, myClose, myRead, mySeek, PNGDraw);
      else if (themeSet > SYSCLK){
        if(themeSet == CLKTHEME1)
          selectedTheme = String(themeDirectory[clockTheme1]) + "five.png";
        else if (themeSet == CLKTHEME2)
          selectedTheme = String(themeDirectory[clockTheme2]) + "five.png";
        selectedTheme.toCharArray(themeAddress,selectedTheme.length()+1);
        rc = png.open(themeAddress, myOpen, myClose, myRead, mySeek, PNGDraw);
      } 
    break;
    case 6:
      if(themeSet == SYSTEM)
        rc = png.open("/systemTheme/timeOffset.png", myOpen, myClose, myRead, mySeek, PNGDraw);
      else if(themeSet == SYSCLK)
        rc = png.open("/systemTheme/six.png", myOpen, myClose, myRead, mySeek, PNGDraw);
      else if (themeSet > SYSCLK){
        if(themeSet == CLKTHEME1)
          selectedTheme = String(themeDirectory[clockTheme1]) + "six.png";
        else if (themeSet == CLKTHEME2)
          selectedTheme = String(themeDirectory[clockTheme2]) + "six.png";
        selectedTheme.toCharArray(themeAddress,selectedTheme.length()+1);
        rc = png.open(themeAddress, myOpen, myClose, myRead, mySeek, PNGDraw);
      } 
    break;
    case 7:
      if(themeSet == SYSTEM)
        rc = png.open("/systemTheme/setBrt.png", myOpen, myClose, myRead, mySeek, PNGDraw);
      else if(themeSet == SYSCLK)
        rc = png.open("/systemTheme/seven.png", myOpen, myClose, myRead, mySeek, PNGDraw);
      else if (themeSet > SYSCLK){
        if(themeSet == CLKTHEME1)
          selectedTheme = String(themeDirectory[clockTheme1]) + "seven.png";
        else if (themeSet == CLKTHEME2)
          selectedTheme = String(themeDirectory[clockTheme2]) + "seven.png";
        selectedTheme.toCharArray(themeAddress,selectedTheme.length()+1);
        rc = png.open(themeAddress, myOpen, myClose, myRead, mySeek, PNGDraw);
      } 
    break;
    case 8:
      if(themeSet == SYSTEM)
        rc = png.open("/systemTheme/loadTheme.png", myOpen, myClose, myRead, mySeek, PNGDraw);
      else if(themeSet == SYSCLK)
        rc = png.open("/systemTheme/eight.png", myOpen, myClose, myRead, mySeek, PNGDraw);
      else if (themeSet > SYSCLK){
        if(themeSet == CLKTHEME1)
          selectedTheme = String(themeDirectory[clockTheme1]) + "eight.png";
        else if (themeSet == CLKTHEME2)
          selectedTheme = String(themeDirectory[clockTheme2]) + "eight.png";
        selectedTheme.toCharArray(themeAddress,selectedTheme.length()+1);
        rc = png.open(themeAddress, myOpen, myClose, myRead, mySeek, PNGDraw);
      } 
    break;
    case 9:
      if(themeSet == SYSTEM)
        rc = png.open("/systemTheme/updateTimeU.png", myOpen, myClose, myRead, mySeek, PNGDraw);
      else if(themeSet == SYSCLK)
        rc = png.open("/systemTheme/nine.png", myOpen, myClose, myRead, mySeek, PNGDraw);
      else if (themeSet > SYSCLK){
        if(themeSet == CLKTHEME1)
          selectedTheme = String(themeDirectory[clockTheme1]) + "nine.png";
        else if (themeSet == CLKTHEME2)
          selectedTheme = String(themeDirectory[clockTheme2]) + "nine.png";
        selectedTheme.toCharArray(themeAddress,selectedTheme.length()+1);
        rc = png.open(themeAddress, myOpen, myClose, myRead, mySeek, PNGDraw);
      } 
    break;
    case 10:
      if(themeSet == SYSTEM)
        rc = png.open("/systemTheme/timeOffsetU.png", myOpen, myClose, myRead, mySeek, PNGDraw);
      else if(themeSet == SYSCLK)
        rc = png.open("/systemTheme/colon.png", myOpen, myClose, myRead, mySeek, PNGDraw);
      else if (themeSet > SYSCLK){
        if(themeSet == CLKTHEME1)
          selectedTheme = String(themeDirectory[clockTheme1]) + "colon.png";
        else if (themeSet == CLKTHEME2)
          selectedTheme = String(themeDirectory[clockTheme2]) + "colon.png";
        selectedTheme.toCharArray(themeAddress,selectedTheme.length()+1);
        rc = png.open(themeAddress, myOpen, myClose, myRead, mySeek, PNGDraw);
      } 
    break;
    case 11:
      if(themeSet == SYSTEM)
        rc = png.open("/systemTheme/setBrtU.png", myOpen, myClose, myRead, mySeek, PNGDraw);
      else if(themeSet == SYSCLK)
        rc = png.open("/systemTheme/dash.png", myOpen, myClose, myRead, mySeek, PNGDraw);
      else if (themeSet > SYSCLK){
        if(themeSet == CLKTHEME1)
          selectedTheme = String(themeDirectory[clockTheme1]) + "dash.png";
        else if (themeSet == CLKTHEME2)
          selectedTheme = String(themeDirectory[clockTheme2]) + "dash.png";
        selectedTheme.toCharArray(themeAddress,selectedTheme.length()+1);
        rc = png.open(themeAddress, myOpen, myClose, myRead, mySeek, PNGDraw);
      } 
    break;
    case 12:
      if(themeSet == SYSTEM)
        rc = png.open("/systemTheme/loadThemeU.png", myOpen, myClose, myRead, mySeek, PNGDraw);
      else if(themeSet == SYSCLK)
        rc = png.open("/systemTheme/space.png", myOpen, myClose, myRead, mySeek, PNGDraw);
      else if (themeSet > SYSCLK){
        if(themeSet == CLKTHEME1)
          selectedTheme = String(themeDirectory[clockTheme1]) + "space.png";
        else if (themeSet == CLKTHEME2)
          selectedTheme = String(themeDirectory[clockTheme2]) + "space.png";
        selectedTheme.toCharArray(themeAddress,selectedTheme.length()+1);
        rc = png.open(themeAddress, myOpen, myClose, myRead, mySeek, PNGDraw);
      } 
    break;
    case 13:
      if(themeSet == SYSTEM)
        rc = png.open("/systemTheme/percent.png", myOpen, myClose, myRead, mySeek, PNGDraw);
      else if (themeSet > SYSCLK){
        if(themeSet == CLKTHEME1)
          selectedTheme = String(themeDirectory[clockTheme1]) + "percent.png";
        else if (themeSet == CLKTHEME2)
          selectedTheme = String(themeDirectory[clockTheme2]) + "percent.png";
        selectedTheme.toCharArray(themeAddress,selectedTheme.length()+1);
        rc = png.open(themeAddress, myOpen, myClose, myRead, mySeek, PNGDraw);
      } 
    break;
    case 14:
      if(themeSet == SYSTEM)
        rc = png.open("/systemTheme/brtScroll.png", myOpen, myClose, myRead, mySeek, PNGDraw);
      else if (themeSet > SYSCLK){
        if(themeSet == CLKTHEME1)
          selectedTheme = String(themeDirectory[clockTheme1]) + "am.png";
        else if (themeSet == CLKTHEME2)
          selectedTheme = String(themeDirectory[clockTheme2]) + "am.png";
        selectedTheme.toCharArray(themeAddress,selectedTheme.length()+1);
        rc = png.open(themeAddress, myOpen, myClose, myRead, mySeek, PNGDraw);
      } 
    break;
    case 15:
      if(themeSet == SYSTEM)
        rc = png.open("/systemTheme/timeOffScroll.png", myOpen, myClose, myRead, mySeek, PNGDraw);
      else if (themeSet > SYSCLK){
        if(themeSet == CLKTHEME1)
          selectedTheme = String(themeDirectory[clockTheme1]) + "pm.png";
        else if (themeSet == CLKTHEME2)
          selectedTheme = String(themeDirectory[clockTheme2]) + "pm.png";
        selectedTheme.toCharArray(themeAddress,selectedTheme.length()+1);
        rc = png.open(themeAddress, myOpen, myClose, myRead, mySeek, PNGDraw);
      } 
    break;
    case 16:
      if(themeSet == SYSTEM)
        rc = png.open("/systemTheme/themeScroll.png", myOpen, myClose, myRead, mySeek, PNGDraw);
      else if (themeSet > SYSCLK){
        if(themeSet == CLKTHEME1)
          selectedTheme = String(themeDirectory[clockTheme1]) + "degrees.png";
        else if (themeSet == CLKTHEME2)
          selectedTheme = String(themeDirectory[clockTheme2]) + "degrees.png";
        selectedTheme.toCharArray(themeAddress,selectedTheme.length()+1);
        rc = png.open(themeAddress, myOpen, myClose, myRead, mySeek, PNGDraw);
      } 
    break;
    case 17:
      if(themeSet == SYSTEM)
        rc = png.open("/systemTheme/saveBack.png", myOpen, myClose, myRead, mySeek, PNGDraw);
      else if (themeSet > SYSCLK){
        if(themeSet == CLKTHEME1)
          selectedTheme = String(themeDirectory[clockTheme1]) + "alarm.png";
        else if (themeSet == CLKTHEME2)
          selectedTheme = String(themeDirectory[clockTheme2]) + "alarm.png";
        selectedTheme.toCharArray(themeAddress,selectedTheme.length()+1);
        rc = png.open(themeAddress, myOpen, myClose, myRead, mySeek, PNGDraw);
      } 
    break;
    case 18:
      if(themeSet == SYSTEM)
        rc = png.open("/systemTheme/mergeBack.png", myOpen, myClose, myRead, mySeek, PNGDraw);
      else if (themeSet > SYSCLK){
        if(themeSet == CLKTHEME1)
          selectedTheme = String(themeDirectory[clockTheme1]) + "brightness.png";
        else if (themeSet == CLKTHEME2)
          selectedTheme = String(themeDirectory[clockTheme2]) + "brightness.png";
        selectedTheme.toCharArray(themeAddress,selectedTheme.length()+1);
        rc = png.open(themeAddress, myOpen, myClose, myRead, mySeek, PNGDraw);
      } 
    break;
    case 19:
      if(themeSet == SYSTEM)
        rc = png.open("/systemTheme/setTheme1.png", myOpen, myClose, myRead, mySeek, PNGDraw); 
    break;
    case 20:
      if(themeSet == SYSTEM)
        rc = png.open("/systemTheme/setTheme2.png", myOpen, myClose, myRead, mySeek, PNGDraw); 
    break;
    case 21:
      if(themeSet == SYSTEM)
        rc = png.open("/systemTheme/timeUpdated.png", myOpen, myClose, myRead, mySeek, PNGDraw); 
    break;
    case 22:
      if(themeSet == SYSTEM)
        rc = png.open("/systemTheme/connecting.png", myOpen, myClose, myRead, mySeek, PNGDraw); 
    break;

    default:
      rc = png.open("/systemTheme/noImage.png", myOpen, myClose, myRead, mySeek, PNGDraw);
      break;
  }
  if (rc == PNG_SUCCESS) {
    rc = png.decode(NULL, 0);
    png.close();
  } else {
    rc = png.open("/systemTheme/noImage.png", myOpen, myClose, myRead, mySeek, PNGDraw);
    Serial.println(themeAddress);
    Serial.println("Check the SPIFFS system data!");
    if (rc == PNG_SUCCESS) {
      rc = png.decode(NULL, 0);
      png.close();
    }
  }
}

void setup() {
  Serial.begin(115200);

  preferences.begin("clockData",false);

  // Load EEPROM Values
  brightness = preferences.getUInt("brightness",0);
  timeOffsetHrs = preferences.getUInt("timeOffsetHrs",0);
  timeOffsetSign = preferences.getUInt("timeOffsetSign",0);
  clockTheme1 = preferences.getUInt("clockTheme1",0);
  clockTheme2 = preferences.getUInt("clockTheme2",0);

  if(timeOffsetSign)
    timeOffset = timeOffsetHrs;
  else
    timeOffset = (-1)*timeOffsetHrs;

  pinMode(ABUTTON, INPUT);
  pinMode(BBUTTON, INPUT);
  pinMode(CBUTTON, INPUT);
  pinMode(DBUTTON, INPUT);

  pinMode(CD_PIN,  INPUT);
  pinMode(TYPEDISP,INPUT);
  pinMode(CS_PIN,  OUTPUT);
  pinMode(AMP_PIN, OUTPUT);
  pinMode(RST_PIN, OUTPUT);

  if(!SPIFFS.begin(true)){
    Serial.println("Unable to open SPIFFS!");
    return;
  }Serial.println("SPIFFS mounted successfully!");
  
  
  ledcSetup(BLK_CHANNEL, BLK_BASE_FREQ, BLK_TIMER_13_BIT);
  ledcAttachPin(BLK_PIN, BLK_CHANNEL);
  setBacklight(BLK_CHANNEL, 0);
  digitalWrite(RST_PIN, LOW);
  delay(100);
  digitalWrite(RST_PIN, HIGH);
  delay(250);

  for (int i = 0; i < NUM_DISPLAYS; i++) {
    if (digitalRead(TYPEDISP)) {
      spilcdInit(&lcd[i], LCD_ST7789_135, FLAGS_INVERT , 12000000,i+1, DC_PIN, -1, -1, MISO_PIN, MOSI_PIN, SCK_PIN);
      spilcdSetOrientation(&lcd[i],  LCD_ORIENTATION_0);
      spilcdFill(&lcd[i], 0x0000, DRAW_TO_LCD);
    } else {
      spilcdInit(&lcd[i], LCD_ST7735S_B, FLAGS_INVERT | FLAGS_SWAP_RB, 40000000,i+1, DC_PIN, -1, -1, MISO_PIN, MOSI_PIN, SCK_PIN);
      spilcdSetOrientation(&lcd[i],  LCD_ORIENTATION_180);
      spilcdFill(&lcd[i], 0x0000, DRAW_TO_LCD);
    }
  }
  Serial.println("IPS Displays initialized!");
  setBacklight(BLK_CHANNEL, brightnessTable[brightness]);
  WiFi.begin(ssid, password);
  Serial.print( "Connecting...");
  while ( WiFi.status() != WL_CONNECTED & wifiConectionAttmps < 6) {
    drawDisplay(wifiConectionAttmps+1, CONNECTNG,  SYSTEM);
    delay(500); Serial.print( "." );
    wifiConectionAttmps++;
  }
  wifiConectionAttmps = 0;
 
  if( WiFi.status() != WL_CONNECTED ){
    Serial.println( "Error, not connected to Wifi." );
    drawDisplay(6, NOWIFI,  SYSTEM);  
    timeClient.setTimeOffset(3600 * timeOffset);  //for RTC at 00:00 use -23280
  }else{
    Serial.println( "Connected successfully!" );
    timeClient.setTimeOffset(3600 * timeOffset);
    drawDisplay(6, TIMEUPDATED,  SYSTEM);
  }

  timeClient.begin();
  timeClient.update();
  Serial.println("Updating RTC time.");
  WiFi.disconnect();  
  Serial.println( "Initializing Clock! " );
}


void loop() {

  if (digitalRead(ABUTTON) == LOW){
    if (currentScreen == CLOCKSCREEN){
      currentScreen = MENUSCREEN;
    }else if (currentScreen == MENUSCREEN){
      delay(50); while(digitalRead(ABUTTON) == 0); Serial.println( "Menu selection <- Left! " );
      if (menuSelection == 0)
        menuSelection = 4;  
      menuSelection--;
    }else if (currentScreen == TIMEOSCREEN){
      delay(50); while(digitalRead(ABUTTON) == 0); Serial.println( "UP TimeOffset! " );
      if(tmpTimeOffset < 12)
        tmpTimeOffset++;
      Serial.print( "TimeOffset: " );Serial.println(tmpTimeOffset);
      if (abs(tmpTimeOffset) == 12){
        drawDisplay(3, 1, SYSCLK);
        drawDisplay(4, 2, SYSCLK);
      } else if (abs(tmpTimeOffset) == 11){
        drawDisplay(3, 1, SYSCLK);
        drawDisplay(4, 1, SYSCLK);
      } else if (abs(tmpTimeOffset) == 10){
        drawDisplay(3, 1, SYSCLK);
        drawDisplay(4, 0, SYSCLK);
      }
      if ((tmpTimeOffset < 0) & (tmpTimeOffset > -10)) {
        drawDisplay(2, DASH, SYSCLK);
        drawDisplay(3, 0, SYSCLK);
        drawDisplay(4, abs(tmpTimeOffset), SYSCLK);
      } else if ((tmpTimeOffset >= 0) & (tmpTimeOffset < 10)) {
        drawDisplay(2, SPACE, SYSCLK);
        drawDisplay(3, 0, SYSCLK);
        drawDisplay(4, tmpTimeOffset, SYSCLK);
      } 
    } else if(currentScreen == BRTSCREEN){
      delay(50); while(digitalRead(ABUTTON) == 0); Serial.println( "UP Brightness! " );
        if(tmpBrightness < 9)
          tmpBrightness++;
        if (brightnessPcnt[tmpBrightness] == 100){
        drawDisplay(2, 1,     SYSCLK);
        drawDisplay(3, 0,     SYSCLK);
        drawDisplay(4, 0,     SYSCLK);
      } else if (brightnessPcnt[tmpBrightness] < 100){
        drawDisplay(2, SPACE, SYSCLK);
        drawDisplay(3, tmpBrightness + 1, SYSCLK);
        drawDisplay(4, 0,     SYSCLK);
      }
      setBacklight(BLK_CHANNEL, brightnessTable[tmpBrightness]);
      Serial.print("Brightness at ");Serial.print(brightnessPcnt[brightness]);Serial.println("%");
    } else if(currentScreen == THMSCREEN){
      Serial.print("Clock theme: ");Serial.print(clockTheme1);Serial.println(".");
      clockTheme1++;
      if(clockTheme1 > 13)
        clockTheme1 = 0;
      drawDisplay(2, random(0,9),  CLKTHEME1);
      drawDisplay(3, random(0,9),  CLKTHEME1);
      drawDisplay(4, random(0,9),  CLKTHEME1);
      drawDisplay(5, random(0,9),  CLKTHEME1);
      delay(200);
    } else if(currentScreen == SETTHMSCREEN){
        preferences.putUInt("clockTheme1",clockTheme1);
        prevHours = 60;
        prevMinutes = 60;
        menuSelection = 10;
        currentScreen = CLOCKSCREEN;
    }
  }
  
  if (digitalRead(CBUTTON) == LOW){
    if (currentScreen == CLOCKSCREEN){
      currentScreen = MENUSCREEN;
    }else if (currentScreen == MENUSCREEN){
      delay(50); while(digitalRead(CBUTTON) == 0); Serial.println( "Menu selection -> Right! " );
      menuSelection++;
      if (menuSelection > 3)
        menuSelection = 0;
    }  else if (currentScreen == TIMEOSCREEN){
      delay(50); while(digitalRead(CBUTTON) == 0); Serial.println( "Save TimeOffset! " );
      timeOffset = tmpTimeOffset;
      timeClient.setTimeOffset(3600 * timeOffset);
      drawDisplay(5, SYSOK, SYSTEM);
      preferences.putUInt("timeOffsetHrs",abs(timeOffset));
      if (timeOffset > 0 )
        preferences.putUInt("timeOffsetSign",1);
      else
        preferences.putUInt("timeOffsetSign",0);
      prevHours = 60;
      prevMinutes = 60;
      menuSelection = 10;
      currentScreen = CLOCKSCREEN;
      delay(1000);
    } else if(currentScreen == BRTSCREEN){
      brightness = tmpBrightness;
      setBacklight(BLK_CHANNEL, brightnessTable[brightness]);
      preferences.putUInt("brightness",brightness);
      prevHours = 60;
      prevMinutes = 60;
      menuSelection = 10;
      currentScreen = CLOCKSCREEN;
    } else if(currentScreen == THMSCREEN){
      // Select a theme mode
      drawDisplay(1, SETTHM1, SYSTEM);
      drawDisplay(6, SETTHM2, SYSTEM);
      currentScreen = SETTHMSCREEN;
      delay(250);
      Serial.println("Select where to save the theme.");
    } else if(currentScreen == SETTHMSCREEN){
      clockTheme2 = clockTheme1;
      clockTheme1 = tmpClockTheme;
      preferences.putUInt("clockTheme2",clockTheme2);
      prevHours = 60;
      prevMinutes = 60;
      menuSelection = 10;
      currentScreen = CLOCKSCREEN;
    }
  }

  if (digitalRead(BBUTTON) == LOW){
    if (currentScreen == CLOCKSCREEN){
      currentScreen = MENUSCREEN;
    }else if (currentScreen == MENUSCREEN){
      delay(50); while(digitalRead(BBUTTON) == 0); 
      if (menuSelection == 0){  Serial.println( "Updating time with Wifi!" );
      spilcdFill(&lcd[0], 0x0000, DRAW_TO_LCD);spilcdFill(&lcd[1], 0x0000, DRAW_TO_LCD);
      spilcdFill(&lcd[2], 0x0000, DRAW_TO_LCD);spilcdFill(&lcd[3], 0x0000, DRAW_TO_LCD);
      spilcdFill(&lcd[4], 0x0000, DRAW_TO_LCD);spilcdFill(&lcd[5], 0x0000, DRAW_TO_LCD);
      
          if( WiFi.status() != WL_CONNECTED ){
            WiFi.begin(ssid, password);
            Serial.print( "Connecting to " ); Serial.print(ssid);
            while ( WiFi.status() != WL_CONNECTED & wifiConectionAttmps < 6) {
            drawDisplay(wifiConectionAttmps+1,  CONNECTNG,  SYSTEM);
            delay(500); Serial.print( "." );
            wifiConectionAttmps++;
            } 
            wifiConectionAttmps = 0;
          }
          if( WiFi.status() != WL_CONNECTED ){
            Serial.println( "Error, not connected to Wifi!" );
            drawDisplay(6, NOWIFI,  SYSTEM);      
          } else {
            Serial.println( "Connected, updating time with Wifi!" );
            drawDisplay(6, TIMEUPDATED,  SYSTEM);
            timeClient.setTimeOffset(3600 * timeOffset);
            timeClient.update();
          }
          WiFi.disconnect();
          Serial.println( "Wifi is off, returning to clockScreen." );
          currentScreen = CLOCKSCREEN;
          prevHours = 60;
          prevMinutes = 60;
          menuSelection = 10;  
          delay(1500); 
      }
      if (menuSelection == 1){  Serial.println( "Set the time offset!" );
        menuSelection = 4;
      }
      if (menuSelection == 2){  Serial.println( "Set the displayArray brightness!" );
        menuSelection = 5;
        tmpBrightness = brightness;
      }
      if (menuSelection == 3){  Serial.println( "Load a new theme!" );
        menuSelection = 6;
      }
    } else if (currentScreen == TIMEOSCREEN){
      delay(50); while(digitalRead(BBUTTON) == 0); Serial.println( "Down TimeOffset! " );
      if(tmpTimeOffset > -12)
        tmpTimeOffset--;
      Serial.print( "TimeOffset: " );Serial.println(tmpTimeOffset);
      if (abs(tmpTimeOffset) == 12){
        drawDisplay(3, 1, SYSCLK);
        drawDisplay(4, 2, SYSCLK);
      } else if (abs(tmpTimeOffset) == 11){
        drawDisplay(3, 1, SYSCLK);
        drawDisplay(4, 1, SYSCLK);
      } else if (abs(tmpTimeOffset) == 10){
        drawDisplay(3, 1, SYSCLK);
        drawDisplay(4, 0, SYSCLK);
      }
      if ((tmpTimeOffset < 0) & (tmpTimeOffset > -10)) {
        drawDisplay(2, DASH, SYSCLK);
        drawDisplay(3, 0, SYSCLK);
        drawDisplay(4, abs(tmpTimeOffset), SYSCLK);
      } else if ((tmpTimeOffset >= 0) & (tmpTimeOffset < 10)) {
        drawDisplay(2, SPACE, SYSCLK);
        drawDisplay(3, 0, SYSCLK);
        drawDisplay(4, tmpTimeOffset, SYSCLK);
      }
    }else if(currentScreen == BRTSCREEN){
      delay(50); while(digitalRead(BBUTTON) == 0); Serial.println( "Down Brightness! " );
      if(tmpBrightness < 2)
        tmpBrightness = 0;
      else
        tmpBrightness--;
      if (brightnessPcnt[tmpBrightness] == 100){
        drawDisplay(2, 1,     SYSCLK);
        drawDisplay(3, 0,     SYSCLK);
        drawDisplay(4, 0,     SYSCLK);
      } else if (brightnessPcnt[tmpBrightness] < 100){
        drawDisplay(2, SPACE, SYSCLK);
        drawDisplay(3, tmpBrightness + 1, SYSCLK);
        drawDisplay(4, 0,     SYSCLK);
      }
      setBacklight(BLK_CHANNEL, brightnessTable[tmpBrightness]);
      Serial.print("Brightness at ");Serial.print(brightnessPcnt[brightness]);Serial.println("%");
    } else if(currentScreen == THMSCREEN){
      Serial.print("Clock theme: ");Serial.print(clockTheme1);Serial.println(".");
      if(clockTheme1 == 0)
        clockTheme1 = 13;
      else  
        clockTheme1--;
      drawDisplay(2, random(0,9),  CLKTHEME1);
      drawDisplay(3, random(0,9),  CLKTHEME1);
      drawDisplay(4, random(0,9),  CLKTHEME1);
      drawDisplay(5, random(0,9),  CLKTHEME1);
      delay(200);
    } else if(currentScreen == SETTHMSCREEN){
        preferences.putUInt("clockTheme1",clockTheme1);
        prevHours = 60;
        prevMinutes = 60;
        menuSelection = 10;
        currentScreen = CLOCKSCREEN;
    }
  }

  if (digitalRead(DBUTTON) == LOW){
    delay(50); while(digitalRead(DBUTTON) == 0); Serial.println( "Menu/Clock button! " );
    if (currentScreen == CLOCKSCREEN){
      currentScreen = MENUSCREEN;
    } else if (currentScreen == MENUSCREEN){
      currentScreen = CLOCKSCREEN;
      prevHours = 60;
      prevMinutes = 60;
      menuSelection = 10;
    } else if (currentScreen == TIMEOSCREEN){
      currentScreen = MENUSCREEN;
      menuSelection = 1;
    } else if (currentScreen == BRTSCREEN){
      setBacklight(BLK_CHANNEL, brightnessTable[brightness]);
      currentScreen = MENUSCREEN;
      menuSelection = 2;
    } else if(currentScreen == THMSCREEN){
      clockTheme1 = tmpClockTheme;
      currentScreen = MENUSCREEN;
      menuSelection = 3;
    } else if(currentScreen == SETTHMSCREEN){
      clockTheme2 = clockTheme1;
      clockTheme1 = tmpClockTheme;
      preferences.putUInt("clockTheme2",clockTheme2);
      prevHours = 60;
      prevMinutes = 60;
      menuSelection = 10;
      currentScreen = CLOCKSCREEN;
    }
    Serial.print( "CurrentScreen; " );Serial.println( currentScreen);
  }
  
 // Display de configuration Menu 
if((currentScreen == MENUSCREEN) & (menuSelection != prevMenuSelection)){
  if(menuSelection > 6)
    menuSelection = 0;
  prevMenuSelection = menuSelection;
  Serial.println( "Clock configuration menu! " );
  drawDisplay(1, LEFTENTER, SYSTEM);          // print clock menu configurations
  drawDisplay(6, RIGHTBACK, SYSTEM);
  switch(menuSelection){
    case 0:
      drawDisplay(2, UPDTTIME,  SYSTEM);
      drawDisplay(3, TIMEOFFSU, SYSTEM);
      drawDisplay(4, SETBRTU,   SYSTEM);
      drawDisplay(5, LOADTHMU,  SYSTEM);
      break;
    case 1:
      drawDisplay(2, UPDTTIMEU,  SYSTEM);
      drawDisplay(3, TIMEOFFS, SYSTEM);
      drawDisplay(4, SETBRTU,   SYSTEM);
      drawDisplay(5, LOADTHMU,  SYSTEM);
      break;
    case 2:
      drawDisplay(2, UPDTTIMEU,  SYSTEM);
      drawDisplay(3, TIMEOFFSU, SYSTEM);
      drawDisplay(4, SETBRT,   SYSTEM);
      drawDisplay(5, LOADTHMU,  SYSTEM);
      break;
    case 3:
      drawDisplay(2, UPDTTIMEU,  SYSTEM);
      drawDisplay(3, TIMEOFFSU, SYSTEM);
      drawDisplay(4, SETBRTU,   SYSTEM);
      drawDisplay(5, LOADTHM,  SYSTEM);
      break;  
    case 4:
      tmpTimeOffset = timeOffset;
      drawDisplay(1, TIMEOSCROLL, SYSTEM);
      drawDisplay(2, SPACE,       SYSCLK);
      drawDisplay(5, SPACE,       SYSCLK);
      drawDisplay(6, SAVEBACK,    SYSTEM);
      if (tmpTimeOffset < 0)
        drawDisplay(2, DASH, SYSCLK);
      if (abs(tmpTimeOffset) == 12){
        drawDisplay(3, 1, SYSCLK);
        drawDisplay(4, 2, SYSCLK);
      } else if (abs(tmpTimeOffset) == 11){
        drawDisplay(3, 1, SYSCLK);
        drawDisplay(4, 1, SYSCLK);
      } else if (abs(tmpTimeOffset) == 10){
        drawDisplay(3, 1, SYSCLK);
        drawDisplay(4, 0, SYSCLK);
      }
      if ((tmpTimeOffset < 0) & (tmpTimeOffset > -10)) {
        drawDisplay(2, DASH, SYSCLK);
        drawDisplay(3, 0, SYSCLK);
        drawDisplay(4, abs(tmpTimeOffset), SYSCLK);
      } else if ((tmpTimeOffset >= 0) & (tmpTimeOffset < 10)) {
        drawDisplay(2, SPACE, SYSCLK);
        drawDisplay(3, 0, SYSCLK);
        drawDisplay(4, tmpTimeOffset, SYSCLK);
      }
      currentScreen = TIMEOSCREEN;
      break;
    case 5:
      Serial.print("Brightness at ");Serial.print(brightnessPcnt[brightness]);Serial.println("%");
      drawDisplay(1, BRTSCROLL, SYSTEM);
      drawDisplay(5, PERCENT,   SYSTEM);
      drawDisplay(6, SAVEBACK,  SYSTEM);
      if (brightnessPcnt[tmpBrightness] == 100){
        drawDisplay(2, 1,     SYSCLK);
        drawDisplay(3, 0,     SYSCLK);
        drawDisplay(4, 0,     SYSCLK);
      } else if (brightnessPcnt[tmpBrightness] < 100){
        drawDisplay(2, SPACE, SYSCLK);
        drawDisplay(3, tmpBrightness + 1,  SYSCLK);
        drawDisplay(4, 0,     SYSCLK);
      }
      currentScreen = BRTSCREEN;
      break;
    case 6 :
      Serial.println("Select a theme: ");
      tmpClockTheme = clockTheme1;
      drawDisplay(1, THMSCROLL, SYSTEM);
      drawDisplay(6, MERGEBACK,  SYSTEM);
      drawDisplay(2, random(0,9),  CLKTHEME1);
      drawDisplay(3, random(0,9),  CLKTHEME1);
      drawDisplay(4, random(0,9),  CLKTHEME1);
      drawDisplay(5, random(0,9),  CLKTHEME1);
      currentScreen = THMSCREEN;
      break;
  }
}

// Display the clock!
if(currentScreen == CLOCKSCREEN){
  if (timeClient.getHours() != prevHours) {
    prevHours = timeClient.getHours();

    if (timeClient.getHours() < 10) {
      drawDisplay(1, 0, CLKTHEME1);
      drawDisplay(2, timeClient.getHours(), CLKTHEME1);
    } else if (timeClient.getHours() < 13) {
      drawDisplay(1, 1, CLKTHEME1);
      drawDisplay(2, timeClient.getHours() - 10, CLKTHEME1);
    } else if (timeClient.getHours() < 22){
      drawDisplay(1, 0, CLKTHEME1);
      drawDisplay(2, timeClient.getHours() - 12, CLKTHEME1);
    } else {
      drawDisplay(1, 1, CLKTHEME1);
      drawDisplay(2, timeClient.getHours() - 22, CLKTHEME1);
    }
  }
  
  if (timeClient.getMinutes() != prevMinutes) {
    prevMinutes = timeClient.getMinutes();

    if (timeClient.getMinutes() < 10) {
      drawDisplay(4, 0, CLKTHEME2);
      drawDisplay(5, timeClient.getMinutes(), CLKTHEME2);
    } else if (timeClient.getMinutes() < 20) {
      drawDisplay(4, 1, CLKTHEME2);
      drawDisplay(5, timeClient.getMinutes() - 10, CLKTHEME2);
    } else if (timeClient.getMinutes() < 30) {
      drawDisplay(4, 2, CLKTHEME2);
      drawDisplay(5, timeClient.getMinutes() - 20, CLKTHEME2);
    } else if (timeClient.getMinutes() < 40) {
      drawDisplay(4, 3, CLKTHEME2);
      drawDisplay(5, timeClient.getMinutes() - 30, CLKTHEME2);
    } else if (timeClient.getMinutes() < 50) {
      drawDisplay(4, 4, CLKTHEME2);
      drawDisplay(5, timeClient.getMinutes() - 40, CLKTHEME2);
    } else {
      drawDisplay(4, 5, CLKTHEME2);
      drawDisplay(5, timeClient.getMinutes() - 50, CLKTHEME2);
    }

    if(timeClient.getHours() < 12){             // Show if the time is AM or PM
      drawDisplay(6, AM, CLKTHEME2);
    }else{
      drawDisplay(6, PM, CLKTHEME2);
    }
  }

  if (millis() > requestDueTime) {              // Blink the seconds indicator colon symbol
    if (secondStatus == 0) {
      drawDisplay(3, COLON, CLKTHEME1);
      secondStatus = 1;
    } else {
      if(clockTheme1 == clockTheme2)
        drawDisplay(3, SPACE, CLKTHEME1);
      else
        drawDisplay(3, COLON, CLKTHEME2);
      secondStatus = 0;
    }
    requestDueTime = millis() + 500;
  }
}
}
