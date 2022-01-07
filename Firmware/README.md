## ESP32-S2 Pinout

![displayArraySaola](https://savageelectronics.com/wp-content/uploads/2021/07/ESP32_SCH.png)

### SPI and Buttons < main.h >
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
## Raspberry Pi Pico Pinout

![displayArrayPico](https://savageelectronics.com/wp-content/uploads/2021/05/g16799-1024x625.png)

### SPI Port < ST7735.h>

```c++
#define PIN_BLK     21
#define PIN_RST     20
#define PIN_DC      19
#define PIN_SDI     18
#define PIN_SCK     17
#define PIN_CS      16
#define PIN_CS1      16
#define PIN_CS2      15
#define PIN_CS3      14
#define PIN_CS4      13
#define PIN_CS5      12
#define PIN_CS6      11
```

### Buttons < main.h >

```c++
#define PIN_BTNA      6
#define PIN_BTNB      7
#define PIN_BTNC      8
#define PIN_BTND      9
```
