/* LwIP SNTP example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_attr.h"
#include "esp_sleep.h"
#include "nvs_flash.h"
#include "protocol_examples_common.h"
#include "esp_sntp.h"

#include "splashscreenPlus.h"
#include "clockBINA.h"
#include "clockFlip.h"
#include "clockNixie.h"
#include "clockVFD.h"
#include "clockFlags.h"

#define zero_Theme      zero_BINA
#define one_Theme       one_BINA
#define two_Theme       two_BINA
#define three_Theme     three_BINA
#define four_Theme      four_BINA
#define five_Theme      five_BINA
#define six_Theme       six_BINA
#define seven_Theme     seven_BINA
#define eight_Theme     eight_BINA
#define nine_Theme      nine_BINA
#define colon_Theme     colon_BINA
#define space_Theme     space_BINA

#define zero_Theme1      zero_Nixie
#define one_Theme1       one_Nixie
#define two_Theme1       two_Nixie
#define three_Theme1     three_Nixie
#define four_Theme1      four_Nixie
#define five_Theme1      five_Nixie
#define six_Theme1       six_Nixie
#define seven_Theme1     seven_Nixie
#define eight_Theme1     eight_Nixie
#define nine_Theme1      nine_Nixie
#define colon_Theme1     colon_Nixie
#define space_Theme1     space_Nixie

#define zero_Theme2      zero_VFD
#define one_Theme2       one_VFD
#define two_Theme2       two_VFD
#define three_Theme2     three_VFD
#define four_Theme2      four_VFD
#define five_Theme2      five_VFD
#define six_Theme2       six_VFD
#define seven_Theme2     seven_VFD
#define eight_Theme2     eight_VFD
#define nine_Theme2      nine_VFD
#define colon_Theme2     colon_VFD
#define space_Theme2     space_VFD

#define zero_Theme3      zero_Flip
#define one_Theme3       one_Flip
#define two_Theme3       two_Flip
#define three_Theme3     three_Flip
#define four_Theme3      four_Flip
#define five_Theme3      five_Flip
#define six_Theme3       six_Flip
#define seven_Theme3     seven_Flip
#define eight_Theme3     eight_Flip
#define nine_Theme3      nine_Flip
#define colon_Theme3     colon_Flip
#define space_Theme3     space_Flip

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

//  The LCD needs a bunch of command/argument values to be initialized. They are stored in this struct.
typedef struct {
    uint8_t cmd;
    uint8_t data[16];
    uint8_t databytes; //  No of data in data; bit 7 = delay after set; 0xFF = end of cmds.
} lcd_init_cmd_t;

//  Place data into DRAM. Constant data gets placed into DROM by default, which is not accessible by DMA.
DRAM_ATTR static const lcd_init_cmd_t st_init_cmds[]={
    {0x11, {0}, 0x80},      // Sleep Out 
    {0x36, {0xC8}, 1},      // Memory Data Access Control, MX=MV=1, MY=ML=MH=0, RGB=0
    {0x3A, {0x05}, 1},      // Interface Pixel Format, 16bits/pixel for RGB/MCU interface
    {0xB1, {0x01, 0x2C, 0x2D}, 3}, // Porch Setting 
    {0xB2, {0x01, 0x2C, 0x2D}, 3},
    {0xB3, {0x01, 0x2C, 0x2D, 0x01, 0x2C, 0x2D}, 6},
    {0xB4, {0x07}, 1},
    {0xC0, {0xA2,0x02,0x84}, 3}, // LCM Control, XOR: BGR, MX, MH 
    {0xC1, {0xC5}, 1},
    {0xC2, {0x0A, 0x00}, 2}, // VDV and VRH Command Enable, enable=1 
    {0xC3, {0x8A,0xEE}, 2},  // VRH Set, Vap=4.4+... 
    {0xC5, {0x0E}, 1},       // VDV Set, VDV=0 
    {0x2A, {0x00, 0x1A, 0x00, 0x69}, 4}, // Column Address 
    {0x2B, {0x00, 0x01, 0x00, 0xA0}, 4}, // Row Address 
    {0xE0, {0x02, 0x1C, 0x07, 0x12, 0x37, 0x32, 0x29, 0x2D, 0x29, 0x25, 0x2B, 0x39, 0x00, 0x01, 0x03, 0x10}, 16}, // Positive Voltage Gamma Control 
    {0xE1, {0x03, 0x1D, 0x07, 0x06, 0x2E, 0x2C, 0x29, 0x2D, 0x2E, 0x2E, 0x37, 0x3F, 0x00, 0x00, 0x02, 0x10}, 16}, // Negative Voltage Gamma Control 
    {0x21, {0}, 0x80},      // Inversion ON 
    {0x13, {0}, 0x80},      // Normal Display 
    {0x29, {0}, 0x80},      // Display On 
    {0x2C, {0}, 0x80},      // Write Data 
    {0, {0}, 0xff}
};

/*
DRAM_ATTR static const lcd_init_cmd_t st89_init_cmds[]={
    {0x01, {0}, 0x80},  // Software Reset 
    {0x11, {0}, 0x80},  // Sleep Out 
    {0x36, {0x00}, 1},  // Memory Data Access Control, MX=MV=1, MY=ML=MH=0, RGB=0 
    {0x3A, {0x55}, 1},  // Interface Pixel Format, 16bits/pixel for RGB/MCU interface
    {0x2A, {0x00, 0x34, 0x00, 0xBA}, 4},  // Column Address 
    {0x2B, {0x00, 0x28, 0x01, 0x17}, 4},  // Row Address 
    {0x21, {0}, 0x80},  // Inversion ON 
    {0x13, {0}, 0x80},  // Normal Display 
    {0x29, {0}, 0x80},  // Display On 
    {0x2C, {0}, 0x80},  // Write Data 
    {0, {0}, 0xff}
};  */

