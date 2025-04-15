#include <xc.h>
#include <stdint.h>
#include <stdbool.h>
#include "interrupts.h"
#include "serial.h"
#include "colourClick.h"
#include "motors.h"
#include "timer.h"
#include "flags.h"

#ifdef __BLINKERS
    volatile uint8_t blinkers_elapsed_ms = 0;
#endif

void interrupts_init(void) {
    PIE4bits.RC4IE = 1; // enable EUSART4 RX interrupt
    PIE0bits.TMR0IE = 1;
    #ifdef __CARD_LED
        PIE5bits.TMR2IE = 1; // enable TMR2 interrupt for LED PWM
    #endif

    INTCONbits.IPEN = 0; // disable interrupt priority levels
    INTCONbits.PEIE = 1; // enable peripheral interrupts
    INTCONbits.GIE = 1; // enable global interrupts
}

void __interrupt() isr(void) {
    if (PIR0bits.TMR0IF) { // TMR0 flag for tracking time
        PIR0bits.TMR0IF = 0;
        ++TMR0_elapsed_ms;
        #ifdef __BLINKERS
            if (++blinkers_elapsed_ms == BLINKER_PERIOD) {
                if (is_flashing_brake) BRAKE_LED = ~BRAKE_LED;
                if (is_flashing_left) LEFT_LED = ~LEFT_LED;
                if (is_flashing_right) RIGHT_LED = ~RIGHT_LED;
            }
        #endif
    }
    
    #ifdef __CARD_LED
        if (PIR5bits.TMR2IF) { // TMR2 flag for PWM
            PIR5bits.TMR2IF = 0; // reset flag
            colourClick_interruptLED();
        }
    #endif
    
    if (PIR4bits.RC4IF) { // data received in RC4REG
        _EUSART4_putCharInRX(RC4REG); // buffer byte, also resets flag
    }
    if (PIR4bits.TX4IF) { // TX4REG is empty
        bool is_empty;
        char ch = _EUSART4_readCharFromTX(&is_empty);
        if (is_empty) {
            PIE4bits.TX4IE = 0; // disable EUSART4 TX when all is sent
        } else {
            TX4REG = ch; // send next character
        }
    }
}