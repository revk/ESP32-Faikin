# ESP32-Faikin

Everyone knows Daikin make some of the best air conditioners out there, mechanically speaking. Sadly their WiFi control modules are not so good, especially the latest models which are all cloud based, require an internet connection to even work, and are slow.

This code/module provides local control via web interface, MQTT, and HomeAssistant integration, all with no cloud crap.

This is all ESP32 based, but there is also an [ESP8266 port](https://github.com/Sonic-Amiga/ESP8266-Faikin) of this code.

PCB designs are included, and also available to buy on [Amazon UK](https://www.amazon.co.uk/dp/B0C2ZYXNYQ). Note, whilst Amazon have done some export, some people have used a parcel forwarder for non UK, such as [Forward2Me](https://forward2me.com).

Support is provided here ([issues](issues) and [discussions](discussions)).

See [wiki/Work-in-progress](https://github.com/revk/ESP32-Faikin/wiki/Work-in-progress) for details of changes and pre-release test.

## PCB

The board from [Amazon UK](https://www.amazon.co.uk/dp/B0C2ZYXNYQ) works with Daikin modiles via an `S21` connector, or `X50A` and `X35A` connectors, or similar. The design is updated from time to time so may not look exactly like this. See wiki [list of supported air-con](https://github.com/revk/ESP32-Faikin/wiki/List-of-confirmed-working-air-con-units).

<img src=Manuals/Board1.jpg width=50%><img src=Manuals/Board2.jpg width=50%>

## Cabling

If you have the existing Daikin wifi unit on a cable with 5 pin `S21` plug, it is simply a matter of changing it to the `Faikin` module. However, in other cases you may need to obtain a cable and connect to your air-con unit. The S21 cable is *one to one* wired, i.e. pin 1 to pin 1. Note that Pin 5 is ⏚.

[JST, EH Female Connector Housing, 2.5mm Pitch, 5 Way, 1 Row](https://uk.rs-online.com/web/p/wire-housings-plugs/3116237)

**WARNING** Messing about in your air-con unit can be dangerous with live power, and could damage the unit. Take all precautions necessary and you mess with your air-con unit at your own risk! Note, `X403` is a non isolated version of `S21` so if you wire to that your Faikin will be live, so needs to be inside the case somewhere, and don't wire anything while the power is on... See [X403](https://github.com/revk/ESP32-Faikin/issues/208).

There have been various discussions on this - Daikin appear to have a *standard* connector that is `S21`. They seem to have this in models that even have build in WiFi modules (see image). You need a JST-EH 5 way connector for `S21`. However the module can also be wired to units that have an `X50A` connector, with slightly different cabling. The discussions have various comments on this, but please do ask.

Another approach is "jumper wires" using header pins or sockets, e.g

<img src=https://github.com/revk/ESP32-Faikin/assets/996983/45a4cb59-da3f-47ab-9d0b-f99bdbcca763 width=50%><img src=https://github.com/revk/ESP32-Faikin/assets/996983/6e062178-7fac-4f75-885e-fb7f1060f89e width=50%>

It is also possible to connect to the `S403` connector. The proper way to do this is with a Daikin `S21` adapter board, as the `S403` is not isolated, and use directly could be dangerous. This [link](https://community.openenergymonitor.org/t/hack-my-heat-pump-and-publish-data-onto-emoncms/2551/99) has some more details of the `S403` connector, as well as [issue 134](https://github.com/revk/ESP32-Faikin/issues/134).

<img src=https://github.com/revk/ESP32-Faikin/assets/996983/992e6057-9ac9-4c6b-abab-93d5e45aa2b0 width=35% align=right>

For `X50A` you need power from `X35A` as well.

<img src=https://github.com/revk/ESP32-Faikin/assets/996983/ab18e1a4-45ad-4b61-9ce7-5dfeefffdfa1 width=55%>

## Why I made this

The history is that, after years of using Daikin air-con in my old home, and using the local http control, in my new house in Wales the WiFi was all cloud based with no local control, and useless, and slow. Just configuring it was a nightmare. I spent all day reverse engineering it and making a new module to provide local control. Pull requests and feature ideas welcome.

This whole project is almost entirely by me, but with some valuable contributions from others (thank you). All of my bits are copyright by me and Andrews & Arnold Ltd who sponsor the whole project, and released under GPL. Whilst not required by the licence, attribution and links would be appreciated if you reuse this.

## How to get one

As mentioned, on [Amazon UK](https://www.amazon.co.uk/dp/B0C2ZYXNYQ) - but not available to export everywhere. Forwarding companies are an option.

But also, the PCB designs are published, including production files for [JLCPCB](https://jlcpcb.com). You would also need something to program them, such as my [Tasmotiser](https://github.com/revk/Tasmotizer-PCB) board.

Someone has made (slightly older versions of) boards for sale in US on [Tindie](https://www.tindie.com/products/elfatronics/faikin-alternative-daikin-wifi-controller/) as well.

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

# Building code yourself

Git clone this `--recursive` to get all the submodules, and it should build with just `make`. There are make targets for other variations, but this hardware is the `make pico` or `make s3` version. The `make` actually runs the normal `idf.py` to build which then uses cmake. `make menuconfig` can be used to fine tune the settings, but the defaults should be mostly sane. `make flash` should work to program. If flashing yourself, you will need a programming lead, e.g. [Tazmotizer](https://github.com/revk/Shelly-Tasmotizer-PCB) or similar, and of course the full ESP IDF environment. The latest boards also have 4 pads for direct USB connection to flash with no adaptor. The modules on Amazon come pre-loaded and can upgrade over the air.

The code is normally set up to automatically upgrade software, checking roughtly once a week. You can change this in settings via MQTT.

If you build yourself, you either need no code signing, or your own signing key. This will break auto-updates which try to load me code releases, so you need to adjuist settings `otahost` and `otaauto` accordingly. You can set these in the build config, along with WiFi settings, etc.

If you want to purchase a pre-loaded assembled PCB, see [A&A circuit boards](https://www.aa.net.uk/etc/circuit-boards/) or [Amazon](https://www.amazon.co.uk/dp/B0C2ZYXNYQ).
