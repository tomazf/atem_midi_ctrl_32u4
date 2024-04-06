# Atem MIDI controller (32u4)

Atem MIDI box with 4x8 buttons with color LED feedback (fixed color and blinking color)

Required HW - Arduino Pro micro, Arduino Leonardo (native MIDI support)

Required libraries for HW (Skaarhoj BI8 boards):
- include "Wire.h"
- include "MCP23017.h"
- include "PCA9685.h"
- include "SkaarhojBI8.h"



3D model            |  working prototype
:-------------------------:|:-------------------------:
![controller](/pics/image000003.jpg)  |  ![controller](/pics/20240406_134347.jpg)

default SETUP:

SENDER:
 - midi_channel = 4 (channel)
 - midi_note_offset = 49 (for buttons)
 - midi_vel = 127 (send velocity)

RECEIVER:
channel and note same as button mapping

**velocity look-up table:**

- state      OFF:  0
- state     ON R:  1
- state     ON G:  2
- state     ON Y:  3
- state     ON YY: 4
- state    ON YYY: 5
- state BLINK   R: 6
- state BLINK   G: 7
- state BLINK   Y: 8
- state BLINK  YY: 9
- state BLINK YYY: 10
- state     ON R:  127

**example:**

one **channel**, **pitch** for buttons, **velocity** for LED state
- CH4, pitch 61, vel: 1 -> 61 = 1st button, 1=red
- CH4, pitch 61, vel: 2 -> 61 = 1st button, 2=green
- CH4, pitch 61, vel: 3 -> 61 = 1st button, 3=yellow
- CH4, pitch 62, vel: 6 -> 62 = 2nd button, 6=red blink
- CH4, pitch 63, vel: 7 -> 63 = 3rd button, 7=green blink
