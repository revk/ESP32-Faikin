# ESP32-Daikin

This is code to run on an ESP32 module and connect to a Daikin aircon unit in place of a BRP069B41, BRP069C41, or similar modules.

* KiCad PCB designs included
* 3D printed case STL files
* Documentation of reverse engineered protocol included

Basically, Daikin have gone all cloudy with the latest WiFi controllers. This module is designed to provide an alternative.

* Simple local web based control with live websocket status, easy to save as desktop icon on a mobile phone.
* MQTT reporting and controls
* Includes linux mysql/mariadb based logging and graphing tools
* Works with https://github.com/revk/ESP32-EnvMon Environmental Monitor for finer control and status display
* Automatically works out if S21 or alternative protocol used on ducted units
* Backwards compatible /aircon/get_control_info and /aircon/set_control_info URLs (work in progress)

# Building

Git clone this `--recursive` to get all the submodules, and it should build with just `make`. There are make targets for other variables, but this hardware is the `make pico` version. The `make` actually runs the normal `idf.py` to build with then uses cmake. `make menuconfig` can be used to fine tune the settings, but the defaults should be mostly sane. `make flash` should work to program. You will need a programming lead, e.g. https://github.com/revk/Shelly-Tasmotizer-PCB or similar, and of course the full ESP IDF environment.

If you want to purchase an assembled PCB, see https://www.aa.net.uk/etc/circuit-boards/

The wiring from the existing wifi modules fits directly (albeit only 4 pins used).

![272012](https://user-images.githubusercontent.com/996983/169694456-bd870348-f9bf-4c31-a2e3-00da13320ffc.jpg)
