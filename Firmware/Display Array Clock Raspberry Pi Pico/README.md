# Display Array Clock Firmware

## Visual Studio Code - Raspberry Pi Pico

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
