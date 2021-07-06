# Display Array Clock Firmware

### Visual Studio Code - Raspberry Pi Pico

For this clock project, I’m using a Raspberry Pi Pico, but the code should be easily ported to any other microcontroller, as the functionality of the clock itself is basic. The Raspberry Pi Pico has a built-in RTC so results easier to implement the clock. Other Microcontrollers with Wifi Capabilities could also obtain the clock data from a web service instead of an intrnal RTC making the board capabilities more atractive as weather or many other kind of IOT notificaions.

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

## Posible error in the Pico SDK
There is a chance that your pico-SDK has an error and the program would not start. In your SDK directory: pico-sdk/src/common/pico_time/time.c file comment line 17, //CU_SELECT_DEBUG_PINS(core) and now the program should run with any problem.

```c++
/*
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include "pico.h"
#include "pico/time.h"
#include "pico/util/pheap.h"
#include "hardware/sync.h"
#include "hardware/gpio.h"

CU_REGISTER_DEBUG_PINS(core)
//CU_SELECT_DEBUG_PINS(core)

const absolute_time_t ABSOLUTE_TIME_INITIALIZED_VAR(nil_time, 0);
// use LONG_MAX not ULONG_MAX so we don't have sign overflow in time diffs
const absolute_time_t ABSOLUTE_TIME_INITIALIZED_VAR(at_the_end_of_time, ULONG_MAX);

typedef struct alarm_pool_entry {
    absolute_time_t target;
    alarm_callback_t callback;
    void *user_data;
} alarm_pool_entry_t;
```
### Visual Studio Code - Esspresif Plugin ESP32-S2

The latest firmware includes the addition of 4 timezones with different clock themes with the corresponding country Flag, the firmware is based on the SNTP example provided by Expressif which allows you to configure and set your Wifi’s SSID and password for automatic clock time according to the chosen timezones.

I am very new with the ESP32 so I have tried with MicroPython, CircuitPython, Arduino, and Espressif repository running in Visual Studio Code, and the easiest for me was this last one, so I took the SNTP example located at the protocols example folder. In Visual Studio Code is the Espressif plug-in which makes it very easy and fast to get started with the ESP32.

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
