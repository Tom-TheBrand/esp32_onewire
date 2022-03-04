/*
 * ds18b20.c
 *
 *  Created on: 01.03.2022
 *      Author: tb_po
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "onewire.h"
#include "ds18b20.h"

#define OWROM_MATCH 0x55

#define FUNC_RDSCRATCH	0xBE
#define FUNC_WRSCRATCH	0x4E
#define FUNC_CONV		0x44

static int resolutionDivider = 4;

int DSconfigure(const uint8_t *deviceAddress)
{
	// send the reset command and fail fast
	int b = OWTouchReset();
	if (b == 0)
		return DS_NODEV;

	OWWriteByte(OWROM_MATCH);
	for (int cc = 0; cc < 8; cc++)
		OWWriteByte(*(deviceAddress + cc));

	OWWriteByte(FUNC_WRSCRATCH);
	OWWriteByte(0x00); // TH register
	OWWriteByte(0x00); // TL register
	OWWriteByte(DS_RESOLUTION); // configuration register, set to 10bit
	resolutionDivider = 4;

	return DS_OK;
}

int DSreadScratchPad(const uint8_t *deviceAddress, uint8_t *scratchPad)
{
	// send the reset command and fail fast
	int b = OWTouchReset();
	if (b == 0)
		return DS_NODEV;

	OWWriteByte(OWROM_MATCH);
	for (int cc = 0; cc < 8; cc++)
		OWWriteByte(*(deviceAddress + cc));

	OWWriteByte(FUNC_RDSCRATCH);
	for (int cc = 0; cc < 9; cc++)
		if (cc < 8)
			*(scratchPad + cc) = OWReadByte();

	return DS_OK;
}

int DSstartConversion(const uint8_t *deviceAddress, const uint8_t waitComplete)
{
	uint16_t timeout = DS_CONVTIMEOUT;

	// send the reset command and fail fast
	int b = OWTouchReset();
	if (b == 0)
		return DS_NODEV;

	OWWriteByte(OWROM_MATCH);
	for (int cc = 0; cc < 8; cc++)
		OWWriteByte(*(deviceAddress + cc));

	OWWriteByte(FUNC_CONV);

	if (waitComplete)
	{
		while (!OWReadByte() && --timeout)
			vTaskDelay(1 / portTICK_PERIOD_MS);
		if (timeout == 0)
			return DS_TIMEOUT;
	}

	return DS_OK;
}

int DSreadTemperature(const uint8_t *deviceAddress, float *temperature)
{
	int rslt;
	uint8_t scratchPad[8];

	*temperature = 0;

	if ((rslt = DSstartConversion(deviceAddress, 1)) != DS_OK)
		return rslt;

	if ((rslt = DSreadScratchPad(deviceAddress, scratchPad)) != DS_OK)
		return rslt;

	*temperature = (float) ((((int) scratchPad[1] << 8) + scratchPad[0]) >> 2) / resolutionDivider;
	return DS_OK;
}
