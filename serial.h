#ifndef _SERIAL_H
#define _SERIAL_H

#include <xc.h>
#include "stdbool.h"

#define PACKET_BUFFER_SIZE 5

void EUSART4_init(void);
char EUSART4_readChar(bool *is_empty);
const char *EUSART4_readLine();
const int16_t EUSART4_read4DigitInt(bool *err);
void EUSART4_sendChar(char ch);
void EUSART4_sendString(const char *string);
uint8_t EUSART4_readPacket(char *buf);

// below are for interrupt operation
void _EUSART4_putCharInRX(char ch);
char _EUSART4_readCharFromTX(bool *is_empty);

#endif	/* SERIAL_H */

