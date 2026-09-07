#!/usr/bin/env python3
"""Verify an existing history backup upgrades without losing saved counters.

Usage: python3 tests/history_backup.py build/AIS-catcher path/to/old.stats
Only temporary copies are opened by AIS-catcher. Requires localhost sockets.
"""
import contextlib
import hashlib
import json
import pathlib
import socket
import struct
import subprocess
import sys
import tempfile
import time
import urllib.request

BINARY = str(pathlib.Path(sys.argv[1]).resolve())
ORIGINAL = pathlib.Path(sys.argv[2])


def read_backup(path):
    data = path.read_bytes()
    offset = 0
    def read(fmt):
        nonlocal offset
        result = struct.unpack_from(fmt, data, offset)
        offset += struct.calcsize(fmt)
        return result
    def stat():
        magic, version, count = read('=3i')
        assert magic == 0x4f82b and 1 <= version <= 4
        vessels = read('=i')[0] if version >= 2 else 0
        types = read('=28i' if version >= 3 else '=27i')
        if version < 3:
            types += (0,)
        channel = read('=4i')
        values = read('=40f')  # four signal/range fields and two 18-bin radars
        errors = read('=4i') if version >= 4 else (0, 0, 0, 0)
        return dict(count=count, vessels=vessels, types=types, channel=channel, values=values, errors=errors)
    total = stat()
    histories = []
    for expected_n, expected_interval in [(60, 1), (60, 60), (24, 3600), (90, 86400)]:
        header = read('=7i')
        assert header[:2] == (0x4f80b, 1)
        assert header[3:5] == (expected_interval, expected_n)
        buckets = [(read('@l')[0], stat()) for _ in range(expected_n)]
        histories.append((header, buckets))
    return total, histories, data[offset:]


def write_legacy(path, snapshot, version):
    total, histories, tail = snapshot
    out = bytearray()
    def write(fmt, *values):
        out.extend(struct.pack(fmt, *values))
    def stat(s):
        write('=3i', 0x4f82b, version, s['count'])
        if version >= 2:
            write('=i', s['vessels'])
        write('=28i' if version >= 3 else '=27i', *s['types'][:28 if version >= 3 else 27])
        write('=4i', *s['channel'])
        write('=40f', *s['values'])
    stat(total)
    for header, buckets in histories:
        write('=7i', *header)
        for timestamp, s in buckets:
            write('@l', timestamp)
            stat(s)
    out.extend(tail)
    path.write_bytes(out)


def comparable(snapshot):
    total, histories, _ = snapshot
    # The existing loader deliberately resets vessels in the active bucket.
    def stable(s, active):
        return {k: (0 if k == 'vessels' and active else v) for k, v in s.items()}
    return total, [(h[3:], [(t, stable(s, i == h[6])) for i, (t, s) in enumerate(buckets)]) for h, buckets in histories]


@contextlib.contextmanager
def receiver(backup):
    with socket.socket() as sock:
        sock.bind(('127.0.0.1', 0))
        port = sock.getsockname()[1]
    with tempfile.TemporaryFile() as log:
        proc = subprocess.Popen([BINARY, '-r', 'txt', '.', '-X', 'off', '-o', '0',
                                 '-N', str(port), 'FILTER', 'on', 'FILE', str(backup)],
                                stdin=subprocess.PIPE, stdout=log, stderr=log)
        def stats():
            with urllib.request.urlopen(f'http://127.0.0.1:{port}/api/stat.json', timeout=1) as response:
                return json.load(response)
        try:
            deadline = time.monotonic() + 10
            while True:
                try:
                    stats()
                    break
                except (OSError, ValueError):
                    if time.monotonic() > deadline:
                        log.seek(0)
                        raise AssertionError(log.read().decode())
                    time.sleep(.05)
            yield proc, stats
        finally:
            proc.terminate()
            proc.communicate(timeout=10)
        log.seek(0)
        output = log.read().decode()
        assert 'Could not load' not in output and 'could not be restored' not in output, output
        assert 'cannot write' not in output and 'write error' not in output, output


original_hash = hashlib.sha256(ORIGINAL.read_bytes()).hexdigest()
source = read_backup(ORIGINAL)
with tempfile.TemporaryDirectory() as tmp:
    for version in (1, 2, 3):
        path = pathlib.Path(tmp) / f'v{version}.stats'
        write_legacy(path, source, version)
        before = read_backup(path)
        with receiver(path) as (_, stats):
            assert stats()['total']['count'] == before[0]['count']
            assert stats()['total']['errors'] == dict.fromkeys(('flagged', 'undersized', 'oversized', 'checksum'), 0)
        assert comparable(before) == comparable(read_backup(path)), f'v{version} changed saved history'
        assert struct.unpack_from('=i', path.read_bytes(), 4)[0] == 4
        with receiver(path) as (proc, stats):
            # An identifiable 162-bit Class A report with a deliberately wrong checksum.
            proc.stdin.write(b'!AIVDM,1,1,,A,13LB1L0vkAQ1<KpOR>31`ADr00S,0*00\n')
            proc.stdin.flush()
            deadline = time.monotonic() + 5
            while stats()['total']['count'] != before[0]['count'] + 1:
                assert time.monotonic() < deadline
                time.sleep(.02)
            assert stats()['total']['errors'] == {'flagged': 1, 'undersized': 1, 'oversized': 0, 'checksum': 1}
        saved = read_backup(path)
        assert saved[0]['errors'] == (1, 1, 0, 1)
        for _, buckets in saved[1]:
            assert tuple(map(sum, zip(*(s['errors'] for _, s in buckets)))) == (1, 1, 0, 1)
        with receiver(path) as (_, stats):
            assert stats()['total']['errors'] == {'flagged': 1, 'undersized': 1, 'oversized': 0, 'checksum': 1}
        assert comparable(saved) == comparable(read_backup(path)), 'v4 reload changed saved history'
        print(f'v{version} -> v4: historical counters preserved; errors persisted across all history resolutions and restart')
assert hashlib.sha256(ORIGINAL.read_bytes()).hexdigest() == original_hash
print('Original backup unchanged:', ORIGINAL)
