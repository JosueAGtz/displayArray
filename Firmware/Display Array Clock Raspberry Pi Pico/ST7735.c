#include "ST7735.h"


void lcdSetDC(bool dc) {
    sleep_us(1);
    gpio_put_masked((1u << PIN_DC) , !!dc << PIN_DC  );
    sleep_us(1);
}

void lcdWriteCMD(PIO pio, uint sm, const uint8_t *cmd, size_t count) {
    lcdWaitIdle(pio, sm);
    lcdSetDC(0);
    lcdPut(pio, sm, *cmd++);
    if (count >= 2) {
        lcdWaitIdle(pio, sm);
        lcdSetDC(1);
        for (size_t i = 0; i < count - 1; ++i)
            lcdPut(pio, sm, *cmd++);
    }
    lcdWaitIdle(pio, sm);
    lcdSetDC(1);
}

void lcdInit(PIO pio, uint sm, const uint8_t *init_seq) {
    const uint8_t *cmd = init_seq;
    while (*cmd) {
        lcdWriteCMD(pio, sm, cmd + 2, *cmd);
        sleep_ms(*(cmd + 1) * 5);
        cmd += *cmd + 2;
    }
}

void lcdStartPx(PIO pio, uint sm) {
    uint8_t cmd = ST7735_RAMWR;
    lcdWriteCMD(pio, sm, &cmd, 1);
    lcdSetDC(1);
}

void selectDisplay(uint8_t display){
    switch(display){
        case 0:
            gpio_put(PIN_CS1, 0);
            gpio_put(PIN_CS2, 0);
            gpio_put(PIN_CS3, 0);
            gpio_put(PIN_CS4, 0);
            gpio_put(PIN_CS5, 0);
            gpio_put(PIN_CS6, 0);
        break;
        case 1:
            gpio_put(PIN_CS1, 0);
            gpio_put(PIN_CS2, 1);
            gpio_put(PIN_CS3, 1);
            gpio_put(PIN_CS4, 1);
            gpio_put(PIN_CS5, 1);
            gpio_put(PIN_CS6, 1);
        break;
        case 2:
            gpio_put(PIN_CS1, 1);
            gpio_put(PIN_CS2, 0);
            gpio_put(PIN_CS3, 1);
            gpio_put(PIN_CS4, 1);
            gpio_put(PIN_CS5, 1);
            gpio_put(PIN_CS6, 1);
        break;
        case 3:
            gpio_put(PIN_CS1, 1);
            gpio_put(PIN_CS2, 1);
            gpio_put(PIN_CS3, 0);
            gpio_put(PIN_CS4, 1);
            gpio_put(PIN_CS5, 1);
            gpio_put(PIN_CS6, 1);
        break;
        case 4:
            gpio_put(PIN_CS1, 1);
            gpio_put(PIN_CS2, 1);
            gpio_put(PIN_CS3, 1);
            gpio_put(PIN_CS4, 0);
            gpio_put(PIN_CS5, 1);
            gpio_put(PIN_CS6, 1);
        break;
        case 5:
            gpio_put(PIN_CS1, 1);
            gpio_put(PIN_CS2, 1);
            gpio_put(PIN_CS3, 1);
            gpio_put(PIN_CS4, 1);
            gpio_put(PIN_CS5, 0);
            gpio_put(PIN_CS6, 1);
        break;
        case 6:
            gpio_put(PIN_CS1, 1);
            gpio_put(PIN_CS2, 1);
            gpio_put(PIN_CS3, 1);
            gpio_put(PIN_CS4, 1);
            gpio_put(PIN_CS5, 1);
            gpio_put(PIN_CS6, 0);
        break;
        default:
            gpio_put(PIN_CS1, 1);
            gpio_put(PIN_CS2, 1);
            gpio_put(PIN_CS3, 1);
            gpio_put(PIN_CS4, 1);
            gpio_put(PIN_CS5, 1);
            gpio_put(PIN_CS6, 1);
        break;
    }
}

void lcdDrawNumber(PIO pio, uint sm, uint8_t Display, uint8_t Number){

    selectDisplay(Display);
    switch(Number){
        case 0:
            for (int i = 0; i < 160*80*2; i++){
                lcdPut(pio, sm, zero_Theme[i]);}
        break;
        case 1:
            for (int i = 0; i < 160*80*2; i++){
                lcdPut(pio, sm, one_Theme[i]);}
        break;
        case 2:
            for (int i = 0; i < 160*80*2; i++){
                lcdPut(pio, sm, two_Theme[i]);}
        break;
        case 3:
            for (int i = 0; i < 160*80*2; i++){
                lcdPut(pio, sm, three_Theme[i]);}
        break;
        case 4:
            for (int i = 0; i < 160*80*2; i++){
                lcdPut(pio, sm, four_Theme[i]);}
        break;
        case 5:
            for (int i = 0; i < 160*80*2; i++){
                lcdPut(pio, sm, five_Theme[i]);}
        break;
        case 6:
            for (int i = 0; i < 160*80*2; i++){
                lcdPut(pio, sm, six_Theme[i]);}
        break;
        case 7:
            for (int i = 0; i < 160*80*2; i++){
                lcdPut(pio, sm, seven_Theme[i]);}
        break;
        case 8:
            for (int i = 0; i < 160*80*2; i++){
                lcdPut(pio, sm, eight_Theme[i]);}
        break;
        case 9:
            for (int i = 0; i < 160*80*2; i++){
                lcdPut(pio, sm, nine_Theme[i]);}
        break;
        case 10:
            for (int i = 0; i < 160*80*2; i++){
                lcdPut(pio, sm, colon_Theme[i]);}
        break;
        case 11:
            for (int i = 0; i < 160*80*2; i++){
                lcdPut(pio, sm, slash_Theme[i]);}
        break;
        case 12:
            for (int i = 0; i < 160*80*2; i++){
                lcdPut(pio, sm, space_Theme[i]);}
        break;
        case 13:
            for (int i = 0; i < 160*80*2; i++){
                lcdPut(pio, sm, am_Theme[i]);}
        break;
        case 14:
            for (int i = 0; i < 160*80*2; i++){
                lcdPut(pio, sm, pm_Theme[i]);}
        break;
        case 15:
            for (int i = 0; i < 160*80*2; i++){
                lcdPut(pio, sm, heart_Theme[i]);}
        break;
    }

}