#include <xc.h>
#include <stdint.h>
#include <stdio.h>
#include "colourClick.h"
#include "i2c.h"
#include "timer.h"
#include "serial.h"
#include "buttons.h"
#include "flags.h"

#define _XTAL_FREQ 64000000 // for __delay_ms

// RGB LED pins
#ifdef __ONBOARD
    #define R_PIN LATAbits.LATA0
    #define G_PIN LATDbits.LATD0
    #define B_PIN LATCbits.LATC2
#else
    #define R_PIN LATGbits.LATG1
    #define G_PIN LATAbits.LATA4
    #define B_PIN LATFbits.LATF7
#endif

#define INT_PIN PORTBbits.RB1

// i2c parameters
#define ADDR 0x52 // colour click i2c address
#define WRITE 0x00 // write to colour click
#define READ 0x01 // read from colour click

// colour click registers
#define R_ENABLE 0x00
#define R_ATIME 0x01
#define R_CONTROL 0x0f
#define AILTL 0x04 // clear channel low threshold low byte
#define AIHTL 0x06 // clear channel high threshold low byte
#define APERS 0x0C // interrupt persistance
#define CDATA 0x14 // clear data low byte
#define RDATA 0x16 // red data low byte
#define GDATA 0x18 // green data low byte
#define BDATA 0x1A // blue data low byte

#define READ_DELAY 200

#ifndef __DEBUG_MODE
typedef struct {
    uint16_t h;
    uint8_t s;
    uint8_t l;
} HSLColour;
#endif

struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} led = {0};

// pink 352, 46
#define NUM_CARDS CLEAR - 2 // a bit cryptic but CLEAR is the number of cards in the Card enum, -2 for excluding black and white
const uint16_t CARD_H[NUM_CARDS] = {349, 147, 213, 13, 351, 352, 186}; // hues of card colours, except black and white
const uint16_t CARD_S[NUM_CARDS] = { 56,  21,  31, 38,  27,  46,  11}; // saturations of card colours, except black and white

#ifdef __CARD_LED // RGB colours for each coloured card, for debugging colour read
const uint8_t CARD_R[NUM_CARDS] = {255,   0,   0, 255, 255, 255,   0};
const uint8_t CARD_G[NUM_CARDS] = {  0, 255,   0, 255,   0, 128, 200};
const uint8_t CARD_B[NUM_CARDS] = {  0,   0, 255,   0, 128,   0, 255};
#endif

uint16_t clear_threshold = 300; // (LED off) above this is CLEAR, below this is wall
const uint8_t range_threshold = 8; // (LED on) if range of RGB values is less than threshold, only consider white/black - no calibration needed
uint16_t white_threshold = 30000; // (LED on) if C channel is larger than threshold, then white, otherwise black

float rgb_scaler[] = {1, 1.327, 1.897};

// -------------------- START LED FUNCTIONS --------------------

void initLED(void) {
    // initialise RGB LED pins
    #ifdef __ONBOARD
        TRISAbits.TRISA0 = 0; // red output
        TRISDbits.TRISD0 = 0; // green output
        TRISCbits.TRISC2 = 0; // blue output
    #else
        TRISGbits.TRISG1 = 0; // red output
        TRISAbits.TRISA4 = 0; // green output
        TRISFbits.TRISF7 = 0; // blue output
    #endif
    TRISBbits.TRISB1 = 1; // input
    ANSELBbits.ANSELB1 = 0; // digital input
    #ifdef __CARD_LED
        TMR2_init(); // initial TMR2 for RGB LED PWM
    #endif
}

void colourClick_onLED(void) {
    #ifdef __CARD_LED
        colourClick_setLED(255, 255, 255);
    #else
        R_PIN = 1;
        B_PIN = 1;
        G_PIN = 1;
    #endif
}

