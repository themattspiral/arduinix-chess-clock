#include <stdio.h>

#include "clock-hardware.h"
#include "clock-data-types.h"
#include "multimap.h"

/*
 * ===============================
 *  Behavior Constants
 * ===============================
 */
const int JACKPOT_STEP_DURATION_MS = 50;
const int JACKPOT_FULL_ROUNDS = 5;
const int JACKPOT_DURATION_MS = JACKPOT_STEP_DURATION_MS * DIGITS_PER_TUBE * JACKPOT_FULL_ROUNDS;
const unsigned long JACKPOT_MIN_ELAPSED_MS = 1000UL;
const unsigned long JACKPOT_MIN_REMAINING_MS = 61000UL;
const unsigned long MAX_DISPLAY_ELAPSED_MS = 359999900UL; // 99:59:59:900
const int TIMEOUT_BLINK_DURATION_MS = 500;
const int MENU_BLINK_DURATION_MS = 300;
const int BUTTON_DEBOUNCE_DELAY_MS = 20;
const int POT_INPUT_CHECK_FREQUENCY_MS = 50;
const int POT_INPUT_CHANGE_THRESHOLD = 10;

// ~120Hz / tube - tested on ИH-2 and ИH-12A tubes
// Given Xms per-tube cycle, 1000ms / (Xms/tube * 6tubes) = Hz
// 1.4ms (1400μs) ~= 120Hz, and 2.8ms (2800μs) ~= 60Hz
const int MULTIPLEX_SINGLE_TUBE_CYCLE_US = 1400;
const int MULTIPLEX_SINGLE_TUBE_LIT_MIN_US = 0;
const int MULTIPLEX_SINGLE_TUBE_LIT_MAX_US = 1350;

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
unsigned long potLastCheckMS = 0UL;
int potVal = 0;

// nixie tube state 🚥🚥
byte multiplexDisplayValues[TUBE_COUNT] = {BLANK, BLANK, BLANK, BLANK, BLANK, BLANK};
byte lastDisplayRefreshTubeIndex = TUBE_COUNT;
unsigned long lastDisplayRefreshTimestampUS = 0UL;
bool multiplexIsLit = false;
int multiplexSingleTubeLitUS = MULTIPLEX_SINGLE_TUBE_LIT_MAX_US;
int multiplexSingleTubeOffUS = MULTIPLEX_SINGLE_TUBE_CYCLE_US - multiplexSingleTubeLitUS;

