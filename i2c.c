#include <xc.h>
#include <stdbool.h>
#include "i2c.h"

#define _XTAL_FREQ 64000000 //note intrinsic _delay function is 62.5ns at 64,000,000Hz  
#define _I2C_CLOCK 100000 //100kHz for I2C

// initialise I2C module and pins
void I2C2_init(void) {
    //i2c config  
    SSP2CON1bits.SSPM = 0b1000; // i2c master mode
    SSP2CON1bits.SSPEN = 1; // enable i2c
    SSP2ADD = (_XTAL_FREQ / (4 * _I2C_CLOCK)) - 1; // baud rate divider bits (in master mode)

    // pin configuration for i2c  
    TRISDbits.TRISD5 = 1; // disable output driver
    TRISDbits.TRISD6 = 1; // disable output driver
    ANSELDbits.ANSELD5 = 0; // digital mode
    ANSELDbits.ANSELD6 = 0; // digital mode
    SSP2DATPPS = 0x1D; // SDA on pin RD5
    SSP2CLKPPS = 0x1E; // SCL on pin RD6
    RD5PPS = 0x1C; // data output
    RD6PPS = 0x1B; // clock output
}

// wait until I2C is idle
void I2C2_waitForIdle(void) {
    while ((SSP2STAT & 0x04) || (SSP2CON2 & 0x1F));
}

// send start bit
void I2C2_start(void) {
    I2C2_waitForIdle();
    SSP2CON2bits.SEN = 1;
}

// send repeated start bit
void I2C2_repeatStart(void) {
    I2C2_waitForIdle();
    SSP2CON2bits.RSEN = 1;
}

// send stop bit
void I2C2_stop() {
    I2C2_waitForIdle();
    SSP2CON2bits.PEN = 1;
}

// send a byte
void I2C2_sendByte(unsigned char data_byte) {
    I2C2_waitForIdle();
    SSP2BUF = data_byte;
}

// read a byte
unsigned char I2C2_readByte(bool ack) {
    unsigned char tmp;
    I2C2_waitForIdle();
    SSP2CON2bits.RCEN = 1; // put the module into receive mode
    I2C2_waitForIdle();
    tmp = SSP2BUF; // read data from SS2PBUF
    I2C2_waitForIdle();
    SSP2CON2bits.ACKDT = !ack; // acknowledge bit is active LOW
    SSP2CON2bits.ACKEN = 1; // start acknowledge sequence
    return tmp;
}