void colourClick_offLED(void) {
    #ifdef __CARD_LED
        colourClick_setLED(0, 0, 0);
    #else
        R_PIN = 0;
        B_PIN = 0;
        G_PIN = 0;
    #endif
}

#ifdef __CARD_LED
void colourClick_setLED(uint8_t r, uint8_t g, uint8_t b) {
    led.r = r;
    led.g = g;
    led.b = b;
}
#endif

#ifdef __CARD_LED
void colourClick_interruptLED(void) {
    static uint8_t counter = 0;
    R_PIN = counter < led.r ? 1 : 0;
    G_PIN = counter < led.g ? 1 : 0;
    B_PIN = counter < led.b ? 1 : 0;
    ++counter;
}
#endif

// -------------------- END LED FUNCTIONS --------------------
// -------------------- START I2C FUNCTIONS --------------------

inline void setAddress(uint8_t address, bool auto_increment) {
    I2C2_sendByte(0x80 | (uint8_t) (auto_increment << 5) | address); // command: register address
}

// write to colour click at a given address
void writeByteTo(uint8_t address, uint8_t value) {
    I2C2_start();
    I2C2_sendByte(ADDR | WRITE); // writing to colour click
    setAddress(address, false); // set address without auto-increment
    I2C2_sendByte(value); // write to address
    I2C2_stop();
}

void setInterruptLowThreshold(uint16_t c) {
    writeByteTo(AILTL, c & 0xff);
    writeByteTo(AILTL + 1, (c >> 8) & 0xff);
    writeByteTo(AIHTL, 0xff);
    writeByteTo(AIHTL + 1, 0xff);
}

// initialise colour click with i2c
void colourClick_init(void) {
    I2C2_init(); // initialise i2c Master
    initLED(); // initialise RGB LED
    writeByteTo(R_ENABLE, 0x01); // enable colour click
    __delay_ms(3); // wait for start up before further configuration
    writeByteTo(R_ENABLE, 0b00010011);
    //                         || |+------ PON: power on
    //                         || +------- AEN: RGBC enable
    //                         |+--------- WEN: wait enable
    //                         +---------- AIEN: RGBC interrupt enable
    writeByteTo(R_ATIME, 0xC0); // set integration time to 0xC0: 154ms; 0x00: 700ms
    writeByteTo(R_CONTROL, 0x11); // 0x10: 16x gain; 0x11: 60x gain
    writeByteTo(APERS, 0b0010); // 2 clear channel consecutive values out of range for interrupt trigger
    setInterruptLowThreshold(clear_threshold);
}

bool readInterrupt(void) {
    return INT_PIN; // note interrupt is active LOW
}

void clearInterrupt(void) {
    I2C2_start();
    I2C2_sendByte(ADDR | WRITE); // writing to colour click
    I2C2_sendByte(0x80 | (uint8_t) (0b11 << 5) | 0b00110); // command: special function: clear interrupt
    I2C2_stop();
}

uint16_t readC(void) {
    I2C2_start();
    I2C2_sendByte(ADDR | WRITE); // writing to colour click
    setAddress(CDATA, false); // set address to clear low byte with auto-increment
    I2C2_repeatStart(); // start another transmission without stopping
    I2C2_sendByte(ADDR | READ); // reading from colour click
    uint16_t value = I2C2_readByte(true); // read low byte with acknowledge
    return value | (uint16_t) (I2C2_readByte(false) << 8); // read high byte without acknowledge
}

const uint16_t *readRGB(void) {
    static uint16_t RGB[3] = {0};
    
    I2C2_start();
    I2C2_sendByte(ADDR | WRITE); // writing to colour click
    setAddress(RDATA, true); // set address to clear low byte with auto-increment
    I2C2_repeatStart(); // start another transmission without stopping
    I2C2_sendByte(ADDR | READ); // reading from colour click
    
    // read RG
    uint16_t value;
    for (uint8_t i = 0; i < 2; ++i) {
        value = I2C2_readByte(true); // read low byte with acknowledge
        RGB[i] = value | (uint16_t) (I2C2_readByte(true) << 8); // read high byte with acknowledge
    }
    // read B separately to avoid final acknowledge bit
    value = I2C2_readByte(true); // read low byte with acknowledge
    RGB[2] = value | (uint16_t) (I2C2_readByte(false) << 8); // read high byte without acknowledge
    
    return RGB;
}