// chess clock state ♟⏲⏲♟
ClockState currentClockState = CLOCK_IDLE;
unsigned long lastStatusUpdateTimestampMS = 0UL;
unsigned long lastJackpotTimestampMS = 0UL;
unsigned long lastEventStepTimestampMS = 0UL;
unsigned long turnStartTimestampMS = 0UL;
bool leftPlayersTurn = false;
bool blinkOn = false;
bool jackpotOn = false;
byte jackpotDigitOrderIndexValues[TUBE_COUNT] = {0, 0, 0, 0, 0, 0};
byte currentTurnTimerOption = 2;

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
  if (multiplexIsLit && micros() - lastDisplayRefreshTimestampUS >= multiplexSingleTubeLitUS) {
    blankTubes();
    multiplexIsLit = false;
    lastDisplayRefreshTimestampUS = micros();
  } else if (!multiplexIsLit && micros() - lastDisplayRefreshTimestampUS >= multiplexSingleTubeOffUS) {
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
    currentClockState = CLOCK_TIMEOUT;
    
    blankTubes();
    setButtonLEDs(leftPlayersTurn, !leftPlayersTurn);
    notifyTimeout(leftPlayersTurn);
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

inline void handleRightButtonPress(unsigned long loopNow) {
  if (currentClockState == CLOCK_RUNNING && !leftPlayersTurn) {
    leftPlayersTurn = !leftPlayersTurn;
    turnStartTimestampMS = loopNow;

    blankTubes();
    setButtonLEDs(leftPlayersTurn, !leftPlayersTurn);
    notifyPlayerTurn(leftPlayersTurn);
  } else if (currentClockState == CLOCK_MENU) {
      // loop around if we're already on the largest option
    if (currentTurnTimerOption == TURN_TIMER_OPTIONS_COUNT - 1) {
      currentTurnTimerOption = 0;
    } else {
      // cycle upward to larger timer level
      currentTurnTimerOption++;
    }
  } else if (currentClockState == CLOCK_IDLE) {
    leftPlayersTurn = false;
    turnStartTimestampMS = loopNow;
    currentClockState = CLOCK_RUNNING;
    
    setButtonLEDs(leftPlayersTurn, !leftPlayersTurn);
    notifyNewGame(false, TURN_TIMER_OPTIONS[currentTurnTimerOption].label);
  } else if (currentClockState == CLOCK_TIMEOUT) {
    currentClockState = CLOCK_IDLE;
  }
}

inline void handleLeftButtonPress(unsigned long loopNow) {
  if (currentClockState == CLOCK_RUNNING && leftPlayersTurn) {
    leftPlayersTurn = !leftPlayersTurn;
    turnStartTimestampMS = loopNow;
    
    blankTubes();
    setButtonLEDs(leftPlayersTurn, !leftPlayersTurn);
    notifyPlayerTurn(leftPlayersTurn);
  } else if (currentClockState == CLOCK_MENU) {
    if (currentTurnTimerOption == 0) {
      // loop around if we're already on the smallest option
      currentTurnTimerOption = TURN_TIMER_OPTIONS_COUNT - 1;
    } else {
      // cycle downward to smaller timer level
      currentTurnTimerOption--;
    }
  } else if (currentClockState == CLOCK_IDLE) {
    leftPlayersTurn = true;
    turnStartTimestampMS = loopNow;
    currentClockState = CLOCK_RUNNING;

    setButtonLEDs(leftPlayersTurn, !leftPlayersTurn);
    notifyNewGame(true, TURN_TIMER_OPTIONS[currentTurnTimerOption].label);
  } else if (currentClockState == CLOCK_TIMEOUT) {
    currentClockState = CLOCK_IDLE;
  }
}

inline void handleUtilityButtonPress(unsigned long loopNow) {
  if (currentClockState == CLOCK_IDLE) {
    currentClockState = CLOCK_MENU;
  } else if (
    currentClockState == CLOCK_MENU
    || currentClockState == CLOCK_TIMEOUT
    || currentClockState == CLOCK_RUNNING
  ) {
    currentClockState = CLOCK_IDLE;
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

// TODO - Move to clock-hardware
const int POT_MAX = 1023;
int LOG_IN_MAP[] = {0,3,9,16,30,48,71,93,126,170,220,300,390,480,580,680,780,880,1015};
int OUT_MAP[] = {0,75,150,225,300,375,450,525,600,675,750,825,900,975,1050,1125,1200,1275,1350};
const int MAP_SIZE = 19;

void loopCheckPotentiometer(unsigned long loopNow, bool enforceMinimumChange) {
  if (loopNow - potLastCheckMS < POT_INPUT_CHECK_FREQUENCY_MS) {
    return;
  }

  int val = analogRead(PIN_POTENTIOMETER);
  potLastCheckMS = loopNow;

  if (!enforceMinimumChange || abs(potVal - val) > POT_INPUT_CHANGE_THRESHOLD) {
    potVal = val;

    // linear map
    // multiplexSingleTubeLitUS = map(
    //   potVal,
    //   0, POT_MAX,
    //   MULTIPLEX_SINGLE_TUBE_LIT_MIN_US,
    //   MULTIPLEX_SINGLE_TUBE_LIT_MAX_US
    // );

    // use multiple segments to account for log pot hardware (volume control)
    // TODO: use a linear pot instead
    multiplexSingleTubeLitUS = multiMapBS<int>(potVal, LOG_IN_MAP, OUT_MAP, MAP_SIZE);
    multiplexSingleTubeOffUS = MULTIPLEX_SINGLE_TUBE_CYCLE_US - multiplexSingleTubeLitUS;

    Serial.print("pot: ");
    Serial.print(potVal);
    Serial.print(" | on: ");
    Serial.print(multiplexSingleTubeLitUS);
    Serial.print(" | off: ");
    Serial.println(multiplexSingleTubeOffUS);
  }
}

/*
 * ===============================
 *  Initial Setup (Power On)
 * ===============================
 */
void setup() {
  setupHardware();

  // initial read doesn't enfore the minimum change
  loopCheckPotentiometer(POT_INPUT_CHECK_FREQUENCY_MS, false);
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

  loopCheckPotentiometer(now, true);
  
  CountdownValues cv = { 0UL, 0UL };

  // handle behavior based on current clock state. generally, update the
  // tube display by making changes to multiplexDisplayValues[], and 
  // occasionally change to other states (e.g. loopCountdown() will change
  // `currentClockState` to CLOCK_TIMEOUT when appropriate)
  if (currentClockState == CLOCK_RUNNING) {
    cv = loopCountdown(now);
  } else if (currentClockState == CLOCK_TIMEOUT) {
    cv = { TURN_TIMER_OPTIONS[currentTurnTimerOption].turnLimitMS, 0UL };
    loopTimeout(now);
  } else if (currentClockState == CLOCK_MENU) {
    loopMenu(now);
  } else if (currentClockState == CLOCK_IDLE) {
    loopIdle();
  }

  // display current values in multiplexDisplayValues[]
  loopMultiplex();

  loopHardware(now);
}
