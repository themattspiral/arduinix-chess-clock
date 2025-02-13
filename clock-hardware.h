/*
 * ============================
 *    Hardware Definitions
 * ============================
 */

#ifndef _CLOCK_HARDWARE_H
#define _CLOCK_HARDWARE_H

typedef struct {
  const byte left;
  const byte right;
  const byte utility;
} ButtonValues;

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

const int HV_STABILIZATION_DELAY_US = 15;

// controller values > 9 result in blank display
const byte BLANK = 15;

// ИH-12A tubes
const byte TUBE_DIGIT_ORDER[] = {3, 8, 9, 4, 0, 5, 7, 2, 6, 1};

// most other tubes
// const byte TUBE_DIGIT_ORDER[] = {6, 7, 5, 8, 4, 3, 9, 2, 0, 1};

// display number cathode bit position to BCD К155ИД1/SN74141 input constants
const byte BIT_0_BCD_PIN_A = 1 << 0;
const byte BIT_1_BCD_PIN_B = 1 << 1;
const byte BIT_2_BCD_PIN_C = 1 << 2;
const byte BIT_3_BCD_PIN_D = 1 << 3;

void printHardwareInfo() {
  #if defined(ARDUINO_UNOWIFIR4)
    Serial.println("Arduino Uno R4 WiFi (Renesas-based) Detected!");
  #elif defined(ARDUINO_AVR_UNO)
    Serial.println("Arduino Uno R1-R3 (AVR-based) Detected!");
  #endif
  
  Serial.println("Pin Hardware Ports:");
  for (byte pin = 0; pin <= A5; pin++) {
    uint16_t port = digitalPinToPort(pin);
    uint16_t mask = digitalPinToBitMask(pin);

    Serial.print(" Pin ");
    Serial.print(pin);
    Serial.print(" - Port: ");
    Serial.print(port);
    Serial.print(" - Bitmask: ");
    Serial.println(mask);
  }
}

// Arduino Uno R1-R3 (AVR) hardware register port manipulation
#if defined(ARDUINO_AVR_UNO)

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

