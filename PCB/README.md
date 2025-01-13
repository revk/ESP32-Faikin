# PCB designs

The `Faikin` directory has the current working design. `Faikin` is the current shipping board. The design is in [KiCad](https://www.kicad.org) and has production files for JLCPCB. The boards are available on [Amazon](https://www.amazon.co.uk/dp/B0C2ZYXNYQ).

Other directories such as `Faikin2` or `Faikin3` are work in progress and may not be tested yet. They will replace `Faikin` once tested.

The `PCBCase` directory is a tool for making 3D cases from the PCBs files, these are usually kept up to date as an `.stl` file in the `Faikin` directory. The `Makefile` simply runs this tool.

## Trademark

This is an open source project, but bear in mind you cannot sell boards bearing the Andrews & Arnold Ltd name, the A&A logo, the registered trademark AJK logo, or the GS1 allocated EANs assigned to Andrews & Arnold Ltd.

![Top](Faikin/Faikin-90.png)

## Panel

The PCB is supplied in a standard 70mm x 70mm panel - this is the minimum panel for assemply for JLCPCB.

The panel incldues slots and v-cuts to allow the unwanted parts of the panel to be snapped off. On the thicker (1.6mm) boards this can be a tad un-nerving, but just bend along the v-cut to snap the board on the cut line. I make the boards at 1.2mm thickness to make this easier.

The board design also allows the panel to be snapped off down to a 45mm x 36mm small board. This is the same size as commonly used for built-in Daikin WiFi modules, so if replacing the standard Daikin WiFi, it should fit exactly in the case. Obviously this only applies to some models.

You can further snap off down to the actual module, which is currently 40mm x 16mm. This is the usual way the module is used. The 3D print design is for this board.

The back of the board has no components, making it simple to stick to a flat surface with tape - you may want to crop the pins on the 5 pin connector if doing this. Do not stick to a metal surface as this may impact the WiFi or short the pins.

## S21 Connector

The connector is `JST-EH` 5 pin connector. This seems to be pretty standard in Daikin units. It is the connector used on the Daikin WiFi modules `BRP069B41` and `BRP069C41`. This means it should usually be possible to get a lead for your Daikin to a `JST-EH` 5 pin `S21` connector. If you have an `S21` connector on your air-con it will likely be the same pin out, so one to one wiring. If you have an `X50A` connector the wiring is different, and getting a suitable lead is a good idea.

<img src=../Manuals/jumper.jpg width=25% align=right>

From what we can tell, the `S21` pin out is as follows :-

|Pin|Meaning|
|---|-------|
|1|5V (not always present)|
|2|Tx from aircon - 5V logic levels|
|3|Rx to aircon - expects 5V logic and has 5V pull up, but some modeles work at 3.3V|
|4|12V, or 14V. We accept 4V to 36V|
|5|GND|

However, there are jumper wires readily available that can be used easily with these connectors, and can be used to connect to the `S21` connector on the aircon PCB one wire at a time.

**Please take care with your air-con unit - disconnect power before opening, and anything you do is at your own risk.**

## GPIO

This board is designed to work with the Daikin air-con, but it is a general purpose board. The latest boards use `ESP32-S3-MINI-N4R2` processor, with previous being `ESP32-MINI-PICO-01` (S1).

The current boards include the GPIO numbers clearly in inverse silk screen.

For example, on this image, GPIO `34` and `48` for Tx and Rx. GPIO `47` is for the WS2812 style LED. GPIO `21` is one of two round pads which can be shorted three times in a row to factory reset the board.

![Bottom](Faikin/Faikin-bottom.png)

## Programming

The board has pads for a TC2030 USB lead for programming directly using `esptool`.
