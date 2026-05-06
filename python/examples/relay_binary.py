#!/usr/bin/env python3
"""Relay AIS from one TCP source to another peer using aiscat's compact
0xac binary packet format.

37-byte packets instead of 100+ byte JSON — useful when forwarding from a
remote receiver to a central aggregator over a metered link. The receiver
on the other end reads with format="dictionary" (or any other format).

Usage: python relay_binary.py src.example.com 4001 aggregator.example.com 5005
"""
import socket
import sys

import aiscat


def main(src_host, src_port, dst_host, dst_port):
    with socket.create_connection((dst_host, dst_port)) as sink:
        for packet in aiscat.from_tcp(src_host, src_port, format="binary"):
            sink.sendall(packet)


if __name__ == "__main__":
    src_host, src_port, dst_host, dst_port = sys.argv[1:5]
    main(src_host, int(src_port), dst_host, int(dst_port))
