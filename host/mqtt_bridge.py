#!/usr/bin/env python3
"""Read JSON telemetry lines from the STM32 gateway over serial and publish them to MQTT.

pip install pyserial paho-mqtt
Test: mosquitto_sub -t 'gw/#' -v
"""
import argparse
import json

import paho.mqtt.client as mqtt
import serial


def make_client():
    try:
        return mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)   # paho-mqtt >= 2.0
    except AttributeError:
        return mqtt.Client()                                   # paho-mqtt 1.x


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", default="/dev/ttyUSB0", help="USB-UART adapter (COMx on Windows)")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--broker", default="localhost")
    ap.add_argument("--mqtt-port", type=int, default=1883)
    args = ap.parse_args()

    client = make_client()
    client.connect(args.broker, args.mqtt_port)
    client.loop_start()

    with serial.Serial(args.port, args.baud, timeout=1) as ser:
        print(f"bridging {args.port} -> mqtt://{args.broker}:{args.mqtt_port}")
        while True:
            raw = ser.readline()
            if not raw:
                continue
            try:
                msg = json.loads(raw.decode("ascii", errors="replace"))
            except json.JSONDecodeError:
                print("skipped malformed line:", raw)
                continue
            client.publish(msg["topic"], json.dumps({"ts": msg["ts"], "value": msg["payload"]}))
            print(f'{msg["topic"]:<16} {msg["payload"]}')


if __name__ == "__main__":
    main()
