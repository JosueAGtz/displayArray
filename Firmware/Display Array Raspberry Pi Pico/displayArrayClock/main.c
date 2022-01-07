#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "hardware/rtc.h"
#include "pico/util/datetime.h"
#include "ST7735.h"

#define ONBOARD_LED 25

#define HRS  0
#define MINS 1

#define OFF  0
#define ON   1

#define PIN_BTNA      6
#define PIN_BTNB      7
#define PIN_BTNC      8
#define PIN_BTND      9

bool Backlight = OFF;
bool ConfigureTime_MODE = OFF;
uint Selection = HRS;


    // Start on Friday 26th of March 2021 12:34:00
    datetime_t t = {
            .year  = 2021,
            .month = 03,
            .day   = 26,
            .dotw  = 5, // 0 is Sunday, so 5 is Friday
            .hour  = 12,
            .min   = 34,
            .sec   = 00
    };

void gpio_callback(uint gpio, uint32_t events) {

   if(gpio == PIN_BTND){
      if(ConfigureTime_MODE == ON){
         ConfigureTime_MODE = OFF;
         t.sec = 0;
         rtc_set_datetime(&t);
      }else if(ConfigureTime_MODE == OFF){
         ConfigureTime_MODE = ON; // Configuration Mode Enabled
      }
    }

    if(ConfigureTime_MODE){

      if(gpio == PIN_BTNA){
         if(Selection  == HRS){
            if(t.hour < 23){
               t.hour += 1;}}
         else if(Selection == MINS){
            if(t.min < 59){
               t.min += 1;}}
      }

      if(gpio == PIN_BTNB){
         if(Selection  == HRS){
            if(t.hour > 0 ){
               t.hour -= 1;}}
         else if(Selection == MINS){
            if(t.min > 0){
               t.min -= 1;}}
      }

      if(gpio == PIN_BTNC){
         if(Selection  == HRS){
            Selection = MINS;}
         else if(Selection == MINS){
            Selection = HRS;}
      }

    }else if(ConfigureTime_MODE == OFF){

       if(gpio == PIN_BTNC){
         if(Backlight  == ON){
            gpio_put(PIN_BLK, OFF);
            Backlight = OFF;
         }else if(Backlight == OFF){
            gpio_put(PIN_BLK, ON);
            Backlight = ON;
            }
      }

    }
}



