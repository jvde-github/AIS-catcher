#!/usr/bin/env python3
"""Decode an NMEA file and write position reports to CSV.

Usage: python to_csv.py session.nmea positions.csv
"""
import csv
import sys

import aiscat


POSITION_TYPES = {1, 2, 3, 18, 19, 27}
COLUMNS = ["rxuxtime", "mmsi", "type", "lat", "lon", "speed", "course", "heading"]


def main(in_path, out_path):
    with open(out_path, "w", newline="") as fp:
        w = csv.DictWriter(fp, COLUMNS, extrasaction="ignore")
        w.writeheader()
        n = 0
        for msg in aiscat.from_file(in_path):
            if msg["type"] in POSITION_TYPES and "lat" in msg:
                w.writerow(msg)
                n += 1
    print(f"wrote {n} rows -> {out_path}")


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
