#ifndef FLAGS_H
#define	FLAGS_H

#define __DECELERATION
#define __BLINKERS

#define __DEBUG_MODE

// TODO RGB LED flag

#ifdef __DEBUG_MODE
//#define __ONBOARD
#define __CARD_LED // requires RGB LED
#define __STEPS_LED
#endif

#endif	/* FLAGS_H */