// -------------------- END I2C FUNCTIONS --------------------
// -------------------- START COLOUR FUNCTIONS --------------------

inline int16_t ab(int16_t a) {
    return a < 0 ? -a : a;
}

inline float max(float a, float b) {
    return a > b ? a : b;
}

inline float min(float a, float b) {
    return a < b ? a : b;
}

const HSLColour *rgb2hsl(const uint8_t R, const uint8_t G, const uint8_t B) {
    static HSLColour HSL = {0};
    
    float r = (float) R / 255;
    float g = (float) G / 255;
    float b = (float) B / 255;
    
    float M = max(r, max(g, b));
    float m = min(r, min(g, b));
    float c = M - m;
    
    float h, s, l;
    
    if (c < 1e-10) {
        h = 0;
    } else {
        float segment = 0;
        float shift = 0;
        if (M == r) {
            segment = (g - b) / c;
            if (segment < 0)
                shift = 6;
        } else if (M == g) {
            segment = (b - r) / c;
            shift = 2;
        } else {
            segment = (r - g) / c;
            shift = 4;
        }
        h = segment + shift;
    }
    
    l = (M + m) / 2;
    if (l == 0 || l == 1) {
        s = 0;
    } else {
        float temp = (2 * l - 1);
        s = c / (1 - (temp < 0 ? -temp : temp));
    }
        
    HSL.h = (uint16_t) (h * 60);
    HSL.s = (uint8_t) (s * 100);
    HSL.l = (uint8_t) (l * 100);
    return &HSL;
}

const uint8_t *readCalibratedRGB(void) {
    static uint8_t rgb[3] = {0};
    const uint16_t *raw_rgb = readRGB();
    for (uint8_t i = 0; i < 3; ++i) {
        uint16_t value = (uint16_t) ((float) raw_rgb[i] * rgb_scaler[i] / 256);
        rgb[i] = value < 256 ? (uint8_t) value : 255;
    }
    return rgb;
}

void colourClick_waitUntilWall(void) {
    clearInterrupt();
    while (readInterrupt()) {} // wait until interrupt is triggered (active LOW)
    return;
}

Card colourClick_readCard(void) {
//    colourClick_offLED();
//    __delay_ms(300);
    // LED is assumed off
    uint16_t c = readC();
    if (c > clear_threshold)
        return CLEAR;
    
    // LED on for colour + white/black measurement
    colourClick_onLED();
    __delay_ms(READ_DELAY);
    
    c = readC();
    const uint8_t *rgb = readCalibratedRGB();
    colourClick_offLED();
    
    // check for white/black
    uint16_t min = rgb[0];
    uint16_t max = rgb[0];
    for (uint8_t i = 1; i < 3; ++i) {
        if (rgb[i] < min) min = rgb[i];
        if (rgb[i] > max) max = rgb[i];
    }
    if (max - min < range_threshold) { // white or black
        if (c > white_threshold) {
            #ifdef __CARD_LED
                LATDbits.LATD7 = 1;
                __delay_ms(300);
                LATDbits.LATD7 = 0;
            #endif
            return WHITE;
        } else {
            #ifdef __CARD_LED
                LATHbits.LATH3 = 1;
                __delay_ms(300);
                LATHbits.LATH3 = 0;
            #endif
            return BLACK;
        }
    }
    
    // check for colour
    const HSLColour *hsl = rgb2hsl(rgb[0], rgb[1], rgb[2]);
    uint32_t smallest = (uint32_t) 1 << 31;
    Card card;
    for (uint8_t i = 0; i < NUM_CARDS; ++i) {
        int16_t h_dist = 180 - ab(ab((int16_t) CARD_H[i] - (int16_t) hsl->h) - 180);
        int8_t s_dist = (int8_t) (CARD_S[i] - hsl->s);
        uint32_t dist = (uint32_t) (h_dist*h_dist + s_dist*s_dist);
        if (dist < smallest) {
            smallest = dist;
            card = i;
        }
    }
    
    #ifdef __CARD_LED // flash detected colour
        __delay_ms(100);
        colourClick_setLED(CARD_R[card], CARD_G[card], CARD_B[card]);
        __delay_ms(300);  
        colourClick_setLED(0, 0, 0);
    #endif
    
    return card;
}