int main(){
	stdio_init_all();

    // Start the RTC
    rtc_init();
    rtc_set_datetime(&t);

    PIO pio = pio0;
    uint sm = 0;
    uint offset = pio_add_program(pio, &SPILCD_program);
    lcdPIOInit(pio, sm, offset, PIN_SDI, PIN_SCK, SERIAL_CLK_DIV);

    gpio_set_irq_enabled_with_callback(PIN_BTNA, GPIO_IRQ_EDGE_FALL, true, &gpio_callback);
    gpio_set_irq_enabled_with_callback(PIN_BTNB, GPIO_IRQ_EDGE_FALL, true, &gpio_callback);
    gpio_set_irq_enabled_with_callback(PIN_BTNC, GPIO_IRQ_EDGE_FALL, true, &gpio_callback);
    gpio_set_irq_enabled_with_callback(PIN_BTND, GPIO_IRQ_EDGE_FALL, true, &gpio_callback);

    gpio_init(ONBOARD_LED);
    gpio_set_dir(ONBOARD_LED, GPIO_OUT);

    gpio_init(PIN_CS1);
    gpio_init(PIN_CS2);
    gpio_init(PIN_CS3);
    gpio_init(PIN_CS4);
    gpio_init(PIN_CS5);
    gpio_init(PIN_CS6);
    gpio_init(PIN_DC);
    gpio_init(PIN_RST);
    gpio_init(PIN_BLK);

    gpio_set_dir(PIN_CS1, GPIO_OUT);
    gpio_set_dir(PIN_CS2, GPIO_OUT);
    gpio_set_dir(PIN_CS3, GPIO_OUT);
    gpio_set_dir(PIN_CS4, GPIO_OUT);
    gpio_set_dir(PIN_CS5, GPIO_OUT);
    gpio_set_dir(PIN_CS6, GPIO_OUT);
    gpio_set_dir(PIN_DC, GPIO_OUT);
    gpio_set_dir(PIN_RST, GPIO_OUT);
    gpio_set_dir(PIN_BLK, GPIO_OUT);

    gpio_put(PIN_CS1, 0);
    gpio_put(PIN_CS2, 0);
    gpio_put(PIN_CS3, 0);
    gpio_put(PIN_CS4, 0);
    gpio_put(PIN_CS5, 0);
    gpio_put(PIN_CS6, 0);
    gpio_put(PIN_RST, 1);
    lcdInit(pio, sm, st7735_initSeq);
    gpio_put(PIN_BLK, ON);
    Backlight = ON;
    gpio_put(ONBOARD_LED, 1);

    lcdStartPx(pio,sm);

    while(1){

       if(ConfigureTime_MODE == OFF){
         rtc_get_datetime(&t);}

       if(t.hour >= 1 & t.hour <= 9){
          lcdDrawNumber(pio,sm,display1,0);
          lcdDrawNumber(pio,sm,display2,t.hour);
       }else if(t.hour >= 10 & t.hour <= 12){
          lcdDrawNumber(pio,sm,display1,1);
          lcdDrawNumber(pio,sm,display2,t.hour-10);
       }else if(t.hour >= 13 & t.hour <= 21){
          lcdDrawNumber(pio,sm,display1,0);
          lcdDrawNumber(pio,sm,display2,t.hour-12);
       }else if(t.hour >= 22 ){
          lcdDrawNumber(pio,sm,display1,1);
          lcdDrawNumber(pio,sm,display2,t.hour-22);
       }else if (t.hour == 0){
          lcdDrawNumber(pio,sm,display1,1);
          lcdDrawNumber(pio,sm,display2,2);
       }

       if(t.min >= 0 & t.min <= 9){
          lcdDrawNumber(pio,sm,display4,0);
          lcdDrawNumber(pio,sm,display5,t.min);
       }else if(t.min >= 10 & t.min <= 19){
          lcdDrawNumber(pio,sm,display4,1);
          lcdDrawNumber(pio,sm,display5,t.min-10);
       }else if(t.min >= 20 & t.min <= 29){
          lcdDrawNumber(pio,sm,display4,2);
          lcdDrawNumber(pio,sm,display5,t.min-20);
       }else if(t.min >= 30 & t.min <= 39 ){
          lcdDrawNumber(pio,sm,display4,3);
          lcdDrawNumber(pio,sm,display5,t.min-30);
       }else if(t.min >= 40 & t.min <= 49 ){
          lcdDrawNumber(pio,sm,display4,4);
          lcdDrawNumber(pio,sm,display5,t.min-40);
       }else if(t.min >= 50 & t.min <= 59 ){
          lcdDrawNumber(pio,sm,display4,5);
          lcdDrawNumber(pio,sm,display5,t.min-50);
       }


       if(t.hour < 12 )
          lcdDrawNumber(pio,sm,display6,AM);
      else
          lcdDrawNumber(pio,sm,display6,PM);


      if(ConfigureTime_MODE == OFF){
         lcdDrawNumber(pio,sm,display3,COLON);
         sleep_ms(500);
         lcdDrawNumber(pio,sm,display3,SPACE);
         sleep_ms(500);
      }else{
         if(Selection  == HRS){
            sleep_ms(200);
            lcdDrawNumber(pio,sm,display1,SPACE);
            lcdDrawNumber(pio,sm,display2,SPACE);
            lcdDrawNumber(pio,sm,display3,COLON);
            sleep_ms(200);
         }else if(Selection == MINS){
            sleep_ms(200);
            lcdDrawNumber(pio,sm,display3,COLON);
            lcdDrawNumber(pio,sm,display4,SPACE);
            lcdDrawNumber(pio,sm,display5,SPACE);
            sleep_ms(200);
         }
         
         
      }
       
    }
}

