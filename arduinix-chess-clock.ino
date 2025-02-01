#include <stdio.h>

#include "clock-hardware.h"
#include "clock-data-types.h"

/*
 * ===============================
 *  Behavior Constants
 * ===============================
 */
const long SERIAL_SPEED_BAUD = 115200L;
const int JACKPOT_STEP_DURATION_MS = 50;
const int JACKPOT_DURATION_MS = JACKPOT_STEP_DURATION_MS * DIGITS_PER_TUBE * 3;
const unsigned long JACKPOT_MIN_ELAPSED_MS = 1000UL;
const unsigned long JACKPOT_MIN_REMAINING_MS = 61000UL;
const int TIMEOUT_BLINK_DURATION_MS = 500;
const int MENU_BLINK_DURATION_MS = 300;
const int BUTTON_DEBOUNCE_DELAY_MS = 20;
const int STATUS_UPDATE_INTERVAL_MS = 1000;
const int STATUS_UPDATE_MAX_CHARS = 30;

// TODO - test for ideal values on ИH-2 and ИH-12A tubes
//      - calculate corresponding Hz
const int MULTIPLEX_SINGLE_TUBE_LIT_DURATION_US = 800;
const int MULTIPLEX_SINGLE_TUBE_OFF_DURATION_US = 300;

/*
 * ===============================
 *  Runtime State
 * ===============================
 */

// button state 🔘🔘
byte rightButtonLastVal = HIGH;
byte leftButtonLastVal = HIGH;
byte utilityButtonLastVal = HIGH;
byte rightButtonVal = HIGH;
byte leftButtonVal = HIGH;
byte utilityButtonVal = HIGH;
unsigned long rightButtonLastDebounceMS = 0UL;
unsigned long leftButtonLastDebounceMS = 0UL;
unsigned long utilityButtonLastDebounceMS = 0UL;

// nixie tube state 🚥🚥
byte multiplexDisplayValues[TUBE_COUNT] = {BLANK, BLANK, BLANK, BLANK, BLANK, BLANK};
byte lastDisplayRefreshTubeIndex = TUBE_COUNT;
unsigned long lastDisplayRefreshTimestampUS = 0UL;
bool multiplexIsLit = false;

// chess clock state ♟⏲⏲♟
ClockState currentClockState = IDLE;
unsigned long lastStatusUpdateTimestampMS = 0UL;
unsigned long lastJackpotTimestampMS = 0UL;
unsigned long lastEventStepTimestampMS = 0UL;
unsigned long turnStartTimestampMS = 0UL;
bool leftPlayersTurn = false;
bool blinkOn = false;
bool jackpotOn = false;
bool jackpotDirectionFTB = true;
byte jackpotDigitOrderIndexValues[TUBE_COUNT] = {0, 0, 0, 0, 0, 0};
char statusUpdate[STATUS_UPDATE_MAX_CHARS] = "";
byte currentTurnTimerOption = 2;

/*
 * ===============================
 *  Initial Setup (Power On)
 * ===============================
 */
void setup() 
{
  pinMode(PIN_ANODE_1, OUTPUT);
  pinMode(PIN_ANODE_2, OUTPUT);
  pinMode(PIN_ANODE_3, OUTPUT);
  pinMode(PIN_ANODE_4, OUTPUT);
  pinMode(PIN_CATHODE_0_A, OUTPUT);
  pinMode(PIN_CATHODE_0_B, OUTPUT);
  pinMode(PIN_CATHODE_0_C, OUTPUT);
  pinMode(PIN_CATHODE_0_D, OUTPUT);
  pinMode(PIN_CATHODE_1_A, OUTPUT);
  pinMode(PIN_CATHODE_1_B, OUTPUT);
  pinMode(PIN_CATHODE_1_C, OUTPUT);
  pinMode(PIN_CATHODE_1_D, OUTPUT);
  
  displayOnTubeExclusive(0, BLANK);

  // use analog inputs as digital inputs for buttons
  pinMode(PIN_BUTTON_RIGHT, INPUT_PULLUP);
  pinMode(PIN_BUTTON_LEFT, INPUT_PULLUP);
  pinMode(PIN_BUTTON_UTILITY, INPUT_PULLUP);

  // use analog inputs as digital outputs for button LEDs
  pinMode(PIN_BUTTON_RIGHT_LED, OUTPUT);
  pinMode(PIN_BUTTON_LEFT_LED, OUTPUT);

  // ground for button assembly
  pinMode(PIN_BUTTON_GROUND, OUTPUT);
  digitalWrite(PIN_BUTTON_GROUND, LOW);

  setButtonLEDs(false, false);

  Serial.begin(SERIAL_SPEED_BAUD);
}

