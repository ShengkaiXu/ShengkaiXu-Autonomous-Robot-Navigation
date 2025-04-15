#include <xc.h>
#include <stdint.h>
#include <stdbool.h>
#include "buggy.h"
#include "motors.h"
#include "flags.h"

#define _XTAL_FREQ 64000000 // for __delay_ms

#define MAP_SIZE 6 // side length of map
#define NUM_DIR 8

#define START_X 0
#define START_Y 0
#define START_DIR DIR_N

typedef enum {
    DIR_N, DIR_NE, DIR_E, DIR_SE, DIR_S, DIR_SW, DIR_W, DIR_NW,
    // 0,1    1,1    1,0    1,-1   0,-1   -1,-1   -1,0    -1,1
} Direction;

typedef struct {
    uint8_t steps; // steps to the start
    uint8_t dir : 3; // direction to the start
    uint8_t walls : 4;
    uint8_t is_forward_diagonal : 1; // if diagonal is like a forward slash /
} Cell;

const int8_t DIR_DX[NUM_DIR] = {0, 1, 1, 1, 0, -1, -1, -1};
const int8_t DIR_DY[NUM_DIR] = {1, 1, 0, -1, -1, -1, 0, 1};
const Direction DIR_OPPOSITE[NUM_DIR] = {DIR_S, DIR_SW, DIR_W, DIR_NW, DIR_N, DIR_NE, DIR_E, DIR_SE};

struct {
    Cell cells[MAP_SIZE][MAP_SIZE];
    int8_t x;
    int8_t y;
    Direction dir;
} map;

inline bool isDirOrthogonal(Direction dir) {
    return dir == DIR_N || dir == DIR_E || dir == DIR_S || dir == DIR_W;
}

inline bool isOutOfBounds(int8_t x, int8_t y) {
    return x < 0 || x >= MAP_SIZE || y < 0 || y >= MAP_SIZE;
}

void updateMap(Direction dir, uint8_t steps) {
    Cell *cur_cell = &(map.cells[map.y][map.x]);
    for (uint8_t k = 0; k < steps; ++k) {
        map.x += DIR_DX[dir]; // NOTE: overflow is ignored, should not happen if setup correctly (bold assumption)
        map.y += DIR_DY[dir];
        Cell *new_cell = &(map.cells[map.y][map.x]);
        
        // update steps and dirs of new and cur cells;
        if (new_cell->steps > cur_cell->steps + 1) { // if new cell is unvisited or less efficient
            new_cell->steps = cur_cell->steps + 1;
            new_cell->dir = DIR_OPPOSITE[dir];
        } else if (new_cell -> steps + 1 < cur_cell->steps) { // if new cell is visited and more efficient
            cur_cell->steps = new_cell -> steps + 1;
            cur_cell->dir = dir;
        }
        
        cur_cell = new_cell; // update cur cell
    }
            
    // update wall at last step
    if (isDirOrthogonal(dir)) {
        // update wall for current cell
        cur_cell->walls |= 0b1 << (dir / 2);
        // update wall for next cell along dir
        int8_t new_x = map.x + DIR_DX[dir];
        int8_t new_y = map.y + DIR_DY[dir];
        if (!isOutOfBounds(new_x, new_y)) {
            map.cells[new_y][new_x].walls |= 0b1 << (DIR_OPPOSITE[dir] / 2);
        }
    } else {
        cur_cell->walls = 0b1111; // indicate diagonal wall
        cur_cell->is_forward_diagonal = (dir == DIR_SE || dir == DIR_NW) ? true : false;
    }
}

// if after a turn, then try to realign backwards (is_forward = 0), if after advance, try to align forwards
void realign(bool is_forward) {
    Direction dir = is_forward ? map.dir : DIR_OPPOSITE[map.dir];
    // if there's a wall marked in dir direction
    if (isDirOrthogonal(dir) && ((map.cells[map.y][map.x].walls >> (dir / 2)) & 0b1)) {
        motors_realign(is_forward);
    } else { // on diagonal path
        // TODO
    }
}

void returnHome(void) {
    Cell *cur_cell = &(map.cells[map.y][map.x]);
    while (cur_cell->steps != 0) {
//        while (PORTFbits.RF2) {}
//        __delay_ms(1000);
        // turn to start direction
        int8_t turn_num = cur_cell->dir - map.dir;
        if (turn_num != 0) { // if turn is needed
            if (turn_num > 4) turn_num -= NUM_DIR;
            else if (turn_num < -4) turn_num += NUM_DIR;
            motors_turn(turn_num);
            map.dir = cur_cell->dir;
        }
        
        // realign after rotation
        realign(false);
        
        // advance once and update position
        motors_advance(1);
        map.x += DIR_DX[cur_cell->dir];
        map.y += DIR_DY[cur_cell->dir];
        cur_cell = &(map.cells[map.y][map.x]);
        
        // realign after advance
        realign(true);
    }
}

// returns whether or not all is completed
bool processCard(Card card) {
    switch (card) {
        case RED: // turn right
            motors_turn(2);
            map.dir = (map.dir + 2) % NUM_DIR;
            break;
        case GREEN: // turn left
            motors_turn(-2);
            map.dir = (map.dir + 6) % NUM_DIR;
            break;
        case DBLUE: // u-turn
            motors_turn(4);
            map.dir = DIR_OPPOSITE[map.dir];
            break;
        case YELLOW: // reverse and turn right
            motors_advance(-1);
            map.x += DIR_DX[DIR_OPPOSITE[map.dir]];
            map.y -= DIR_DY[DIR_OPPOSITE[map.dir]];
            realign(false); // try to realign after reversing
            motors_turn(2);
            map.dir = (map.dir + 2) % NUM_DIR;
            break;
        case PINK: // reverse and turn left
            motors_advance(-1);
            map.x += DIR_DX[DIR_OPPOSITE[map.dir]];
            map.y -= DIR_DY[DIR_OPPOSITE[map.dir]];
            realign(false); // try to realign after reversing
            motors_turn(-2);
            map.dir = (map.dir + 6) % NUM_DIR;
            break;
        case ORANGE: // turn 135 degrees right
            motors_turn(3);
            map.dir = (map.dir + 3) % NUM_DIR;
            break;
        case LBLUE:
            motors_turn(-3);
            map.dir = (map.dir + 5) % NUM_DIR;
            break;
        case WHITE: // fall through
        case BLACK: // fall through
        case CLEAR: // return home if white or error (black/clear)
            returnHome();
            return true;
            break;
    }
    realign(false); // try to realign after direction change
    return false;
}

void buggy_init(void) {
    colourClick_init();
    motors_init();
}

void buggy_navigate(void) {
    // initialisation for new navigation routine
    for (uint8_t j = 0; j < MAP_SIZE; ++j) {
        for (uint8_t i = 0; i < MAP_SIZE; ++i) {
            map.cells[j][i] = (Cell) {.steps = 99, .dir = 0, .walls = 0b0000, .is_forward_diagonal = false};
        }
    }
    map.x = START_X;
    map.y = START_Y;
    map.dir = START_DIR;
    map.cells[map.y][map.x].steps = 0; // starting cell

    bool finished = false;
    while (!finished) {
        uint8_t cells_moved;
        Card card = motors_search(&cells_moved); // advance till wall; TODO handle diagonal distance
        updateMap(map.dir, cells_moved); // update internal map for cells covered
        motors_recentre(); // return to centre; TODO handle diagonal case
        finished = processCard(card);
    }
}