void lcd_cmd(spi_device_handle_t spi, const uint8_t cmd){
    esp_err_t ret;
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));       // Zero out the transaction
    t.length=8;                     // Command is 8 bits
    t.tx_buffer=&cmd;               // The data is the cmd itself
    t.user=(void*)0;                // D/C needs to be set to 0
    ret=spi_device_polling_transmit(spi, &t);  // Transmit!
    assert(ret==ESP_OK);            // Should have had no issues.
}

void lcd_data(spi_device_handle_t spi, const uint8_t *data, int len){
    esp_err_t ret;
    spi_transaction_t t;
    if (len==0) return;             // no need to send anything
    memset(&t, 0, sizeof(t));       // Zero out the transaction
    t.length=len*8;                 // Len is in bytes, transaction length is in bits.
    t.tx_buffer=data;               // Data
    t.user=(void*)1;                // D/C needs to be set to 1
    ret=spi_device_polling_transmit(spi, &t);  // Transmit!
    assert(ret==ESP_OK);            // Should have had no issues.
}

// This function is called (in irq context!) just before a transmission starts. It will
// set the D/C line to the value indicated in the user field.
void lcd_spi_pre_transfer_callback(spi_transaction_t *t){
    int dc=(int)t->user;
    gpio_set_level(PIN_NUM_DC, dc);
}

// Initialize the display
void lcd_init(spi_device_handle_t spi){
    int cmd=0;
    const lcd_init_cmd_t* lcd_init_cmds;
    lcd_init_cmds = st_init_cmds;

    // Initialize non-SPI GPIOs
    gpio_set_direction(PIN_NUM_DC, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_NUM_RST, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_NUM_BLK, GPIO_MODE_OUTPUT);

    gpio_set_direction(PIN_NUM_CS1, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_NUM_CS2, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_NUM_CS3, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_NUM_CS4, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_NUM_CS5, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_NUM_CS6, GPIO_MODE_OUTPUT);

    gpio_set_direction(PIN_NUM_BTA, GPIO_MODE_INPUT);
    gpio_set_direction(PIN_NUM_BTB, GPIO_MODE_INPUT);
    gpio_set_direction(PIN_NUM_BTC, GPIO_MODE_INPUT);
    gpio_set_direction(PIN_NUM_BTD, GPIO_MODE_INPUT);
    gpio_set_direction(PIN_NUM_BTN, GPIO_MODE_INPUT);

    gpio_set_level(PIN_NUM_CS1, 0);
    gpio_set_level(PIN_NUM_CS2, 0);
    gpio_set_level(PIN_NUM_CS3, 0);
    gpio_set_level(PIN_NUM_CS4, 0);
    gpio_set_level(PIN_NUM_CS5, 0);
    gpio_set_level(PIN_NUM_CS6, 0);
    gpio_set_level(PIN_NUM_BLK, 0);

    // Reset the display
    gpio_set_level(PIN_NUM_RST, 0);
    vTaskDelay(100 / portTICK_RATE_MS);
    gpio_set_level(PIN_NUM_RST, 1);
    vTaskDelay(100 / portTICK_RATE_MS);

    // Send all the commands
    while (lcd_init_cmds[cmd].databytes!=0xff) {
        lcd_cmd(spi, lcd_init_cmds[cmd].cmd);
        lcd_data(spi, lcd_init_cmds[cmd].data, lcd_init_cmds[cmd].databytes&0x1F);
        if (lcd_init_cmds[cmd].databytes&0x80) {
            vTaskDelay(100 / portTICK_RATE_MS);}
        cmd++;
    }
}

