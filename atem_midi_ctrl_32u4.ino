/*****************

   MIDI box - input on specific channel and velocity and send note button on press
   @Ferbi v0.3
      - added keypad mapping
      - added LED color slots
      - added blinking feature
      - updated HW - Arduino Micro 32u4

   @Ferbi v0.4    5.4.2024
      - added 3 1x8 button boards (extended HW)
      - new enclosure
      - array map for new buttons
*/

// All related to library "SkaarhojBI8"
#include "Wire.h"
#include "MCP23017.h"
#include "PCA9685.h"
#include "SkaarhojBI8.h"

#include <MIDIUSB.h>      // for Atmega32u4 - Micro, Pro Micro, Leonardo...
#include <Streaming.h>

//#define SERIAL_DMP        // serial DEBUG
#define pinPot A0         // potentiometer input
#define CC_ID 20          // control change id (free to choose)

// MIDI events
//
void noteOn(byte channel, byte pitch, byte velocity) {
  midiEventPacket_t noteOn = {0x09, 0x90 | (channel - 1), pitch, velocity};   // -1 because we start with 0
  MidiUSB.sendMIDI(noteOn);
}

void noteOff(byte channel, byte pitch, byte velocity) {
  midiEventPacket_t noteOff = {0x08, 0x80 | (channel - 1), pitch, velocity};
  MidiUSB.sendMIDI(noteOff);
}

void controlChange(byte channel, byte control, byte value) {
  midiEventPacket_t event = {0x0B, 0xB0 | (channel - 1), control, value};
  MidiUSB.sendMIDI(event);
}

// Setting up a board - objects
SkaarhojBI8 board[4];           // array for 4 boards with 8 buttons each

// button mappings
uint8_t buttonMap[][8] = {              // HW fixed: button -> note (for 2 boards)
  {2, 1, 4, 8, 3, 5, 7, 6},
  {8, 7, 6, 5, 4, 3, 2, 1}
};
uint8_t buttonMapS[][8] = {             // HW fixed: note -> button (for 2 boards)
  {2, 1, 5, 3, 6, 8, 7, 4},
  {8, 7, 6, 5, 4, 3, 2, 1}
};

// I2C address for HW boards
uint8_t board_addr[] = {7, 0, 2, 1};    // HW fixed: board addresses

// MIDI setup
uint8_t midi_channel = 4;               // define MIDI channel
uint8_t num_buttons = 8;                // buttons per board
uint8_t midi_vel = 127;
uint8_t midi_note_offset = 49;          // for sending and receiving - button
uint8_t board_note_offset = 16;         // for sending and receiving - button (board)
uint8_t board_detect = 0;
bool updated_state = false;
bool ledON = false;
int period_blink = 400;
unsigned long time_now = 0;
int potValue = 0;
int potValue_old = 0;
uint8_t potValueSet = 0;
uint8_t potValueSet_old = 0;

// do not CHANGE
uint8_t receive_channel_offset = 0x8F;  // for receive only ?

// LED defines
uint8_t LEDstateMap[][8] = {            // LED state - 8x LED - based on midi_note_offset and velocity
  {0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0},
  {0, 0, 0, 0, 0, 0, 0, 0}
};

// velocity look-up table:
//
// state      OFF:  0
// state     ON R:  1
// state     ON G:  2
// state     ON Y:  3
// state     ON YY: 4
// state    ON YYY: 5
// state BLINK   R: 6
// state BLINK   G: 7
// state BLINK   Y: 8
// state BLINK  YY: 9
// state BLINK YYY: 10
// state     ON R:  127

// example
//
//en kanal, različne note za gumbe, velocity določi stanje LEDice
//CH4, note 61, vel: 1 -> 61 = prvi gumb, 1=red
//CH4, note 61, vel: 2 -> 61 = prvi gumb, 2=green
//CH4, note 61, vel: 3 -> 61 = prvi gumb, 3=yellow
//CH4, note 62, vel: 6 -> 62 = drugi gumb, 6=red blink
//CH4, note 63, vel: 7 -> 63 = tretji gumb, 7=green blink

