#!/usr/bin/env python3
"""Live decode of the Norwegian Coastal Administration's public AIS feed.

A free, registration-less TCP/IP feed of AIS traffic around Norwegian waters,
released under the Norwegian Licence for Open Government Data (NLOD 1.0):
    https://kystverket.no/en/navigation-and-monitoring/ais/access-to-ais-data/

Useful for kicking the tyres without setting up a receiver.

Usage: python from_norway.py
"""
import aiscat


HOST, PORT = "153.44.253.27", 5631
POSITION_TYPES = {1, 2, 3, 18, 19, 27}


def main():
    for msg in aiscat.from_tcp(HOST, PORT, country=True):
        if msg["type"] in POSITION_TYPES and "lat" in msg:
            print(
                f"{msg['mmsi']:>10}  "
                f"{msg.get('country_code', '??'):2}  "
                f"{msg['lat']:>9.5f}, {msg['lon']:>9.5f}  "
                f"{msg.get('speed', 0):>5.1f} kn"
            )


if __name__ == "__main__":
    main()
