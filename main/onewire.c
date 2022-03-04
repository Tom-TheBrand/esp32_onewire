/*
 * onewire.c
 *
 *  Created on: 26.02.2022
 *      Author: thebrand
 */
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/timer.h"
#include "esp_log.h"

#include "onewire.h"

#define TAG "ONEWIRE"
#define STACK_SIZE 2000

#define BITMODE_RESET 0xFF
#define BITMODE_SENDB 0x01
#define BITMODE_READB 0x02
#define BITMODE_SCAN1 0x11
#define BITMODE_SCAN2 0x12

#define EVENTGROUP_TMR	0x01
#define EVENTGROUP_INI	0x02

typedef struct
{
	volatile uint8_t dataByte;
	volatile uint8_t bitIdx;
	volatile uint8_t bitMode;
	volatile uint16_t timing;
} t_owdata;

static volatile t_owdata owSendData;
static volatile EventGroupHandle_t x1WTimer;

static bool IRAM_ATTR timer_group_isr_callback(void *args)
{
	BaseType_t high_task_awoken = pdFALSE;
	uint16_t time;
	timer_pause(OWTIMER_GROUP, OWTIMER_TIMER);

	if (owSendData.timing == 70 || owSendData.timing >= 960)
	{
		owSendData.timing = 0;
		owSendData.bitIdx++;

		if ((owSendData.bitIdx < 8 && (owSendData.bitMode == BITMODE_SENDB || owSendData.bitMode == BITMODE_READB)) ||
			(owSendData.bitIdx < 2 && owSendData.bitMode == BITMODE_SCAN2) ||
			(owSendData.bitIdx < 1 && owSendData.bitMode == BITMODE_SCAN1))
			timer_start(OWTIMER_GROUP, OWTIMER_TIMER);
		else
		{
			if(owSendData.bitMode == BITMODE_SCAN2) owSendData.dataByte >>= 6;
			xEventGroupSetBits(x1WTimer, EVENTGROUP_TMR);
			owSendData.bitIdx = 0;
		}
		return high_task_awoken;
	}

	if (owSendData.timing == 0)
	{	// teil 1: send, read & reset
		gpio_set_level(OWPin, 0);
		if (owSendData.bitMode == BITMODE_SENDB || owSendData.bitMode == BITMODE_SCAN1)
			time = ((owSendData.dataByte & 0x01) == 1) ? 6 : 60;
		else if (owSendData.bitMode == BITMODE_READB || owSendData.bitMode == BITMODE_SCAN2)
			time = 6;
		else if (owSendData.bitMode == BITMODE_RESET)
			time = 480;
		else
			time = 10; // fallback, darf nicht vorkommen!
	}
	else if (owSendData.timing == 6 || owSendData.timing == 60)
	{	// teil 2: send, read
		gpio_set_level(OWPin, 1);
		if (owSendData.bitMode == BITMODE_SENDB || owSendData.bitMode == BITMODE_SCAN1)
		{
			time = ((owSendData.dataByte & 0x01) == 1) ? 64 : 10;
			owSendData.dataByte >>= 1;
		}
		else
		{
			time = 9;
		}
	}
	else if (owSendData.timing == 15)
	{ 	// teil 3: der read sequenz
		owSendData.dataByte >>= 1;
		if (gpio_get_level(OWPin))
			owSendData.dataByte |= 0x80;
		time = 55;
	}
	else if (owSendData.timing == 480)
	{	// teil 2 der reset sequenz
		gpio_set_level(OWPin, 1);
		time = 70;
	}
	else if (owSendData.timing == 550)
	{	// teil 3 der reset sequenz
		owSendData.dataByte = (gpio_get_level(OWPin) & 0x01) ^ 0x01;
		time = 410;
	}
	else
	{	// fallback, darf nicht vorkommen; wenn beim rechnen was passiert!
		time = 10;
	}
	owSendData.timing += time;
		timer_group_set_alarm_value_in_isr(OWTIMER_GROUP, OWTIMER_TIMER, time);
	timer_start(OWTIMER_GROUP, OWTIMER_TIMER);

	return high_task_awoken == pdTRUE; // return whether we need to yield at the end of ISR
}

static void timerInitTask(void *pvParameters)
{	// eigene funktion, damit ISR auf cpu1 gebunden werden kann

	/* Select and initialize basic parameters of the timer */
	timer_config_t config =
	{ .divider = 80, .counter_dir = TIMER_COUNT_UP, .counter_en = TIMER_PAUSE, .alarm_en = TIMER_ALARM_EN, .auto_reload = 1 }; // default clock source is APB

	timer_init(OWTIMER_GROUP, OWTIMER_TIMER, &config);
	timer_set_counter_value(OWTIMER_GROUP, OWTIMER_TIMER, 0);

	timer_enable_intr(OWTIMER_GROUP, OWTIMER_TIMER);
	timer_isr_callback_add(OWTIMER_GROUP, OWTIMER_TIMER, timer_group_isr_callback, NULL, 0);

	xEventGroupSetBits(x1WTimer, EVENTGROUP_INI);
	vTaskDelete(NULL);
}


