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
|`tpredicts`|Sample time (seconds) for predictive adjustment|
|`tprecictt`|Total prediction time (seconds) for predictive adjustment|
|`switch10` `push10`|This is our own hysteresis - it applies to min/max settings to allow for overshoot on heating/cooling.|
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
|`auto0`|Numeric in format HHMM, time to automatically turn off (0000 means don't turn off)|
|`auto1`|Numeric in format HHMM, time to automatically turn on (0000 means don't turn on)|
|`autop`|Boolean, if we automatically turn on/off power based on temperature|
|`autop10`|Temperature offset for auto turn on with `autop` x 10|


An `info` update `automation` is sent every `tsample` seconds whilst automatic control is in place.

The automation works based on the current *min* and *max* target and *current* temperature. However, *min* and *max* are adjusted by `push10` and `switch10`. The *current* temperature is also adjusted based on `tpredict` settings. These show on the `automation` status report. These are then used to set a target tempurature on the aircon itself that is higher or lower than the unit thinks the current temperature is based on `coolover`/`heatover`, effectively turning it on/off.

### Automatic on/off

Every `tsample` seconds the relationship of the adjusted *min*, *max* and *current* are assessed to consider how much time was *approaching* the target band, in the target band, or *beyond* the target band. Two whole samples in a row are considered. Sampling is reset on change of power or mode.

If `auto1` is set, the power on at start of that minute. If `auto0` is set, the power off at start of that minute.

If `autop` is set, and the last sample period is entirely outside the target band, and the current temperature is more than `autop10`/10 degrees above or below the target band, then automatic power on.

If `autop` is set, and the last two sample periods are entirely inside the target band, then automatic power off.

### Remote

The system is designed to work with an external remote [Environmental monitor](https://github.com/revk/ESP32-EnvMon). This sends a command `control` periodically containing JSON with `env` being current temperature, and `target` being an array of *min* and *max* target temperature. When remote working `autop` is assumed if `autop10` is not `0`.

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
|`autor`|Range for automation, `0.0` means off - this sets the `autor` setting to 10 times this value|
|`autot`|Target temp for automation, This sets the `autot` setting to 10 times this value|
|`autob`|The name of the BLE device. This sets the `autob` setting|
|`auto0`|Time to turn off HH:MM, `00:00` is don't turn off. This sets the `auto0` setting|
|`auto1`|Time to turn off HH:MM, `00:00` is don't turn on. This sets the `auto1` setting|
