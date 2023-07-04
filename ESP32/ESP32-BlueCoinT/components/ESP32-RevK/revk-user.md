# ESP32 RevK support library

The `ESP32-RevK` support library is used in almost all of the ESP32 projects on this repository. It provides basic management of WiFi connections, settings, MQTT, and JSON.

This manual covers the user view of this library in terms of interacting with any of the projects using it over MQTT.


## App name

The application will have a name. A good example is the Daikin appi, e.g. see https://github.com/revk/ESP32-Daikin which is, as you may guess `Daikin`.

## WiFi Settings

One of the key features is allowing WiFi to be configured on a new unit. This is a config option so does not have to be enabled, but if it is then when a unit is unable to connect to local WiFI, it changes to be an access point.
The WiFI name you see starts with the app name.

![IMG_2446](https://user-images.githubusercontent.com/996983/218394248-b409626b-2614-439d-95f4-71527c00aaa7.PNG)

If you select this on an iPhone, it auto loads the config page. On other devices you have to check the router IP for the subnet and enter that in to a browser. Either way you get the WiFi settings page that will look something like this (other fields may be added over time).

![IMG_2448](https://user-images.githubusercontent.com/996983/218394254-e03537f7-09a0-490d-aa38-b6964a5cd77f.PNG)

You can click the buttons for existing SSID, or enter manually, and enter password. It then tries to connect and shows the IP so you can connect to any web interface on the device. Not all devices have a web interface.

However, the MQTT settings allow control and settings to be configured over MQTT.

## Software updates

It is usually a good idea to ensure software is up to date. The system has a setting for the `otahost` which will default to `ota.revk.uk` but can be your own server, obviously.

You can do an upgrade from the web control pages for most apps with a link from the WiFi settings page. You can also do an `upgrade` command from MQTT (which allows the full URL and file to be specified, if needed).

However, for most apps (based on the `otaauto` settings) upgrades will be checked every few days, and done if needed. This is normally within one to two hours of start up (to allow you time to make any changes if needed, including turning off auto update), or the middle of the night (to minimise disruption). The server is checked and the update and restart is only done if there is a new version.



## MQTT 

The system will connect to an MQTT server and provide information via MQTT, allow commands over MQTT, and allow settings over MQTT.

### Topics

There are two styles, which depend on a config option. One style has (`/` separated) *prefix*, then *app name*, then *device id*, then an optional suffix. The other format omits the *app name*.

The *device id* is either a hex device ID, its MAC address, or the `hostname` setting value if set.

Where the topic has an *app name* you can command all of the device with that *app name* by using a *device name* of `*`.
Where the topic does not have the *app name* you can command all of the devices with the same *app name* by using the *app name* as the *device name*. 

#### Messages to the device

In most cases the payload, if any, is JSON. This could however by a JSON data type such as a number, or string, rather than an actual object.

|Prefix|Meaning|
|------|-------|
|`command`|Does a command. This does not change settings but could change some live status of some sort, or do an action. In some cases commands can talk to external devices, such as the SDC41 in the Environmental monitor.|
|`setting`|Changes a setting value, or gets settings. See below|

#### Messages from the device

In most cases the payload is JSON, usually as a JSON object.

|Prefix|Meaning|
|------|-------|
|`state`|This is sent with *retain* and relates to the state of some aspect of the device. With no suffix, this is a top level state for the device, in JSON, including either `"up":false` or `"up":`*time*. With a suffix this is the state of some aspect of the device.|
|`event`|This is for an event, rather than a state change. A good example might be a key press on a device.|
|`info`|This is information of some sort|
|`error`|This is an error message|
|`setting`|This is the current settings, as a JSON object, if requested|

### Commands

The device may have any number of commands documents, but there are some commands provided directly by the library for all devices.

|Command|Meaning|
|-------|-------|
|`upgrade`|This does an *over the air* upgrade from the setting defined `otahost`. You can include a URL as the argument (`http://` only, not `https`). Usually the device will be build with code signing to ensure the file is genuine.|
|`restart`|This does a restart of the device|
|`factory`|This does a factory reset of all settings, the argument has to be a string of the MAC address and the app name, e.g. `112233445566TestApp`|
|`ps`|This provides a process list, if `FREERTOS_USE_TRACE_FACILITY` is set in `sdkconfig`|

### Settings

The settings system currently uses JSON, but does also have a fallback for a single setting using the full setting name.

For example, setting the `otahost` could be done by sending `setting/App/Dev/otahost example.com` or by sending `setting/App/Dev {"otahost":"example.com"}`. The recommended method is to use JSON, which means no suffix on the `setting` message.

Sending a `setting` message with no suffix and no payload causes a `setting` response to be sent with current settings.  If the settings are too long for one message then multiple messages are sent covering all of th settings using a number of distinct objects.

All settings have a simple name, but some settings can also be accessed in a nested style, as a sub object. The settings sent back from the device use this. Also, some settings have have a number of values, and these can be sent as a JSON array. For example ther are settings `mqtthost`, `mqttuser`, `mqttpass`, etc. These can be sent as as `{"mqtt":{"host":"whatever","user":"whatever","pass":"whatever"}`. Sending an object like this will mean clearing any sub objects not included in the object, e.g. sending `{"mqtt":{"host":"example.com"}` means that `mqttuser` and `mqttpass` are unset (set to empty string).

#### Main settings

|Setting|Meaning|
|-------|-------|
|`appname`|The name of the application. You do not normally want to override this.|
|`hostname`|The name to use for this device in topic, and DHCP and so on. This defaults the hex MAC address if not set or set to an empty string.|
|`otahost`|Hostname for *over the air* updates|
|`otaauto`|If not `0` then check for updates periodically (this many days, approx), and do upgrade if needed.|
|`wifissid`|WiFi SSID to use|
|`wifipass`|WiFi passphrase to use|
|`mqtthost`|MQTT hostname|

#### Advanced settings

|Setting|Meaning|
|-------|-------|
|`mqttuser`|MQTT username|
|`mqttpass`|MQTT password|
|`mqttport`|MQTT TCP port|
|`mqttcert`|MQTT certificate - PEM format TLS certificate for the server. If set this forces MQTT over TLS and a default port of `8883`|
|`clientkey`|PEM format TLS client private key for use on TLS (e.g. for MQTTS and HTTPS)|
|`clientcert`|PEM format TLS client certificate for use on TLS (e.g. for MQTTS and HTTPS)|
|`otacert`|PEM format TLS certificate for the server, forces OTA using HTTPS - only sensible on devices with enough RAM. Default is to use HTTP and have signed code.|
|`nodename`|This is not usually used, but if set it means `"node":`*nodename* will be included in various types of message sent.|
|`wifichan`|WiFi channel to use, default is scan all channels|
|`wifibssid`|Hex WiFi BSSID to use, default is any|
|`wifireset`|Time (in seconds) with no WiFI before resetting device|
|`wifiip`|Static IPv4 for WiFi|
|`wifigw`|Static IPv4 gateway for WiFi|
|`wifidns`|Static IPv4 DNS server for WiFi|
|`wifimqtt`|This is a special case, an SSID to use if possible (falls back to normal `wifissid`), and connect via MQTT to the router IP address received.|
|`blink`|Either one GPIO number or an array of GPIO numbers for *R*, *G*, *B* LEDs, prefix each with `-` for active low LED.|
|`apport`|The TCP port for access point web page (normally 80)|
|`apwait`|The time in seconds before entering AP mode when no WiFi|
|`aptime`|The time to stay in AP mode|
|`apgpio`|The GPIO to check to force in to AP mode, this allows a button to force AP mode, for example.|
|`apssid`|The SSID to use in AP mode, defaults to the `appname` based AP name|
|`appass`|The passphrase to use in AP mode, usually you want this as an empty string for no password|
|`aplr`|Do AP mode using ESP specific *long range* WiFi mode|
|`aphide`|Do AP mode as hidden SSID|
|`meshid`|Work in mesh mode and use this as the mesh ID, a 12 character HEX string|
|`mesgpass`|The passphrase for mesh mode working|
|`prefixcommand`|The prefix for `command`|
|`prefixsetting`|The prefix for `setting`|
|`prefixstate`|The prefix for `state`|
|`prefixevent`|The prefix for `event`|
|`prefixinfo`|The prefix for `info`|
|`prefixerror`|The prefix for `error`|