int OWInit()
{
	gpio_set_direction(OWPin, GPIO_MODE_INPUT_OUTPUT_OD);
	//gpio_set_pull_mode(OWPin, GPIO_PULLUP_ONLY);

	owSendData.bitIdx = 0;
	owSendData.dataByte = 0x00;
	owSendData.bitMode = 0;
	owSendData.timing = 0;

	x1WTimer = xEventGroupCreate();
	if (x1WTimer == NULL)
	{
		ESP_LOGI(TAG, "Could not create EventGroupHandle (OOM?)");
		return 1;
	}

	xTaskCreatePinnedToCore(timerInitTask, "TimerINIT", STACK_SIZE, NULL, tskIDLE_PRIORITY, NULL, 1);
	xEventGroupWaitBits(x1WTimer, EVENTGROUP_INI, pdTRUE, pdFALSE, 100 / portTICK_PERIOD_MS);
	return 0;
}

//-----------------------------------------------------------------------------
// Generate a 1-Wire reset, return 1 if no presence detect was found,
// return 0 otherwise.
// (NOTE: Does not handle alarm presence from DS2404/DS1994)
//
int OWTouchReset(void)
{
	owSendData.bitMode = BITMODE_RESET;
	owSendData.dataByte = 0;
	timer_set_alarm_value(OWTIMER_GROUP, OWTIMER_TIMER, 1);
	timer_start(OWTIMER_GROUP, OWTIMER_TIMER);
	xEventGroupWaitBits(x1WTimer, EVENTGROUP_TMR, pdTRUE, pdFALSE, 100 / portTICK_PERIOD_MS);
	return owSendData.dataByte;
}

////-----------------------------------------------------------------------------
//// Send a 1-Wire write bit. Provide 10us recovery time.
////
//void OWWriteBit(int bit)
//{
//	if (bit)
//	{
//		// Write '1' bit
//		gpio_set_level(OWPin, 0x00); // Drives DQ low
//		tickDelay(A);
//		gpio_set_level(OWPin, 0x01); // Releases the bus
//		tickDelay(B); // Complete the time slot and 10us recovery
//	}
//	else
//	{
//		// Write '0' bit
//		gpio_set_level(OWPin, 0x00); // Drives DQ low
//		tickDelay(C);
//		gpio_set_level(OWPin, 0x01); // Releases the bus
//		tickDelay(D);
//	}
//}
//
////-----------------------------------------------------------------------------
//// Read a bit from the 1-Wire bus and return it. Provide 10us recovery time.
////
//int OWReadBit(void)
//{
//	int result;
//
//	gpio_set_level(OWPin, 0x00); // Drives DQ low
//	tickDelay(A);
//	gpio_set_level(OWPin, 0x01); // Releases the bus
//	tickDelay(E);
//	result = gpio_get_level(OWPin); // Sample the bit value from the slave
//	tickDelay(F); // Complete the time slot and 10us recovery
//
//	return result;
//}

//-----------------------------------------------------------------------------
// Write 1-Wire data byte
//
void OWWriteByte(int data)
{
	owSendData.dataByte = data;
	owSendData.bitMode = BITMODE_SENDB;
	timer_set_alarm_value(OWTIMER_GROUP, OWTIMER_TIMER, 1);
	timer_start(OWTIMER_GROUP, OWTIMER_TIMER);

	xEventGroupWaitBits(x1WTimer, EVENTGROUP_TMR, pdTRUE, pdFALSE, 100 / portTICK_PERIOD_MS);
}

//-----------------------------------------------------------------------------
// Read 1-Wire data byte and return it
//
int OWReadByte(void)
{
	owSendData.dataByte = 0;
	owSendData.bitMode = BITMODE_READB;
	timer_set_alarm_value(OWTIMER_GROUP, OWTIMER_TIMER, 1);
	timer_start(OWTIMER_GROUP, OWTIMER_TIMER);

	xEventGroupWaitBits(x1WTimer, EVENTGROUP_TMR, pdTRUE, pdFALSE, 100 / portTICK_PERIOD_MS);
	return owSendData.dataByte;
}


// global search state
static int LastDiscrepancy;
static int LastFamilyDiscrepancy;
static int LastDeviceFlag;
static unsigned char crc8;

