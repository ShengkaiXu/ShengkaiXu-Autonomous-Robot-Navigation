# Buggy Mine Navigation

## Objective

This project implements a robust control system for a robot buggy, integrating motor control, sensor data processing, and autonomous navigation capabilities. The codebase ensures precise movement, efficient sensor communication, and reliable debugging for development and testing. The buggy is designed to navigate a "mine" environment, interact with colour-coded navigation instructions, and return to its starting position reliably.

## Background

An autonomous robot (buggy) has to navigate a mine consisting of black walls 100mm high with coloured cards on some of the walls.
 
![image](https://github.com/user-attachments/assets/519a2baa-d0e7-4f83-a19b-5411555e9ff6)

The colour of the cards indicate the required action for the buggy to take, defined in the table below.

     | Color      | Instruction                         |
     | ---------- | ----------------------------------- |
     | Red        | Turn Right 90°                      |
     | Green      | Turn Left 90°                       |
     | Blue       | Turn 180°                           |
     | Yellow     | Reverse 1 square and turn right 90° |
     | Pink       | Reverse 1 square and turn left 90°  |
     | Orange     | Turn Right 135°                     |
     | Light Blue | Turn Left 135°                      |
     | White      | Finish (return home)                |
     | Black      | Maze wall color                     |
     
Upon completion of the maze/mine (white card), the buggy is required to return to its starting point. The buggy is equipped with four motors and a ColourClick sensor (RGB sensor + RGB LED), controlled by a PIC18 microcontroller.

## Colour Detection

The ColourClick consists of a 4 channel colour sensor (red, green, blue, and clear), and an RGB LED. The communication with the ColourClick is via I2C and documented well in the challenge brief. The solution employed used the LED to illuminate the wall ahead with 'white' light, then a colour reading is made.

The colour detection is based on the HSL colourspace. By converting from RGB to HSL, the lightness (or sometimes brightness) aspect of the colour can be removed, leaving only the base colours – defined by H (hue) and S (saturation). Therefore, if the hue and saturation of all the given cards are known, a 'distance' from the measured colour to the known values can be worked out. This is simply done by summing the squared differences of the H and S values, done in `colourClick.c`.

However, by removing lightness, black and white are no longer well defined in HSL space. Therefore, an early comparison is done to check the range of the raw RGB values, i.e. `range = max(r,g,b) - min(r,g,b)`. If this is sufficiently below a threshold, the buggy concludes that it must be black or white.

Altogether, the colour detection procedure is as follows:

1. Check if the area in front of the sensor is a wall by comparing the clear channel to a low threshold; near a wall is darker than away from a wall. This is done using the internal interrupt of the ColourClick. If a wall is not detected, then exit.
2. Check if the range of the calibrated RGB values is within a range threshold to determine black and white. If satisfied, then use the clear channel to decide between black and white and exit.
3. Convert the RGB values to HSL, to then be compared to the known card HSL values. The closest in Euclidean distance is deemed the detected colour.

Note that step 2 mentions calibrated RGB values. This specifically means "calibrated to white", such that when measuring the white card, all three RGB channels should return equal values. This is necessary because the sensors are not equally sensitive, as seen in the figure below.

![image](https://github.com/user-attachments/assets/4b0a9daa-f62c-41a7-b93e-7e677e8ad553)


Additionally, the individual RGB LEDs are not equally powerful either. Notably, green is much brighter than blue. This can be corrected by employing PWM control on each individual pin to adjust the white balance of the emitted light. However, the calibration to white method mentioned was sufficient. Instead, the PWM control was used in debugging to echo the measured colour. The PWM was implemented using Timer2 on the PIC as the built-it CCP cannot be wired to the required output pins.

Lastly, to reduce the influence of ambient lighting, a shroud was created for the ColourPick as seen in the figure below. This blocks out the ambient light when the buggy is up against a wall. This allows for better wall detection and more consistent colour readings.

![image](https://github.com/user-attachments/assets/a957a98a-6680-4f2c-a71e-2d64f16d30a9)


## Motor Control

The motors are controlled through the PWM capabilities of the microcontroller. By varying the duty cycle, the motor power output can be varied and different speeds achieved.

For this implementation, a few 'fundamental' actions are defined:

* `motors_advance(cells)`: advance (forwards or backwards) an integer number of mine 'cells'
* `motors_turn(num_45)`: turn (left or right) an integer number of 45 degree angles
* `motors_recentre()`: reverse from wall to cell centre
* `motors_realign()`: advance (forwards or backwards) towards a wall to realign the buggy axis, then return to centre

The main motor function for wall discovery is dependent on the fundamental actions above.

* `motors_search()`: advance forwards indefinitely until a wall is detected, return the colour of the wall and an estimate of the distance travelled

## Navigation

### Rationale

The navigation is based on impulsed, or discrete cells travelled, as opposed to the most continuous method of tracking time travelled then reversing actions. This is because the mine is built on a grid, so by recording the buggy's position and orientation as it traverses, the spatial positions of encountered walls can be recorded. This allows for more powerful path simplification algorithms back to the start, as well as more greedy realignment routines against known walls whenever possible. Fewer moves in the return path and more realignment attempts result in a more reliable buggy overall.

### Principle

In software, the buggy stores a map (a grid of cells), where cells are defined by (2 bytes per cell)

```
typedef struct {
    uint8_t steps; // steps to the start
    uint8_t dir : 3; // direction to the start
    uint8_t walls : 4; // LSB is N wall, all the way to MSB for W wall; if is_walls_diagonal, then LSB is / wall and next is \ wall
    uint8_t is_walls_diagonal : 1; // if walls are diagonal, otherwise they are orthogonal
} Cell;
```

Beginning at the starting square (predefined), the buggy updates the map such that each cell it encounters stores the number of steps taken to reach it. If more than one visit to a cell is encountered, then the smallest number of steps back to the start is the one kept. Similarly, the direction towards the start is also stored. Therefore, once the end is reached, the buggy can retrace an optimised path back to the start following the `dir` parameter of each cell it is on.

Additionally, collisions with walls are recorded, such that the same walls can be used to realign the buggy whenever possible.

Overall, this is a more powerful solution than tracking the time of actions, because with the known constraints of the mine (known square grids), information is no longer sequential but known spatially for additional processing (i.e. wall finding, path optimisation). However, this comes at a cost of memory space, which is not viable for larger mines.
 
## Results
### First test
In the first test, the buggy navigated the course at a relatively slow speed. The color recognition, as well as the left and right turns, worked accurately and reliably. The return logic was correct and executed properly. The straight-line motion had minimal error. However, due to differences between the test and exam environments and a failure to calibrate the motors_advance function, the buggy miscalculated the distance for a single grid square. The buggy's internal calculation for one grid was shorter than the actual distance, leading it to believe it had traveled three squares when it had only traveled two. This resulted in the buggy reversing too far during the goHome process. After then, we calibrated the buggy's forward distance for one grid square. After this adjustment, the buggy performed perfectly, successfully navigating the course and returning to the starting position without any errors.

https://github.com/user-attachments/assets/4f154dc2-802d-4331-b84a-709c3f9b71e5

### Second test
In second test, to complete the task in a shorter time, we redefined left_fast_power and right_fast_power to allow the buggy to move at a faster speed. The forward distance for one grid square was recalibrated for this higher speed. With these adjustments, the buggy executed all steps quickly and accurately, and it successfully returned to the starting position.To complete the task in a shorter time, we redefined left_fast_power and right_fast_power to allow the buggy to move at a faster speed. The forward distance for one grid square was recalibrated for this higher speed. With these adjustments, the buggy executed all steps quickly and accurately, and it successfully returned to the starting position.

https://github.com/user-attachments/assets/68d86b3d-549d-4f7c-bad8-08dbde002960

Note that at 1:03 of the above video, the path optimisation can be seen as a large chuck of the mine is ignored on the way home.


### Final test
In the final test, the buggy successfully completed the navigation task. However, when detecting the white card to initiate the return function, a programming error in the updateMap function caused an issue:

```
case YELLOW: // reverse and turn right
    motors_advance(-1);
    map.x += DIR_DX[DIR_OPPOSITE[map.dir]];
    map.y -= DIR_DY[DIR_OPPOSITE[map.dir]];
```
In the final line, the minus sign should be changed to plus. This error led the buggy to misinterpret its position and fail to return to the starting point.

https://github.com/user-attachments/assets/5ba05334-dd23-4a3b-b0bd-0362c78ec1c0

After correcting this mistake, we tested the second half of the maze due to time constraints. The buggy demonstrated that it could correctly execute the return function, completing the task successfully.

https://github.com/user-attachments/assets/da7ec5f1-4168-4f51-bb42-2bebc0258891

## Further Work

###  Onboard Calibration
Add code to enable onboard calibration of color detection, turning angles, and forward/backward motion without needing a computer. This feature would allow quick adjustments in varying environments.

### Higher Speed Calibration
Since the code supports higher speeds, calibrate the buggy to operate at increased speeds and ensure precision during navigation. This would enable tasks to be completed faster and more efficiently.

### Stable Distance Prediction
Improving the distance prediction algorithm will make the buggy's movements more stable and consistent. This can be achieved by refining the logic in the motors_advance function to better handle variations in speed, surface friction, and wheel slippage. Enhanced distance sensors or encoders could also be integrated to provide more accurate feedback for distance tracking.

## Additional Buggy Features

The buggy includes extra LEDs that can provide visual feedback for debugging:

- **H.LAMPS**: Turns on front white and rear red LEDs at reduced brightness.
- **M.BEAM and BRAKE**: Enables full-brightness mode.
- **TURN-L and TURN-R**: Controls turn signals without brightness control.

