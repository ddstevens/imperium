/*
Imperium - An all in one arcade USB adapter
Copyright (C) 2022  Doug Stevens

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <Arduino.h>
#include "Adafruit_TinyUSB.h"

// Choose your microprocessor, uncomment only one!
#include "weact-f411.h"
// #include "devebox-h743.h"

// #define DEBUG

// Debounce counter max. How many checks after a button is released before sending the release.
#define DBC_MAX 32 // Set to 1 to disable

// Debounce counter for each input.
uint8_t input[64]  = {
  DBC_MAX, DBC_MAX, DBC_MAX, DBC_MAX, DBC_MAX, DBC_MAX, DBC_MAX, DBC_MAX,
  DBC_MAX, DBC_MAX, DBC_MAX, DBC_MAX, DBC_MAX, DBC_MAX, DBC_MAX, DBC_MAX,
  DBC_MAX, DBC_MAX, DBC_MAX, DBC_MAX, DBC_MAX, DBC_MAX, DBC_MAX, DBC_MAX,
  DBC_MAX, DBC_MAX, DBC_MAX, DBC_MAX, DBC_MAX, DBC_MAX, DBC_MAX, DBC_MAX,
  DBC_MAX, DBC_MAX, DBC_MAX, DBC_MAX, DBC_MAX, DBC_MAX, DBC_MAX, DBC_MAX,
  DBC_MAX, DBC_MAX, DBC_MAX, DBC_MAX, DBC_MAX, DBC_MAX, DBC_MAX, DBC_MAX,
  DBC_MAX, DBC_MAX, DBC_MAX, DBC_MAX, DBC_MAX, DBC_MAX, DBC_MAX, DBC_MAX,
  DBC_MAX, DBC_MAX, DBC_MAX, DBC_MAX, DBC_MAX, DBC_MAX, DBC_MAX, DBC_MAX,
};

// HID Report descriptor
static const uint8_t hidReportDescriptor[] PROGMEM = {

0x06, 0x00, 0xFF,  // Usage Page (Vendor Defined 0xFF00)
0xA1, 0x01,        // Collection (Application)
0x15, 0x00,        //   Logical Minimum (0)
0x26, 0xFF, 0x00,  //   Logical Maximum (255)
0x75, 0x08,        //   Report Size (8)
0x95, 0x18,        //   Report Count (24)
0x09, 0x01,        //   Usage (0x01)
0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
0x09, 0x01,        //   Usage (0x01)
0x91, 0x02,        //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
0xC0,              // End Collection

};

// HID Report
typedef struct {
  uint32_t inputs[2];
  int8_t spinner[8];
  uint8_t paddle[8];
} HidReport;

// Common encoder handling code

int8_t lastEncoderState[4] = {0, 0, 0, 0};
 
void updateEncoder(int8_t idx)
{
  uint16_t portA = GPIOA->IDR;
  uint8_t a = !(portA & encoderPinMask[idx][1]);
  uint8_t b = !(portA & encoderPinMask[idx][0]);

  int8_t currState = (a << 1) | b;
  int8_t combinedStates  = (lastEncoderState[idx] << 2) | currState;

  // A leads B, A0 -> AB -> 0B -> 00
  if (combinedStates == 0b0010 || combinedStates == 0b1011 || combinedStates == 0b1101 || combinedStates == 0b0100)
  {
    if (paddle[idx] < paddleMax[idx] - 1) paddle[idx]++;
    spinner[idx]++;
  }
   
  // B leads A, 0B -> AB -> A0 -> 00
  if (combinedStates == 0b0001 || combinedStates == 0b0111 || combinedStates == 0b1110 || combinedStates == 0b1000)
  {
    if (paddle[idx] > 0) paddle[idx]--;
    spinner[idx]--;
  }

  lastEncoderState[idx] = currState;
}

// Interrupt service routines

void encoder0()
{
  updateEncoder(0);
}

void encoder1()
{
  updateEncoder(1);
}

void encoder2()
{
  updateEncoder(2);
}

void encoder3()
{
  updateEncoder(3);
}

void encoder4()
{
  updateEncoder(4);
}

void encoder5()
{
  updateEncoder(5);
}

void encoder6()
{
  updateEncoder(6);
}

void encoder7()
{
  updateEncoder(7);
}

// Setup

Adafruit_USBD_HID usb_hid(hidReportDescriptor, sizeof(hidReportDescriptor), HID_ITF_PROTOCOL_NONE, 1, false);

void setup()
{
  // USB start
  TinyUSB_Device_Init(0);
  TinyUSBDevice.setID(0x1234,0x0000);
  TinyUSBDevice.setManufacturerDescriptor("Doug");
  TinyUSBDevice.setProductDescriptor("Imperium");
  TinyUSBDevice.setSerialDescriptor("Imperium V1.0");
  usb_hid.begin();

  #ifdef DEBUG
  SerialTinyUSB.begin(115200);
  #endif
  
  while(!TinyUSBDevice.mounted())
  {
    TinyUSB_Device_Task();
    TinyUSB_Device_FlushCDC();
  }
  
  // Button and joystick pins  
  for (uint8_t i = 0; i < NUM_A_INPUTS; i++)
    pinMode(inputPinA[i], INPUT_PULLUP);
     
  for (uint8_t i = 0; i < NUM_B_INPUTS; i++)
    pinMode(inputPinB[i], INPUT_PULLUP);
     
  for (uint8_t i = 0; i < NUM_C_INPUTS; i++)
    pinMode(inputPinC[i], INPUT_PULLUP);

  // Encoders setup and init
  for (uint8_t i = 0; i < NUM_ENCODERS; i++)
  {
    pinMode(encoderPin[i][0], INPUT_PULLUP);
    pinMode(encoderPin[i][1], INPUT_PULLUP);
    updateEncoder(i);  // Establish a "lastState" so next update will be correct
    spinner[i] = 0;
  }

  // Register interrupt service routines
  if (NUM_ENCODERS > 0) {
    attachInterrupt(digitalPinToInterrupt(encoderPin[0][0]), encoder0, CHANGE);
    attachInterrupt(digitalPinToInterrupt(encoderPin[0][1]), encoder0, CHANGE);
  }
  if (NUM_ENCODERS > 1) {
    attachInterrupt(digitalPinToInterrupt(encoderPin[1][0]), encoder1, CHANGE);
    attachInterrupt(digitalPinToInterrupt(encoderPin[1][1]), encoder1, CHANGE);
  }
  if (NUM_ENCODERS > 2) {
    attachInterrupt(digitalPinToInterrupt(encoderPin[2][0]), encoder2, CHANGE);
    attachInterrupt(digitalPinToInterrupt(encoderPin[2][1]), encoder2, CHANGE);
  }
  if (NUM_ENCODERS > 3) {
    attachInterrupt(digitalPinToInterrupt(encoderPin[3][0]), encoder3, CHANGE);
    attachInterrupt(digitalPinToInterrupt(encoderPin[3][1]), encoder3, CHANGE);
  }
  if (NUM_ENCODERS > 4) {
    attachInterrupt(digitalPinToInterrupt(encoderPin[4][0]), encoder4, CHANGE);
    attachInterrupt(digitalPinToInterrupt(encoderPin[4][1]), encoder4, CHANGE);
  }
  if (NUM_ENCODERS > 5) {
    attachInterrupt(digitalPinToInterrupt(encoderPin[5][0]), encoder5, CHANGE);
    attachInterrupt(digitalPinToInterrupt(encoderPin[5][1]), encoder5, CHANGE);
  }
  if (NUM_ENCODERS > 6) {
    attachInterrupt(digitalPinToInterrupt(encoderPin[6][0]), encoder6, CHANGE);
    attachInterrupt(digitalPinToInterrupt(encoderPin[6][1]), encoder6, CHANGE);
  }
  if (NUM_ENCODERS > 7) {
    attachInterrupt(digitalPinToInterrupt(encoderPin[7][0]), encoder7, CHANGE);
    attachInterrupt(digitalPinToInterrupt(encoderPin[7][1]), encoder7, CHANGE);
  }
}

// Main loop

HidReport report;
HidReport last_report;

uint16_t portA = 0;
uint16_t portB = 0;
uint16_t portC = 0;

uint32_t lastSendTime = micros();

void loop()
{
  // Calculate paddle/spinner values for each controller
  for (uint8_t i = 0; i < NUM_ENCODERS; i++)
  {
    // paddle
    report.paddle[i] = (256 * paddle[i]) / paddleMax[i];
    
    // spinner
    report.spinner[i] += spinner[i];
    spinner[i] = 0;
  }

  // Read / debounce inputs (joysticks and buttons)  
  portA = GPIOA->IDR;
  portB = GPIOB->IDR;
  portC = GPIOC->IDR;

  uint8_t j = 0;
  for (uint8_t i = 0; i < NUM_A_INPUTS; i++, j++)
    if (!(portA & inputPinMaskA[i])) input[j] = 0; else if (input[j]  < DBC_MAX) input[j]++;

  for (uint8_t i = 0; i < NUM_B_INPUTS; i++, j++)
    if (!(portB & inputPinMaskB[i])) input[j] = 0; else if (input[j]  < DBC_MAX) input[j]++;
  
  for (uint8_t i = 0; i < NUM_C_INPUTS; i++, j++)
    if (!(portC & inputPinMaskC[i])) input[j] = 0; else if (input[j]  < DBC_MAX) input[j]++;
  
  // Pack inputs
  report.inputs[0] = 0;
  report.inputs[1] = 0;
  for (uint8_t i = 0; i < j; i++)
    if (input[i] < DBC_MAX) report.inputs[i / 32] |= (1 << (i % 32));
  
  // Send the data if it's been long enough
  if (micros() - lastSendTime >= 1000)
  {
    if (memcmp(&last_report, &report, sizeof(HidReport)))
    {
      memcpy(&last_report, &report, sizeof(HidReport));

      #ifdef DEBUG
      SerialTinyUSB.print((String) "Inputs: ");
      printBits(report.inputs[1]);
      SerialTinyUSB.print(" ");
      printBits(report.inputs[0]);
      for (uint8_t i = 0; i < NUM_ENCODERS; i++) {
        SerialTinyUSB.print((String)" " + report.paddle[i] + " " + report.spinner[i]);
      }
      SerialTinyUSB.println("");
      #endif
      
      lastSendTime = micros();
  
      usb_hid.sendReport(0, &report, sizeof(HidReport));
      
      report.inputs[0] = 0;      
      report.inputs[1] = 0;      
      for (uint8_t i = 0; i < NUM_ENCODERS; i++) {
        report.spinner[i] = 0;
      }
    }
  }
  
  TinyUSB_Device_Task();
  TinyUSB_Device_FlushCDC();
}

void printBits(uint32_t in)
{
  for (int b = 31; b >= 0; b--) {
    SerialTinyUSB.print(bitRead(in, b));
  }
}
