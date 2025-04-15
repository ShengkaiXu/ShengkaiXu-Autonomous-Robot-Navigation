#ifndef _DC_MOTOR_H
#define _DC_MOTOR_H

#include <xc.h>
#include <stdint.h>
#include <stdbool.h>
#include "colourClick.h"

#define BLINKER_PERIOD 200 // ms
#define BRAKE_LED LATDbits.LATD4
#define LEFT_LED LATFbits.LATF0
#define RIGHT_LED LATHbits.LATH0

#ifdef __BLINKERS
    extern bool is_flashing_brake;
    extern bool is_flashing_left;
    extern bool is_flashing_right;
#endif

void motors_init(void);
void motors_setPower(int8_t left, int8_t right);
void motors_advance(int8_t cells);
void motors_turn(int8_t num_45);
void motors_recentre(void);
void motors_realign(bool is_forward);
Card motors_search(uint8_t *cells_moved);
void motors_calibrateAll(void);

void testForward(void);

void testReverse(void);

void testRightTurn(void);

void testLeftTurn(void);



#endif
