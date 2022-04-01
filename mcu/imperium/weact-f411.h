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

// Total number of encoder pairs (1 per spinner, 2 per trackball)
#define NUM_ENCODERS 4

// Spinner states per revolution
#define SPINNER_MAX 1200    // Ultimarc spinner

// Trackball states per revolution
#define TRACKBALL_MAX 1860  // HAPP 3" Trackball (self measured)

// Initial paddle and spinner values
int16_t spinner[NUM_ENCODERS] = {0, 0, 0, 0};
uint32_t paddle[NUM_ENCODERS] = {SPINNER_MAX / 2, SPINNER_MAX / 2, TRACKBALL_MAX / 2, TRACKBALL_MAX / 2};

// Keep track of the max for each encoder, used for paddle calculations later 
const uint16_t paddleMax[4] = {SPINNER_MAX, SPINNER_MAX, TRACKBALL_MAX, TRACKBALL_MAX};

// Pins for the rotary encoders
const int8_t encoderPin[NUM_ENCODERS][2] = {{PA7, PA8}, {PA2, PA1}, {PA3, PA4}, {PA6, PA5}};
const int16_t encoderPinMask[NUM_ENCODERS][2] = {{1<<7, 1<<8}, {1<<2, 1<<1}, {1<<3, 1<<4}, {1<<6, 1<<5}};

// Pins for the buttons / joysticks split by port
#define NUM_A_INPUTS 6
const int8_t inputPinA[NUM_A_INPUTS] = {PA0, PA9, PA10, PA13, PA14, PA15};
const int16_t inputPinMaskA[NUM_A_INPUTS] = {1<<0, 1<<9, 1<<10, 1<<13, 1<<14, 1<<15};

#define NUM_B_INPUTS 14
const int8_t inputPinB[NUM_B_INPUTS] = {PB0, PB1, PB3, PB4, PB5, PB6, PB7, PB8, PB9, PB10, PB12, PB13, PB14, PB15};
const int16_t inputPinMaskB[NUM_B_INPUTS] = {1<<0, 1<<1, 1<<3, 1<<4, 1<<5, 1<<6, 1<<7, 1<<8, 1<<9, 1<<10, 1<<12, 1<<13, 1<<14, 1<<15};

#define NUM_C_INPUTS 3
const int8_t inputPinC[NUM_C_INPUTS] = {PC13, PC14, PC15};
const int16_t inputPinMaskC[NUM_C_INPUTS] = {1<<13, 1<<14, 1<<15};
