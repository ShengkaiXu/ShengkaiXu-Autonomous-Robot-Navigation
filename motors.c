#include <xc.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "motors.h"
#include "colourClick.h"
#include "serial.h"
#include "timer.h"
#include "buttons.h"

#define _XTAL_FREQ 64000000 // for __delay_ms (shouldn't be used, use TMR0_delay_ms instead) 

#define LAMP_LED LATHbits.LATH1
#define BEAM_LED LATDbits.LATD3

#define NUM_TESTS 5 // number of calibration tests

#define PWM_PERIOD 100 // 10kHz with 1:16 pre-scaler
#define STALL_POWER 50
#define ACCEL_DURATION 100 // time in ms for deceleration

#define PAUSE_DURATION 500 // time in ms for pauses between actions
#define ALIGN_DURATION 500 // time in ms for full power alignment

#ifdef __BLINKERS
    bool is_flashing_brake = false;
    bool is_flashing_left = false;
    bool is_flashing_right = false;
#endif

int8_t left_fast_power = 96; // power for left side for going forward
int8_t right_fast_power = 100; // power for left side for going forward
int8_t left_slow_power = 60; // power for left side for going forward
int8_t right_slow_power = 70; // power for left side for going forward
int8_t turn_power = 85; // power for both wheels when performing turning

uint16_t forward_fast_duration = 690; // time in ms for moving one cell forward
uint16_t forward_duration = 2000; // time in ms for moving one cell forward
uint16_t backward_duration = 2000;// time in ms for moving one cell backward
uint16_t left_turn_duration = 350; // time in ms for completing 90deg left turn
uint16_t right_turn_duration = 350; // time in ms for completing 90deg right turn
uint16_t recenter_duration = 550; // time in ms for backing from wall to cell centre

typedef struct {
    uint8_t power; // motor power, out of 100
    bool is_forward : 1; // motor direction, forward(1), reverse(0)
    bool is_brake : 1; // short or fast decay (brake or coast)
    volatile unsigned char * const POS_DUTY; // PWM duty address for motor +ve side
    volatile unsigned char * const NEG_DUTY; // PWM duty address for motor -ve side
} Motor;

Motor motor_left = {
    .power = 0,
    .is_forward = true,
    .is_brake = false,
    .POS_DUTY = &CCPR1H,
    .NEG_DUTY = &CCPR2H,
};

Motor motor_right = {
    .power = 0,
    .is_forward = true,
    .is_brake = false,
    .POS_DUTY = &CCPR3H,
    .NEG_DUTY = &CCPR4H,
};

// function initialise T2 and CCP for DC motor control
void motors_init(void) {
    // TRIS and LAT pins for LEDs
    BRAKE_LED = 0;
    LEFT_LED = 0;
    RIGHT_LED = 0;
    LAMP_LED = 0;
    BEAM_LED = 0;
    TRISDbits.TRISD4 = 0;
    TRISFbits.TRISF0 = 0;
    TRISHbits.TRISH0 = 0;
    TRISHbits.TRISH1 = 0;
    TRISDbits.TRISD3 = 0;
    
    
    // TRIS and LAT registers for PWM  
    TRISEbits.TRISE2 = 0; // output
    TRISEbits.TRISE4 = 0; // output
    TRISCbits.TRISC7 = 0; // output
    TRISGbits.TRISG6 = 0; // output

    // configure PPS to map CCP modules to pins
    RE2PPS = 0x05; //CCP1 on RE2
    RE4PPS = 0x06; //CCP2 on RE4
    RC7PPS = 0x07; //CCP3 on RC7
    RG6PPS = 0x08; //CCP4 on RG6

    // TMR2 config
    T2CONbits.CKPS = 0b100; // 1:16 prescaler
    T2HLTbits.MODE = 0b00000; // free Running Mode, software gate only
    T2CLKCONbits.CS = 0b0001; // Fosc/4
    T2PR = PWM_PERIOD;
    T2CONbits.ON = 1;

    // setup CCP modules to output PMW signals
    // initial duty cycles 
    CCPR1H = 0;
    CCPR2H = 0;
    CCPR3H = 0;
    CCPR4H = 0;

    //use tmr2 for all CCP modules used
    CCPTMRS0bits.C1TSEL = 0;
    CCPTMRS0bits.C2TSEL = 0;
    CCPTMRS0bits.C3TSEL = 0;
    CCPTMRS0bits.C4TSEL = 0;

    //configure each CCP
    CCP1CONbits.FMT = 1; // left aligned duty cycle (we can just use high byte)
    CCP1CONbits.CCP1MODE = 0b1100; // PWM mode  
    CCP1CONbits.EN = 1; //turn on

    CCP2CONbits.FMT = 1; // left aligned
    CCP2CONbits.CCP2MODE = 0b1100; // PWM mode  
    CCP2CONbits.EN = 1; //turn on

    CCP3CONbits.FMT = 1; // left aligned
    CCP3CONbits.CCP3MODE = 0b1100; // PWM mode  
    CCP3CONbits.EN = 1; //turn on

    CCP4CONbits.FMT = 1; // left aligned
    CCP4CONbits.CCP4MODE = 0b1100; // PWM mode  
    CCP4CONbits.EN = 1; //turn on
    
    TMR0_init(); // enable TMR0 for delays
}

