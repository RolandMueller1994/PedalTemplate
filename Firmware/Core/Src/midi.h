/*
 * midi.h
 *
 *  Created on: Feb 16, 2025
 *      Author: roland
 */

#ifndef SRC_MIDI_H_
#define SRC_MIDI_H_

#include "stm32f0xx_hal.h"

#define MIDI_H_RING_BUFFER_LENGTH 32
#define MIDI_H_PACKAGE_SIZE 10

#define MIDI_NA 0
#define MIDI_CC 1
#define MIDI_PC 2

struct midiMsg {
	uint8_t msgType;
	uint8_t channel;
	uint8_t val1;
	uint8_t val2;
};

void midiInit();

void midiGetMessage(struct midiMsg* msg);

uint8_t* midiMessageReceived();

#endif /* SRC_MIDI_H_ */
