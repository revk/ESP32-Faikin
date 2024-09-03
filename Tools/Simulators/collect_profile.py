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
commands = ["F8", "FC", "F2", "F3", "F4", "FB", "FG", "FK", "FN", "FP", "FQ", "FR", "FS", "FT"]
cmd_index = 0

def send_command(client):
    global cmd_index
    client.publish("command/{}/send".format(faikin_name), commands[cmd_index])
    cmd_index = cmd_index + 1

# The callback for when the client receives a CONNACK response from the server.
def on_connect(client, userdata, flags, reason_code, properties):
    print(f"Connected with result code {reason_code}")
    # Subscribing in on_connect() means that if we lose the connection and
    # reconnect then subscriptions will be renewed.
    client.subscribe("info/{}/rx".format(faikin_name))
    send_command(client)

def parse_response(hex_str):
    # Example string: 02474B71733531DC03
    data = bytearray.fromhex(hex_str)
    data[1] = data[1] - 1 # Deduce command from response
    cmd = data[1:3].decode("ascii")

    if cmd != commands[cmd_index - 1]:
        return False;

    # Reset 'error 252' flag
    if cmd == "F4":
        data[5] = data[5] & 0xDF

    if cmd == "FC":
        print("model", data[6:2:-1].decode("ascii"))
        return True
    if cmd == "F8":
        cmd = "protocol"
    print("%s 0x%02X 0x%02X 0x%02X 0x%02X" % (cmd, data[3], data[4], data[5], data[6]))

    return True

def parse_empty_response(cmd, what):
    if cmd != commands[cmd_index - 1]:
        return False;
    
    print("%s <%s>" % (cmd, what))
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