// function to set CCP PWM output from the values in the motor structure
void motors_setMotorPWM(Motor *m) {
    uint8_t posDuty, negDuty; // duty cycle values for different sides of the motor

    if (m->is_brake) {
        posDuty = (uint8_t) (PWM_PERIOD - ((uint16_t) m->power * PWM_PERIOD) / 100); //inverted PWM duty
        negDuty = PWM_PERIOD; //other side of motor is high all the time
    } else {
        posDuty = 0; // other side of motor is low all the time
        negDuty = (uint8_t) (((uint16_t) m->power * PWM_PERIOD) / 100); // PWM duty
    }

    if (m->is_forward) {
        *(m->POS_DUTY) = posDuty; // assign values to the CCP duty cycle registers
        *(m->NEG_DUTY) = negDuty;
    } else {
        *(m->POS_DUTY) = negDuty; // do it the other way around to change direction
        *(m->NEG_DUTY) = posDuty;
    }
}


void motors_updatePWM(void) {
    motors_setMotorPWM(&motor_left);
    motors_setMotorPWM(&motor_right);
}

void motors_setPower(int8_t left, int8_t right) {
    motor_left.is_forward = left > 0;
    motor_right.is_forward = right > 0;
    motor_left.power = (uint8_t) (motor_left.is_forward ? left : -left);
    motor_right.power = (uint8_t) (motor_right.is_forward ? right : -right);
    
    if (motor_left.power < STALL_POWER) motor_left.power = 0;
    if (motor_right.power < STALL_POWER) motor_right.power = 0;
    motors_updatePWM();
//    int8_t left_power_start = motor_left.is_forward ? (int8_t) motor_left.power : -(int8_t) motor_left.power;
//    int8_t right_power_start = motor_right.is_forward ? (int8_t) motor_right.power : -(int8_t) motor_right.power;
//    int8_t left_power_change = left - left_power_start;
//    int8_t right_power_change = right - right_power_start;
//    
//    for (uint8_t i = 0; i <= ACCEL_DURATION; ++i) {
//        int8_t left_power = (int8_t) (left_power_start + ((int16_t) left_power_change * i) / ACCEL_DURATION);
//        motor_left.is_forward = left_power > 0;
//        motor_left.power = (uint8_t) (motor_left.is_forward ? left_power : -left_power);
//        
//        int8_t right_power = (int8_t) (right_power_start + ((int16_t) right_power_change * i) / ACCEL_DURATION);
//        motor_right.is_forward = right_power > 0;
//        motor_right.power = (uint8_t) (motor_right.is_forward ? right_power : -right_power);
//
//        motors_updatePWM();
//        TMR0_delay_ms(1);
//    }
//    
//    if (motor_left.power < STALL_POWER) motor_left.power = 0;
//    if (motor_right.power < STALL_POWER) motor_right.power = 0;
//    motors_updatePWM();
}

inline void enableBrakeLights(void) {
    #ifdef __BLINKERS
        is_flashing_brake = true;
    #else
        BRAKE_LED = 1;
    #endif
}

inline void disableBrakeLights(void) {
    #ifdef __BLINKERS
        is_flashing_brake = false;
    #endif
    BRAKE_LED = 0;
}


void motors_advance(int8_t cells) {
    if (cells == 0) return;
    bool is_reversing = cells < 0;
    if (is_reversing) enableBrakeLights();
    for (uint8_t i = 0; i < (is_reversing ? -cells : cells); ++i) {
        if (is_reversing) {
            motors_setPower(-left_slow_power, -right_slow_power);
            __delay_ms(backward_duration);
        } else {
            motors_setPower(left_fast_power, right_fast_power);
            __delay_ms(forward_fast_duration);
        }
        motors_setPower(0, 0);
        __delay_ms(PAUSE_DURATION);
    }
    disableBrakeLights();
}