uint8_t clockTheme = 0;
uint8_t prevHours,prevMinutes,prevSeconds;
uint8_t imgBuffer[100];

uint8_t backlightStatus = 1;
uint8_t secondsCounter = 0;
uint8_t secondsStatus = 0;

void selectDisplay(uint8_t);
void lcdDrawNumber(spi_device_handle_t,uint8_t,uint8_t);
void lcdDrawFlag(spi_device_handle_t,uint8_t,uint8_t);
void lcdDrawSplashscreen(spi_device_handle_t,uint8_t,uint8_t);

static void obtain_time(void);
static void initialize_sntp(void);

void app_main(void){
    esp_err_t ret;
    spi_device_handle_t spi;
    spi_bus_config_t buscfg={
        .miso_io_num=PIN_NUM_MISO,
        .mosi_io_num=PIN_NUM_MOSI,
        .sclk_io_num=PIN_NUM_CLK,
        .quadwp_io_num=-1,
        .quadhd_io_num=-1,
        .max_transfer_sz=100
    };
    spi_device_interface_config_t devcfg={
        .clock_speed_hz=40*1000*1000,           // Clock out at 40 MHz
        .mode=0,                                // SPI mode 0 for ST7735 and mode 3 for ST7789
        .queue_size=7,                          // We want to be able to queue 7 transactions at a time
        .pre_cb=lcd_spi_pre_transfer_callback,  // Specify pre-transfer callback to handle D/C line
    };
   
    ret=spi_bus_initialize(SPI2_HOST, &buscfg, SPI2_HOST);  // Initialize the SPI bus
    ESP_ERROR_CHECK(ret);
    ret=spi_bus_add_device(SPI2_HOST, &devcfg, &spi);  // Attach the LCD to the SPI bus
    ESP_ERROR_CHECK(ret);
    
    lcd_init(spi);  // Initialize the LCD
    lcdDrawSplashscreen(spi,1,3);
    lcdDrawSplashscreen(spi,2,4);
    lcdDrawSplashscreen(spi,3,5);
    lcdDrawSplashscreen(spi,4,6);
    lcdDrawSplashscreen(spi,5,7);
    lcdDrawSplashscreen(spi,6,8);
    
    gpio_set_level(PIN_NUM_BLK, 1);  // Enable backlight
   
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    if (timeinfo.tm_year < (2016 - 1900)) { // Is time set? If not, tm_year will be (1970 - 1900).
        obtain_time();
        time(&now); // update 'now' variable with current time
    }

    // Set timezone to Mexico City Central Standard Time 
    setenv("TZ", "CST6CDT,M4.1.0/2,M10.5.0", 1); // EST5EDT,M3.2.0/2,M11.1.0
    tzset();
    localtime_r(&now, &timeinfo);

    prevHours = 60;
    prevMinutes = 60;
    prevSeconds = 60;

    lcdDrawNumber(spi,0,10); 
    lcdDrawNumber(spi,3,10); 
    lcdDrawFlag(spi,6,0);

    while(1){
        
        if(timeinfo.tm_hour != prevHours){
            prevHours = timeinfo.tm_hour;
    
            if(timeinfo.tm_hour < 10){
                lcdDrawNumber(spi,1,0);
                lcdDrawNumber(spi,2,timeinfo.tm_hour);
            }else if (timeinfo.tm_hour < 20){
                lcdDrawNumber(spi,1,1);
                lcdDrawNumber(spi,2,timeinfo.tm_hour-10);
            }else{
                lcdDrawNumber(spi,1,2);
                lcdDrawNumber(spi,2,timeinfo.tm_hour-20);
            }
        }     

        if(timeinfo.tm_min != prevMinutes){
            prevMinutes = timeinfo.tm_min;

            if(timeinfo.tm_min < 10){
                lcdDrawNumber(spi,4,0);
                lcdDrawNumber(spi,5,timeinfo.tm_min);
            }else if (timeinfo.tm_min < 20){
                lcdDrawNumber(spi,4,1);
                lcdDrawNumber(spi,5,timeinfo.tm_min-10);
            }else if (timeinfo.tm_min < 30){
                lcdDrawNumber(spi,4,2);
                lcdDrawNumber(spi,5,timeinfo.tm_min-20);
            }else if (timeinfo.tm_min < 40){
                lcdDrawNumber(spi,4,3);
                lcdDrawNumber(spi,5,timeinfo.tm_min-30);
            }else if (timeinfo.tm_min < 50){
                lcdDrawNumber(spi,4,4);
                lcdDrawNumber(spi,5,timeinfo.tm_min-40);
            }else{
                lcdDrawNumber(spi,4,5);
                lcdDrawNumber(spi,5,timeinfo.tm_min-50);
            }
        } 

        vTaskDelay(20 / portTICK_PERIOD_MS);
        secondsCounter++;
        time(&now);                     // update 'now' variable with current time
        localtime_r(&now, &timeinfo);

        if(gpio_get_level(PIN_NUM_BTA) == 0){
            clockTheme = 0;
            setenv("TZ", "CST6CDT,M4.1.0/2,M10.5.0", 1); // Mexico City
            tzset();
            localtime_r(&now, &timeinfo);
            prevHours = 60;
            prevMinutes = 60;
            prevSeconds = 60;
            lcdDrawNumber(spi,1,0);
            lcdDrawNumber(spi,2,0);
            lcdDrawNumber(spi,4,0);
            lcdDrawNumber(spi,5,0);
            lcdDrawFlag(spi,6,0);
            lcdDrawNumber(spi,3,10);
            vTaskDelay(120 / portTICK_PERIOD_MS);
        }

        if(gpio_get_level(PIN_NUM_BTB) == 0){
            clockTheme = 1;
            setenv("TZ", "CET1CEST,M3.5.0/2,M10.5.0", 1); // Germany Berlin
            tzset();
            localtime_r(&now, &timeinfo);
            prevHours = 60;
            prevMinutes = 60;
            prevSeconds = 60;
            lcdDrawNumber(spi,1,0);
            lcdDrawNumber(spi,2,0);
            lcdDrawNumber(spi,4,0);
            lcdDrawNumber(spi,5,0);
            lcdDrawFlag(spi,6,1);
            lcdDrawNumber(spi,3,10);
            vTaskDelay(120 / portTICK_PERIOD_MS);
        }

        if(gpio_get_level(PIN_NUM_BTC) == 0){
            clockTheme = 2;
            setenv("TZ", "EST5EDT,M3.2.0/2,M11.1.0", 1); // USA New York
            tzset();
            localtime_r(&now, &timeinfo);
            prevHours = 60;
            prevMinutes = 60;
            prevSeconds = 60;
            lcdDrawNumber(spi,1,0);
            lcdDrawNumber(spi,2,0);
            lcdDrawNumber(spi,4,0);
            lcdDrawNumber(spi,5,0);
            lcdDrawFlag(spi,6,2);
            lcdDrawNumber(spi,3,10);
            vTaskDelay(120 / portTICK_PERIOD_MS);
        }

        if (gpio_get_level(PIN_NUM_BTD) == 0){
            clockTheme = 3;
            setenv("TZ", "GMT0BST,M3.5.0/2,M10.5.0", 1); // UK London 
            tzset();
            localtime_r(&now, &timeinfo);
            prevHours = 60;
            prevMinutes = 60;
            prevSeconds = 60;
            lcdDrawNumber(spi,1,0);
            lcdDrawNumber(spi,2,0);
            lcdDrawNumber(spi,4,0);
            lcdDrawNumber(spi,5,0);
            lcdDrawFlag(spi,6,3);
            lcdDrawNumber(spi,3,10);
            vTaskDelay(120 / portTICK_PERIOD_MS);
        }

        if((secondsCounter > 25) & (secondsStatus == 0)){
            lcdDrawNumber(spi,3,10); 
            secondsStatus = 1;
            secondsCounter = 0;
        } else if((secondsCounter > 25) & (secondsStatus == 1)){
            lcdDrawNumber(spi,3,11); 
            secondsStatus = 0;
            secondsCounter = 0;
        }

        if((gpio_get_level(PIN_NUM_BTN) == 0) & (backlightStatus == 1)){
            backlightStatus = 0;
            gpio_set_level(PIN_NUM_BLK, 0);
            vTaskDelay(120 / portTICK_PERIOD_MS);
        } else if((gpio_get_level(PIN_NUM_BTN) == 0) & (backlightStatus == 0)){
            backlightStatus = 1;
            gpio_set_level(PIN_NUM_BLK, 1);
            vTaskDelay(120 / portTICK_PERIOD_MS);
        }
    }
}

