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

// Arduino Uno R1-R3 Direct Port Manipulation
const byte PIN_CATHODE_0_A_DPM_BIT = 1 << 2;  // pin 2: PORTD bit 2
const byte PIN_CATHODE_0_B_DPM_BIT = 1 << 3;  // pin 3: PORTD bit 3
const byte PIN_CATHODE_0_C_DPM_BIT = 1 << 4;  // pin 4: PORTD bit 4
const byte PIN_CATHODE_0_D_DPM_BIT = 1 << 5;  // pin 5: PORTD bit 5
const byte PIN_CATHODE_1_A_DPM_BIT = 1 << 6;  // pin 6: PORTD bit 6
const byte PIN_CATHODE_1_B_DPM_BIT = 1 << 7;  // pin 7: PORTD bit 7
const byte PIN_CATHODE_1_C_DPM_BIT = 1 << 0;  // pin 8: PORTB bit 0
const byte PIN_CATHODE_1_D_DPM_BIT = 1 << 1;  // pin 9: PORTB bit 1
const byte PIN_ANODE_1_DPM_BIT = 1 << 2;  // pin 10: PORTB bit 2
const byte PIN_ANODE_2_DPM_BIT = 1 << 3;  // pin 11: PORTB bit 3
const byte PIN_ANODE_3_DPM_BIT = 1 << 4;  // pin 12: PORTB bit 4
const byte PIN_ANODE_4_DPM_BIT = 1 << 5;  // pin 13: PORTB bit 5

// button pins
const byte PIN_BUTTON_RIGHT = A0;
const byte PIN_BUTTON_LEFT = A1;
const byte PIN_BUTTON_RIGHT_LED = A2;
const byte PIN_BUTTON_LEFT_LED = A3;
const byte PIN_BUTTON_UTILITY = A4;
const byte PIN_BUTTON_GROUND = A5;

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

// controller values > 9 are unwired, and result in blank display
const byte BLANK = 15;

// ИH-12A tubes
const byte TUBE_DIGIT_ORDER[] = {3, 8, 9, 4, 0, 5, 7, 2, 6, 1};

// most other tubes
// const byte TUBE_DIGIT_ORDER[] = {6, 7, 5, 8, 4, 3, 9, 2, 0, 1};

#endif _CLOCK_HARDWARE_H
