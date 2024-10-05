# This script collects replies for listed commands from the A/C and outputs
# dump, suitable for .settings file
# Usage: collect_profile.py <host> <Faikin name>
# Works reliably only with Faikin debug turned off
# Prerequisites: pip install paho-mqtt

import json
import paho.mqtt.client as mqtt
import sys
import time

faikin_name = ""
commands = [
    # Common commands
    "F8", "FC", "F2", "F3", "F4", "FB", "FG", "FK", "FN", "FP", "FQ", "FR", "FS", "FT", "FV",
    # 1-character commands
    "M", "V",
    # Discovered by analyzing v3 protocol, but at least v2 A/Cs also have it
    "VS000M",
    # v3 extended commands
    "FY00", "FY10", "FY20", "FU00", "FU02", "FU04",
    "FU05", "FU15", "FU25", "FU35", "FU45", "FU55", "FU65", "FU75", "FU85", "FU95",
    "FX00", "FX10", "FX20", "FX30", "FX40", "FX50", "FX60", "FX70", "FX80", "FX90", "FXA0", "FXB0", "FXC0", "FXD0", "FXE0", "FXF0",
    "FX01", "FX11", "FX21", "FX31", "FX41", "FX51", "FX61", "FX71", "FX81"
]
cmd_index = 0

protocol_ver = 0
profile_data = []

def dump_profile():
    global protocol_ver
    print("--- cut simulator profile begins here ---")
    print("protocol", protocol_ver)
    for item in profile_data:
        print(item[0], item[1])

def send_command(client):
    global cmd_index

    cmd = commands[cmd_index]
    print(cmd, end=' ', flush=True)

    # Due to Faikin internal restrictions (it assumes at least 2-char code)
    # we replicate the character twice. It works because A/C ignores excess data.
    if len(cmd) == 1:
        cmd = cmd + cmd

    client.publish("command/{}/send".format(faikin_name), cmd)
    cmd_index = cmd_index + 1

# The callback for when the client receives a CONNACK response from the server.
def on_connect(client, userdata, flags, reason_code, properties):
    print(f"Connected with result code {reason_code}")
    # Subscribing in on_connect() means that if we lose the connection and
    # reconnect then subscriptions will be renewed.
    client.subscribe("info/{}/rx".format(faikin_name))
    send_command(client)

def parse_response(hex_str):
    global protocol_ver
    # Example string: 02474B71733531DC03
    data = bytearray.fromhex(hex_str)

    # Deduce originating command from response.
    # Replies to M and V don't have 1st byte incremented
    if data[1] == 0x56 and data[2] == 0x53: # VS
        cmd = "VS000M"
        payload_offset = 3
    elif data[1] == 0x4D or data[1] == 0x56: # M or V
        cmd = chr(data[1])
        payload_offset = 2
    else:
        data[1] = data[1] - 1
        cmd = data[1:3].decode("ascii")
        if cmd == 'FU' or cmd == 'FX' or cmd == 'FY':
            # These commands are 4-char long
            cmd = data[1:5].decode("ascii")
        payload_offset = 1 + len(cmd)

    if cmd != commands[cmd_index - 1]:
        return False; # Response to something else, ignore

    payload = data[payload_offset:-2]

    for b in payload:
        print("%02X" % b, end=' ')
    print()

    if cmd == "F8":
        protocol_ver = payload[1] & ~0x30
    elif cmd == "FC":
        profile_data.append(["model", payload[::-1].decode("ascii")])
    elif cmd == "FY00":
        ver_string = payload.decode("ascii")
        protocol_ver = ver_string[2] + '.' + ver_string[1] + ver_string[0]
        if ver_string[3] != '0':
            protocol_ver = ver_string[3] + protocol_ver
    else:
        # Reset 'error 252' flag
        if cmd == "F4":
            if payload[2] & (1 << 5) != 0:
                print("   WARNING! Unit error bit was detected and reset!");
                payload[2] = payload[2] & ~(1 << 5)
        profile_data.append([cmd, ' '.join('0x' + format(x, '02X') for x in payload)])

    return True

def parse_empty_response(cmd, what):
    sent = commands[cmd_index - 1]
    # NAK message from Faikin only contains two characters of the code
    if len(sent) > 2 and len(cmd) == 2:
        sent = sent[0:2]
    if cmd != sent:
        return False
    
    print("<%s>" % what)
    return True

# The callback for when a PUBLISH message is received from the server.
def on_message(client, userdata, msg):
    data = json.loads(msg.payload.decode("ascii"))
    got_response = False

    if 'dump' in data:
        got_response = parse_response(data['dump'])
    elif 'ack' in data:
        got_response = parse_empty_response(data['cmd'], "ACK")
    elif 'nak' in data:
        got_response = parse_empty_response(data['cmd'], "NAK")

    if not got_response:
        return # Something we aren't waiting for

    if (cmd_index == len(commands)):
        print("Done!")
        dump_profile()
        sys.exit(0)

    # Wait a bit before sending next command, or it can be lost
    time.sleep(0.5)
    send_command(client)

if len(sys.argv) < 3:
    print("Usage: {} <host> <Faikin name>".format(sys.argv[0]))
    sys.exit(255)

faikin_name = sys.argv[2]

mqttc = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
mqttc.on_connect = on_connect
mqttc.on_message = on_message

mqttc.connect(sys.argv[1], 1883, 60)

# Blocking call that processes network traffic, dispatches callbacks and
# handles reconnecting.
# Other loop*() functions are available that give a threaded interface and a
# manual interface.
mqttc.loop_forever()
