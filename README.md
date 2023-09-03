# ESP32-Faikin

Everyone knows Daikin make some of the best air conditioners out there, mechanically speaking. Sadly their WiFi control modules are not so good, especially the latest models which are all cloud based, require an internet connection to even work, and are slow.

This code/module provides local control via web interface, MQTT, and HomeAssistant integration, all with no cloud crap.

Wiki: [List of supported air-con](https://github.com/revk/ESP32-Faikin/wiki/List-of-confirmed-working-air-con-units) **Please update this with your model**

**SUPPORT** please start a discussion here and I can help.

This is all ESP32 based, but there is also an [ESP8266 port](https://github.com/Sonic-Amiga/ESP8266-Faikin) of this code.

PCB designs are included, and also available to buy on [Amazon UK](https://www.amazon.co.uk/dp/B0C2ZYXNYQ). Note, whilst Amazon have done some export, some people have used a parcel forwarder for non UK, such as [Forward2Me](https://forward2me.com).

<img src=Manuals/Board1.jpg width=50%><img src=Manuals/Board2.jpg width=50%>

## Cabling

There have been various discussions on this - Daikin appear to have a *standard* connector that is `S21`. They seem to have this in models that even have build in WiFi modules (see image). You need a JST-EH 5 way connector for `S21`. However the module can also be wired to units that have an `X50A` connector, with slightly different cabling. The discussions have various comments on this, but please do ask.

![251828896-0d572c0d-d937-43cf-9d39-6c7f4d853075](https://github.com/revk/ESP32-Faikin/assets/996983/45a4cb59-da3f-47ab-9d0b-f99bdbcca763)

One approach is "jumper wires" using header pins or sockets, e.g

![photo_2023-08-29_17-49-13](https://github.com/revk/ESP32-Faikin/assets/996983/6e062178-7fac-4f75-885e-fb7f1060f89e)

## Why I made this

The history is that, after years of using Daikin air-con in my old home, and using the local http control, in my new house in Wales the WiFi was all cloud based with no local control, and useless, and slow. Just configuring it was a nightmare. I spent all day reverse engineering it and making a new module to provide local control. Pull requests and feature ideas welcome.

## How to get one

As mentioned, on [Amazon UK](https://www.amazon.co.uk/dp/B0C2ZYXNYQ) - but not available to export everywhere. Forwarding companies are an option.

But also, the PCB designs are published, including production files for [JLCPCB](https://jlcpcb.com). You would also need somethign to program them, such as my [Tasmotiser](https://github.com/revk/Tasmotizer-PCB) board.

# Set-up

Appears as access point with simple web page to set up on local WiFI. On iPhone the setup page auto-loads.

![WiFi1](Manuals/WiFi1.png)

![WiFi2](Manuals/WiFi2.png)

# Operation

Local interactive web control page using *hostname*.local, no app required, no external internet required.

![WiFi3](Manuals/WiFi3.png)

- [Setup](Manuals/Setup.md) Manual
- [Controls](Manuals/Controls.md) Manual
- [Advanced](Manuals/Advanced.md) Manual

# Design

* KiCad PCB designs included, with JLCPCB production files.
* 3D printed case STL files
* Documentation of reverse engineered protocol included

Basically, Daikin have gone all cloudy with the latest WiFi controllers. This module is designed to provide an alternative.

<img src="Manuals/MiSensor.jpg" align=right width="25%">

* Simple local web based control with live websocket status, easy to save as desktop icon on a mobile phone.
* MQTT reporting and controls
* Works with Home Assistant over MQTT - note Home Assistant can work with HomeKit
* Includes linux mysql/mariadb based logging and graphing tools
* Works with [EnvMon](https://github.com/revk/ESP32-EnvMon) Environmental Monitor for finer control and status display
* or, works with BlueCoinT and Telink [BLE temperature sensor](Manuals/BLE.md) as a remote reference in an auto mode
* Automatically works out if S21 or X50 protocol (used on bigger/ducted units)
* Backwards compatible direct `/aircon/...` URLs

# Building

Git clone this `--recursive` to get all the submodules, and it should build with just `make`. There are make targets for other variations, but this hardware is the `make pico` version. The `make` actually runs the normal `idf.py` to build which then uses cmake. `make menuconfig` can be used to fine tune the settings, but the defaults should be mostly sane. `make flash` should work to program. If flashing yourself, you will need a programming lead, e.g. [Tazmotizer](https://github.com/revk/Shelly-Tasmotizer-PCB) or similar, and of course the full ESP IDF environment. The modules on Amazon come pre-loaded and can upgrade over the air.

The code is normally set up to automatically upgrade software, checking roughtly once a week. You can change this in settings via MQTT.

If you want to purchase an assembled PCB, see [A&A circuit boards](https://www.aa.net.uk/etc/circuit-boards/) or [Amazon](https://www.amazon.co.uk/dp/B0C2ZYXNYQ).
