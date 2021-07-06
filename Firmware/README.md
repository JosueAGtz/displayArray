# Display Array Clock Firmware

The latest firmware includes the addition of 4 timezones with different clock themes with the corresponding country Flag, the firmware is based on the SNTP example provided by Expressif which allows you to configure and set your Wifi’s SSID and password for automatic clock time according to the chosen timezones.

I am very new with the ESP32 so I have tried with MicroPython, CircuitPython, Arduino, and Espressif repository running in Visual Studio Code, and the easiest for me was this last one, so I took the SNTP example located at the protocols example folder. In Visual Studio Code is the Espressif plug-in which makes it very easy and fast to get started with the ESP32.

### Visual Studio Code - Esspresif Plugin
![displayArrayESP32](https://savageelectronics.com/wp-content/uploads/2021/05/EspressifPlugIn-1024x579.png)

After installing and opening the example we just need to configure the example for the Wifi credentials.

![displayArrayWifi](https://savageelectronics.com/wp-content/uploads/2021/05/WifiConf-1024x588.png)

And now is as easy as adding the SPI configurations for the displays.

```c++

#define PIN_NUM_RST     33
#define PIN_NUM_DC      34
#define PIN_NUM_MOSI    35
#define PIN_NUM_CLK     36
#define PIN_NUM_BLK     37
#define PIN_NUM_MISO    -1

#define PIN_NUM_CS1     1
#define PIN_NUM_CS2     2
#define PIN_NUM_CS3     3
#define PIN_NUM_CS4     4
#define PIN_NUM_CS5     5
#define PIN_NUM_CS6     6
#define PIN_NUM_CS7     7
#define PIN_NUM_CS8     8

#define PIN_NUM_BTN     0
#define PIN_NUM_BTA     10
#define PIN_NUM_BTB     11
#define PIN_NUM_BTC     12 
#define PIN_NUM_BTD     13

```
