#include <stdio.h>

#include "clock-hardware.h"
#include "clock-data-types.h"

/*
 * ============================
 *    Behavior Constants
 * ============================
 */
const int MUX_SINGLE_TUBE_DELAY_US = 500;   // 300-3000µs is ideal for IN-2 tubes, 100-1000µs for IN-12 tubes
const int DEMO_STEP_DURATION_MS = 150;
const int TIMEOUT_BLINK_DURATION_MS = 500;
const int MENU_BLINK_DURATION_MS = 300;
const int BUTTON_DEBOUNCE_DELAY_MS = 20;
const int STATUS_UPDATE_INTERVAL_MS = 1000;
const int STATUS_UPDATE_MAX_SIZE = 30;

/*
 * ============================
 *    Runtime State
 * ============================
 */

// button state 🔘🔘
int rightButtonLastVal = HIGH;
int leftButtonLastVal = HIGH;
int utilityButtonLastVal = HIGH;
int rightButtonVal = HIGH;
int leftButtonVal = HIGH;
int utilityButtonVal = HIGH;
unsigned long rightButtonLastDebounceMS = 0UL;
unsigned long leftButtonLastDebounceMS = 0UL;
unsigned long utilityButtonLastDebounceMS = 0UL;

// nixie tube state 🚥🚥
int mux[TUBE_COUNT] = {BLANK, BLANK, BLANK, BLANK, BLANK, BLANK};
int lastDisplayRefreshTubeIndex = -1;
unsigned long lastDisplayRefreshTimestampUs = 0UL;

// chess clock state ♟⏲⏲♟
ClockState currentClockState = IDLE;
unsigned long lastStatusUpdateTimestampMs = 0UL;
unsigned long lastEventStepTimestampMs = 0UL;
unsigned long turnStartTimestampMS = 0UL;
bool leftPlayersTurn = false;
bool blinkOn = false;
char statusUpdate[STATUS_UPDATE_MAX_SIZE] = "";
int currentTurnTimerOption = 2;

/*
 * ============================
 *    INITIAL SETUP (ON BOOT)
 * ============================
 */
void setup() 
{
  Serial.begin(115200);

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
  pinMode(PIN_BUTTON_UTILITY, INPUT_PULLUP);

  // use analog inputs as digital outputs for button LEDs
  pinMode(PIN_BUTTON_RIGHT_LED, OUTPUT);
  pinMode(PIN_BUTTON_LEFT_LED, OUTPUT);
  digitalWrite(PIN_BUTTON_RIGHT_LED, LOW);
  digitalWrite(PIN_BUTTON_LEFT_LED, LOW);

  // ground
  pinMode(PIN_BUTTON_GROUND, OUTPUT);
  digitalWrite(PIN_BUTTON_GROUND, LOW);
}


/*
 * ============================
 *    Internal Functions
 * ============================
 */

void setCathode(boolean ctrl0, int displayNumber) {
  byte a, b, c, d;
  d = c = b = a = 1;
  
  // binary representation
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
  
  // write to output pins
  if (ctrl0) {
    // controller 0
    digitalWrite(PIN_CATHODE_0_D, d);
    digitalWrite(PIN_CATHODE_0_C, c);
    digitalWrite(PIN_CATHODE_0_B, b);
    digitalWrite(PIN_CATHODE_0_A, a);
  } else {
    // controller 1
    digitalWrite(PIN_CATHODE_1_D, d);
    digitalWrite(PIN_CATHODE_1_C, c);
    digitalWrite(PIN_CATHODE_1_B, b);
    digitalWrite(PIN_CATHODE_1_A, a);
  }
}

void displayOnTube(int tubeIndex, int displayVal) {
  int anode = TUBE_ANODES[tubeIndex];
  bool cathodeCtrl0 = TUBE_CATHODE_CTRL_0[tubeIndex];
  
  switch(anode) {
    case 1:
      digitalWrite(PIN_ANODE_2, LOW);
      digitalWrite(PIN_ANODE_3, LOW);
      digitalWrite(PIN_ANODE_1, displayVal == BLANK ? LOW : HIGH);
      break;
    case 2:
      digitalWrite(PIN_ANODE_1, LOW);
      digitalWrite(PIN_ANODE_3, LOW);
      digitalWrite(PIN_ANODE_2, displayVal == BLANK ? LOW : HIGH);
      break;
    case 3:
      digitalWrite(PIN_ANODE_1, LOW);
      digitalWrite(PIN_ANODE_2, LOW);
      digitalWrite(PIN_ANODE_3, displayVal == BLANK ? LOW : HIGH);
      break;
  }
  
  setCathode(!cathodeCtrl0, BLANK);
  setCathode(cathodeCtrl0, displayVal);
}

