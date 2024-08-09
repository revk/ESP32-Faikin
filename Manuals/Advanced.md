# Advanced operation

There are a number of settings and commands that are available via MQTT. To use these you must have set up the device to connect to an MQTT server. You then need a means to send messages to the MQTT server for the device.

The *hostname* you picked is used for these messages. The format for commands and settings is defined in [RevK library](https://github.com/revk/ESP32-RevK). But is basically quite simple. We'll use `GuestAC` as the example hostname.

|Type|Format|Meaning|
|----|------|-------|
|Command|`command/GuestAC/command`|This issues commands to the device to do something now.|
|Setting|`setting/GuestAC`|This allows settings to be set. The payload is JSON, with one or more settings. e.g. `setting/GuestAC {"fanstep":2,"reporting":60}`. With no payload the existing settings are returned in the same format.|
|Setting (direct)|`setting/GuestAC/thing`|This allows a single setting to be set, the payload is the value, e.g. `setting/GuestAC/debug 1`|
|State|`state/GuestAC/whatever`|Status messages - these are normally retained and relate to specific aspects of the operation. With no paramater, e.g. `state/GuestAC` this is the state of the device itself, with `false` meaning it is now offline.|
|Event|`event/GuestAC/whatever`|Event messages are one of things that have happened and are not retained.|
|Info|`info/GuestAC/whatever`|Info messages are informational, and don't relate to a specific event happening.|
|Error|`error/GuestAC/whatever`|Errors are reported using this.|

## Faikin auto

The *Faikin auto* logic allows a set point (with a margin) and a reference to external temperature. It uses simple prediction to see if we are going to exceed the maximum or minimum or be within range (see below), and aims to stay in range by basically turning the aircon just *on* or *off*. It does the *on* or *off* by changing the set point given to the air con to be *higher* or *lower*, in simplest terms.

However there are a few settings to control what *on* and *off* mean in this case. the simplest, default, is *much higher* than current temperature or *much lower* than current temperature, i.e. around ±6℃.

The actual set point logic set a set point, and adds `heatover` or subtracts `heatback` adjustment for *heating* or subtracts `coolover` or adds `coolback` for *cooling*. The basis for these is one of there options.

1. The default (`temptrack` off, `tempadjust` on) is your requested set point, adjusted for the air con temperature not matching your external temperature.
2. Non adjusted (`temptrack` off, `tempadjust` off) just uses the set point you have asked for as a basis.
3. Tracking (`temptrack` on) uses what the air con sees as the current temperature, allowing you to constantly nudge the aircon towards the target you want

There is also a setting hold off adjustments (e.g. when tracking) for a time period (`tempnoflap`).

### Staying in range

The auto mode has a target range, a *min* and *max*. On the web page this is set by a target and tolerance, so if you set 21℃ with ±1℃ that is a range of 20℃ to 22℃. This is the comfort zone, the range in which you would like the temperature to stay.

To achieve this the Faikin looks at the temperature and sets the heating/cooling *on* or *off* as explained above. When heating, it is basically looking to keep the temperature just above *min*, and for cooling just below *max*. Note, it it not *aiming* for the middle, just to be within the range.

When looking at the current temperature it looks ahead, as the aircon has some inertia, it samples every `tpredicts` seconds and looks ahead `tpredictt` periods. This allows it to act before the temperature goes too far one way or the other.

I simplest terms, if heating, if the temperature will be above *min* it turns off, and if it will be below *min* it turns on. However, that would simply mean a temperature hovering around *min*. To aim for somewhere between *min* and *max* it actually adjusts its target, increasing *min* by `pushtemp` (default 0.1℃) so it hovers a bit above *min*. For cooling this works the other way around and relates to *max*.

The temperature band is also used to work out if we need to reverse heating/cooling. This is where *max* comes in for heating (*min* for cooling). If we are heating but spending all the time over *max* we switch to cooling. If we are always within *min* to *max* it will turn off. Bear in mind this is based on predicted temperature, so we may be within *min* to *max* because the heating is turned on/off to keep us there, and that does not turn off as the predicted temp will have gone out of range. There is an adjustment to the switching temperature, e.g. in heating *max* is adjusted by `switchtemp`.

### Thermostat mode

You can set the `thermostat` - this changes the target we use. When not in this mode, heating aims for *min* and cooling aims for *max*. In thermostat mode heating aims for *max* until it gets there, and then lets things cool to *min* before turning back on again, adding hysteresis.

Thermostat mode also disables the `pushtemp`/`switchtemp` adjustment, and makes the default set point reference `min` or `max` depending on the current heating mode and hysteresis.

## Settings

There are a number of standard settings as per the [RevK library](https://github.com/revk/ESP32-RevK), including things like `hostname`, `mqtthost`, `wifissid`, etc. Settings specific to the Faikin module are as follows.

The web pages provide settings for the basic WiFi/MQTT, the Faikin specific settings, and an Advanced settings page listing a lot of more detailed settings.

Note that web settings can be disabled with `websettings`, and the web based control pages can be disabled with `webcontrol`. It is also possible to apply a password for the web settings 9this is not sent security, so use with care on a local network which you control).

### Automatic on/off

Every `tsample` seconds the relationship of the adjusted *min*, *max* and *current* are assessed to consider how much time was *approaching* the target band, in the target band, or *beyond* the target band. Two whole samples in a row are considered. Sampling is reset on change of power or mode.

If `auto1` is set, the power on at start of that minute. If `auto0` is set, the power off at start of that minute.

If `autop` is set, and the last sample period is entirely outside the target band, and the current temperature is more than `autoptemp` degrees above or below the target band, then automatic power on.

If `autop` is set, and the last two sample periods are entirely inside the target band, then automatic power off.

### Remote

The system is designed to work with an external remote [Environmental monitor](https://github.com/revk/ESP32-EnvMon). This sends a command `control` periodically containing JSON with `env` being current temperature, and `target` being an array of *min* and *max* target temperature. When remote working `autop` is assumed if `autoptemp` is not `0`.

### Special settings

Some more advances settings which you are unlikely to need to ever change.

|Setting|Meaning|
|-------|-------|
|`debug`|`true` means output lots of debug - notable for S21 this is one line with a set of poll responses. This also causes more fields to be polled than normal, so slower response times.|
|`dump`|`true` means output raw serial communications|
|`uart`|Which internal UART to use|
|`tx`|Which GPIO for tx, prefix `-` to invert the port|
|`rx`|Which GPIO for rx, prefix `-` to invert the port|

## Commands

|Command|Meaning|
|-------|-------|
|`on` `off`|Power on/off|
|`heat` `cool` `auto` `fan` `dry`|Change mode|
|`low` `medium` `high`|Change fan speed|
|`temp`|Set target temp (argument is temp)|
|`status`|Force a status report to be sent|
|`control`|JSON payload with aircon controls, see below|
|`send`|Force sending S21 message, e.g. `D62000`|

## Status

Regular status messages are sent.

- `state/` topic indicate current state, and are reported periodically and on some state changes.
- `Faikin/` topic are sent, typically every minute, and intended for the `faikinlog` command to store in a database. 
- `*MAC*/` topic are sent for HomeAssistant if enabled, and are reported periodically and on some state changes.

The setting `livestatus` causes the `state/` topic on any change.

|Attribute|Meaning|
|---------|-------|
|`online`|Boolean, if the aircon is connected and online|
|`heat`|Boolean, if in heating mode|
|`slave`|Boolean, set if we are not master for heat/cool and hence cannot do requested mode|
|`antifreeze`|Boolean, set if in antifreeze mode and hence not operating as normal|
|`model`|Model name, if known|
|`home`|Temperature at remote / measured|
|`outside`|Outside temperature, if known|
|`inlet`|Inlet temperature, if known|
|`liquid`|Liquid coolant feed temperature, if known|
|`control`|Boolean, if we are under external/automatic control|

The `faikinglog` reports the last periods for values. For each value, if it is the same for the whole period it is reported as is. If not, then for numeric is reported as an array of *min*, *ave*, *max*. For an enumerated type it is the current value. For a Boolean, it is a value `0.0` to `1.0` indicating how much it was `true` in the period.

The `fixstatus` setting forces the format as if the value had changed during the period, i.e. min/ave/max array or 0.0-1.0 for Boolean.

## Aircon control

The controls are things you can change. These can be sent in a JSON payload in an MQTT `control` command (with no suffix), and are reported in the `status` MQTT JSON.

|Attribute|Meaning|
|---------|-------|
|`power`|Boolean, if aircon powered on|
|`mode`|One of `H` (Heat), `C` (Cool), `A` (Auto), `D` (Dry), `F` (Fan)|
|`temp`|Temperature, in Celcius|
|`fan`|One of `A` (Auto), `Q` (Night) and `1` to `5` for manual fan levels|
|`swingh`|Boolean - horizontal louvre swing|
|`swingv`|Boolean - vertical louvre swing|
|`powerful`|Boolean|
|`econo`|Boolean|
|`autor`|Range for automation, `0.0` means off - this sets the `autor` setting to 10 times this value|
|`autot`|Target temp for automation, This sets the `autot` setting to 10 times this value|
|`autob`|The name of the BLE device. This sets the `autob` setting|
|`auto0`|Time to turn off HH:MM, `00:00` is don't turn off. This sets the `auto0` setting|
|`auto1`|Time to turn off HH:MM, `00:00` is don't turn on. This sets the `auto1` setting|
|`target`|This can be a single temperature, or an array of two temperatures (`min`/`max`) which forces Faikin auto mode|
|`env`|This is the temperature reference used for the Faikin auto mode|
|`streamer`| Boolean|

For remote controlled Faikin auto mode, you typically send a `control` message with `target` and `env` in the JSON payload. This needs to be sent regularly to avoid it revertign to normal (not Faikin auto mode).

## Debug

All of the protocols are reverese engineered, and some times now things come to light and new models.

The `snoop` and `dump` and `debug` settings can help decode what is happening.

For anyone in the UK trying to reverse engineer operations using an offical remote / control, we have a small number of dual port *pass through* modules to assist with debug.

<img src='https://github.com/revk/ESP32-Faikin/assets/996983/5f998a5f-d99d-40ca-bf39-fd1206c664df' width=50%><img src='https://github.com/revk/ESP32-Faikin/assets/996983/6c45b348-035e-48a7-81fb-dc43c849b11e' width=50%>
