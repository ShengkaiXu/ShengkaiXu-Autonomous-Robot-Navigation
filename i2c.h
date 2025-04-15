#ifndef _i2c_H
#define _i2c_H

#include <stdbool.h>

void I2C2_init(void);
void I2C2_waitForIdle(void);
void I2C2_start(void);
void I2C2_repeatStart(void);
void I2C2_stop(void);
void I2C2_sendByte(unsigned char data_byte);
unsigned char I2C2_readByte(bool ack);

#endif
