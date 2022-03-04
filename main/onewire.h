/*
 * onewire.h
 *
 * Library for OneWire communication implemented as software
 * Developed for ESP32 nanoMCU (ESP32-WROOM-32)
 *
 * IMPORTANT: 1-Wire timing is realized by using hardware timer.
 * The timer ISR is pinned to CPU-1, so take care to keep enough CPU cycles free (during 1-Wire operation)!
 *
 * Parts of the library are based on:
 * https://www.maximintegrated.com/en/design/technical-documents/app-notes/1/126.html
 * https://www.maximintegrated.com/en/design/technical-documents/app-notes/1/187.html
 *
 *  Created on: 26.02.2022
 *      Author: thebrand
 */


#ifndef MAIN_ONEWIRE_H_
#define MAIN_ONEWIRE_H_

#include "driver/gpio.h"


#define OWPin GPIO_NUM_19

#define OWTIMER_GROUP TIMER_GROUP_0
#define OWTIMER_TIMER TIMER_0


unsigned char ROM_NO[8];


// Setup Port (OWPin), setup Timer and pin isr to cpu 1
int OWInit();

// https://www.maximintegrated.com/en/design/technical-documents/app-notes/1/126.html
int OWTouchReset(void);
void OWWriteByte(int data);
int OWReadByte(void);
int OWTouchByte(int data); // not tested
void OWBlock(unsigned char *data, int data_len); // not tested

// https://www.maximintegrated.com/en/design/technical-documents/app-notes/1/187.html
int  OWFirst();
int  OWNext();
int  OWVerify(); // not tested

void OWTargetSetup(unsigned char family_code);
void OWFamilySkipSetup(); // not tested

unsigned char docrc8(unsigned char value); // not tested

#endif /* MAIN_ONEWIRE_H_ */
