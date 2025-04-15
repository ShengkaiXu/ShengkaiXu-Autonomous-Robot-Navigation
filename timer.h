#ifndef TIMER_H
#define	TIMER_H

#include <stdint.h>
#include "flags.h"

extern volatile uint16_t TMR0_elapsed_ms;

void TMR0_init(void);
void TMR0_delay_ms(uint16_t ms);
void TMR0_startTimer(void);
uint16_t TMR0_endTimer(void);

#ifdef __CARD_LED
void TMR2_init(void);
#endif

#endif	/* TIMER_H */