# Advanced operation

There are a number of settings and commands that are available via MQTT. To use these you must have set up the device to connect to an MQTT server. You then need a means to send messages to the MQTT server for the device.

The *hostname* you picked is used for these messages. The format for commands and settings is defined in [RevK library](https://github.com/revk/ESP32-RevK). But is basically quite simple. We'll use `GuestAC` as the example hostname.

|Type|Format|Meaning|
|----|------|-------|
|Command|`command/GuestAC/command`|This issues commands to the device to do something now.|
|Setting|`setting/GuestAC`|This allows settings to be set. The payload is JSON, with one or more settings. e.g. `setting/GuestAC {"fanstep":2,"reporting":60}`. With no payload the existing settings are returned in the same format.|
|Setting (direct)|`setting/GuestAC/thing`|This allows a single setting to be set, the payload is the value, e.g. `setting/GuestAC/debug 1`|
|State|`state/GuestAC/whatever`|Status messages - these are normally retained and relate to specific aspects of the operation. With no paramater, e.g. `state/GuestAC` this is the state of the device itself, with `false` meaning it is now off line.|
|Event|`event/GuestAC/whatever`|Event messages are one of things that have happened and are not retained.|
|Info|`info/GuestAC/whatever`|Info messages are informational, and don't relate to a specific event happening.|
|Error|`error/GuestAC/whatever`|Errors are reported using this.|

## Settings

There are a number of standard settings as per the library, including things like `hostname`, `mqtthost`, `wifissid`, etc. Settings specific to the Faikin module are as follows.

|Setting|Meaning|
|-------|-------|
|`webcontrol`|`0` means no web access, `1` means just aircon settings not WiFI, etc. `2` means all controls|
|`ha`|`true` means work with Home Assistant via MQTT|
|`reporting`|Interval for reporting state (seconds)|

### External/automatic controls

An external unit can set external *min*/*max* controls and reference temperature. This is planned to be made an internal feature for automatic control in future.

|Setting|Meaning|
|-------|-------|
|`coolover` `heatover`|When we want to turn on heating or cooling the temperature is set to the target, adjusted for the temperature the air-con unit is seeing. To encourage the air-con to actually apply the heating/cooling this has a number of degrees added (for heat) or reduced (for cool) as set by these settings.
|`coolback` `heatback`|When we want to turn off the heating or cooling, we set a temperature that backs away from the target temperature by this many degrees.|
|`tsample`|Automation sampling time period (seconds), usually `900`|
|`autoband`|If to turn on/off automatically, this applies if not `0`. Turns off if in band for two sample periods. Turns on if the difference between *min* and *max* from external/automatic control is less than `autoband` degrees and out of band for two sample period|
|`switch10`|This is our own hysteresis - it applies to temp or min/max settings meaning we have to got this much extra beyond limits before considering switching heat/cool.|
|`switchtime`|This is a minimum time in heat/cool mode before allowing switching|
|`switchdelay`|This is a minimum time that we have to have been beyond the target before switching is allowed - it is to allow for an initial overshoot typically when direct turned on and reaching target temperature the first time.|
|`autotime`|This is the time the auto command is considered valid after which we revert to simply setting auto mode on the aircon unit itself. You need to ensure the external control (environmental monitor) sends the auto command more often than this.|
|`fantime`|This is how long we wait before changing fan - i.e. if we are not able to reach target temperature in this time the fan is increased.|
|`fanstep`|Usually worked out automatically, but this says if internally the fan settings are 1/3/5 (Low/medium/high) or 1/2/3/4/5. `2` for the 1/3/5 mode. `0` for automatic.|
|`thermref`|A percentage, `0` means reference is `inlet` temperature, `100` means reference is `home` temperature.|
|`ble`|Boolean, if set then enable BLE working for external BlueCoinT device|
|`autor`|A range value in 0.1C, `0` means disable local automation, `5` means ±0.5C automated controls|
|`autot`|When `autor` is not `0` this is the target, with `autor` setting the range|
|`autob`|When set this is the name of a BlueCoinT temperature sensor to use as the reference temperature|

An `info` update `automation` is sent every `tsample` seconds whilst automatic control is in place.

### Special settings

Some more advances settings which you are unlikely to need to ever change.

|Setting|Meaning|
|-------|-------|
|`debug`|`true` means output lots of debug info|
|`dump`|`true` means output raw serial communications|
|`morepoll`|`true` means poll all known messages (S21) rather than just those needed - slows things dowm, only use to try and find new settings for s/w development|

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

## Status

The status are further fields, which you cannot change, and reported in the status MQTT JSON. This is the snapshot status.

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

The reporting data for logging contains the controls and status, but has arrays of min/ave/max for temperatures, min/max for target temp, and value between 0 and 1 for booleans when not all the same for the period. For enumerated codes (fan, mode) the data is a snapshot.

## Aircon control

The controls are things you can change. These can be sent in a JSON payload in an MQTT command (with no suffix), and are reported in the `status` MQTT JSON.

|Attribute|Meaning|
|---------|-------|
|`power`|Boolean, if aircon powered on|
|`mode`|One of `H` (Heat), `C` (Cool), `A` (Auto), `D` (Dry), `F` (Fan)|
|`temp`|Temperature, in Celcius|
|`fan`|One of `A` (Auto) and `1` to `5` for manual fan levels|
|`swingh`|Boolean - horizontal louvre swing|
|`swingv`|Boolean - vertical louvre swing|
|`powerful`|Boolean|
|`econo`|Boolean|
