/*
 * ============================
 *    Hardware Definitions
 * ============================
 */

#ifndef _CLOCK_HARDWARE_H
#define _CLOCK_HARDWARE_H

// ArduiNIX controller 0 К155ИД1 (or SN74141)
const byte PIN_CATHODE_0_A = 2;
const byte PIN_CATHODE_0_B = 3;
const byte PIN_CATHODE_0_C = 4;
const byte PIN_CATHODE_0_D = 5;

// ArduiNIX controller 1 К155ИД1 (or SN74141)
const byte PIN_CATHODE_1_A = 6;
const byte PIN_CATHODE_1_B = 7;
const byte PIN_CATHODE_1_C = 8;
const byte PIN_CATHODE_1_D = 9;

// ArduiNIX anode pins
const byte PIN_ANODE_1 = 10;
const byte PIN_ANODE_2 = 11;
const byte PIN_ANODE_3 = 12;
const byte PIN_ANODE_4 = 13;

// button pins
const byte PIN_BUTTON_RIGHT = A0;
const byte PIN_BUTTON_LEFT = A1;
const byte PIN_BUTTON_RIGHT_LED = A2;
const byte PIN_BUTTON_LEFT_LED = A3;
const byte PIN_BUTTON_UTILITY = A4;
const byte PIN_BUTTON_GROUND = A5;

// Arduino Uno R1-R3 Direct Port Manipulation
const byte PIN_CATHODE_0_A_DPM_BIT = 1 << 2;        // pin 2: PORTD bit 2
const byte PIN_CATHODE_0_B_DPM_BIT = 1 << 3;        // pin 3: PORTD bit 3
const byte PIN_CATHODE_0_C_DPM_BIT = 1 << 4;        // pin 4: PORTD bit 4
const byte PIN_CATHODE_0_D_DPM_BIT = 1 << 5;        // pin 5: PORTD bit 5
const byte PIN_CATHODE_1_A_DPM_BIT = 1 << 6;        // pin 6: PORTD bit 6
const byte PIN_CATHODE_1_B_DPM_BIT = 1 << 7;        // pin 7: PORTD bit 7
const byte PIN_CATHODE_1_C_DPM_BIT = 1 << 0;        // pin 8: PORTB bit 0
const byte PIN_CATHODE_1_D_DPM_BIT = 1 << 1;        // pin 9: PORTB bit 1
const byte PIN_ANODE_1_DPM_BIT = 1 << 2;            // pin 10: PORTB bit 2
const byte PIN_ANODE_2_DPM_BIT = 1 << 3;            // pin 11: PORTB bit 3
const byte PIN_ANODE_3_DPM_BIT = 1 << 4;            // pin 12: PORTB bit 4
const byte PIN_ANODE_4_DPM_BIT = 1 << 5;            // pin 13: PORTB bit 5
const byte PIN_BUTTON_RIGHT_DPM_BIT = 1 << 0;       // pin A0 (14): PORTC bit 0
const byte PIN_BUTTON_LEFT_DPM_BIT = 1 << 1;        // pin A1 (15): PORTC bit 1
const byte PIN_BUTTON_RIGHT_LED_DPM_BIT = 1 << 2;   // pin A2 (16): PORTC bit 2
const byte PIN_BUTTON_LEFT_LED_DPM_BIT = 1 << 3;    // pin A3 (17): PORTC bit 3
const byte PIN_BUTTON_UTILITY_DPM_BIT = 1 << 4;     // pin A4 (18): PORTC bit 4
const byte PIN_BUTTON_GROUND_DPM_BIT = 1 << 5;      // pin A5 (19): PORTC bit 5

// The clock is designed to use 6 tubes, each one wired to a unique combination
// of an anode pin (live power) and a cathode controller chip К155ИД1 (ground).
// 
// К155ИД1/SN74141 controllers are BCD-to-decimal, and are wired such that
// inputting 0-9 in binary will ground the associated Nixie tube cathode (and
// light the decimal, if the anode is also active).
const byte TUBE_COUNT = 6;
const byte DIGITS_PER_TUBE = 10;
const byte TUBE_ANODES[TUBE_COUNT] = {1, 1, 2, 2, 3, 3};
const bool TUBE_CATHODE_CTRL_0[TUBE_COUNT] = {false, true, false, true, false, true};

// controller values > 9 result in blank display
const byte BLANK = 15;

// ИH-12A tubes
const byte TUBE_DIGIT_ORDER[] = {3, 8, 9, 4, 0, 5, 7, 2, 6, 1};

// most other tubes
// const byte TUBE_DIGIT_ORDER[] = {6, 7, 5, 8, 4, 3, 9, 2, 0, 1};

// bit position to BCD input constants
const byte BIT_0_BCD_PIN_A = 1 << 0;
const byte BIT_1_BCD_PIN_B = 1 << 1;
const byte BIT_2_BCD_PIN_C = 1 << 2;
const byte BIT_3_BCD_PIN_D = 1 << 3;

// high-voltage stabilization delay between anodes off and on
const int HV_STABILIZATION_DELAY_US = 5;