//--------------------------------------------------------------------------
// Perform the 1-Wire Search Algorithm on the 1-Wire bus using the existing
// search state.
// Return TRUE  : device found, ROM number in ROM_NO buffer
//        FALSE : device not found, end of search
//
static int OWSearch()
{
	int id_bit_number;
	int last_zero, rom_byte_number, search_result;
	int id_bit, cmp_id_bit;
	unsigned char rom_byte_mask, search_direction;

	// initialize for search
	id_bit_number = 1;
	last_zero = 0;
	rom_byte_number = 0;
	rom_byte_mask = 1;
	search_result = 0;
	crc8 = 0;

	// if the last call was not the last one
	if (!LastDeviceFlag)
	{
		// 1-Wire reset
		if (!OWTouchReset())
		{
			// reset the search
			LastDiscrepancy = 0;
			LastDeviceFlag = 0;
			LastFamilyDiscrepancy = 0;
			return 0;
		}

		// issue the search command
		OWWriteByte(0xF0);

		// loop to do the search
		do
		{
			owSendData.bitMode = BITMODE_SCAN2;
			timer_set_alarm_value(OWTIMER_GROUP, OWTIMER_TIMER, 1);
			timer_start(OWTIMER_GROUP, OWTIMER_TIMER);
			xEventGroupWaitBits(x1WTimer, EVENTGROUP_TMR, pdTRUE, pdFALSE, 100 / portTICK_PERIOD_MS);

			// read a bit and its complement
			id_bit = owSendData.dataByte & 0x01;
			cmp_id_bit = (owSendData.dataByte >> 1) & 0x01;

			// check for no devices on 1-wire
			if ((id_bit == 1) && (cmp_id_bit == 1))
				break;
			else
			{
				// all devices coupled have 0 or 1
				if (id_bit != cmp_id_bit)
					search_direction = id_bit;  // bit write value for search
				else
				{
					// if this discrepancy if before the Last Discrepancy
					// on a previous next then pick the same as last time
					if (id_bit_number < LastDiscrepancy)
						search_direction = ((ROM_NO[rom_byte_number] & rom_byte_mask) > 0);
					else
						// if equal to last pick 1, if not then pick 0
						search_direction = (id_bit_number == LastDiscrepancy);

					// if 0 was picked then record its position in LastZero
					if (search_direction == 0)
					{
						last_zero = id_bit_number;

						// check for Last discrepancy in family
						if (last_zero < 9)
							LastFamilyDiscrepancy = last_zero;
					}
				}

				// set or clear the bit in the ROM byte rom_byte_number
				// with mask rom_byte_mask
				if (search_direction == 1)
					ROM_NO[rom_byte_number] |= rom_byte_mask;
				else
					ROM_NO[rom_byte_number] &= ~rom_byte_mask;

				// serial number search direction write bit
				owSendData.bitMode = BITMODE_SCAN1;
				owSendData.dataByte = search_direction;
				timer_set_alarm_value(OWTIMER_GROUP, OWTIMER_TIMER, 1);
				timer_start(OWTIMER_GROUP, OWTIMER_TIMER);
				xEventGroupWaitBits(x1WTimer, EVENTGROUP_TMR, pdTRUE, pdFALSE, 100 / portTICK_PERIOD_MS);

				// increment the byte counter id_bit_number
				// and shift the mask rom_byte_mask
				id_bit_number++;
				rom_byte_mask <<= 1;

				// if the mask is 0 then go to new SerialNum byte rom_byte_number and reset mask
				if (rom_byte_mask == 0)
				{
					docrc8(ROM_NO[rom_byte_number]);  // accumulate the CRC
					rom_byte_number++;
					rom_byte_mask = 1;
				}
			}
		} while (rom_byte_number < 8);  // loop until through all ROM bytes 0-7

		// if the search was successful then
		if (!((id_bit_number < 65) || (crc8 != 0)))
		{
			// search successful so set LastDiscrepancy,LastDeviceFlag,search_result
			LastDiscrepancy = last_zero;

			// check for last device
			if (LastDiscrepancy == 0)
				LastDeviceFlag = 1;

			search_result = 1;
		}
	}

	// if no device found then reset counters so next 'search' will be like a first
	if (!search_result || !ROM_NO[0])
	{
		LastDiscrepancy = 0;
		LastDeviceFlag = 0;
		LastFamilyDiscrepancy = 0;
		search_result = 0;
	}

	return search_result;
}

//--------------------------------------------------------------------------
// Find the 'first' devices on the 1-Wire bus
// Return TRUE  : device found, ROM number in ROM_NO buffer
//        FALSE : no device present
//
int OWFirst()
{
	// reset the search state
	LastDiscrepancy = 0;
	LastDeviceFlag = 0;
	LastFamilyDiscrepancy = 0;

	return OWSearch();
}

