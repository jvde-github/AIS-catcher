#!/usr/bin/env python3
"""Forward an AIS TCP feed to a Kafka topic, keyed by MMSI.

Keying by MMSI gives consumers per-vessel ordering when the topic is
partitioned. format="json" skips the dict allocation — aiscat emits the
JSON envelope as bytes, ready to ship.

Requires: pip install confluent-kafka
Usage: python to_kafka.py ais.example.com 4001 broker:9092 ais-raw
"""
import re
import sys

from confluent_kafka import Producer

import aiscat


MMSI_RE = re.compile(rb'"mmsi":(\d+)')


def main(ais_host, ais_port, broker, topic):
    producer = Producer({"bootstrap.servers": broker, "linger.ms": 50})
    n = 0
    try:
        for payload in aiscat.from_tcp(ais_host, ais_port, format="json"):
            m = MMSI_RE.search(payload)
            key = m.group(1) if m else None
            producer.produce(topic, value=payload, key=key)
            n += 1
            if n % 1000 == 0:
                producer.poll(0)
    finally:
        producer.flush()
        print(f"forwarded {n} messages")


if __name__ == "__main__":
    ais_host, ais_port, broker, topic = sys.argv[1:5]
    main(ais_host, int(ais_port), broker, topic)