void setButtonLEDs(bool left, bool right) {
  digitalWrite(PIN_BUTTON_LEFT_LED, left ? HIGH : LOW);
  digitalWrite(PIN_BUTTON_RIGHT_LED, right ? HIGH : LOW);
}

void setMux(int t0, int t1, int t2, int t3, int t4, int t5) {
  mux[0] = t0;
  mux[1] = t1;
  mux[2] = t2;
  mux[3] = t3;
  mux[4] = t4;
  mux[5] = t5;
}

void multiplex() {
  if (micros() - lastDisplayRefreshTimestampUs > MUX_SINGLE_TUBE_DELAY_US) {
    // move onto next tube
    if (lastDisplayRefreshTubeIndex == TUBE_COUNT - 1) {
      lastDisplayRefreshTubeIndex = 0;
    } else {
      lastDisplayRefreshTubeIndex++;
    }

    displayOnTube(lastDisplayRefreshTubeIndex, mux[lastDisplayRefreshTubeIndex]);

    lastDisplayRefreshTimestampUs = micros();
  }
}

void setMuxClockTime(unsigned long turnTimeMS) {
  unsigned long elapsedSec = turnTimeMS / 1000;
  int hours = elapsedSec / 3600;
  int hoursRemainder = elapsedSec % 3600;
  int min = hoursRemainder / 60;
  int sec = hoursRemainder % 60;

  if (hours > 0) {
    setMux(hours / 10, hours % 10, min / 10, min % 10, sec / 10, sec % 10);
  } else if (min > 0) {
    if (leftPlayersTurn) {
      setMux(min / 10, min % 10, sec / 10, sec % 10, BLANK, BLANK);
    } else {
      setMux(BLANK, BLANK, min / 10, min % 10, sec / 10, sec % 10);
    }
  } else {
    int fractionalSec = (turnTimeMS % 1000) / 10;

    if (leftPlayersTurn) {
      setMux(sec / 10, sec % 10, fractionalSec / 10, fractionalSec % 10, BLANK, BLANK);
    } else {
      setMux(BLANK, BLANK, sec / 10, sec % 10, fractionalSec / 10, fractionalSec % 10);
    }
  }
}

CountdownValues loopCountdown(unsigned long loopNow) {
  unsigned long timeoutLimit = TURN_TIMER_OPTIONS[currentTurnTimerOption].turnLimitMS;
  unsigned long elapsedMS = loopNow - turnStartTimestampMS;
  unsigned long remainingMS = timeoutLimit - elapsedMS;

  setButtonLEDs(leftPlayersTurn, !leftPlayersTurn);

  if (timeoutLimit == 0UL) {
    // when limit is special value 0, show elapsed time and never timeout
    setMuxClockTime(elapsedMS);
  } else if (elapsedMS >= timeoutLimit) {
    // countdown expired, change state
    remainingMS = 0UL;
    currentClockState = TIMEOUT;
  } else {
    // countdown is running - show remaining time
    setMuxClockTime(remainingMS);
  }

  return { elapsedMS, remainingMS };
}

void loopTimeout(unsigned long loopNow) {
  if (loopNow - lastEventStepTimestampMs > TIMEOUT_BLINK_DURATION_MS) {
    lastEventStepTimestampMs = loopNow;
    blinkOn = !blinkOn;
  }

  if (blinkOn) {
    setButtonLEDs(true, true);

    if (leftPlayersTurn) {
      // left player timed out
      setMux(0, 0, 0, 0, BLANK, BLANK);
    } else {
      // right player timed out
      setMux( BLANK, BLANK, 0, 0, 0, 0);
    }
  } else {
    setButtonLEDs(!leftPlayersTurn, leftPlayersTurn);
    setMux(BLANK, BLANK, BLANK, BLANK, BLANK, BLANK);
  }
}