static void obtain_time(void){

    ESP_ERROR_CHECK( nvs_flash_init() );
    ESP_ERROR_CHECK( esp_netif_init() );
    ESP_ERROR_CHECK( esp_event_loop_create_default() );

    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.     */
    ESP_ERROR_CHECK(example_connect());

    initialize_sntp();
    
    time_t now = 0;                     // wait for time to be set
    struct tm timeinfo = { 0 };
    int retry = 0;
    const int retry_count = 10;
    while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < retry_count) {
        vTaskDelay(2000 / portTICK_PERIOD_MS);}
    time(&now);
    localtime_r(&now, &timeinfo);
    ESP_ERROR_CHECK( example_disconnect() );
}

static void initialize_sntp(void){
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_init();
}

void selectDisplay(uint8_t display){
    switch(display){
        case 0:
            gpio_set_level(PIN_NUM_CS1, 0);
            gpio_set_level(PIN_NUM_CS2, 0);
            gpio_set_level(PIN_NUM_CS3, 0);
            gpio_set_level(PIN_NUM_CS4, 0);
            gpio_set_level(PIN_NUM_CS5, 0);
            gpio_set_level(PIN_NUM_CS6, 0);
        break;
        case 1:
            gpio_set_level(PIN_NUM_CS1, 0);
            gpio_set_level(PIN_NUM_CS2, 1);
            gpio_set_level(PIN_NUM_CS3, 1);
            gpio_set_level(PIN_NUM_CS4, 1);
            gpio_set_level(PIN_NUM_CS5, 1);
            gpio_set_level(PIN_NUM_CS6, 1);
        break;
        case 2:
            gpio_set_level(PIN_NUM_CS1, 1);
            gpio_set_level(PIN_NUM_CS2, 0);
            gpio_set_level(PIN_NUM_CS3, 1);
            gpio_set_level(PIN_NUM_CS4, 1);
            gpio_set_level(PIN_NUM_CS5, 1);
            gpio_set_level(PIN_NUM_CS6, 1);
        break;
        case 3:
            gpio_set_level(PIN_NUM_CS1, 1);
            gpio_set_level(PIN_NUM_CS2, 1);
            gpio_set_level(PIN_NUM_CS3, 0);
            gpio_set_level(PIN_NUM_CS4, 1);
            gpio_set_level(PIN_NUM_CS5, 1);
            gpio_set_level(PIN_NUM_CS6, 1);
        break;
        case 4:
            gpio_set_level(PIN_NUM_CS1, 1);
            gpio_set_level(PIN_NUM_CS2, 1);
            gpio_set_level(PIN_NUM_CS3, 1);
            gpio_set_level(PIN_NUM_CS4, 0);
            gpio_set_level(PIN_NUM_CS5, 1);
            gpio_set_level(PIN_NUM_CS6, 1);
        break;
        case 5:
            gpio_set_level(PIN_NUM_CS1, 1);
            gpio_set_level(PIN_NUM_CS2, 1);
            gpio_set_level(PIN_NUM_CS3, 1);
            gpio_set_level(PIN_NUM_CS4, 1);
            gpio_set_level(PIN_NUM_CS5, 0);
            gpio_set_level(PIN_NUM_CS6, 1);
        break;
        case 6:
            gpio_set_level(PIN_NUM_CS1, 1);
            gpio_set_level(PIN_NUM_CS2, 1);
            gpio_set_level(PIN_NUM_CS3, 1);
            gpio_set_level(PIN_NUM_CS4, 1);
            gpio_set_level(PIN_NUM_CS5, 1);
            gpio_set_level(PIN_NUM_CS6, 0);
        break;
        case 7:
            gpio_set_level(PIN_NUM_CS1, 1);
            gpio_set_level(PIN_NUM_CS2, 1);
            gpio_set_level(PIN_NUM_CS3, 1);
            gpio_set_level(PIN_NUM_CS4, 1);
            gpio_set_level(PIN_NUM_CS5, 1);
            gpio_set_level(PIN_NUM_CS6, 1);
           // gpio_set_level(PIN_NUM_CS7, 0);
           // gpio_set_level(PIN_NUM_CS8, 1);
        break;
        case 8:
            gpio_set_level(PIN_NUM_CS1, 1);
            gpio_set_level(PIN_NUM_CS2, 1);
            gpio_set_level(PIN_NUM_CS3, 1);
            gpio_set_level(PIN_NUM_CS4, 1);
            gpio_set_level(PIN_NUM_CS5, 1);
            gpio_set_level(PIN_NUM_CS6, 1);
           // gpio_set_level(PIN_NUM_CS7, 1);
           // gpio_set_level(PIN_NUM_CS8, 0);
        break;
        case 9:
            gpio_set_level(PIN_NUM_CS1, 0);
            gpio_set_level(PIN_NUM_CS2, 0);
            gpio_set_level(PIN_NUM_CS3, 0);
            gpio_set_level(PIN_NUM_CS4, 0);
            gpio_set_level(PIN_NUM_CS5, 0);
            gpio_set_level(PIN_NUM_CS6, 0);
        break;
        default:
            gpio_set_level(PIN_NUM_CS1, 1);
            gpio_set_level(PIN_NUM_CS2, 1);
            gpio_set_level(PIN_NUM_CS3, 1);
            gpio_set_level(PIN_NUM_CS4, 1);
            gpio_set_level(PIN_NUM_CS5, 1);
            gpio_set_level(PIN_NUM_CS6, 1);
        break;
    }
}