//--------------------------------------------------------------------------
// Find the 'next' devices on the 1-Wire bus
// Return TRUE  : device found, ROM number in ROM_NO buffer
//        FALSE : device not found, end of search
//
int OWNext()
{
	// leave the search state alone
	return OWSearch();
}

//--------------------------------------------------------------------------
// Verify the device with the ROM number in ROM_NO buffer is present.
// Return TRUE  : device verified present
//        FALSE : device not present
//
int OWVerify()
{
	unsigned char rom_backup[8];
	int i, rslt, ld_backup, ldf_backup, lfd_backup;

	// keep a backup copy of the current state
	for (i = 0; i < 8; i++)
		rom_backup[i] = ROM_NO[i];
	ld_backup = LastDiscrepancy;
	ldf_backup = LastDeviceFlag;
	lfd_backup = LastFamilyDiscrepancy;

	// set search to find the same device
	LastDiscrepancy = 64;
	LastDeviceFlag = 0;

	if (OWSearch())
	{
		// check if same device found
		rslt = 1;
		for (i = 0; i < 8; i++)
		{
			if (rom_backup[i] != ROM_NO[i])
			{
				rslt = 0;
				break;
			}
		}
	}
	else
		rslt = 0;

	// restore the search state
	for (i = 0; i < 8; i++)
		ROM_NO[i] = rom_backup[i];
	LastDiscrepancy = ld_backup;
	LastDeviceFlag = ldf_backup;
	LastFamilyDiscrepancy = lfd_backup;

	// return the result of the verify
	return rslt;
}

//--------------------------------------------------------------------------
// Setup the search to find the device type 'family_code' on the next call
// to OWNext() if it is present.
//
void OWTargetSetup(unsigned char family_code)
{
	int i;

	// set the search state to find SearchFamily type devices
	ROM_NO[0] = family_code;
	for (i = 1; i < 8; i++)
		ROM_NO[i] = 0;
	LastDiscrepancy = 64;
	LastFamilyDiscrepancy = 0;
	LastDeviceFlag = 0;
}

//--------------------------------------------------------------------------
// Setup the search to skip the current device type on the next call
// to OWNext().
//
void OWFamilySkipSetup()
{
	// set the Last discrepancy to last family discrepancy
	LastDiscrepancy = LastFamilyDiscrepancy;
	LastFamilyDiscrepancy = 0;

	// check for end of list
	if (LastDiscrepancy == 0)
		LastDeviceFlag = 1;
}

// TEST BUILD
static unsigned char dscrc_table[] =
{ 0, 94, 188, 226, 97, 63, 221, 131, 194, 156, 126, 32, 163, 253, 31, 65, 157, 195, 33, 127, 252, 162, 64, 30, 95, 1, 227, 189, 62, 96, 130, 220, 35, 125, 159,
		193, 66, 28, 254, 160, 225, 191, 93, 3, 128, 222, 60, 98, 190, 224, 2, 92, 223, 129, 99, 61, 124, 34, 192, 158, 29, 67, 161, 255, 70, 24, 250, 164, 39,
		121, 155, 197, 132, 218, 56, 102, 229, 187, 89, 7, 219, 133, 103, 57, 186, 228, 6, 88, 25, 71, 165, 251, 120, 38, 196, 154, 101, 59, 217, 135, 4, 90,
		184, 230, 167, 249, 27, 69, 198, 152, 122, 36, 248, 166, 68, 26, 153, 199, 37, 123, 58, 100, 134, 216, 91, 5, 231, 185, 140, 210, 48, 110, 237, 179, 81,
		15, 78, 16, 242, 172, 47, 113, 147, 205, 17, 79, 173, 243, 112, 46, 204, 146, 211, 141, 111, 49, 178, 236, 14, 80, 175, 241, 19, 77, 206, 144, 114, 44,
		109, 51, 209, 143, 12, 82, 176, 238, 50, 108, 142, 208, 83, 13, 239, 177, 240, 174, 76, 18, 145, 207, 45, 115, 202, 148, 118, 40, 171, 245, 23, 73, 8,
		86, 180, 234, 105, 55, 213, 139, 87, 9, 235, 181, 54, 104, 138, 212, 149, 203, 41, 119, 244, 170, 72, 22, 233, 183, 85, 11, 136, 214, 52, 106, 43, 117,
		151, 201, 74, 20, 246, 168, 116, 42, 200, 150, 21, 75, 169, 247, 182, 232, 10, 84, 215, 137, 107, 53 };

//--------------------------------------------------------------------------
// Calculate the CRC8 of the byte value provided with the current
// global 'crc8' value.
// Returns current global crc8 value
//
unsigned char docrc8(unsigned char value)
{
	// See Application Note 27

	// TEST BUILD
	crc8 = dscrc_table[crc8 ^ value];
	return crc8;
}
