# ESP32-Daikin

## work in progress

This is code to run on an ESP32 module and connect to a Daikin aircon unit in place of a BRP069C41 module.

* KiCad PCB designs included.
* Documentation of reverse engineered protocol include

Basically, Daikin have gone all cloudy with the latest WiFi controllers. This module is designed to provide an alternative.

This plan is MQTT linked, one day maybe even Matter linked, and maybe support the old style Daikin WiFi commands/URLs as well, we'll see.

# Building

Git clone this `--recursive` to get all the submodules, and it should build with just `make`. That actually runs the normal `idf.py` to build. `make menuconfig` can be used to fine tune the settings, but the defaults should be mostly sane. `make flash` should work to program.