void lcdDrawNumber(spi_device_handle_t spi, uint8_t Display, uint8_t Number){
    selectDisplay(Display);
    switch(Number){
        case 0:
            for(int k = 0; k < 256; k++){
                for(int j = 0; j < 100; j++){
                    if(clockTheme == 0)
                        imgBuffer[j] = zero_Theme[j+(100*k)];
                    else if (clockTheme == 1)
                        imgBuffer[j] = zero_Theme1[j+(100*k)];
                    else if (clockTheme == 2)
                        imgBuffer[j] = zero_Theme2[j+(100*k)];
                    else if (clockTheme == 3)
                        imgBuffer[j] = zero_Theme3[j+(100*k)];
                }
                lcd_data(spi, imgBuffer,100);
            }
        break;
        case 1:
            for(int k = 0; k < 256; k++){
                for(int j = 0; j < 100; j++){
                    if(clockTheme == 0)
                        imgBuffer[j] = one_Theme[j+(100*k)];
                    else if (clockTheme == 1)
                        imgBuffer[j] = one_Theme1[j+(100*k)];
                    else if (clockTheme == 2)
                        imgBuffer[j] = one_Theme2[j+(100*k)];
                    else if (clockTheme == 3)
                        imgBuffer[j] = one_Theme3[j+(100*k)];
                }
                lcd_data(spi, imgBuffer,100);
            }
        break;
        case 2:
            for(int k = 0; k < 256; k++){
                for(int j = 0; j < 100; j++){
                    if(clockTheme == 0)
                        imgBuffer[j] = two_Theme[j+(100*k)];
                    else if (clockTheme == 1)
                        imgBuffer[j] = two_Theme1[j+(100*k)];
                    else if (clockTheme == 2)
                        imgBuffer[j] = two_Theme2[j+(100*k)];
                    else if (clockTheme == 3)
                        imgBuffer[j] = two_Theme3[j+(100*k)];
                }
                lcd_data(spi, imgBuffer,100);
            }
        break;
        case 3:
            for(int k = 0; k < 256; k++){
                for(int j = 0; j < 100; j++){
                    if(clockTheme == 0)
                        imgBuffer[j] = three_Theme[j+(100*k)];
                    else if (clockTheme == 1)
                        imgBuffer[j] = three_Theme1[j+(100*k)];
                    else if (clockTheme == 2)
                        imgBuffer[j] = three_Theme2[j+(100*k)];
                    else if (clockTheme == 3)
                        imgBuffer[j] = three_Theme3[j+(100*k)];
                }
                lcd_data(spi, imgBuffer,100);
            }
        break;
        case 4:
            for(int k = 0; k < 256; k++){
                for(int j = 0; j < 100; j++){
                    if(clockTheme == 0)
                        imgBuffer[j] = four_Theme[j+(100*k)];
                    else if (clockTheme == 1)
                        imgBuffer[j] = four_Theme1[j+(100*k)];
                    else if (clockTheme == 2)
                        imgBuffer[j] = four_Theme2[j+(100*k)];
                    else if (clockTheme == 3)
                        imgBuffer[j] = four_Theme3[j+(100*k)];
                }
                lcd_data(spi, imgBuffer,100);
            }
        break;
        case 5:
            for(int k = 0; k < 256; k++){
                for(int j = 0; j < 100; j++){
                    if(clockTheme == 0)
                        imgBuffer[j] = five_Theme[j+(100*k)];
                    else if (clockTheme == 1)
                        imgBuffer[j] = five_Theme1[j+(100*k)];
                    else if (clockTheme == 2)
                        imgBuffer[j] = five_Theme2[j+(100*k)];
                    else if (clockTheme == 3)
                        imgBuffer[j] = five_Theme3[j+(100*k)];
                }
                lcd_data(spi, imgBuffer,100);
            }
        break;
        case 6:
            for(int k = 0; k < 256; k++){
                for(int j = 0; j < 100; j++){
                    if(clockTheme == 0)
                        imgBuffer[j] = six_Theme[j+(100*k)];
                    else if (clockTheme == 1)
                        imgBuffer[j] = six_Theme1[j+(100*k)];
                    else if (clockTheme == 2)
                        imgBuffer[j] = six_Theme2[j+(100*k)];
                    else if (clockTheme == 3)
                        imgBuffer[j] = six_Theme3[j+(100*k)];
                }
                lcd_data(spi, imgBuffer,100);
            }
        break;
        case 7:
            for(int k = 0; k < 256; k++){
                for(int j = 0; j < 100; j++){
                    if(clockTheme == 0)
                        imgBuffer[j] = seven_Theme[j+(100*k)];
                    else if (clockTheme == 1)
                        imgBuffer[j] = seven_Theme1[j+(100*k)];
                    else if (clockTheme == 2)
                        imgBuffer[j] = seven_Theme2[j+(100*k)];
                    else if (clockTheme == 3)
                        imgBuffer[j] = seven_Theme3[j+(100*k)];
                }
                lcd_data(spi, imgBuffer,100);
            }
        break;
        case 8:
            for(int k = 0; k < 256; k++){
                for(int j = 0; j < 100; j++){
                    if(clockTheme == 0)
                        imgBuffer[j] = eight_Theme[j+(100*k)];
                    else if (clockTheme == 1)
                        imgBuffer[j] = eight_Theme1[j+(100*k)];
                    else if (clockTheme == 2)
                        imgBuffer[j] = eight_Theme2[j+(100*k)];
                    else if (clockTheme == 3)
                        imgBuffer[j] = eight_Theme3[j+(100*k)];
                }
                lcd_data(spi, imgBuffer,100);
            }
        break;
        case 9:
            for(int k = 0; k < 256; k++){
                for(int j = 0; j < 100; j++){
                    if(clockTheme == 0)
                        imgBuffer[j] = nine_Theme[j+(100*k)];
                    else if (clockTheme == 1)
                        imgBuffer[j] = nine_Theme1[j+(100*k)];
                    else if (clockTheme == 2)
                        imgBuffer[j] = nine_Theme2[j+(100*k)];
                    else if (clockTheme == 3)
                        imgBuffer[j] = nine_Theme3[j+(100*k)];
                }
                lcd_data(spi, imgBuffer,100);
            }
        break;
        case 10:
            for(int k = 0; k < 256; k++){
                for(int j = 0; j < 100; j++){
                    if(clockTheme == 0)
                        imgBuffer[j] = colon_Theme[j+(100*k)];
                    else if (clockTheme == 1)
                        imgBuffer[j] = colon_Theme1[j+(100*k)];
                    else if (clockTheme == 2)
                        imgBuffer[j] = colon_Theme2[j+(100*k)];
                    else if (clockTheme == 3)
                        imgBuffer[j] = colon_Theme3[j+(100*k)];
                }
                lcd_data(spi, imgBuffer,100);
            }
        break;
        case 11:
            for(int k = 0; k < 256; k++){
                for(int j = 0; j < 100; j++){
                    if(clockTheme == 0)
                        imgBuffer[j] = space_Theme[j+(100*k)];
                    else if (clockTheme == 1)
                        imgBuffer[j] = space_Theme1[j+(100*k)];
                    else if (clockTheme == 2)
                        imgBuffer[j] = space_Theme2[j+(100*k)];
                    else if (clockTheme == 3)
                        imgBuffer[j] = space_Theme3[j+(100*k)];
                }
                lcd_data(spi, imgBuffer,100);
            }
        break;
    }
}