/*
 * ===============================
 *  Internal Functions - Hardware
 * ===============================
 */

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
  if ((c1Val & BIT_2) == 0) {
    portBLowBitmask |= PIN_CATHODE_1_C_DPM_BIT;
  } else {
    portBHighBitmask |= PIN_CATHODE_1_C_DPM_BIT;
  }
  if ((c1Val & BIT_3) == 0) {
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
  if ((c0Val & BIT_0) == 0) {
    portDLowBitmask |= PIN_CATHODE_0_A_DPM_BIT;
  } else {
    portDHighBitmask |= PIN_CATHODE_0_A_DPM_BIT;
  }
  if ((c0Val & BIT_1) == 0) {
    portDLowBitmask |= PIN_CATHODE_0_B_DPM_BIT;
  } else {
    portDHighBitmask |= PIN_CATHODE_0_B_DPM_BIT;
  }
  if ((c0Val & BIT_2) == 0) {
    portDLowBitmask |= PIN_CATHODE_0_C_DPM_BIT;
  } else {
    portDHighBitmask |= PIN_CATHODE_0_C_DPM_BIT;
  }
  if ((c0Val & BIT_3) == 0) {
    portDLowBitmask |= PIN_CATHODE_0_D_DPM_BIT;
  } else {
    portDHighBitmask |= PIN_CATHODE_0_D_DPM_BIT;
  }
  if ((c1Val & BIT_0) == 0) {
    portDLowBitmask |= PIN_CATHODE_1_A_DPM_BIT;
  } else {
    portDHighBitmask |= PIN_CATHODE_1_A_DPM_BIT;
  }
  if ((c1Val & BIT_1) == 0) {
    portDLowBitmask |= PIN_CATHODE_1_B_DPM_BIT;
  } else {
    portDHighBitmask |= PIN_CATHODE_1_B_DPM_BIT;
  }

  // LOW mask PORTB first to power OFF Anodes FIRST
  PORTB &= ~portBLowBitmask;
  PORTD &= ~portDLowBitmask;

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

/*
 * ===============================
 *  Internal Functions - Logical
 * ===============================
 */

inline void setMultiplexDisplay(byte t0, byte t1, byte t2, byte t3, byte t4, byte t5) {
  multiplexDisplayValues[0] = t0;
  multiplexDisplayValues[1] = t1;
  multiplexDisplayValues[2] = t2;
  multiplexDisplayValues[3] = t3;
  multiplexDisplayValues[4] = t4;
  multiplexDisplayValues[5] = t5;
}

inline void loopMultiplex() {
  if (multiplexIsLit && micros() - lastDisplayRefreshTimestampUS >= MULTIPLEX_SINGLE_TUBE_LIT_DURATION_US) {
    displayOnTubeExclusive(lastDisplayRefreshTubeIndex, BLANK);
    multiplexIsLit = false;
    lastDisplayRefreshTimestampUS = micros();
  } else if (!multiplexIsLit && micros() - lastDisplayRefreshTimestampUS >= MULTIPLEX_SINGLE_TUBE_OFF_DURATION_US) {
    if (lastDisplayRefreshTubeIndex == 0) {
      lastDisplayRefreshTubeIndex = TUBE_COUNT - 1;
    } else {
      lastDisplayRefreshTubeIndex--;
    }

    displayOnTubeExclusive(lastDisplayRefreshTubeIndex, multiplexDisplayValues[lastDisplayRefreshTubeIndex]);
    multiplexIsLit = true;
    lastDisplayRefreshTimestampUS = micros();
  }
}

inline void handleJackpot(unsigned long loopNow) {
  setMultiplexDisplay(
    TUBE_DIGIT_ORDER[jackpotDigitOrderIndexValues[0]],
    TUBE_DIGIT_ORDER[jackpotDigitOrderIndexValues[1]],
    TUBE_DIGIT_ORDER[jackpotDigitOrderIndexValues[2]],
    TUBE_DIGIT_ORDER[jackpotDigitOrderIndexValues[3]],
    TUBE_DIGIT_ORDER[jackpotDigitOrderIndexValues[4]],
    TUBE_DIGIT_ORDER[jackpotDigitOrderIndexValues[5]]
  );
  
  if (loopNow - lastJackpotTimestampMS > JACKPOT_DURATION_MS) {
    // jackpot over
    jackpotOn = false;
  } else if (loopNow - lastEventStepTimestampMS > JACKPOT_STEP_DURATION_MS) {
    // advance to next jackpot step
    for (byte i = 0; i < TUBE_COUNT; i++) {
      if (jackpotDigitOrderIndexValues[i] == 9) {
        jackpotDigitOrderIndexValues[i] = 0;
      } else {
        jackpotDigitOrderIndexValues[i]++;
      }
    }

    lastEventStepTimestampMS = loopNow;
  }
}

inline void setMultiplexClockTime(unsigned long elapsedMS, unsigned long remainingMS, unsigned long loopNow, bool displayElapsed) {
  unsigned long totalMS = displayElapsed ? elapsedMS : remainingMS;
  unsigned long totalSec = totalMS / 1000;
  int hours = totalSec / 3600;
  int hoursRemainder = totalSec % 3600;
  int min = hoursRemainder / 60;
  int sec = hoursRemainder % 60;

  // activate jackpot scroll on every whole minute (excluding when clock starts 
  // and times out), in an effort to preserve tubes / prevent uneven burn
  // if (sec == 0 && jackpotOn == false && elapsedMS > JACKPOT_MIN_ELAPSED_MS && remainingMS > JACKPOT_MIN_REMAINING_MS) {
  if ((sec % 10 == 0) && jackpotOn == false && elapsedMS > JACKPOT_MIN_ELAPSED_MS && remainingMS > JACKPOT_MIN_REMAINING_MS) {
    jackpotOn = true;
    lastJackpotTimestampMS = loopNow;
    
    for (byte i = 0; i < TUBE_COUNT; i++) {
      jackpotDigitOrderIndexValues[i] = 9;
    }
  }

  if (jackpotOn) {
    handleJackpot(loopNow);
  } else if (hours > 0) {
    setMultiplexDisplay(hours / 10, hours % 10, min / 10, min % 10, sec / 10, sec % 10);
  } else if (min > 0) {
    if (leftPlayersTurn) {
      setMultiplexDisplay(min / 10, min % 10, sec / 10, sec % 10, BLANK, BLANK);
    } else {
      setMultiplexDisplay(BLANK, BLANK, min / 10, min % 10, sec / 10, sec % 10);
    }
  } else {
    int fractionalSec = (totalMS % 1000) / 10;

    if (leftPlayersTurn) {
      setMultiplexDisplay(sec / 10, sec % 10, fractionalSec / 10, fractionalSec % 10, BLANK, BLANK);
    } else {
      setMultiplexDisplay(BLANK, BLANK, sec / 10, sec % 10, fractionalSec / 10, fractionalSec % 10);
    }
  }
}

inline CountdownValues loopCountdown(unsigned long loopNow) {
  unsigned long timeoutLimit = TURN_TIMER_OPTIONS[currentTurnTimerOption].turnLimitMS;
  unsigned long elapsedMS = loopNow - turnStartTimestampMS;
  unsigned long remainingMS = timeoutLimit - elapsedMS;

  setButtonLEDs(leftPlayersTurn, !leftPlayersTurn);

  if (timeoutLimit == 0UL) {
    // special value 0: show elapsed time & never timeout
    setMultiplexClockTime(elapsedMS, remainingMS, loopNow, true);
  } else if (elapsedMS >= timeoutLimit) {
    // countdown expired: change state
    remainingMS = 0UL;
    currentClockState = TIMEOUT;
  } else {
    // countdown running: show remaining time
    setMultiplexClockTime(elapsedMS, remainingMS, loopNow, false);
  }

  return { elapsedMS, remainingMS };
}

inline void loopTimeout(unsigned long loopNow) {
  if (loopNow - lastEventStepTimestampMS > TIMEOUT_BLINK_DURATION_MS) {
    lastEventStepTimestampMS = loopNow;
    blinkOn = !blinkOn;
  }

  if (blinkOn) {
    setButtonLEDs(true, true);

    if (leftPlayersTurn) {
      setMultiplexDisplay(0, 0, 0, 0, BLANK, BLANK);
    } else {
      setMultiplexDisplay(BLANK, BLANK, 0, 0, 0, 0);
    }
  } else {
    setButtonLEDs(!leftPlayersTurn, leftPlayersTurn);
    setMultiplexDisplay(BLANK, BLANK, BLANK, BLANK, BLANK, BLANK);
  }
}

inline void loopMenu(unsigned long loopNow) {
  if (loopNow - lastEventStepTimestampMS > MENU_BLINK_DURATION_MS) {
    lastEventStepTimestampMS = loopNow;
    blinkOn = !blinkOn;
  }

  if (blinkOn) {
    setButtonLEDs(true, true);
    setMultiplexDisplay(
      TURN_TIMER_OPTIONS[currentTurnTimerOption].displayValues[0],
      TURN_TIMER_OPTIONS[currentTurnTimerOption].displayValues[1],
      TURN_TIMER_OPTIONS[currentTurnTimerOption].displayValues[2],
      TURN_TIMER_OPTIONS[currentTurnTimerOption].displayValues[3],
      TURN_TIMER_OPTIONS[currentTurnTimerOption].displayValues[4],
      TURN_TIMER_OPTIONS[currentTurnTimerOption].displayValues[5]
    );
  } else {
    setButtonLEDs(false, false);
    setMultiplexDisplay(BLANK, BLANK, BLANK, BLANK, BLANK, BLANK);
  }
}

inline void loopIdle() {
  setButtonLEDs(false, false);
  setMultiplexDisplay(BLANK, BLANK, BLANK, BLANK, BLANK, BLANK);
}

inline void loopSendStatusUpdate(unsigned long loopNow, unsigned long elapsedMs, unsigned long remainingMs) {
  if (loopNow - lastStatusUpdateTimestampMS > STATUS_UPDATE_INTERVAL_MS) {
    snprintf(statusUpdate, STATUS_UPDATE_MAX_CHARS, "%d,%s,%d,%lu,%lu",
      currentClockState,
      TURN_TIMER_OPTIONS[currentTurnTimerOption].label,
      leftPlayersTurn,
      elapsedMs,
      remainingMs
    );
    Serial.println(statusUpdate);
    lastStatusUpdateTimestampMS = millis();
  }
}

inline void handleRightButtonPress(unsigned long loopNow) {
  if (currentClockState == RUNNING && !leftPlayersTurn) {
    leftPlayersTurn = !leftPlayersTurn;
    turnStartTimestampMS = loopNow;
  } else if (currentClockState == MENU) {
    if (currentTurnTimerOption == TURN_TIMER_OPTIONS_COUNT - 1) {
      currentTurnTimerOption = 0;
    } else {
      currentTurnTimerOption++;
    }
  } else if (currentClockState == IDLE || currentClockState == DEMO) {
    leftPlayersTurn = false;
    turnStartTimestampMS = loopNow;
    currentClockState = RUNNING;
  } else if (currentClockState == TIMEOUT) {
    currentClockState = IDLE;
  }
}

inline void handleLeftButtonPress(unsigned long loopNow) {
  if (currentClockState == RUNNING && leftPlayersTurn) {
    leftPlayersTurn = !leftPlayersTurn;
    turnStartTimestampMS = loopNow;
  } else if (currentClockState == MENU) {
    if (currentTurnTimerOption == 0) {
      currentTurnTimerOption = TURN_TIMER_OPTIONS_COUNT - 1;
    } else {
      currentTurnTimerOption--;
    }
  } else if (currentClockState == IDLE || currentClockState == DEMO) {
    leftPlayersTurn = true;
    turnStartTimestampMS = loopNow;
    currentClockState = RUNNING;
  } else if (currentClockState == TIMEOUT) {
    currentClockState = IDLE;
  }
}

inline void handleUtilityButtonPress(unsigned long loopNow) {
  if (currentClockState == IDLE) {
    currentClockState = MENU;
  } else if (currentClockState == DEMO || currentClockState == MENU || currentClockState == TIMEOUT || currentClockState == RUNNING) {
    currentClockState = IDLE;
    turnStartTimestampMS = 0;
  }
}

inline void loopCheckButtons(unsigned long loopNow) {
  byte rightButtonReading = PINC & PIN_BUTTON_RIGHT_DPM_BIT;
  byte leftButtonReading = PINC & PIN_BUTTON_LEFT_DPM_BIT;
  byte utilityButtonReading = PINC & PIN_BUTTON_UTILITY_DPM_BIT;

  // handle press state change (set debounce timer)
  if (rightButtonReading != rightButtonLastVal) {
    rightButtonLastDebounceMS = loopNow;
  }
  if (leftButtonReading != leftButtonLastVal) {
    leftButtonLastDebounceMS = loopNow;
  }
  if (utilityButtonReading != utilityButtonLastVal) {
    utilityButtonLastDebounceMS = loopNow;
  }

  // check right button debounce time exceeded with un-flickering, changed value
  unsigned long rightDebounceDiff = loopNow - rightButtonLastDebounceMS;
  if (rightDebounceDiff > BUTTON_DEBOUNCE_DELAY_MS && rightButtonReading != rightButtonVal) {
    rightButtonVal = rightButtonReading;

    // using internal pull-up resistor means a pressed button goes LOW
    if (rightButtonVal == LOW) {
      handleRightButtonPress(loopNow);
    }
  }

  // check left button debounce time exceeded with un-flickering, changed value
  unsigned long leftDebounceDiff = loopNow - leftButtonLastDebounceMS;
  if (leftDebounceDiff > BUTTON_DEBOUNCE_DELAY_MS && leftButtonReading != leftButtonVal) {
    leftButtonVal = leftButtonReading;

    // using internal pull-up resistor means a pressed button goes LOW
    if (leftButtonVal == LOW) {
      handleLeftButtonPress(loopNow);
    }
  }

  // check utility button debounce time exceeded with un-flickering, changed value
  unsigned long utilityDebounceDiff = loopNow - utilityButtonLastDebounceMS;
  if (utilityDebounceDiff > BUTTON_DEBOUNCE_DELAY_MS && utilityButtonReading != utilityButtonVal) {
    utilityButtonVal = utilityButtonReading;

    // using internal pull-up resistor means a pressed button goes LOW
    if (utilityButtonVal == LOW) {
      handleUtilityButtonPress(loopNow);
    }
  }

  // store reading as last value to compare to in next loop
  rightButtonLastVal = rightButtonReading;
  leftButtonLastVal = leftButtonReading;
  utilityButtonLastVal = utilityButtonReading;
}

/*
 * ============================
 *  Main Loop (Continuous)
 * ============================
 */
void loop() {
  unsigned long now = millis();

  // check for button presses and change state if needed
  loopCheckButtons(now);

  CountdownValues cv = { 0UL, 0UL };

  // update values to display in multiplexDisplayValues[] based on current clock state
  if (currentClockState == RUNNING) {
    cv = loopCountdown(now);
  } else if (currentClockState == TIMEOUT) {
    cv = { TURN_TIMER_OPTIONS[currentTurnTimerOption].turnLimitMS, 0UL };
    loopTimeout(now);
  } else if (currentClockState == MENU) {
    loopMenu(now);
  } else if (currentClockState == DEMO) {
    // loopDemoCount(now);
  } else if (currentClockState == IDLE) {
    loopIdle();
  }

  // display current values in multiplexDisplayValues[]
  loopMultiplex();

  loopSendStatusUpdate(now, cv.elapsedMS, cv.remainingMS);
}