void loopMenu(unsigned long loopNow) {
  if (loopNow - lastEventStepTimestampMs > MENU_BLINK_DURATION_MS) {
    lastEventStepTimestampMs = loopNow;
    blinkOn = !blinkOn;
  }

  if (blinkOn) {
    setButtonLEDs(true, true);
    setMux(
      TURN_TIMER_OPTIONS[currentTurnTimerOption].displayValues[0],
      TURN_TIMER_OPTIONS[currentTurnTimerOption].displayValues[1],
      TURN_TIMER_OPTIONS[currentTurnTimerOption].displayValues[2],
      TURN_TIMER_OPTIONS[currentTurnTimerOption].displayValues[3],
      TURN_TIMER_OPTIONS[currentTurnTimerOption].displayValues[4],
      TURN_TIMER_OPTIONS[currentTurnTimerOption].displayValues[5]
    );
  } else {
    setButtonLEDs(false, false);
    setMux(BLANK, BLANK, BLANK, BLANK, BLANK, BLANK);
  }
}

void loopIdle() {
  setButtonLEDs(false, false);
  setMux(BLANK, BLANK, BLANK, BLANK, BLANK, BLANK);
}

void loopDemoCount(unsigned long loopNow) {
  if (loopNow - lastEventStepTimestampMs > DEMO_STEP_DURATION_MS) {
    lastEventStepTimestampMs = loopNow;

    for (int i = 0; i < TUBE_COUNT; i++) {
      if (mux[i] == 9) {
        mux[i] = 0;
      } else {
        mux[i]++;
      }
    }
  }
}

void loopSendStatusUpdate(unsigned long loopNow, unsigned long elapsedMs, unsigned long remainingMs) {
  if (loopNow - lastStatusUpdateTimestampMs > STATUS_UPDATE_INTERVAL_MS) {
    snprintf(statusUpdate, STATUS_UPDATE_MAX_SIZE, "%d,%s,%d,%lu,%lu", currentClockState, TURN_TIMER_OPTIONS[currentTurnTimerOption].label, leftPlayersTurn, elapsedMs, remainingMs);
    Serial.println(statusUpdate);
    lastStatusUpdateTimestampMs = millis();
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
  int utilityButtonReading = digitalRead(PIN_BUTTON_UTILITY);

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

  // check right button debounce time exceeded with un-flickering changed value
  unsigned long rightDebounceDiff = loopNow - rightButtonLastDebounceMS;
  if (rightDebounceDiff > BUTTON_DEBOUNCE_DELAY_MS && rightButtonReading != rightButtonVal) {
    rightButtonVal = rightButtonReading;

    // using internal pull-up resistor means a pressed button goes LOW
    if (rightButtonVal == LOW) {
      handleRightButtonPress(loopNow);
    }
  }

  // check left button debounce time exceeded with un-flickering changed value
  unsigned long leftDebounceDiff = loopNow - leftButtonLastDebounceMS;
  if (leftDebounceDiff > BUTTON_DEBOUNCE_DELAY_MS && leftButtonReading != leftButtonVal) {
    leftButtonVal = leftButtonReading;

    // using internal pull-up resistor means a pressed button goes LOW
    if (leftButtonVal == LOW) {
      handleLeftButtonPress(loopNow);
    }
  }

  // check utility button debounce time exceeded with un-flickering changed value
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
 *    MAIN LOOP (CONTINUOUS)
 * ============================
 */
void loop() {
  unsigned long now = millis();

  // check for button presses and change state if needed
  loopCheckButtons(now);

  CountdownValues cv = { 0UL, 0UL };

  // update values to display based on current state
  if (currentClockState == RUNNING) {
    cv = loopCountdown(now);
  } else if (currentClockState == TIMEOUT) {
    cv = { TURN_TIMER_OPTIONS[currentTurnTimerOption].turnLimitMS, 0UL };
    loopTimeout(now);
  } else if (currentClockState == MENU) {
    loopMenu(now);
  } else if (currentClockState == DEMO) {
    loopDemoCount(now);
  } else if (currentClockState == IDLE) {
    loopIdle();
  }

  // display current values
  multiplex();

  loopSendStatusUpdate(now, cv.elapsedMS, cv.remainingMS);
}