void lcdDrawFlag(spi_device_handle_t spi, uint8_t Display, uint8_t Flag){
    selectDisplay(Display);
    switch (Flag){
        case 0:
            for(int k = 0; k < 256; k++){
                for(int j = 0; j < 100; j++){
                        imgBuffer[j] = mex_Flag[j+(100*k)];  
                }
                lcd_data(spi, imgBuffer,100);
            }
        break;
        case 1:
            for(int k = 0; k < 256; k++){
                for(int j = 0; j < 100; j++){
                        imgBuffer[j] = deu_Flag[j+(100*k)];
                }
                lcd_data(spi, imgBuffer,100);
            }
        break;
        case 2:
            for(int k = 0; k < 256; k++){
                for(int j = 0; j < 100; j++){
                        imgBuffer[j] = usa_Flag[j+(100*k)];
                }
                lcd_data(spi, imgBuffer,100);
            }
        break;
        case 3:
            for(int k = 0; k < 256; k++){
                for(int j = 0; j < 100; j++){
                        imgBuffer[j] = gbr_Flag[j+(100*k)];
                }
                lcd_data(spi, imgBuffer,100);
            }
        break;
    }
}

void lcdDrawSplashscreen(spi_device_handle_t spi, uint8_t Display, uint8_t Splash){
    selectDisplay(Display);
    switch (Splash){
        case 1:
            for(int k = 0; k < 256; k++){
                for(int j = 0; j < 100; j++){
                        imgBuffer[j] = SP1[j+(100*k)];  
                }
                lcd_data(spi, imgBuffer,100);
            }
        break;
        case 2:
            for(int k = 0; k < 256; k++){
                for(int j = 0; j < 100; j++){
                        imgBuffer[j] = SP2[j+(100*k)];
                }
                lcd_data(spi, imgBuffer,100);
            }
        break;
        case 3:
            for(int k = 0; k < 256; k++){
                for(int j = 0; j < 100; j++){
                        imgBuffer[j] = SP3[j+(100*k)];
                }
                lcd_data(spi, imgBuffer,100);
            }
        break;
        case 4:
            for(int k = 0; k < 256; k++){
                for(int j = 0; j < 100; j++){
                        imgBuffer[j] = SP4[j+(100*k)];
                }
                lcd_data(spi, imgBuffer,100);
            }
        break;
        case 5:
            for(int k = 0; k < 256; k++){
                for(int j = 0; j < 100; j++){
                        imgBuffer[j] = SP5[j+(100*k)];
                }
                lcd_data(spi, imgBuffer,100);
            }
        break;
        case 6:
            for(int k = 0; k < 256; k++){
                for(int j = 0; j < 100; j++){
                        imgBuffer[j] = SP6[j+(100*k)];
                }
                lcd_data(spi, imgBuffer,100);
            }
        break;
        case 7:
            for(int k = 0; k < 256; k++){
                for(int j = 0; j < 100; j++){
                        imgBuffer[j] = SP7[j+(100*k)];
                }
                lcd_data(spi, imgBuffer,100);
            }
        break;
        case 8:
            for(int k = 0; k < 256; k++){
                for(int j = 0; j < 100; j++){
                        imgBuffer[j] = SP8[j+(100*k)];
                }
                lcd_data(spi, imgBuffer,100);
            }
        break;
    }
}