// SETUP
//
void setup() {

#ifdef SERIAL_DMP
  // local serial setup
  Serial.begin(115200);
  while (!Serial);
  Serial.print("Serial BOOT - disable when finished debugging!");
#endif

  // Always initialize Wire before setting up the SkaarhojBI8 class!
  Wire.begin();

  // Set up the boards - array loop
  for (byte i = 0; i < (sizeof(board_addr) / sizeof(board_addr[0])); i++) {
    board[i].begin(board_addr[i], false);             // Address from array, dip-switch on board is OFF
    board[i].testSequence(40);                        // Runs LED test sequence
    board[i].setDefaultColor(0);                      // turn OFF all LEDs - colors:  0 - OFF, 1 - Y, 2 - R, 3 - G, 4 - YY, 5 - YYY
    board[i].setButtonColorsToDefault();              // turn off button LEDs
  }

  // define input for pot
  pinMode(pinPot, INPUT);

  // SETUP done
}

// main LOOP
//
void loop() {

  // check if button is pressed
  checkButtons();           // for MIDI send

  // read incoming MIDI data
  readMIDI();               // for setting LED state

  // check potentiometer value (if changed)
  checkPot();               // CC send

  // set LED color mode
  parseLEDstateMAP();       // only for color change

  // handle blink
  ledBlinkCheck();          // continous blinking
}


// check if button is pressed
//
void checkButtons()
{
  //send MIDI data if pressed
  for (int b = 0; b < (sizeof(board_addr) / sizeof(board_addr[0])); b++)
  {
    for (int i = 1; i <= 8; i++) {
      if (board[b].buttonDown(i)) {                        // note send ON if button down
        noteOn(midi_channel, b * board_note_offset + midi_note_offset + buttonMap[(b > 0) ? 1 : 0][i - 1] - 1, midi_vel);
        MidiUSB.flush();
        delay(2);

#ifdef SERIAL_DMP
        Serial << "\n" << "button pressed - note: " << b * board_note_offset + midi_note_offset + buttonMap[(b > 0) ? 1 : 0][i - 1] - 1;
#endif
      }
      if (board[b].buttonUp(i)) {                          // note send OFF if button up
        noteOff(midi_channel, b * board_note_offset + midi_note_offset + buttonMap[(b > 0) ? 1 : 0][i - 1] - 1, midi_vel);
        MidiUSB.flush();
        delay(2);
      }
    }
  }
}

// handle if note received - only noteOn implemented!
//
void HandleNoteOn(byte channel, byte pitch, byte velocity)
{
  if ( (channel - receive_channel_offset) == midi_channel) {                              // check channel - offset by 0x8F !!?
    board_detect = 0;                                                                     // board parser
    for (int b = 0; b < (sizeof(board_addr) / sizeof(board_addr[0])); b++)
    {
      if ((pitch >= midi_note_offset + b * board_note_offset) && (pitch < midi_note_offset + num_buttons + b * board_note_offset))
      {
        board_detect = b + 1;

#ifdef SERIAL_DMP
        Serial << "\n" << "board NUM: " << board_detect << " from handler NoteOn: " << pitch << " vel: " << velocity;
#endif
        break;
      }
    }

    if ( board_detect > 0 ) {                                                             // check note and board OK
      LEDstateMap[board_detect - 1][pitch - midi_note_offset - (board_detect - 1)*board_note_offset] = velocity;             // received useful data for LED color, save
      updated_state = true;
    }
  }
}

