# ESP32-Faikin

Everyone knows Daikin make some of the best air conditioners out there, mechanically speaking. But their implementation of IoT (Internet of Things); remote control over the Internet -  leaves a lot to be desired. Their app is not well supported, and does not work well. If you are planning on integrating your air conditioners into something like an MQTT set up in your home or office, or simply want very basic web control / status for your units, this unit provides these features. It plugs in where the old Daikin WiFi module goes. The code is being updated from time to time and now incldues Home Assistant integration.

Wiki: [List of supported air-con](https://github.com/revk/ESP32-Faikin/wiki/List-of-confirmed-working-air-con-units) **Please update this with your model**

This is code and PCB design to run on an ESP32 module and connect to a Daikin aircon unit in place of a BRP069B41, BRP069C41, or similar modules.

Buy on [Amazon](https://www.amazon.co.uk/dp/B0C2ZYXNYQ) This is UK only as Amazon won't export it for some reason, but one customer had success using [Forward2Me](https://www.forward2me.com/) to order on Amazon and export.

<img src=Manuals/Board.jpg width=33%><img src=Manuals/Board2.jpg width=33%><img src=Manuals/Cased.jpg width=33%>

## Why I made this

The history is that, after years of using Daikin air-con in my old home, and using the local http control, in my new house in Wales the WiFi was all cloud based with no local control, and useless, and slow. Just configuring it was a nightmare. I spent all day reverse engineering it and making a new module to provide local control. Pull requests and feature ideas welcome.

# Set-up

Appears as access point with simple web page to set up on local WiFI

![WiFi1](Manuals/WiFi1.png)

![WiFi2](Manuals/WiFi2.png)

# Operation

Local interactive web control page using *hostname*.local, no app required, no external internet required.

![WiFi3](Manuals/WiFi3.png)

- [Setup](Manuals/Setup.md) Manual
- [Controls](Manuals/Controls.md) Manual
- [Advanced](Manuals/Advanced.md) Manual

# Design

* KiCad PCB designs included
* 3D printed case STL files
* Documentation of reverse engineered protocol included

Basically, Daikin have gone all cloudy with the latest WiFi controllers. This module is designed to provide an alternative.

* Simple local web based control with live websocket status, easy to save as desktop icon on a mobile phone.
* MQTT reporting and controls
* Works with Home Assistant over MQTT
* Includes linux mysql/mariadb based logging and graphing tools
* Works with [EnvMon](https://github.com/revk/ESP32-EnvMon) Environmental Monitor for finer control and status display
* or, works with BlueCoinT BLE temperature sensor as a remote reference in an auto mode
* Automatically works out if S21 or X50 protocol (used on bigger/ducted units0
* Backwards compatible `/aircon/get_control_info` and `/aircon/set_control_info` URLs

# Building

Git clone this `--recursive` to get all the submodules, and it should build with just `make`. There are make targets for other variables, but this hardware is the `make pico` version. The `make` actually runs the normal `idf.py` to build with then uses cmake. `make menuconfig` can be used to fine tune the settings, but the defaults should be mostly sane. `make flash` should work to program. You will need a programming lead, e.g. [Tazmotizer](https://github.com/revk/Shelly-Tasmotizer-PCB) or similar, and of course the full ESP IDF environment.

If you want to purchase an assembled PCB, see [A&A circuit boards](https://www.aa.net.uk/etc/circuit-boards/) or [Amazon](https://www.amazon.co.uk/dp/B0C2ZYXNYQ).