inline ButtonValues readButtonValues() {
  return {
    (PINC & PIN_BUTTON_LEFT_DPM_BIT) ? HIGH : LOW,
    (PINC & PIN_BUTTON_RIGHT_DPM_BIT) ? HIGH : LOW,
    (PINC & PIN_BUTTON_UTILITY_DPM_BIT) ? HIGH : LOW
  };
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

inline void blankTubes() {
  // anodes off
  PORTB &= ~(
      PIN_ANODE_1_DPM_BIT
    | PIN_ANODE_2_DPM_BIT
    | PIN_ANODE_3_DPM_BIT
  );

  // cathode 0 blank
  PORTD |= (
      PIN_CATHODE_0_A_DPM_BIT
    | PIN_CATHODE_0_B_DPM_BIT
    | PIN_CATHODE_0_C_DPM_BIT
    | PIN_CATHODE_0_D_DPM_BIT
  );

  // cathode 1 blank
  PORTD |= (PIN_CATHODE_1_A_DPM_BIT | PIN_CATHODE_1_B_DPM_BIT);
  PORTB |= (PIN_CATHODE_1_C_DPM_BIT | PIN_CATHODE_1_D_DPM_BIT);
}

inline void displayOnTube(byte tubeIndex, byte displayVal) {
  if (displayVal == BLANK) {
    return;
  }

  byte anode = TUBE_ANODES[tubeIndex];
  bool cathodeCtrl0 = TUBE_CATHODE_CTRL_0[tubeIndex];

  byte portBHighBitmask, portBLowBitmask, portDHighBitmask, portDLowBitmask;
  portBHighBitmask = portBLowBitmask = portDHighBitmask = portDLowBitmask = 0;
  
  byte cathodeA = displayVal & BIT_0_BCD_PIN_A;
  byte cathodeB = displayVal & BIT_1_BCD_PIN_B;
  byte cathodeC = displayVal & BIT_2_BCD_PIN_C;
  byte cathodeD = displayVal & BIT_3_BCD_PIN_D;

  if (cathodeCtrl0) {
    if (cathodeA) {
      portDHighBitmask |= PIN_CATHODE_0_A_DPM_BIT;
    } else {
      portDLowBitmask |= PIN_CATHODE_0_A_DPM_BIT;
    }
    if (cathodeB) {
      portDHighBitmask |= PIN_CATHODE_0_B_DPM_BIT;
    } else {
      portDLowBitmask |= PIN_CATHODE_0_B_DPM_BIT;
    }
    if (cathodeC) {
      portDHighBitmask |= PIN_CATHODE_0_C_DPM_BIT;
    } else {
      portDLowBitmask |= PIN_CATHODE_0_C_DPM_BIT;
    }
    if (cathodeD) {
      portDHighBitmask |= PIN_CATHODE_0_D_DPM_BIT;
    } else {
      portDLowBitmask |= PIN_CATHODE_0_D_DPM_BIT;
    }
  } else {
    if (cathodeA) {
      portDHighBitmask |= PIN_CATHODE_1_A_DPM_BIT;
    } else {
      portDLowBitmask |= PIN_CATHODE_1_A_DPM_BIT;
    }
    if (cathodeB) {
      portDHighBitmask |= PIN_CATHODE_1_B_DPM_BIT;
    } else {
      portDLowBitmask |= PIN_CATHODE_1_B_DPM_BIT;
    }
    if (cathodeC) {
      portBHighBitmask |= PIN_CATHODE_1_C_DPM_BIT;
    } else {
      portBLowBitmask |= PIN_CATHODE_1_C_DPM_BIT;
    }
    if (cathodeD) {
      portBHighBitmask |= PIN_CATHODE_1_D_DPM_BIT;
    } else {
      portBLowBitmask |= PIN_CATHODE_1_D_DPM_BIT;
    }
  }

  PORTB &= ~portBLowBitmask;
  PORTD &= ~portDLowBitmask;
  PORTB |= portBHighBitmask;
  PORTD |= portDHighBitmask;

  delayMicroseconds(HV_STABILIZATION_DELAY_US);

  // set high anode pin
  switch (anode) {
    case 1:
      PORTB |= PIN_ANODE_1_DPM_BIT;
      break;
    case 2:
      PORTB |= PIN_ANODE_2_DPM_BIT;
      break;
    case 3:
      PORTB |= PIN_ANODE_3_DPM_BIT;
      break;
  }
}

// Arduino Uno R4 WiFi (Renesas) hardware register port manipulation
#elif defined(ARDUINO_UNOWIFIR4)

const uint16_t PIN_CATHODE_0_A_DPM_BIT = digitalPinToBitMask(2);
const uint16_t PIN_CATHODE_0_B_DPM_BIT = digitalPinToBitMask(3);
const uint16_t PIN_CATHODE_0_C_DPM_BIT = digitalPinToBitMask(4);
const uint16_t PIN_CATHODE_0_D_DPM_BIT = digitalPinToBitMask(5);
const uint16_t PIN_CATHODE_1_A_DPM_BIT = digitalPinToBitMask(6);
const uint16_t PIN_CATHODE_1_B_DPM_BIT = digitalPinToBitMask(7);
const uint16_t PIN_CATHODE_1_C_DPM_BIT = digitalPinToBitMask(8);
const uint16_t PIN_CATHODE_1_D_DPM_BIT = digitalPinToBitMask(9);
const uint16_t PIN_ANODE_1_DPM_BIT = digitalPinToBitMask(10);
const uint16_t PIN_ANODE_2_DPM_BIT = digitalPinToBitMask(11);
const uint16_t PIN_ANODE_3_DPM_BIT = digitalPinToBitMask(12);
const uint16_t PIN_ANODE_4_DPM_BIT = digitalPinToBitMask(13);
const uint16_t PIN_BUTTON_RIGHT_DPM_BIT = digitalPinToBitMask(A0);
const uint16_t PIN_BUTTON_LEFT_DPM_BIT = digitalPinToBitMask(A1);
const uint16_t PIN_BUTTON_RIGHT_LED_DPM_BIT = digitalPinToBitMask(A2);
const uint16_t PIN_BUTTON_LEFT_LED_DPM_BIT = digitalPinToBitMask(A3);
const uint16_t PIN_BUTTON_UTILITY_DPM_BIT = digitalPinToBitMask(A4);
const uint16_t PIN_BUTTON_GROUND_DPM_BIT = digitalPinToBitMask(A5);

inline ButtonValues readButtonValues() {
  return {
    (R_PORT0->PIDR & PIN_BUTTON_LEFT_DPM_BIT) ? HIGH : LOW,
    (R_PORT0->PIDR & PIN_BUTTON_RIGHT_DPM_BIT) ? HIGH : LOW,
    (R_PORT1->PIDR & PIN_BUTTON_UTILITY_DPM_BIT) ? HIGH : LOW
  };
}

inline void setButtonLEDs(bool leftOn, bool rightOn) {
  if (leftOn) {
    R_PORT0->POSR = PIN_BUTTON_LEFT_LED_DPM_BIT;
  } else {
    R_PORT0->PORR = PIN_BUTTON_LEFT_LED_DPM_BIT;
  }

  if (rightOn) {
    R_PORT0->POSR = PIN_BUTTON_RIGHT_LED_DPM_BIT;
  } else {
    R_PORT0->PORR = PIN_BUTTON_RIGHT_LED_DPM_BIT;
  }
}

inline void blankTubes() {
  // anodes off
  R_PORT1->PORR = PIN_ANODE_1_DPM_BIT;
  R_PORT4->PORR = PIN_ANODE_2_DPM_BIT;
  R_PORT4->PORR = PIN_ANODE_3_DPM_BIT;

  // cathode 0 blank
  R_PORT1->POSR = PIN_CATHODE_0_A_DPM_BIT;
  R_PORT1->POSR = PIN_CATHODE_0_B_DPM_BIT;
  R_PORT1->POSR = PIN_CATHODE_0_C_DPM_BIT;
  R_PORT1->POSR = PIN_CATHODE_0_D_DPM_BIT;

  // cathode 1 blank
  R_PORT1->POSR = PIN_CATHODE_1_A_DPM_BIT;
  R_PORT1->POSR = PIN_CATHODE_1_B_DPM_BIT;
  R_PORT3->POSR = PIN_CATHODE_1_C_DPM_BIT;
  R_PORT3->POSR = PIN_CATHODE_1_D_DPM_BIT;
}

inline void displayOnTube(byte tubeIndex, byte displayVal) {
  if (displayVal == BLANK) {
    return;
  }

  byte anode = TUBE_ANODES[tubeIndex];
  bool cathodeCtrl0 = TUBE_CATHODE_CTRL_0[tubeIndex];

  byte cathodeA = displayVal & BIT_0_BCD_PIN_A;
  byte cathodeB = displayVal & BIT_1_BCD_PIN_B;
  byte cathodeC = displayVal & BIT_2_BCD_PIN_C;
  byte cathodeD = displayVal & BIT_3_BCD_PIN_D;
  
  if (cathodeCtrl0) {
    if (cathodeA) {
      R_PORT1->POSR = PIN_CATHODE_0_A_DPM_BIT;
    } else {
      R_PORT1->PORR = PIN_CATHODE_0_A_DPM_BIT;
    }
    if (cathodeB) {
      R_PORT1->POSR = PIN_CATHODE_0_B_DPM_BIT;
    } else {
      R_PORT1->PORR = PIN_CATHODE_0_B_DPM_BIT;
    }
    if (cathodeC) {
      R_PORT1->POSR = PIN_CATHODE_0_C_DPM_BIT;
    } else {
      R_PORT1->PORR = PIN_CATHODE_0_C_DPM_BIT;
    }
    if (cathodeD) {
      R_PORT1->POSR = PIN_CATHODE_0_D_DPM_BIT;
    } else {
      R_PORT1->PORR = PIN_CATHODE_0_D_DPM_BIT;
    }
  } else {
    if (cathodeA) {
      R_PORT1->POSR = PIN_CATHODE_1_A_DPM_BIT;
    } else {
      R_PORT1->PORR = PIN_CATHODE_1_A_DPM_BIT;
    }
    if (cathodeB) {
      R_PORT1->POSR = PIN_CATHODE_1_B_DPM_BIT;
    } else {
      R_PORT1->PORR = PIN_CATHODE_1_B_DPM_BIT;
    }
    if (cathodeC) {
      R_PORT3->POSR = PIN_CATHODE_1_C_DPM_BIT;
    } else {
      R_PORT3->PORR = PIN_CATHODE_1_C_DPM_BIT;
    }
    if (cathodeD) {
      R_PORT3->POSR = PIN_CATHODE_1_D_DPM_BIT;
    } else {
      R_PORT3->PORR = PIN_CATHODE_1_D_DPM_BIT;
    }
  }

  delayMicroseconds(HV_STABILIZATION_DELAY_US);

  // set high anode pin
  switch (anode) {
    case 1:
      R_PORT1->POSR = PIN_ANODE_1_DPM_BIT;
      break;
    case 2:
      R_PORT4->POSR = PIN_ANODE_2_DPM_BIT;
      break;
    case 3:
      R_PORT4->POSR = PIN_ANODE_3_DPM_BIT;
      break;
  }
}

#endif

#endif _CLOCK_HARDWARE_H
