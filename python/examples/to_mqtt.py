#!/usr/bin/env python3
"""Forward an AIS TCP feed to MQTT, one topic per MMSI.

Subscribers can listen to ais/+/position for every vessel's reports, or
ais/<mmsi>/+ for one specific vessel.

Requires: pip install paho-mqtt
Usage: python to_mqtt.py ais.example.com 4001 mqtt.example.com
"""
import json
import sys

import paho.mqtt.client as mqtt

import aiscat


POSITION_TYPES = {1, 2, 3, 18, 19, 27}


def main(ais_host, ais_port, mqtt_host, mqtt_port=1883):
    client = mqtt.Client()
    client.connect(mqtt_host, mqtt_port)
    client.loop_start()
    try:
        for msg in aiscat.from_tcp(ais_host, ais_port):
            kind = "position" if msg["type"] in POSITION_TYPES else "static"
            topic = f"ais/{msg['mmsi']}/{kind}"
            client.publish(topic, json.dumps(msg, separators=(",", ":")))
    finally:
        client.loop_stop()
        client.disconnect()


if __name__ == "__main__":
    ais_host, ais_port, mqtt_host = sys.argv[1], int(sys.argv[2]), sys.argv[3]
    main(ais_host, ais_port, mqtt_host)