// read incoming MIDI data
//
void readMIDI()
{
  // read incoming MIDI data
  midiEventPacket_t rx;

  do {
    rx = MidiUSB.read();
    if (rx.header != 0) {

      if (rx.header == 0x09) {                                // noteOn - trigger only if noteOn received
        HandleNoteOn(rx.byte1, rx.byte2, rx.byte3);
      }
      /*if (rx.header == 0x09) {                            // noteOn
        HandleNote(rx.byte1, rx.byte2, rx.byte3, true);
        }
        else if (rx.header == 0x08) {                       // noteOff
        HandleNote(rx.byte1, rx.byte2, rx.byte3, false);
        }*/
    }
  } while (rx.header != 0);
}

// handle button LED color
//
void parseLEDstateMAP() {
  if (updated_state) {
    for (int i = 0; i <= 3; i++) {
      for (int j = 0; j <= 7; j++) {
        switch (LEDstateMap[i][j]) {
          case 0: board[i].setButtonColor(buttonMapS[(i > 0) ? 1 : 0][j], 0); break;
          case 1: board[i].setButtonColor(buttonMapS[(i > 0) ? 1 : 0][j], 2); break;
          case 2: board[i].setButtonColor(buttonMapS[(i > 0) ? 1 : 0][j], 3); break;
          case 3: board[i].setButtonColor(buttonMapS[(i > 0) ? 1 : 0][j], 1); break;
          case 4: board[i].setButtonColor(buttonMapS[(i > 0) ? 1 : 0][j], 4); break;
          case 5: board[i].setButtonColor(buttonMapS[(i > 0) ? 1 : 0][j], 5); break;
          case 127: board[i].setButtonColor(buttonMapS[(i > 0) ? 1 : 0][j], 2); break;    // default red for 127 velocity
          default: break;
        }
      }
    }
    updated_state = false;
  }
}

// handle button LED blink
//
void ledBlinkCheck() {
  if (millis() >= time_now + period_blink) {
    time_now += period_blink;
    if (!ledON) {
      for (int i = 0; i <= 3; i++) {
        for (int j = 0; j <= 7; j++) {
          if (LEDstateMap[i][j] > 5 && LEDstateMap[i][j] <= 10)
          {
            switch (LEDstateMap[i][j]) {
              case 6: board[i].setButtonColor(buttonMapS[(i > 0) ? 1 : 0][j], 2); break;
              case 7: board[i].setButtonColor(buttonMapS[(i > 0) ? 1 : 0][j], 3); break;
              case 8: board[i].setButtonColor(buttonMapS[(i > 0) ? 1 : 0][j], 1); break;
              case 9: board[i].setButtonColor(buttonMapS[(i > 0) ? 1 : 0][j], 4); break;
              case 10: board[i].setButtonColor(buttonMapS[(i > 0) ? 1 : 0][j], 5); break;
              default: break;
            }
          }
        }
      }
      ledON = true;
    }
    else {
      for (int i = 0; i <= 3; i++) {
        for (int j = 0; j <= 7; j++) {
          if (LEDstateMap[i][j] > 5 && LEDstateMap[i][j] <= 10)
            board[i].setButtonColor(buttonMapS[(i > 0) ? 1 : 0][j], 0);
        }
      }
      ledON = false;
    }
  }
}

// handle potentiometer
//
void checkPot()
{
  potValue = map(analogRead(pinPot), 0, 1023, 127, 0);    // calculate MAP
  if ( abs(potValue - potValue_old) > 1 )                 // diff detected - send value (for flicker, else: potValue != potValue_old) or: abs(potValue - potValue_old) > 1
  {
    potValue_old = potValue;

    if ( potValue < 2 ) potValueSet = 0;                  // lower and upper bounds fix
    else if (potValue > 125) potValueSet = 127;           // MIDI must be min or max at ends !!
    else potValueSet = potValue;

    if (potValueSet_old != potValueSet)
    {
      potValueSet_old = potValueSet;
      controlChange(midi_channel, CC_ID, potValueSet);
      MidiUSB.flush();
      delay(2);
    }

#ifdef SERIAL_DMP
    Serial << "\n" << "POT: " << potValue << " raw: " << analogRead(pinPot);
#endif
  }
}
