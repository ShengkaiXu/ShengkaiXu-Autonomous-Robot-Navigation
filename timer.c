#include <xc.h>
#include <stdint.h>
#include "timer.h"

#define TMR0_FREQ 16e6 / 64

#define TMR2_FREQ 16e6 / 16
#define PWM_FREQ 200 // frequency of LED flashing

volatile uint16_t TMR0_elapsed_ms = 0;

// -------------------- START TMR0 --------------------

void TMR0_init(void) {
    // Timer0 configuration
    T0CON0bits.T016BIT = 0; // timer is 8-bit
    T0CON0bits.T0OUTPS = 0b0000; // 1:1 post-scaler
    T0CON1bits.T0CS = 0b010; // Fosc/4 timer source
    T0CON1bits.T0ASYNC = 0; // timer is synchronised to Fosc/4 (see errata)
    T0CON1bits.T0CKPS = 0b0110; // 1:64 pre-scaler
    TMR0H = (uint8_t) ((uint32_t) TMR0_FREQ / 1000) - 1; // 1000Hz
    TMR0L = 0;
    T0CON0bits.T0EN = 1; // enable timer
}

void TMR0_delay_ms(uint16_t ms) {
    TMR0_elapsed_ms = 0;
    while (TMR0_elapsed_ms < ms) {}
    return;
}

void TMR0_startTimer(void) {
    TMR0_elapsed_ms = 0;
}

uint16_t TMR0_endTimer(void) {
    return TMR0_elapsed_ms;
}

// -------------------- END TMR0 --------------------
// -------------------- START TMR2 --------------------

#ifdef __CARD_LED
void TMR2_init(void) { // TODO Timer2 is not running at the right frequency
    // Timer2 configuration
    T2CONbits.CKPS = 0b100; // 1:16 pre-scaler
    T2CONbits.OUTPS = 0b0000; // 1:1 post-scaler
    T2CLKCONbits.CS = 0b0001; // Fosc/4 timer source
    T2HLTbits.PSYNC = 1; // timer is synchronised to Fosc/4
    T2HLTbits.MODE = 0b00000; // free running period mode
    T2PR = (uint8_t) ((uint32_t) TMR2_FREQ / PWM_FREQ / 256) - 1;
    T2TMR = 0;
    T2CONbits.ON = 1; // timer enabled
}
#endif

// -------------------- END TMR2 --------------------