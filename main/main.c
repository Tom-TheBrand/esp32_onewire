#include "freertos/FreeRTOS.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_event.h"
#include "string.h"

#include "driver/gpio.h"

#include "onewire.h"
#include "ds18b20.h"

#define TAG "ESP32_ONEWIRE"

void app_main(void)
{
	int btnPressed0 = 1;
	int count;
	int rslt;
	int ret;
	float temperature;
	int64_t tick = 0;
	uint8_t sensorROM[8];
	uint8_t scratchPat[8];

	gpio_set_direction(GPIO_NUM_0, GPIO_MODE_INPUT);

	OWInit();

	while (true)
	{
		int newlevel = gpio_get_level(GPIO_NUM_0);
		if (btnPressed0 == newlevel && tick < esp_timer_get_time())
		{
			tick = esp_timer_get_time() + 40000; // zum entprellen
			btnPressed0 = !newlevel;

			ESP_LOGI(TAG, "Taster: %d", btnPressed0);
//			if(btnPressed0)
//				OWTouchReset();
//			else
//				OWWriteByte(0xE5);

			if (btnPressed0)
			{
				count = 0;

				// scan one wire devices
				OWTargetSetup(0x28); // scan for devices with 0x28 as family code (first byte)
				do
				{
					rslt = OWNext();
					if (rslt)
					{
						if (count == 0) // just remember the first sensor
							memcpy(sensorROM, ROM_NO, 8);

						count++;
						ESP_LOGI(TAG, " 1W (%d): %02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X", count, ROM_NO[0], ROM_NO[1], ROM_NO[2], ROM_NO[3], ROM_NO[4],
								ROM_NO[5], ROM_NO[6], ROM_NO[7]);
					}
				} while (rslt);
				ESP_LOGI(TAG, "%d 1-Wire devices found", count);

				ESP_LOGI(TAG, "Configure first DS18B20 sensor to convert using 10 bit resolution");
				if(DSconfigure(sensorROM) != DS_OK)
				{
					ESP_LOGI(TAG, "> FAILED!");
					continue;
				}


				ESP_LOGI(TAG, "Get current temperature..");
				if((ret = DSreadTemperature(sensorROM, &temperature)) != DS_OK)
				{
					ESP_LOGI(TAG, "> FAILED (Code: %X)", ret);
					continue;
				}
				ESP_LOGI(TAG, "Temperature: %.2f", temperature);

//				ESP_LOGI(TAG, "Start and wait for temperature convert..");
//				if((ret = DSstartConversion(sensorROM, 1)) != DS_OK)
//				{
//					ESP_LOGI(TAG, "> FAILED (Code: %X)", ret);
//					continue;
//				}
//
//				ESP_LOGI(TAG, "Read scratch pad..");
//				if(DSreadScratchPad(sensorROM, scratchPat) != DS_OK)
//				{
//					ESP_LOGI(TAG, "> FAILED!");
//					continue;
//				}
//
//				ESP_LOGI(TAG, "Temperature: %.2f", (float)((((int)scratchPat[1] << 8) + scratchPat[0]) >> 2) / 4);

			}

		}

		vTaskDelay(1);
	}
}

