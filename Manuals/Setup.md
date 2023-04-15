# Faikin - set-up

The *Faikin* is a small circuit board that can replace the common Daikin air-con control WiFi modules.

- Local web control over WiFi, no cloud/account needed, no Internet needed
- MQTT control and reporting
- Integration with Home Assistant using MQTT

## Installation

<img src="Install1.jpg" width=20% align=right>The module plugs in to the existing lead used for Daikin WiFi modules. If you do not have a lead you will need one.

The plug on the lead has five positions, but only four wires, these are connected as shown.

Later modules have a five pin socket matching the plug.

A 3D prinabale case design is included on GitHub.

## WiFi set up

One installed, the LED should light up and blink.

Look for a WiF Access Point called Daikin (or Faikin), e.g.

![WiFiAP](WiFi1.png)

Select this and it should connect, needing no password.

![WiFiAP](WiFi2.png)

On an iPhone this should automatically open a web page. On other devices you may need to check the IP settings and enter the *router IP* in to your browser. The page looks like this.

![WiFi](WiFi3.png)

Enter details and press **Set**.

### Hostname

Pick a simple one work hostname to describe your air-con, e.g. GuestAC.

### SSID/Password

Enter the details for your own WiFi. You will note a list of SSIDs that have been seen are shown - you can click on one to set the SSID to save typing it. Make sure you enter the passphrase carefully. If the device is unable to connect the page should show an error and allow you to put in settings again. Only 2.4GHz WiFi is supported, and some special characters in SSID may not be supported.

### MQTT

If using MQTT, which could be Home Assistant running MQTT, enter the hostname or IP address of the MQTT server.

In addition you will usually see the option for an MQTT *username* and *password* - these are usually needed if using Home Assistant MQTT server. You can add a user on Home Assistant and then enter the details here.

## Accessing controls

One set up, the device connects to your WIFi. From the same WiFi you should be able to access from a web browser using the hostanme you have picked followed by `.local`, e.g. `GuestAC.local`.

The controls page shows teh controls for your air-con, and also has a link for *WiFi settings* allowing you to change the WiFi and MQTT settings if needed.

# Software Upgrade

We recommend you upgrade the software when you receive the device, as new features are often added.

Go to the web page, and select *WiFi settings*. You can click on *Upgrade*. This does need internet access.