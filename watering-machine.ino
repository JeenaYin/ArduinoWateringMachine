/*
 * Project 2
 * Jeena Yin (qyin)
 * It took two weeks
 *
 * Collaboration:
 * Referenced code in 
 * https://learn.adafruit.com/adafruit-trellis-diy-open-source-led-keypad/connecting
 * https://courses.ideate.cmu.edu/60-223/f2019/tutorials/code-bites#blink-without-blocking
 * 
 * Summary: The code below waters 4 plant with different watering 
 * amount according to the setting
 * 
 * Inputs:
 * Adafruit Trellis LED buttons | PIN A2
 * Valve 0 button               | PIN 4
 * Valve 1 button               | PIN 5
 * Valve 2 button               | PIN 7
 * Valve 3 button               | PIN 8
 * 
 * Outputs:
 * Valve 0 servo | PIN 6 (PWM)
 * Valve 1 servo | PIN 9 (PWM)
 * Valve 2 servo | PIN 10 (PWM)
 * Valve 3 servo | PIN 11 (PWM)
 * Pump motor    | PIN 3
 */

#include <Wire.h>
#include <Servo.h>
#include "Adafruit_Trellis.h"

#define NUMTRELLIS 1
#define numKeys (NUMTRELLIS * 16)
#define INTPIN A2

// Valve 0
const int VALVE0_PIN = 6;        // PWM
const int VALVE0_SWITCH_PIN = 4; // for testing

// Valve 1
const int VALVE1_PIN = 9;        // PWM
const int VALVE1_SWITCH_PIN = 5; // for testing

// Valve 2
const int VALVE2_PIN = 10;       // PWM
const int VALVE2_SWITCH_PIN = 7; // for 

// Valve 3
const int VALVE3_PIN = 11;       // PWM
const int VALVE3_SWITCH_PIN = 8; // for testing

// Pump
const int PUMP_PIN = 3;

const int VALVE_CLOSE_POS = 0;
const int VALVE_OPEN_POS = 140;

// 10 seconds as cycle length: if time set to 2 then water every 20 seconds
const int CYCLELENGTH = 10; 
// 5 seconds as unit for watering amount
const int WATERAMOUNTUNIT = 5; 

// Blink the red LED as feedback confirmation
const int blinkLEDdelay = 70;

// Array that stores the water amount setting
int waterAmount[] = {0, 0, 0, 0};
int waterFrequency = 0;
unsigned long microTimer = 0;
unsigned long macroTimer = 0;
unsigned long quarterMacroTimer = 0; // wait time between watering cycles in second
unsigned long quarterMicroTimer = 0; // wait time between each plant in second
int wateringPlantId = -1; // id of plant being watered

// only water if frequency > 0 and wateramount is > 0 for any plant
bool shouldWater = false; 
bool waitingForNextCycle = false;

Servo valve0;
Servo valve1;
Servo valve2;
Servo valve3;

bool valveStates[] = {false, false, false, false};

Adafruit_Trellis matrix = Adafruit_Trellis();
Adafruit_TrellisSet trellis = Adafruit_TrellisSet(&matrix);

void setup() {
  // put your setup code here, to run once:
  pinMode(PUMP_PIN, OUTPUT);
  pinMode(VALVE0_SWITCH_PIN, INPUT_PULLUP);
  pinMode(VALVE1_SWITCH_PIN, INPUT_PULLUP);
  pinMode(VALVE2_SWITCH_PIN, INPUT_PULLUP);
  pinMode(VALVE3_SWITCH_PIN, INPUT_PULLUP);
  pinMode(INTPIN, INPUT);
  Serial.begin(9600);
  valve0.attach(VALVE0_PIN);
  valve1.attach(VALVE1_PIN);
  valve2.attach(VALVE2_PIN);
  valve3.attach(VALVE3_PIN);
  
  for(int i = 0; i < 4; i++){
    OpenValveForPlant(i);
  }
  OpenAllValves();
  
  digitalWrite(INTPIN, HIGH);
  trellis.begin(0x70);

  // turn on all LEDs
  for (uint8_t i=0; i<numKeys; i++) {
    trellis.setLED(i);
    trellis.writeDisplay();    
    delay(50);
  }
  // then turn them off
  for (uint8_t i=0; i<numKeys; i++) {
    trellis.clrLED(i);
    trellis.writeDisplay();    
    delay(50);
  }
  trellis.setLED(0); // light up the red button only
  trellis.writeDisplay();
}

