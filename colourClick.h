#ifndef _color_H
#define _color_H

#include <stdint.h>
#include <stdbool.h>
#include "flags.h"

#ifdef __DEBUG_MODE
typedef struct {
    uint16_t h;
    uint8_t s;
    uint8_t l;
} HSLColour;
#endif

typedef enum {
    RED    = 0,
    GREEN  = 1,
    DBLUE  = 2,
    YELLOW = 3,
    PINK   = 4,
    ORANGE = 5,
    LBLUE  = 6,
    WHITE  = 7,
    BLACK  = 8,
    CLEAR = 9,
} Card;

void colourClick_init(void);
void colourClick_onLED(void);
void colourClick_offLED(void);
bool readInterrupt(void);
void clearInterrupt(void);
void colourClick_waitUntilWall(void);
Card colourClick_readCard(void);
void colourClick_calibrateAll(void);

#ifdef __CARD_LED
void colourClick_setLED(uint8_t r, uint8_t g, uint8_t b);
// used by interrupts.c for software LED PWM (hardware CCPs don't route to the right pins)
void colourClick_interruptLED(void);
#endif

#ifdef __DEBUG_MODE
uint16_t readC(void);
const uint16_t *readRGB(void);
const uint8_t *readCalibratedRGB(void);
const HSLColour *rgb2hsl(const uint8_t R, const uint8_t G, const uint8_t B);
#endif

#endif
