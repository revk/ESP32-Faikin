# Reflashing

Ideally you build the code and flash, but that will be your code signing.

You can also flash these binaries.

You need `esptool`

## esptool

`esptool/esptool.py -p (PORT) -b 460800 --before default_reset --after hard_reset --chip esp32s3 write_flash --flash_mode dio --flash_size keep --flash_freq 40m 0x0 bootloader.bin 0x8000 partition-table.bin 0xd000 ota_data_initial.bin 0x10000 Faikin.bin`

- The `bootloader.bin` needs to be the relevant release bootloader binary file for the chip used.
- The `Faikin.bin` needs to be the relevant release binary file for the chip used.