//uint16_t clear_threshold = 400; // (LED off) above this is CLEAR, below this is wall
//uint16_t white_threshold = 30000; // (LED on) if C channel is larger than threshold, then white, otherwise black

void colourClick_calibrateAll(void) {
    char buf[50];
    EUSART4_sendString("> CALIBRATING colours <\r\n");
    __delay_ms(100);
    EUSART4_sendString("RF2: ready; RF3: done\r\n");
    __delay_ms(100);
    
    // scaler calibration
    while (1) {
        sprintf(buf, "scalers=%u,%u,%u\r\n", (uint8_t) (rgb_scaler[0] * 100), (uint8_t) (rgb_scaler[1] * 100), (uint8_t) (rgb_scaler[2] * 100)); EUSART4_sendString(buf);
        EUSART4_sendString("Place buggy at wall against white\r\n");
        if (buttons_readInput() == RF3_DOWN) break;
        colourClick_onLED();
        __delay_ms(READ_DELAY);
        const uint16_t *white = readRGB();
        colourClick_offLED();
        uint16_t highest = 0;
        for (uint8_t i = 0; i < 3; ++i) {
            if (white[i] > highest) highest = white[i];
        }
        for (uint8_t i = 0; i < 3; ++i) {
            if (white[i] != 0) {
                rgb_scaler[i] = (float) highest / white[i];
            }
        }
    }
    
    __delay_ms(300);
    
//    // clear_threshold calibration
//    colourClick_offLED();
//    while (1) {
//        sprintf(buf, "clear_threshold=%u\r\n", clear_threshold); EUSART4_sendString(buf);
//        EUSART4_sendString("Place buggy at wall against white\r\n");
//        if (buttons_readInput() == RF3_DOWN) break;
//        __delay_ms(READ_DELAY);
//        uint16_t wall = readC();        
//        EUSART4_sendString("Place buggy away from wall\r\n");
//        if (buttons_readInput() == RF3_DOWN) break;
//        __delay_ms(READ_DELAY);
//        uint16_t clear = readC();
//        
//        clear_threshold = (wall + clear) / 2;
//    }
//    
//    __delay_ms(300);
//
//    // white_threshold calibration
//    while (1) {
//        sprintf(buf, "white_threshold=%u\r\n", white_threshold); EUSART4_sendString(buf);
//        EUSART4_sendString("Place buggy at wall against white\r\n");
//        if (buttons_readInput() == RF3_DOWN) break;
//        colourClick_onLED();
//        __delay_ms(READ_DELAY);
//        uint16_t white = readC();
//        colourClick_offLED();
//        EUSART4_sendString("Place buggy at wall against black\r\n");
//        if (buttons_readInput() == RF3_DOWN) break;
//        colourClick_onLED();
//        __delay_ms(READ_DELAY);
//        uint16_t black = readC();
//        colourClick_offLED();
//        
//        white_threshold = (uint16_t) (((uint32_t) black + (uint32_t) white) / 2);
//    }
    
    EUSART4_sendString(">CALIBRATION complete<\r\n");
}

// -------------------- END COLOUR FUNCTIONS --------------------