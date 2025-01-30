/*
 * ============================
 *    Hardware Definitions
 * ============================
 */

#ifndef _CLOCK_HARDWARE_H
#define _CLOCK_HARDWARE_H

// ArduiNIX controller 0 К155ИД1 (or SN74141)
const int PIN_CATHODE_0_A = 2;
const int PIN_CATHODE_0_B = 3;
const int PIN_CATHODE_0_C = 4;
const int PIN_CATHODE_0_D = 5;

// ArduiNIX controller 1 К155ИД1 (or SN74141)
const int PIN_CATHODE_1_A = 6;
const int PIN_CATHODE_1_B = 7;
const int PIN_CATHODE_1_C = 8;
const int PIN_CATHODE_1_D = 9;

// ArduiNIX anode pins
const int PIN_ANODE_1 = 10;
const int PIN_ANODE_2 = 11;
const int PIN_ANODE_3 = 12;
const int PIN_ANODE_4 = 13;

// button pins
const int PIN_BUTTON_RIGHT = A0;
const int PIN_BUTTON_LEFT = A1;
const int PIN_BUTTON_RIGHT_LED = A2;
const int PIN_BUTTON_LEFT_LED = A3;
const int PIN_BUTTON_UTILITY = A4;
const int PIN_BUTTON_GROUND = A5;

// The clock is designed to use 6 tubes, each one wired to a unique combination
// of an anode pin (live power) and a cathode controller chip К155ИД1 (ground).
// 
// К155ИД1/SN74141 controllers are BCD-to-decimal, and are wired such that
// inputting 0-9 in binary will ground the associated Nixie tube cathode (and
// light the decimal, if the anode is also active).
const int TUBE_COUNT = 6;
const int DIGITS_PER_TUBE = 10;
const int TUBE_ANODES[TUBE_COUNT] = {1, 1, 2, 2, 3, 3};
const bool TUBE_CATHODE_CTRL_0[TUBE_COUNT] = {false, true, false, true, false, true};

// controller values > 9 are unwired, and result in blank display
const int BLANK = 15;

// ИH-12A tubes
const int TUBE_DIGIT_ORDER[] = {3, 8, 9, 4, 0, 5, 7, 2, 6, 1};

// most other tubes
// const int TUBE_DIGIT_ORDER[] = {6, 7, 5, 8, 4, 3, 9, 2, 0, 1};

#endif _CLOCK_HARDWARE_H
