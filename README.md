# ESP32-Daikin

This is code to run on an ESP32 module and connect to a Daikin aircon unit in place of a BRP069B41 or BRP069C41 module.

* KiCad PCB designs included
* 3D printed case STL files
* Documentation of reverse engineered protocol include

Basically, Daikin have gone all cloudy with the latest WiFi controllers. This module is designed to provide an alternative.

* Simple local web based control, easy to save as desktop icon on phone
* MQTT reporting and controls
* Works with https://github.com/revk/ESP32-EnvMon Environmental Monitor for finer control

# Building

Git clone this `--recursive` to get all the submodules, and it should build with just `make`. There are make targets for other variables, but this hardware is the `make pico` version. The `make` actually runs the normal `idf.py` to build with then uses cmake. `make menuconfig` can be used to fine tune the settings, but the defaults should be mostly sane. `make flash` should work to program. You will need a programming lead, e.g. https://github.com/revk/Shelly-Tasmotizer-PCB or similar, and of course the full ESP IDF environment.

# Complete module in 3D printed case

The wiring from the existing wifi modules fits directly (albeit only 4 pins used).

![272012](https://user-images.githubusercontent.com/996983/169694456-bd870348-f9bf-4c31-a2e3-00da13320ffc.jpg)
