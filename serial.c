#include <xc.h>
#include <stdbool.h>
#include <stdint.h>
#include "serial.h"

#define RX_BUFFER_SIZE 20
#define TX_BUFFER_SIZE 50

#define PACKET_START '<'
#define PACKET_END '>'

// -------------------- RingBuffer --------------------

typedef struct {
    unsigned char *data;
    unsigned char start;
    unsigned char end;
    unsigned char num_data;
    const unsigned char SIZE;
} RingBuffer;

void ringBufferAppend(RingBuffer *buf, char ch) {
    buf->data[buf->end] = ch;
    if (++buf->end == buf->SIZE) buf->end = 0; // wrap around
    
    if (buf->num_data == buf->SIZE) { // buffer was full before this append
        buf->start = buf->end; // move start pointer forward
    } else {
        ++buf->num_data; // update data count
    }
}

char ringBufferRead(RingBuffer *buf, bool *is_empty) {
    if (buf->num_data == 0) { // buffer is empty
        *is_empty = true;
        return 0; 
    }
    
    *is_empty = false;
    char ch = buf->data[buf->start];
    if (++buf->start == buf->SIZE) buf->start = 0; // wrap around
    --buf->num_data; // update data count
    return ch;
}

// -------------------- Serial --------------------

RingBuffer EUSART4_RX_buffer = {
    .data = (unsigned char[RX_BUFFER_SIZE]) {0},
    .start = 0,
    .end = 0,
    .num_data = 0,
    .SIZE = RX_BUFFER_SIZE,
};

RingBuffer EUSART4_TX_buffer = {
    .data = (unsigned char[TX_BUFFER_SIZE]) {0},
    .start = 0,
    .end = 0,
    .num_data = 0,
    .SIZE = TX_BUFFER_SIZE,
};

void EUSART4_init(void) {
    // setup PPS mapping of EUSART TX/RX
    RC0PPS = 0x12; // map EUSART4 TX to output at C0
    RX4PPS = 0b00010001; // map EUSART RX to input at C1
    //           | |+++------- PIN 1
    //           +++---------- PORT C

    // setup EUSART4
    BAUD4CONbits.SCKP = 0; // idle TX state is high level (non-inverted)
    BAUD4CONbits.BRG16 = 0; // 8-bit baud rate
    BAUD4CONbits.WUE = 0; // disable wake-up enable bit
    BAUD4CONbits.ABDEN = 0; // disable auto-baud detect
    TX4STAbits.TX9 = 0; // 8-bit transmit
    TX4STAbits.SYNC = 0; // asynchronous mode
    TX4STAbits.SENDB = 0; // disable break bit
    TX4STAbits.BRGH = 0; // low speed baud rate
    RC4STAbits.RX9 = 0; // 8-bit reception
    RC4STAbits.CREN = 1; // enable continuous reception
    SP4BRGL = 103; // n = 103 => baudrate = 64e6/(64*(n+1)) = 9615bps
    SP4BRGH = 0; // not used

    TX4STAbits.TXEN = 1; // enable TX
    RC4STAbits.SPEN = 1; // enable RX serial port
}

inline void EUSART4_flushTX(void) {
    PIE4bits.TX4IE = 1; // enable TX interrupt to send content
}

char EUSART4_readChar(bool *is_empty) {
    return ringBufferRead(&EUSART4_RX_buffer, is_empty);
}

const char *EUSART4_readLine() { // blocking call, waits for a line
    static char str[RX_BUFFER_SIZE] = {0};
    char c;
    bool is_empty;
    for (uint8_t i = 0; i < RX_BUFFER_SIZE - 1; ++i) {
        while (c = ringBufferRead(&EUSART4_RX_buffer, &is_empty), is_empty) {}
        if (c == '\n' || c == '\r') {
            EUSART4_sendString("\r\n"); // feedback
            LATDbits.LATD7 = 1;
            str[i] = '\0';
            break;
        } else {
            EUSART4_sendChar(c); // feedback
            str[i] = c;
        }
    }
    return str;
}

const int16_t EUSART4_read4DigitInt(bool *err) { // waits for a number, range is -9999 to 9999
    const char *line = EUSART4_readLine();
    int16_t num = 0;
    bool negative = false;
    if (line[0] == '-') {
        negative = true;
        ++line;
    }
    for (uint8_t i = 0; i < 4; ++i) {
        char c = *line++;
        if (c == '\0') break;
        if (c < '0' || c > '9') {
            *err = true;
            break;
        }
        num = 10 * num + (uint8_t) (c - '0');
    }
    *err = false;
    return negative ? -num : num;
}

void EUSART4_sendChar(char ch) {
    ringBufferAppend(&EUSART4_TX_buffer, ch);
    EUSART4_flushTX();
}

void EUSART4_sendString(const char *string) {
    while (*string != '\0') {
        ringBufferAppend(&EUSART4_TX_buffer, *string++);
    }
    EUSART4_flushTX();
}

// below are functions for interrupt operation

void _EUSART4_putCharInRX(char ch) {
    ringBufferAppend(&EUSART4_RX_buffer, ch);
}

char _EUSART4_readCharFromTX(bool *is_empty) {
    return ringBufferRead(&EUSART4_TX_buffer, is_empty);
}

// -------------------- Data Packets --------------------

uint8_t EUSART4_readPacket(char *buf) {
    static bool packet_in_progress = false;
    static unsigned char idx = 0;
    bool is_empty;
    char ch = EUSART4_readChar(&is_empty);
    if (!is_empty) { // input available
//        EUSART4_sendChar(ch); // feedback, disable for serial datetime output in main
        
        if (packet_in_progress) {
            if (ch == PACKET_END) {
                buf[idx] = '\0'; // terminate string
                packet_in_progress = false;
//                EUSART4_sendString("\r\n"); // new line
                return idx; // return length of completed packet
            } else { // ch is part of packet
                buf[idx] = ch;
                if (++idx == PACKET_BUFFER_SIZE) { // limit length
                    idx = PACKET_BUFFER_SIZE - 1;
                }
            }
        } else if (ch == PACKET_START) {
            idx = 0;
            packet_in_progress = true;
        }
    }
    return 0; // packet not ready
}
