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
const int POT_INPUT_CHANGE_WINDOW = 30;

// ~120Hz / tube - tested on ИH-2 and ИH-12A tubes
// 1.8ms ~= 92Hz
// 2.0ms ~= 83Hz
// 2.2ms ~= 76Hz
// 2.4ms ~= 69Hz
// 2.6mz ~= 64Hz
// 2.8mz ~= 60Hz

// using a 1.4ms (1400µs) per-tube cycle: 1000ms / (1.4ms/tube * 6tubes)
// const int MULTIPLEX_SINGLE_TUBE_LIT_DURATION_US = 1200; // ~86% lit
// const int MULTIPLEX_SINGLE_TUBE_OFF_DURATION_US = 200;  // ~14% off

const int MULTIPLEX_SINGLE_TUBE_CYCLE_PART_MAX_DURATION_US = 2700;
const int MULTIPLEX_SINGLE_TUBE_CYCLE_PART_MIN_DURATION_US =  100;
const int MULTIPLEX_SINGLE_TUBE_CYCLE_DURATION_US = 
  MULTIPLEX_SINGLE_TUBE_CYCLE_PART_MAX_DURATION_US
  + MULTIPLEX_SINGLE_TUBE_CYCLE_PART_MIN_DURATION_US;

int MULTIPLEX_SINGLE_TUBE_LIT_DURATION_US = 0;
int MULTIPLEX_SINGLE_TUBE_OFF_DURATION_US = 0;

const int HV_STABILIZATION_DELAY_US = 10;


/*
 * ===============================
 *  Runtime State
 * ===============================
 */

// button state 🔘🔘
int rightButtonLastVal = HIGH;
int leftButtonLastVal = HIGH;
int utilityButtonLastVal = HIGH;
int rightButtonVal = HIGH;
int leftButtonVal = HIGH;
int utilityButtonVal = HIGH;
int potVal = -50;
unsigned long rightButtonLastDebounceMS = 0UL;
unsigned long leftButtonLastDebounceMS = 0UL;
unsigned long utilityButtonLastDebounceMS = 0UL;

// nixie tube state 🚥🚥
int multiplexDisplayValues[TUBE_COUNT] = {BLANK, BLANK, BLANK, BLANK, BLANK, BLANK};
int lastDisplayRefreshTubeIndex = TUBE_COUNT;
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
int jackpotDigitOrderIndexValues[TUBE_COUNT] = {0, 0, 0, 0, 0, 0};
char statusUpdate[STATUS_UPDATE_MAX_CHARS] = "";
int currentTurnTimerOption = 2;

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
  
  digitalWrite(PIN_ANODE_1, LOW);
  digitalWrite(PIN_ANODE_2, LOW);
  digitalWrite(PIN_ANODE_3, LOW);
  digitalWrite(PIN_ANODE_4, LOW);
  
  setCathode(true, BLANK);
  setCathode(false, BLANK);

  // use analog inputs as digital inputs for buttons
  pinMode(PIN_BUTTON_RIGHT, INPUT_PULLUP);
  pinMode(PIN_BUTTON_LEFT, INPUT_PULLUP);
  // pinMode(PIN_BUTTON_UTILITY, INPUT_PULLUP);

  // use analog inputs as digital outputs for button LEDs
  pinMode(PIN_BUTTON_RIGHT_LED, OUTPUT);
  pinMode(PIN_BUTTON_LEFT_LED, OUTPUT);
  digitalWrite(PIN_BUTTON_RIGHT_LED, LOW);
  digitalWrite(PIN_BUTTON_LEFT_LED, LOW);

  // ground for button assembly
  pinMode(PIN_BUTTON_GROUND, OUTPUT);
  digitalWrite(PIN_BUTTON_GROUND, LOW);

  pinMode(PIN_BUTTON_UTILITY, INPUT); // not needed probably

  Serial.begin(SERIAL_SPEED_BAUD);
}

/*
 * ===============================
 *  Internal Functions - Hardware
 * ===============================
 */

