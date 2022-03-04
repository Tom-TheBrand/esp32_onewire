# esp32_onewire
ESP32-WROOM-32 does not contain 1-wire hardware registers. This project implements the 1-Wire protocol in software using an bidirectional IO Pin, talking to DS18B20 temperature sensors.

**Requirements:**
I'm using ESP32 nodeMCU by AZ-Delivery (https://www.az-delivery.de/products/esp32-developmentboard), through a small daughter board with 4.7k pull up resistor i connected two DS18B20 temperature sensors (https://www.az-delivery.de/products/2xds18b20wasserdicht). Power for sensors and pull up is taken from 3V3 Pin. The data pin is connected to G19 (defined in onewire.h "OWPin").
> Most other pins should also work, just pay attention to use an input / output pin (e.g. DO NOT USE G34-G39). 

## Run Code

### install espresif sdk (esp-idf)
https://docs.espressif.com/projects/esp-idf/en/v4.4/esp32/get-started/index.html#installation-step-by-step

### clone, build, run:
> replace _COM3_ by *your* serial port
```
git clone https://github.com/Tom-TheBrand/esp32_onewire.git
idf.py build
idf.py -p COM3 flash
idf.py monitor
```

### expected monitor output

**after start**
```
I (311) cpu_start: Starting scheduler on PRO CPU.
I (0) cpu_start: Starting scheduler on APP CPU.
I (360) ESP32_ONEWIRE: Taster: 0
```
**after pressing BOOT button (on nodeMCU module)**
```
I (7240) ESP32_ONEWIRE: Taster: 1
I (7260) ESP32_ONEWIRE:  1W (1): 28-E6-22-16-12-21-01-91
I (7290) ESP32_ONEWIRE:  1W (2): 28-49-C8-15-12-21-01-06
I (7290) ESP32_ONEWIRE: 2 1-Wire devices found
I (7290) ESP32_ONEWIRE: Configure first DS18B20 sensor to convert using 10 bit resolution
I (7310) ESP32_ONEWIRE: Start and wait for temperature convert..
I (7490) ESP32_ONEWIRE: Read scratch pad..
I (7510) ESP32_ONEWIRE: Temperature: 22.00
I (8310) ESP32_ONEWIRE: Taster: 0
```

**Result**

Two sensors are found, the first one is configured, an temperature conversion is trigged, result shown.


# Remarks, use the library for your own project

The library pin's itself to use CPU-1 to have lowest possible latencies allowing fullfilling 1-Wire timing constraints.
Therefore pay attention if other tasks are also using CPU-1 which uses to many CPU cycles.

**oscilloscope graph of 1-Wire RESET**
![Graph of reset sequence](/graph/1w_reset.jpg)
**oscilloscope graph of sending 0xE5 on 1-Wire bus**
![Graph of 1 Byte sequence](/graph/1w_0xE5.jpg)

