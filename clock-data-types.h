/*
 * ============================
 *    Data Types
 * ============================
 */

#include "clock-hardware.h"

#ifndef _CLOCK_DATA_TYPES_H
#define _CLOCK_DATA_TYPES_H

enum ClockState { IDLE, RUNNING, TIMEOUT, MENU, DEMO };

typedef struct {
  byte displayValues[TUBE_COUNT];
  unsigned long turnLimitMS;
  char label[4];
} TurnTimerOption;

typedef struct {
  unsigned long elapsedMS;
  unsigned long remainingMS;
} CountdownValues;

const byte TURN_TIMER_OPTIONS_COUNT = 13;
const TurnTimerOption TURN_TIMER_OPTIONS[TURN_TIMER_OPTIONS_COUNT] = {
  { { 7, 2, BLANK, BLANK, BLANK, BLANK }, 259201000UL, "72h" },
  { { 4, 8, BLANK, BLANK, BLANK, BLANK }, 172801000UL, "48h" },
  { { 2, 4, BLANK, BLANK, BLANK, BLANK }, 86401000UL, "24h" },
  { { 0, 2, BLANK, BLANK, BLANK, BLANK }, 7201000UL, "2h" },
  { { 0, 1, BLANK, BLANK, BLANK, BLANK }, 3601000UL, "1h" },
  { { BLANK, BLANK, 3, 0, BLANK, BLANK }, 1800000UL, "30m" },
  { { BLANK, BLANK, 1, 5, BLANK, BLANK }, 900000UL, "15m" },
  { { BLANK, BLANK, 1, 0, BLANK, BLANK }, 600000UL, "10m" },
  { { BLANK, BLANK, 0, 5, BLANK, BLANK }, 300000UL, "5m" },
  { { BLANK, BLANK, 0, 3, BLANK, BLANK }, 180000UL, "3m" },
  { { BLANK, BLANK, 0, 1, BLANK, BLANK }, 60000UL, "1m" },
  { { BLANK, BLANK, BLANK, BLANK, 1, 0 }, 10000UL, "10s" },
  { { BLANK, BLANK, BLANK, BLANK, BLANK, 0 }, 0UL, "n0L" }
};

#endif _CLOCK_DATA_TYPES_H
