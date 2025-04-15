#include <xc.h>
#include "ADC.h"


#define BAT_VSENSE_CHANNEL 0b101110 // RF6 (BAT-VSENSE connected here)

void ADC_init(void) {

    
    TRISFbits.TRISF6 = 1; // RF6 as input
    ANSELFbits.ANSELF6 = 1; // Ensure analogue circuitry is active
    
    // Set up the ADC module - check section 32 of the datasheet for more details
    ADREFbits.ADNREF = 0; // Use Vss (0V) as negative reference
    ADREFbits.ADPREF = 0b00; // Use Vdd (3.3V) as positive reference
//    ADPCH = 0b001011; // Select channel RB3/ANB3 for ADC
    ADPCH = 0b101110; //Select channel RF6/ANF6 for ADC;
    ADCON0bits.ADFM = 0; // LEFT-justified result
    ADCON0bits.ADCS = 1; // Use internal Fast RC (FRC) oscillator as clock source for conversion
    ADCON0bits.ADON = 1; // Enable ADC
}

void ADC_setChannel(uint8_t channel) {
    ADCON0bits.ADON = 0; // disable ADC
    ADPCH = channel;
    ADCON0bits.ADON = 1; // enable ADC
}

uint16_t ADC_getValue(void) {
    ADCON0bits.GO = 1;
    while (ADCON0bits.GO);
    return (ADRESH << 8) | ADRESL;
}

 
uint16_t ADC_readBATVoltage(void) {
    ADC_setChannel(BAT_VSENSE_CHANNEL); // Set channel to BAT-VSENSE
    uint16_t adc_value = ADC_getValue(); // Get raw ADC value

    // Convert ADC value to voltage in millivolts:
    // V_BAT = (adc_value * 330 / 1023) * 3
    uint16_t bat_vsense = (adc_value * 330) / 1023; //  (Vdd = 330)
    return bat_vsense * 3; // Scale to actual battery voltage
}