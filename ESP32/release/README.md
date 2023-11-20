# Reflashing

Ideally you build the code and flash, but that will be your code signing.

You can also flash these binaries.

You need `esptool`

## esptool

`esptool.py -p /dev/ttyUSB0 write_flash 0x1000 Faikin-S1-PICO-bootloader.bin 0x8000 partition-table.bin 0xd000 ota_data_initial.bin 0x10000 Faikin-S1-PICO.bin`

- The `Faikin-S1-PICO-bootloader.bin` needs to be the relevant release bootloader binary file for the chip used.
- The `Faikin-S1-PICO.bin` needs to be the relevant release binary file for the chip used.