void setCathode(boolean ctrl0, int displayNumber) {
  byte a, b, c, d;
  d = c = b = a = 1;
  
  // given a decimal display number, set the matching binary representation
  // as input to the specified cathode controller
  switch(displayNumber) {
    case 0: d=0; c=0; b=0; a=0; break;
    case 1: d=0; c=0; b=0; a=1; break;
    case 2: d=0; c=0; b=1; a=0; break;
    case 3: d=0; c=0; b=1; a=1; break;
    case 4: d=0; c=1; b=0; a=0; break;
    case 5: d=0; c=1; b=0; a=1; break;
    case 6: d=0; c=1; b=1; a=0; break;
    case 7: d=0; c=1; b=1; a=1; break;
    case 8: d=1; c=0; b=0; a=0; break;
    case 9: d=1; c=0; b=0; a=1; break;
    default: d=1; c=1; b=1; a=1;
  }  
  
  if (ctrl0) {
    digitalWrite(PIN_CATHODE_0_D, d);
    digitalWrite(PIN_CATHODE_0_C, c);
    digitalWrite(PIN_CATHODE_0_B, b);
    digitalWrite(PIN_CATHODE_0_A, a);
  } else {
    digitalWrite(PIN_CATHODE_1_D, d);
    digitalWrite(PIN_CATHODE_1_C, c);
    digitalWrite(PIN_CATHODE_1_B, b);
    digitalWrite(PIN_CATHODE_1_A, a);
  }
}

void setAnode(int anode, int displayVal) {
  switch(anode) {
    case 1:
      digitalWrite(PIN_ANODE_2, LOW);
      digitalWrite(PIN_ANODE_3, LOW);
      delayMicroseconds(HV_STABILIZATION_DELAY_US);

      digitalWrite(PIN_ANODE_1, displayVal == BLANK ? LOW : HIGH);
      delayMicroseconds(HV_STABILIZATION_DELAY_US);

      break;
    case 2:
      digitalWrite(PIN_ANODE_1, LOW);
      digitalWrite(PIN_ANODE_3, LOW);
      delayMicroseconds(HV_STABILIZATION_DELAY_US);

      digitalWrite(PIN_ANODE_2, displayVal == BLANK ? LOW : HIGH);
      delayMicroseconds(HV_STABILIZATION_DELAY_US);

      break;
    case 3:
      digitalWrite(PIN_ANODE_1, LOW);
      digitalWrite(PIN_ANODE_2, LOW);
      delayMicroseconds(HV_STABILIZATION_DELAY_US);

      digitalWrite(PIN_ANODE_3, displayVal == BLANK ? LOW : HIGH);
      delayMicroseconds(HV_STABILIZATION_DELAY_US);

      break;
  }
}

void displayOnTubeExclusive(int tubeIndex, int displayVal) {
  int anode = TUBE_ANODES[tubeIndex];
  bool cathodeCtrl0 = TUBE_CATHODE_CTRL_0[tubeIndex];
  
  setAnode(anode, displayVal);
  setCathode(!cathodeCtrl0, BLANK);
  setCathode(cathodeCtrl0, displayVal);
}

void setButtonLEDs(bool leftOn, bool rightOn) {
  digitalWrite(PIN_BUTTON_LEFT_LED, leftOn ? HIGH : LOW);
  digitalWrite(PIN_BUTTON_RIGHT_LED, rightOn ? HIGH : LOW);
}

/*
 * ===============================
 *  Internal Functions - Logical
 * ===============================
 */

void setMultiplexDisplay(int t0, int t1, int t2, int t3, int t4, int t5) {
  multiplexDisplayValues[0] = t0;
  multiplexDisplayValues[1] = t1;
  multiplexDisplayValues[2] = t2;
  multiplexDisplayValues[3] = t3;
  multiplexDisplayValues[4] = t4;
  multiplexDisplayValues[5] = t5;
}

