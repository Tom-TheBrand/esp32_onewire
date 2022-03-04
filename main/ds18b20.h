/*
 * ds18b20.h
 *
 * Library to communicate with DS18B20 TEmperatur sensors.
 *
 *  Created on: 01.03.2022
 *      Author: thebrand
 *
 * Datasheet: https://cdn.shopify.com/s/files/1/1509/1638/files/DS18B20_1mCable_datasheet.pdf?v=1644321082
 */

#ifndef MAIN_DS18B20_H_
#define MAIN_DS18B20_H_

#include "onewire.h"

#define DS_OK		0
#define DS_ERR		1
#define DS_TIMEOUT	2
#define DS_NODEV	3

#define DS_RESOLUTION 0x20
#define DS_CONVTIMEOUT 200


// Reads DS18B20's Scratchpad (Byte 0 - 7), CRC is not returned
// deviceAddress: pointer to 8-Byte Array
// scratchPad: pointer to 8-Byte, retrieving Data#
// return: DS_OK, DS_NODEV
int DSreadScratchPad(const uint8_t* deviceAddress, uint8_t* scratchPad);

// Issue Startconversion Command to DS18B20
// deviceAddress: pointer to 8-Byte Array
// if waitComplete <> 0, function will wait up to DS_CONVTIMEOUT [ms] for conversion to complete
// return: DS_OK, DS_NODEV, DS_TIMEOUT
int DSstartConversion(const uint8_t* deviceAddress, uint8_t waitComplete);


// Setup DS18B20 to use 10bit resolution (DS_RESOLUTION; if changed, also adopt DS_CONVTIMEOUT!)
// return: DS_OK, DS_NODEV
int DSconfigure(const uint8_t* deviceAddress);

#endif /* MAIN_DS18B20_H_ */