void loop() {
  if (trellis.readSwitches()) {
    for (uint8_t i=0; i<numKeys; i++) {
      if (i == 0 && trellis.justReleased(i)) {
        BlinkLED(i);
      } 
      if (trellis.justPressed(i)) {
        ButtonPressed(i);
      }
    }
    // tell the trellis to set the LEDs we requested
    trellis.writeDisplay();
  }

  // Watering plants
  if(shouldWater) {
    if(millis()/1000 - microTimer >= quarterMicroTimer) {
      // switch to next plant
      if(wateringPlantId == 3) WaitForNextCycle(); // Cycle ends
      else{
        GetReadyForPlant(wateringPlantId + 1);
      }
    }
  }
  // Waiting for next watering cycle
  else if(waitingForNextCycle) {
    if(millis()/1000 - macroTimer >= quarterMacroTimer) {
      Execute();
      macroTimer = millis()/1000;
    }
  }
  // Only check test switches when it's not watering.
  else{
    CheckSwitches();
  }
  delay(30);
}

void ButtonPressed(int i) { 
  // The red button is pressed
  if(i == 0) {
    Execute();
    // if it was pressed, turn it on
    if (trellis.justPressed(i)) {
      trellis.setLED(i);
    }
    return;
  }
  
  int col = i % 4;
  int row = (int) (i / 4);

  // Set Time
  if(col == 0) { // Interface design silimar to a slider
    if (trellis.isLED(i)) {
      if(row == 3 || !trellis.isLED(i+4)) { // Frequency = 0
        SetFrequency(0);
        for(int k = 1; k <= 3; k++) {
          trellis.clrLED(k*4);
        }
      }
      else {                                // Adjust Frequency
        SetFrequency(row);
        for(int k = 1; k <= row; k++) {
          trellis.setLED(k*4);
        }
        for(int k = row+1; k <= 3; k++) {
          trellis.clrLED(k*4);
        }
      }
    }
    else {                                  // Adjust Frequency 
      SetFrequency(row);
      for(int k = 1; k <= row; k++) {
        trellis.setLED(k*4);
      }
      for(int k = row+1; k <= 3; k++) {
        trellis.clrLED(k*4);
      }
    }
  }
  // Set water level
  else { // Interface design silimar to a slider
    if (trellis.isLED(i)) {
      if(col == 3 || !trellis.isLED(i+1)) { // No water
        SetWater(row, 0);
        for(int k = 1; k <= 3; k++) {
          trellis.clrLED(k+row*4);
        }
      }
      else {                                // Adjust water level
        SetWater(row, col);
        for(int k = 1; k <= col; k++) {
          trellis.setLED(k+row*4);
        }
        for(int k = col+1; k <= 3; k++) {
          trellis.clrLED(k+row*4);
        }
      }
    }
    else {                                  // Adjust water level 
      SetWater(row, col);
      for(int k = 1; k <= col; k++) {
        trellis.setLED(k+row*4);
      }
      for(int k = col+1; k <= 3; k++) {
        trellis.clrLED(k+row*4);
      }
    }
  }
}

// Helper function that starts watering the plant right now
void Execute() {
  // Close all valves
  for(int i = 0; i < 4; i++) {
    CloseValve(i);
  }

  // only water if frequency > 0 and total wateramount is > 0
  shouldWater = (waterFrequency > 0 && WaterAmountNonZero());
  waitingForNextCycle = false;
  
  if(shouldWater) {
    Serial.println("Start watering");
    TurnOnPump();
    // Initialize global variables 
    GetReadyForPlant(0);
  }
  else {
    TurnOffPump();
    OpenAllValves();
    Serial.println("Stop watering");
  }
}

void GetReadyForPlant(int id) {
  Serial.print("Get ready for plant "); Serial.println(id);
  microTimer = millis()/1000;
  quarterMicroTimer = GetWaterTimeForPlant(id);
  Serial.print("Water amount(seconds): "); Serial.println(quarterMicroTimer);
  wateringPlantId = id;
  OpenValveForPlant(id);
}