void loopMultiplex() {
  unsigned long refreshDiffUS = micros() - lastDisplayRefreshTimestampUS;

  if (multiplexIsLit && refreshDiffUS >= MULTIPLEX_SINGLE_TUBE_LIT_DURATION_US) {
    displayOnTubeExclusive(lastDisplayRefreshTubeIndex, BLANK);
    multiplexIsLit = false;
    lastDisplayRefreshTimestampUS = micros();
  } else if (!multiplexIsLit && refreshDiffUS >= MULTIPLEX_SINGLE_TUBE_OFF_DURATION_US) {
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

void handleJackpot(unsigned long loopNow) {
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
    for (int i = 0; i < TUBE_COUNT; i++) {
      if (jackpotDigitOrderIndexValues[i] == 9) {
        jackpotDigitOrderIndexValues[i] = 0;
      } else {
        jackpotDigitOrderIndexValues[i]++;
      }
    }

    lastEventStepTimestampMS = loopNow;
  }
}

void setMultiplexClockTime(unsigned long elapsedMS, unsigned long remainingMS, unsigned long loopNow, bool displayElapsed) {
  unsigned long totalMS = displayElapsed ? elapsedMS : remainingMS;
  unsigned long totalSec = totalMS / 1000;
  int hours = totalSec / 3600;
  int hoursRemainder = totalSec % 3600;
  int min = hoursRemainder / 60;
  int sec = hoursRemainder % 60;

  // activate jackpot scroll on every whole minute (excluding when clock starts 
  // and times out), in an effort to preserve tubes / prevent uneven burn
  if (sec == 0 && jackpotOn == false && elapsedMS > JACKPOT_MIN_ELAPSED_MS && remainingMS > JACKPOT_MIN_REMAINING_MS) {
    jackpotOn = true;
    lastJackpotTimestampMS = loopNow;
    
    for (int i = 0; i < TUBE_COUNT; i++) {
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

CountdownValues loopCountdown(unsigned long loopNow) {
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

void loopTimeout(unsigned long loopNow) {
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

void loopMenu(unsigned long loopNow) {
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

void loopIdle() {
  setButtonLEDs(false, false);
  setMultiplexDisplay(BLANK, BLANK, BLANK, BLANK, BLANK, BLANK);
}

void loopSendStatusUpdate(unsigned long loopNow, unsigned long elapsedMs, unsigned long remainingMs) {
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

void handleRightButtonPress(unsigned long loopNow) {
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

void handleLeftButtonPress(unsigned long loopNow) {
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

void handleUtilityButtonPress(unsigned long loopNow) {
  if (currentClockState == IDLE) {
    currentClockState = MENU;
  } else if (currentClockState == DEMO || currentClockState == MENU || currentClockState == TIMEOUT || currentClockState == RUNNING) {
    currentClockState = IDLE;
    turnStartTimestampMS = 0;
  }
}

void loopCheckButtons(unsigned long loopNow) {
  int rightButtonReading = digitalRead(PIN_BUTTON_RIGHT);
  int leftButtonReading = digitalRead(PIN_BUTTON_LEFT);
  // int utilityButtonReading = digitalRead(PIN_BUTTON_UTILITY);

  // handle press state change (set debounce timer)
  if (rightButtonReading != rightButtonLastVal) {
    rightButtonLastDebounceMS = loopNow;
  }
  if (leftButtonReading != leftButtonLastVal) {
    leftButtonLastDebounceMS = loopNow;
  }
  // if (utilityButtonReading != utilityButtonLastVal) {
  //   utilityButtonLastDebounceMS = loopNow;
  // }

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
  // unsigned long utilityDebounceDiff = loopNow - utilityButtonLastDebounceMS;
  // if (utilityDebounceDiff > BUTTON_DEBOUNCE_DELAY_MS && utilityButtonReading != utilityButtonVal) {
  //   utilityButtonVal = utilityButtonReading;

  //   // using internal pull-up resistor means a pressed button goes LOW
  //   if (utilityButtonVal == LOW) {
  //     handleUtilityButtonPress(loopNow);
  //   }
  // }

  // store reading as last value to compare to in next loop
  rightButtonLastVal = rightButtonReading;
  leftButtonLastVal = leftButtonReading;
  // utilityButtonLastVal = utilityButtonReading;
}

void loopCheckPot() {
  int val = analogRead(PIN_BUTTON_UTILITY);

  if (abs(potVal - val) > POT_INPUT_CHANGE_WINDOW) {
    potVal = val;
    
    MULTIPLEX_SINGLE_TUBE_LIT_DURATION_US = map(
      potVal, 1024, 0,
      MULTIPLEX_SINGLE_TUBE_CYCLE_PART_MIN_DURATION_US,
      MULTIPLEX_SINGLE_TUBE_CYCLE_PART_MAX_DURATION_US
    );

    MULTIPLEX_SINGLE_TUBE_OFF_DURATION_US = MULTIPLEX_SINGLE_TUBE_CYCLE_DURATION_US - MULTIPLEX_SINGLE_TUBE_LIT_DURATION_US;
  }
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

  loopCheckPot();

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
