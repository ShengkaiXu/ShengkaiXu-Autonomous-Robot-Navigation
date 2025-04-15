#include <xc.h>
#include "buttons.h"

#define RF2 !PORTFbits.RF2
#define RF3 !PORTFbits.RF3

void buttons_init(void) {
    TRISFbits.TRISF2 = 1; // input
    ANSELFbits.ANSELF2 = 0; // digital
    TRISFbits.TRISF3 = 1; // input
    ANSELFbits.ANSELF3 = 0; // digital
}

ButtonsState buttons_readInput(void) {
    while (1) {
        if (RF2) return RF2_DOWN;
        if (RF3) return RF3_DOWN;
    }
}