void motors_turn(int8_t num_45) {
    if (num_45 == 0) return;
    int8_t num_90 = num_45 / 2;
    bool is_turning_right = num_45 > 0;
    int8_t power = is_turning_right ? turn_power : -turn_power;
    
    // blinkers
    if (is_turning_right) {
        #ifdef __BLINKERS
            is_flashing_right = true;
        #else
            RIGHT_LED = 1;
        #endif
    } else {
        #ifdef __BLINKERS
            is_flashing_left = true;
        #else
            LEFT_LED = 1;
        #endif
    }
    
//    uint16_t duration = is_turning_right ? right_turn_duration : left_turn_duration;
    for (uint8_t i = 0; i < (is_turning_right ? num_90 : -num_90); ++i) {
        motors_setPower(power, -power);
//        TMR0_delay_ms(duration);
        if (is_turning_right)
            __delay_ms(right_turn_duration);
        else
            __delay_ms(left_turn_duration);
        motors_setPower(0, 0);
        __delay_ms(PAUSE_DURATION);
    }
    if (2 * num_90 != num_45) { // since turning durations are calibrated to 90deg, odd num_45 needs an additional half turn
        motors_setPower(power, -power);
//        TMR0_delay_ms(duration / 2);
        if (is_turning_right)
            __delay_ms(right_turn_duration/2);
        else
            __delay_ms(left_turn_duration/2);
        motors_setPower(0, 0);
    }
    
    #ifdef __BLINKERS
        is_flashing_left = false;
        is_flashing_right = false;
    #endif
    RIGHT_LED = 0;
    LEFT_LED = 0;
    __delay_ms(PAUSE_DURATION);
}

void motors_recentre(void) {
    enableBrakeLights();
    motors_setPower(-left_slow_power, -right_slow_power);
    __delay_ms(recenter_duration);
    motors_setPower(0, 0);
    disableBrakeLights();
    __delay_ms(PAUSE_DURATION);
}

void motors_realign(bool is_forward) {
    int8_t left_power = is_forward ? left_slow_power : -left_slow_power;
    int8_t right_power = is_forward ? right_slow_power : -right_slow_power;
    int8_t full_power = is_forward ? 100 : -100;
    
    if (!is_forward) enableBrakeLights();
    
    // move forward to wall and align
    motors_setPower(left_power, right_power);
    __delay_ms(forward_duration / 2); // need 1/3 duration to wall, but use 1/2 to be safe
    motors_setPower(full_power, full_power);
    __delay_ms(ALIGN_DURATION);
    motors_setPower(0, 0);
    __delay_ms(PAUSE_DURATION);
    
    disableBrakeLights();
    
    // return to centre
    motors_setPower(-left_power, -right_power);
    __delay_ms(recenter_duration);
    motors_setPower(0, 0);
    __delay_ms(PAUSE_DURATION);
}

Card motors_search(uint8_t *cells_moved) {
    motors_setPower(left_fast_power, right_fast_power);
    TMR0_startTimer();
    colourClick_waitUntilWall();
    uint16_t elapsed_time = TMR0_endTimer();
    *cells_moved = (uint8_t) ((float) elapsed_time / forward_duration);
    
    __delay_ms(200);
    motors_setPower(0, 0);
    __delay_ms(100);
    motors_setPower(100, 100);
    __delay_ms(ALIGN_DURATION);
    motors_setPower(0, 0);
    
    Card card = colourClick_readCard();
    
    #ifdef __STEPS_LED // flash number of steps estimated from time
        __delay_ms(1000);
        for (uint8_t i = 0; i < *cells_moved; ++i) {
            LATHbits.LATH3 = 1;
            __delay_ms(200);
            LATHbits.LATH3 = 0;
            __delay_ms(200);
        }
    #endif
    
    return card;
}

// -------------------- START CALIBRATION FUNCTIONS --------------------

void testForward(void) {
    motors_advance(1);
}

void testReverse(void) {
    motors_advance(-1);
}

void testRightTurn(void) {
    motors_turn(8);
}

void testLeftTurn(void) {
    motors_turn(-8);
}

//void motors_calibrateAll(void) {
//    static void (*const tests[NUM_TESTS])(void) = {motors_recentre, testForward, testReverse, testRightTurn, testLeftTurn};
//    static const char *const test_names[NUM_TESTS] = {"recentre", "forward 1", "reverse 1", "right 8", "left 8"};
//    static uint16_t *const durations[NUM_TESTS] = {&recenter_duration, &forward_duration, &backward_duration, &right_turn_duration, &left_turn_duration};
//
//    char buf[30];
//    bool err;
//    for (uint8_t i = 0; i < NUM_TESTS; ++i) {
//        sprintf(buf, "> CALIBRATING %s <\r\n", test_names[i]); EUSART4_sendString(buf);
//        TMR0_delay_ms(300);
//        while (1) {
//            sprintf(buf, "Current duration: %u\r\n", *(durations[i])); EUSART4_sendString(buf);
//            EUSART4_sendString("Run: RF2; Done: RF3\r\n");
//            if (buttons_readInput() == RF3_DOWN) break;
//            (*tests[i])();
//            EUSART4_sendString("New duration: ");
//            int16_t value = EUSART4_read4DigitInt(&err);
//            if (err) continue;
//            if (value > 0) *(durations[i]) = (uint16_t) value;
//        }
//        sprintf(buf, "> FINISHED %s <\r\n", test_names[i]); EUSART4_sendString(buf);
//    }
//    EUSART4_sendString("> MOTORS CALIBRATED <");
//}

// -------------------- END CALIBRATION FUNCTIONS --------------------