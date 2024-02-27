# Reflashing

Ideally you build the code and flash, but that will be your code signing.

You can also flash these binaries.

You need `esptool`

## esptool

`esptool.py -p /dev/ttyUSB0 write_flash 0x1000 Faikin-S1-PICO-bootloader.bin 0x8000 partition-table.bin 0xd000 ota_data_initial.bin 0x10000 Faikin-S1-PICO.bin`

- The `Faikin-S1-PICO-bootloader.bin` needs to be the relevant release bootloader binary file for the chip used.
- The `Faikin-S1-PICO.bin` needs to be the relevant release binary file for the chip used.

## 8266 port

The ESP8266 port is maintained by Pavel Fedin; project URL is: https://github.com/Sonic-Amiga/ESP8266-Faikin .
Since this is a backport; it may lack some features of the newest ESP32 version. Please contact the author for any questions.

Flashing command (addresses differ from ESP32 !!!)

`esptool.py -p COM3 -b 460800 --after hard_reset write_flash --flash_mode dio --flash_freq 40m --flash_size 8MB 0x8000 partition-table-8266.bin 0xd000 ota_data_initial-8266.bin 0x0 Faikin-8266-bootloader.bin 0x10000 Faikin-8266.bin`