inline void displayOnTubeExclusive(byte tubeIndex, byte displayVal) {
  byte anode = TUBE_ANODES[tubeIndex];
  bool cathodeCtrl0 = TUBE_CATHODE_CTRL_0[tubeIndex];
  
  byte c0Val, c1Val;

  // blank the non-active cathode so that the anode for the specified tube
  // won't light both of the tubes that it's connected to when activated
  if (cathodeCtrl0) {
    c0Val = displayVal;
    c1Val = BLANK;
  } else {
    c0Val = BLANK;
    c1Val = displayVal;
  }

  // bitmasks to be used when setting PORTB and PORTD hardware registers,
  // aka Direct Port Manipulation
  byte portBHighBitmask, portBLowBitmask, portDHighBitmask, portDLowBitmask;
  portBHighBitmask = portBLowBitmask = portDHighBitmask = portDLowBitmask = 0;

  // Build PORTB low and high bitmasks for digital pins 8-13,
  // which control the high half of cathode 1 and all 4 anodes
  if ((c1Val & BIT_2_BCD_PIN_C) == 0) {
    portBLowBitmask |= PIN_CATHODE_1_C_DPM_BIT;
  } else {
    portBHighBitmask |= PIN_CATHODE_1_C_DPM_BIT;
  }
  if ((c1Val & BIT_3_BCD_PIN_D) == 0) {
    portBLowBitmask |= PIN_CATHODE_1_D_DPM_BIT;
  } else {
    portBHighBitmask |= PIN_CATHODE_1_D_DPM_BIT;
  }

  if (displayVal == BLANK) {
    portBLowBitmask |= PIN_ANODE_1_DPM_BIT | PIN_ANODE_2_DPM_BIT | PIN_ANODE_3_DPM_BIT;
  } else {
    switch(anode) {
      case 1:
        portBLowBitmask |= PIN_ANODE_2_DPM_BIT | PIN_ANODE_3_DPM_BIT;
        portBHighBitmask |= PIN_ANODE_1_DPM_BIT;
        break;
      case 2:
        portBLowBitmask |= PIN_ANODE_1_DPM_BIT | PIN_ANODE_3_DPM_BIT;
        portBHighBitmask |= PIN_ANODE_2_DPM_BIT;
        break;
      case 3:
        portBLowBitmask |= PIN_ANODE_1_DPM_BIT | PIN_ANODE_2_DPM_BIT;
        portBHighBitmask |= PIN_ANODE_3_DPM_BIT;
        break;
    } 
  }

  // Build PORTD bitmasks for digital pins 0-7,
  // which control cathode 0 and the low half of cathode 1
  if ((c0Val & BIT_0_BCD_PIN_A) == 0) {
    portDLowBitmask |= PIN_CATHODE_0_A_DPM_BIT;
  } else {
    portDHighBitmask |= PIN_CATHODE_0_A_DPM_BIT;
  }
  if ((c0Val & BIT_1_BCD_PIN_B) == 0) {
    portDLowBitmask |= PIN_CATHODE_0_B_DPM_BIT;
  } else {
    portDHighBitmask |= PIN_CATHODE_0_B_DPM_BIT;
  }
  if ((c0Val & BIT_2_BCD_PIN_C) == 0) {
    portDLowBitmask |= PIN_CATHODE_0_C_DPM_BIT;
  } else {
    portDHighBitmask |= PIN_CATHODE_0_C_DPM_BIT;
  }
  if ((c0Val & BIT_3_BCD_PIN_D) == 0) {
    portDLowBitmask |= PIN_CATHODE_0_D_DPM_BIT;
  } else {
    portDHighBitmask |= PIN_CATHODE_0_D_DPM_BIT;
  }
  if ((c1Val & BIT_0_BCD_PIN_A) == 0) {
    portDLowBitmask |= PIN_CATHODE_1_A_DPM_BIT;
  } else {
    portDHighBitmask |= PIN_CATHODE_1_A_DPM_BIT;
  }
  if ((c1Val & BIT_1_BCD_PIN_B) == 0) {
    portDLowBitmask |= PIN_CATHODE_1_B_DPM_BIT;
  } else {
    portDHighBitmask |= PIN_CATHODE_1_B_DPM_BIT;
  }

  // LOW mask PORTB first to power OFF Anodes FIRST
  PORTB &= ~portBLowBitmask;
  PORTD &= ~portDLowBitmask;
  delayMicroseconds(HV_STABILIZATION_DELAY_US);

  // HIGH mask PORTD first to power ON Anodes LAST
  PORTD |= portDHighBitmask;
  PORTB |= portBHighBitmask;
}

inline void setButtonLEDs(bool leftOn, bool rightOn) {
  if (leftOn) {
    PORTC |= PIN_BUTTON_LEFT_LED_DPM_BIT;
  } else {
    PORTC &= ~PIN_BUTTON_LEFT_LED_DPM_BIT;
  }

  if (rightOn) {
    PORTC |= PIN_BUTTON_RIGHT_LED_DPM_BIT;
  } else {
    PORTC &= ~PIN_BUTTON_RIGHT_LED_DPM_BIT;
  }
}

#endif _CLOCK_HARDWARE_H
