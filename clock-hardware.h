/*
 * ============================
 *    Hardware Definitions
 * ============================
 */

#ifndef _CLOCK_HARDWARE_H
#define _CLOCK_HARDWARE_H

// ArduiNIX controller 0 (SN74141/K155ID1)
const int PIN_CATHODE_0_A = 2;
const int PIN_CATHODE_0_B = 3;
const int PIN_CATHODE_0_C = 4;
const int PIN_CATHODE_0_D = 5;

// ArduiNIX controller 1 (SN74141/K155ID1)
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

// 6 tubes, each wired to a unique combination of anode pin and cathode controller
const int TUBE_COUNT = 6;
const int TUBE_ANODES[TUBE_COUNT] = {1, 1, 2, 2, 3, 3};
const bool TUBE_CATHODE_CTRL_0[TUBE_COUNT] = {false, true, false, true, false, true};

// SN74141/K155ID1 controllers are BCD-to-decimal, and are wired such that
// giving them 0-9 in BCD will represent themselves as Nixie tube decimals, 
// and anything else is unconnected (therefore blank)
const int BLANK = 15;

#endif _CLOCK_HARDWARE_H