void WaitForNextCycle() {
  Serial.println("Wait for next cycle... ");
  TurnOffPump();
  OpenAllValves();
  shouldWater = false;
  waitingForNextCycle = true;
  macroTimer = millis()/1000;
  quarterMacroTimer = waterFrequency * CYCLELENGTH;
  Serial.print("Wait time(seconds): "); Serial.println(quarterMacroTimer);
}

int GetWaterTimeForPlant(int id) {
  return waterAmount[id] * WATERAMOUNTUNIT;
}

// Change the global watering frequency per minute
void SetFrequency(int frequency) {
  Serial.print("Set time: "); Serial.println(frequency);
  waterFrequency = frequency;
}

// Set the array of watering amount
void SetWater(int plantId, int water) {
  Serial.print("Set water: "); Serial.print(plantId); Serial.print(" "); Serial.println(water);
  waterAmount[plantId] = water;
}

// Only close one valve
void CloseValve(int valve) {
  // Don't close if is already closed
  if(!valveStates[valve]) return;
  else valveStates[valve] = false;
  Serial.print("Close "); Serial.println(valve);
  switch(valve) {
    case 0:
      valve0.write(VALVE_CLOSE_POS);
      break;
    case 1:
      valve1.write(VALVE_CLOSE_POS);
      break;
    case 2:
      valve2.write(VALVE_CLOSE_POS);
      break;
    case 3:
      valve3.write(VALVE_CLOSE_POS);
      break;
    default:
      break;
  }
  delay(50);
}

void OpenValve(int valve) {
  // Don't open if is already open
  if(valveStates[valve]) return;
  else valveStates[valve] = true;
  Serial.print("Open "); Serial.println(valve);
  switch(valve) {
    case 0:
      valve0.write(VALVE_OPEN_POS);
      break;
    case 1:
      valve1.write(VALVE_OPEN_POS);
      break;
    case 2:
      valve2.write(VALVE_OPEN_POS);
      break;
    case 3:
      valve3.write(VALVE_OPEN_POS);
      break;
    default:
      break;
  }
  delay(50);
}

// Helper function returns true if any plant water amount is > 0
bool WaterAmountNonZero() {
  for(int i = 0; i < 4; i++) {
    if(waterAmount[i] > 0) return true;
  }
  return false;
}

void CheckSwitches() {
//  return;
  if(!digitalRead(VALVE0_SWITCH_PIN)) {
    OpenValveForPlant(0);
  }
  if(!digitalRead(VALVE1_SWITCH_PIN)) {
    OpenValveForPlant(1);
  }
  if(!digitalRead(VALVE2_SWITCH_PIN)) {
    OpenValveForPlant(2);
  }
  if(!digitalRead(VALVE3_SWITCH_PIN)) {
    OpenValveForPlant(3);
  }
}

// Open valve for only plant id, close all other valves
void OpenValveForPlant(int id) {
  for(int i = 0; i < 4; i++) {
    if(i == id) OpenValve(i);
    else CloseValve(i);
  }
  Serial.print("Opening valve for only plant "); Serial.println(id);
}

void TurnOnPump() {
  digitalWrite(PUMP_PIN, HIGH);
  Serial.println("Pump on");
}

void TurnOffPump() {
  digitalWrite(PUMP_PIN, LOW);
  Serial.println("Pump off");
}

// Blink the LED
void BlinkLED(int i) {
  trellis.clrLED(i);
  trellis.writeDisplay();
  delay(blinkLEDdelay);
  trellis.setLED(i);
  trellis.writeDisplay();
  delay(blinkLEDdelay);
  trellis.clrLED(i);
  trellis.writeDisplay();
  delay(blinkLEDdelay);
  trellis.setLED(i);
  trellis.writeDisplay();
  delay(blinkLEDdelay);
  trellis.clrLED(i);
  trellis.writeDisplay();
  delay(blinkLEDdelay);
  trellis.setLED(i);
  trellis.writeDisplay();
  delay(blinkLEDdelay);
}

void OpenAllValves() {
  Serial.println("Open all");
  for(int i = 0; i < 4; i++){
    OpenValve(i);
  }
}
