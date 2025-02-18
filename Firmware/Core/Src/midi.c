/*
 * midi.c
 *
 *  Created on: Feb 16, 2025
 *      Author: roland.mueller.1994@gmx.de
 */

#include "midi.h"
#include <stdbool.h>
#include <string.h>

uint8_t midiRingBuffer[MIDI_H_RING_BUFFER_LENGTH][MIDI_H_PACKAGE_SIZE];

uint8_t writePointer;
uint8_t readPointer;

void midiInit() {
	writePointer = 0;
	readPointer = 0;
}

void midiGetMessage(struct midiMsg *msg) {
	/*
	 * Get a midi message if possible. If the type of the message is unknown (not yet implemented)
	 * or there is no message, msg->msgType will be MIDI_NA
	 *
	 * Midi messages are structured as follows:
	 * Byte0[7:4]: Message type (CC, PC, Note on, ...)
	 * Byte0[3:0]: Channel
	 * Byte1: First value -> Controller number for CC, Program number for PC
	 * Byte2: Second value -> Controller value for CC
	 */

	if ((readPointer + 1) % MIDI_H_RING_BUFFER_LENGTH == writePointer) {
		msg->msgType = MIDI_NA;
	} else {
		uint8_t msgType = (midiRingBuffer[readPointer][0] & 0xF0) >> 4;
		uint8_t channel = midiRingBuffer[readPointer][0] & 0x0F;
		if (msgType == 0xb) {
			// CC Message
			msg->msgType = MIDI_CC;

		} else if (msgType == 0xc) {
			// PC Message
			msg->msgType = MIDI_CC;
		} else {
			msg->msgType = MIDI_NA;
		}
		msg->channel = channel;
		msg->val1 = midiRingBuffer[readPointer][1];
		msg->val2 = midiRingBuffer[readPointer][2];
		readPointer++;
		readPointer = readPointer % MIDI_H_RING_BUFFER_LENGTH;
	}
}

uint8_t* midiMessageReceived() {
	/*
	 * Callback from UART after message was received to update write pointer.
	 *
	 * Will return the new buffer for next UART interrupt.
	 */
	uint8_t* retVal = midiRingBuffer[writePointer];
	writePointer = (writePointer + 1) % MIDI_H_RING_BUFFER_LENGTH;
	return retVal;
}

uint8_t* midiGetCurrentBuffer() {
	/*
	 * Get the current buffer where the current transfer shall be stored.
	 *
	 * This can e.g. be used if UART has to be restarted in case an error occured.
	 */
	return midiRingBuffer[writePointer];
}

