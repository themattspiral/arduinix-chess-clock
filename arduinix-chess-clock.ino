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
const int JACKPOT_FULL_ROUNDS = 5;
const int JACKPOT_DURATION_MS = JACKPOT_STEP_DURATION_MS * DIGITS_PER_TUBE * JACKPOT_FULL_ROUNDS;
const unsigned long JACKPOT_MIN_ELAPSED_MS = 1000UL;
const unsigned long JACKPOT_MIN_REMAINING_MS = 61000UL;
const unsigned long MAX_DISPLAY_ELAPSED_MS = 359999900UL; // 99:59:59:900
const int TIMEOUT_BLINK_DURATION_MS = 500;
const int MENU_BLINK_DURATION_MS = 300;
const int BUTTON_DEBOUNCE_DELAY_MS = 20;
const int STATUS_UPDATE_INTERVAL_MS = 1000;
const int STATUS_UPDATE_MAX_CHARS = 30;

// ~120Hz / tube - tested on ИH-2 and ИH-12A tubes
// Given Xms per-tube cycle, 1000ms / (Xms/tube * 6tubes) = Hz
// 1.4ms ~= 120Hz
// 2.8mz ~= 60Hz

// const long MULTIPLEX_SINGLE_TUBE_LIT_DURATION_US = 750000L;
// const long MULTIPLEX_SINGLE_TUBE_OFF_DURATION_US = 500000L;
// const int MULTIPLEX_SINGLE_TUBE_LIT_DURATION_US = 2700;
// const int MULTIPLEX_SINGLE_TUBE_OFF_DURATION_US =  100;
const int MULTIPLEX_SINGLE_TUBE_LIT_DURATION_US = 1300;
const int MULTIPLEX_SINGLE_TUBE_OFF_DURATION_US =  100;

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
  
  blankTubes();

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

  printHardwareInfo();
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
    blankTubes();
    multiplexIsLit = false;
    lastDisplayRefreshTimestampUS = micros();
  } else if (!multiplexIsLit && micros() - lastDisplayRefreshTimestampUS >= MULTIPLEX_SINGLE_TUBE_OFF_DURATION_US) {
    if (lastDisplayRefreshTubeIndex == 0) {
      lastDisplayRefreshTubeIndex = TUBE_COUNT - 1;
    } else {
      lastDisplayRefreshTubeIndex--;
    }

    displayOnTube(lastDisplayRefreshTubeIndex, multiplexDisplayValues[lastDisplayRefreshTubeIndex]);
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
  if (sec == 0 && jackpotOn == false && elapsedMS > JACKPOT_MIN_ELAPSED_MS && (displayElapsed || remainingMS > JACKPOT_MIN_REMAINING_MS)) {
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
    remainingMS = 0UL;

    // reset elapsed to 0 when we can't display any higher numbers
    if (elapsedMS >= MAX_DISPLAY_ELAPSED_MS) {
      turnStartTimestampMS = loopNow;
    }
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
  ButtonValues vals = readButtonValues();

  // handle press state change (set debounce timer)
  if (vals.left != leftButtonLastVal) {
    leftButtonLastDebounceMS = loopNow;
  }
  if (vals.right != rightButtonLastVal) {
    rightButtonLastDebounceMS = loopNow;
  }
  if (vals.utility != utilityButtonLastVal) {
    utilityButtonLastDebounceMS = loopNow;
  }

  // check left button debounce time exceeded with un-flickering, changed value
  unsigned long leftDebounceDiff = loopNow - leftButtonLastDebounceMS;
  if (leftDebounceDiff > BUTTON_DEBOUNCE_DELAY_MS && vals.left != leftButtonVal) {
    leftButtonVal = vals.left;

    // using internal pull-up resistor means a pressed button goes LOW
    if (leftButtonVal == LOW) {
      handleLeftButtonPress(loopNow);
    }
  }

  // check right button debounce time exceeded with un-flickering, changed value
  unsigned long rightDebounceDiff = loopNow - rightButtonLastDebounceMS;
  if (rightDebounceDiff > BUTTON_DEBOUNCE_DELAY_MS && vals.right != rightButtonVal) {
    rightButtonVal = vals.right;

    // using internal pull-up resistor means a pressed button goes LOW
    if (rightButtonVal == LOW) {
      handleRightButtonPress(loopNow);
    }
  }

  // check utility button debounce time exceeded with un-flickering, changed value
  unsigned long utilityDebounceDiff = loopNow - utilityButtonLastDebounceMS;
  if (utilityDebounceDiff > BUTTON_DEBOUNCE_DELAY_MS && vals.utility != utilityButtonVal) {
    utilityButtonVal = vals.utility;

    // using internal pull-up resistor means a pressed button goes LOW
    if (utilityButtonVal == LOW) {
      handleUtilityButtonPress(loopNow);
    }
  }

  // store reading as last value to compare to in next loop
  leftButtonLastVal = vals.left;
  rightButtonLastVal = vals.right;
  utilityButtonLastVal = vals.utility;
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

  #if defined(ARDUINO_AVR_UNO)
    loopSendStatusUpdate(now, cv.elapsedMS, cv.remainingMS);
  #endif
}
