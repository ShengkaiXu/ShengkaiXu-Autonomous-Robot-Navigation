// CONFIG1L
#pragma config FEXTOSC = HS     // External Oscillator mode Selection bits (HS (crystal oscillator) above 8 MHz; PFM set to high power)
#pragma config RSTOSC = EXTOSC_4PLL// Power-up default value for COSC bits (EXTOSC with 4x PLL, with EXTOSC operating per FEXTOSC bits)

// CONFIG3L
#pragma config WDTCPS = WDTCPS_31// WDT Period Select bits (Divider ratio 1:65536; software control of WDTPS)
#pragma config WDTE = OFF        // WDT operating mode (WDT enabled regardless of sleep)

#define _XTAL_FREQ 64000000 //note intrinsic _delay function is 62.5ns at 64,000,000Hz

#include <xc.h>
#include <stdint.h>
#include <stdbool.h>
//#include "GoHome.h"
//#include "ADC.h"
#include "serial.h"
#include "buggy.h"
#include "interrupts.h"
#include "flags.h"
#include "buttons.h"

#ifdef __DEBUG_MODE
#include <stdio.h>
#include "colourClick.h"
#include "motors.h"
#include "timer.h"
#endif

void main(void) {
    //    ADC_init();
    EUSART4_init();
    buggy_init();
    buttons_init();
    interrupts_init();
    
    TRISDbits.TRISD7 = 0;
    LATDbits.LATD7 = 0;
    TRISHbits.TRISH3 = 0;
    LATHbits.LATH3 = 0;
    
//    while (PORTFbits.RF2) {}
//    __delay_ms(1000);
//    colourClick_calibrateAll();   
//    motors_calibrateAll();

    char buf[30];
    while (1) {
        while (PORTFbits.RF2) {}
        __delay_ms(1000);
        motors_advance();
//        
        while (PORTFbits.RF2) {}
        __delay_ms(1000);
        testReverse();
       
        
        while (PORTFbits.RF2) {}
        __delay_ms(1000);
        testRightTurn();
        
        while (PORTFbits.RF2) {}
        __delay_ms(1000);
        testLeftTurn();
//        
//        while (PORTFbits.RF2) {}
//        __delay_ms(1000);
//        motors_recentre();

//        Card card = colourClick_readCard();
//        buggy_navigate();
//        LATDbits.LATD7 = 1;
    }
}



