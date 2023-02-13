# ESP32-Daikin

Everyone knows Daikin make some of the best air conditioners out there, mechanically speaking. But their implementation of IoT (Internet of Things); remote control over the Internet -  leaves a lot to be desired. Their app is not well supported, and does not work well. If you are planning on integrating your air conditioners into something like an MQTT set up in your home or office, or simply want very basic web control / status for your units, this unit provides these features. It plugs in where the old Daikin WiFi module goes.

This is code and PCB design to run on an ESP32 module and connect to a Daikin aircon unit in place of a BRP069B41, BRP069C41, or similar modules.

# Set-up

Appears as access point with simple web page to set up on local WiFI

![IMG_2446](https://user-images.githubusercontent.com/996983/218394248-b409626b-2614-439d-95f4-71527c00aaa7.PNG)

![IMG_2448](https://user-images.githubusercontent.com/996983/218394254-e03537f7-09a0-490d-aa38-b6964a5cd77f.PNG)

# Operation

Local interactive web control page using *hostname*.local, no app required, no external internet required.

![IMG_2421](https://user-images.githubusercontent.com/996983/218394392-48b47be4-5989-474e-beab-734dd6ef83d9.PNG)

# Design

* KiCad PCB designs included
* 3D printed case STL files
* Documentation of reverse engineered protocol included

Basically, Daikin have gone all cloudy with the latest WiFi controllers. This module is designed to provide an alternative.

* Simple local web based control with live websocket status, easy to save as desktop icon on a mobile phone.
* MQTT reporting and controls
* Includes linux mysql/mariadb based logging and graphing tools
* Works with https://github.com/revk/ESP32-EnvMon Environmental Monitor for finer control and status display
* Automatically works out if S21 or alternative protocol used on ducted units
* Backwards compatible `/aircon/get_control_info` and `/aircon/set_control_info` URLs (work in progress)

# Building

Git clone this `--recursive` to get all the submodules, and it should build with just `make`. There are make targets for other variables, but this hardware is the `make pico` version. The `make` actually runs the normal `idf.py` to build with then uses cmake. `make menuconfig` can be used to fine tune the settings, but the defaults should be mostly sane. `make flash` should work to program. You will need a programming lead, e.g. https://github.com/revk/Shelly-Tasmotizer-PCB or similar, and of course the full ESP IDF environment.

If you want to purchase an assembled PCB, see https://www.aa.net.uk/etc/circuit-boards/

The wiring from the existing wifi modules fits directly (albeit only 4 pins used).

![272012](https://user-images.githubusercontent.com/996983/169694456-bd870348-f9bf-4c31-a2e3-00da13320ffc.jpg)
