#ifndef BUTTONS_H
#define	BUTTONS_H

typedef enum {
    NONE, RF2_DOWN, RF3_DOWN,
} ButtonsState;

void buttons_init(void);
ButtonsState buttons_readInput(void);

#endif	/* BUTTONS_H */

