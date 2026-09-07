#!/usr/bin/env python3
"""Integration checks: python3 tests/nmea_errors.py build/AIS-catcher.

Uses temporary files and localhost HTTP; community sharing is disabled.
"""
import json
import pathlib
import socket
import subprocess
import sys
import tempfile
import time
import unittest
import urllib.request

BINARY = str(pathlib.Path(sys.argv.pop(1) if len(sys.argv) > 1 else 'build/AIS-catcher').resolve())
BASE = '13LB1L0vkAQ1<KpOR>31`ADr00S4'


def payload(mmsi):
    bits = ''.join(f'{ord(c)-48-(8 if ord(c)>87 else 0):06b}' for c in BASE)
    bits = bits[:8] + f'{mmsi:030b}' + bits[38:]
    return ''.join(chr(int(bits[i:i+6], 2) + (48 if int(bits[i:i+6], 2) < 40 else 56)) for i in range(0, len(bits), 6))


def sentence(p, fill=0, bad=False, total=1, part=1):
    seq = "1" if total > 1 else ""
    body = f'AIVDM,{total},{part},{seq},A,{p},{fill}'
    checksum = 0
    for c in body:
        checksum ^= ord(c)
    return f'!{body}*{checksum ^ int(bad):02X}\n'


def fixture():
    return ''.join([
        sentence(payload(244000001)),
        sentence(payload(244000002)[:-1]),
        sentence(payload(244000003) + '000000'),
        sentence(payload(244000004), bad=True),
        sentence(payload(244000005)[:-1], bad=True),
        sentence(payload(244000006)),
    ]).encode()


class Errors(unittest.TestCase):
    def run_file(self, data, *extra):
        with tempfile.TemporaryDirectory() as tmp:
            path = pathlib.Path(tmp) / 'input'
            path.write_bytes(data)
            return subprocess.run([BINARY, '-r', 'txt', str(path), '-X', 'off', '-o', '5', *extra], capture_output=True, timeout=15, check=True)

    def decode(self, data, *extra):
        return [json.loads(l) for l in self.run_file(data, *extra).stdout.splitlines() if l.startswith(b'{')]

    def test_flags_and_json_roundtrip(self):
        rows = self.decode(fixture())
        self.assertTrue(all("error" not in row for row in rows))
        self.assertEqual([r.get('quality', 0) for r in rows], [0, 1024, 2048, 512, 1536, 0])
        # An upstream checksum error remains even when the embedded NMEA is valid.
        rows[3]['nmea'] = [sentence(payload(244000004)).strip()]
        forwarded = ('\n'.join(json.dumps(r) for r in rows) + '\n').encode()
        self.assertEqual([r.get('quality', 0) for r in self.decode(forwarded)], [0, 1024, 2048, 512, 1536, 0])

    def test_binary_roundtrip(self):
        with tempfile.TemporaryDirectory() as tmp:
            out = pathlib.Path(tmp) / 'forwarded'
            self.run_file(fixture(), '-f', str(out), 'MSGFORMAT', 'BINARY_NMEA')
            self.assertEqual([r.get('quality', 0) for r in self.decode(out.read_bytes())], [0, 1024, 2048, 512, 1536, 0])

    def test_shared_quality_layout(self):
        # Recalculate stale length flags on valid NMEA, preserving the
        # checksum flag and all six reception-quality marks without shifting.
        row = {'class': 'AIS', 'device': 'AIS-catcher',
               'quality': 0x0E3F, 'nmea': [sentence(BASE).strip()]}
        source = (json.dumps(row) + '\n').encode()
        self.assertEqual(self.decode(source)[0]['quality'], 0x023F)
        with tempfile.TemporaryDirectory() as tmp:
            out = pathlib.Path(tmp) / 'forwarded'
            self.run_file(source, '-f', str(out), 'MSGFORMAT', 'BINARY_NMEA')
            self.assertEqual(self.decode(out.read_bytes())[0]['quality'], 0x023F)

    def test_invalid_mmsi(self):
        self.assertEqual(self.decode(sentence(payload(0)).encode()), [])

    def test_multipart(self):
        # Type 5: 71 characters with two fill bits. Include a first-fragment
        # checksum failure and a shortened second fragment in the same message.
        p = '5' + payload(244000010)[1:] + '0' * 43
        good = sentence(p[:56], total=2, part=1) + sentence(p[56:], fill=2, total=2, part=2)
        bad = sentence(p[:56], bad=True, total=2, part=1) + sentence(p[56:-1], fill=2, total=2, part=2)
        rows = self.decode((bad + good).encode())
        self.assertEqual([r.get('quality', 0) for r in rows], [1536, 0])

    def test_167_and_variable_length(self):
        self.assertEqual(self.decode(sentence(BASE, fill=1).encode())[0]['quality'], 1024)
        # Type 8 binary broadcast is variable length and may validly be 120 bits.
        self.assertNotIn('quality', self.decode(sentence('8' + BASE[1:20]).encode())[0])

    def test_validation_bounds(self):
        # Type 8 needs at least 56 bits, even with an identifiable sender.
        short = sentence('8' + BASE[1:8]).encode()
        self.assertEqual(self.decode(short), [])
        # An unreadable sender, invalid type, or excessive length is rejected.
        for p in [BASE[:6], '0' + BASE[1:], 'M' + BASE[1:], '8' + BASE[1:] + '0' * 200]:
            with self.subTest(payload=p):
                self.assertEqual(self.decode(sentence(p).encode()), [])

    def test_map_and_counters(self):
        for policy, only_errors, enabled, expected in [(None, False, False, 6), ('all', True, False, 6), (None, False, True, 3), ('all', False, True, 2), ('none', False, True, 6), ('checksum', False, True, 4), ('undersized,checksum', False, True, 3), ('none', True, True, 4), ('oversized', True, True, 3), (None, True, True, 1), ('all', True, True, 0)]:
            with self.subTest(policy=policy, only_errors=only_errors, enabled=enabled), tempfile.TemporaryFile() as log:
                with socket.socket() as sock:
                    sock.bind(('127.0.0.1', 0))
                    port = sock.getsockname()[1]
                args = [BINARY, '-r', 'txt', '.', '-X', 'off', '-o', '0', '-N', str(port)]
                args += ['FILTER', 'on' if enabled else 'off', 'ONLY_ERRORS', 'on' if only_errors else 'off']
                if policy is not None:
                    args += ['EXCLUDE_ERRORS', policy]
                proc = subprocess.Popen(args, stdin=subprocess.PIPE, stdout=log, stderr=log)
                def get(path):
                    with urllib.request.urlopen(f'http://127.0.0.1:{port}/api/{path}.json', timeout=1) as response:
                        return json.load(response)
                try:
                    deadline = time.monotonic() + 8
                    while True:
                        try:
                            get('stat')
                            break
                        except (OSError, ValueError):
                            if time.monotonic() > deadline:
                                log.seek(0)
                                self.fail(log.read().decode())
                            time.sleep(.05)
                    proc.stdin.write(fixture())
                    proc.stdin.flush()
                    while True:
                        stats = get('stat')
                        if stats['session']['count'] == (4 if enabled and only_errors else 6):
                            break
                        if time.monotonic() > deadline:
                            self.fail(str(stats))
                        time.sleep(.05)
                    self.assertEqual(stats['session']['errors'], {'flagged': 4, 'undersized': 2, 'oversized': 1, 'checksum': 2})
                    self.assertEqual(stats['vessel_count'], expected)
                    ships = get('ships')
                    self.assertEqual(len(ships['ships']), expected)
                finally:
                    proc.terminate()
                    proc.communicate(timeout=5)

    def test_output_filter(self):
        for enabled, policy, only, expected in [(False, 'all', True, 6), (True, None, False, 3), (True, 'all', False, 2), (True, 'none', True, 4)]:
            for fmt in ('NMEA', 'JSON_FULL'):
                with self.subTest(enabled=enabled, policy=policy, only=only, fmt=fmt), tempfile.TemporaryDirectory() as tmp:
                    out = pathlib.Path(tmp) / 'filtered'
                    args = ['-f', str(out), 'MSGFORMAT', fmt, 'FILTER', 'on' if enabled else 'off', 'ONLY_ERRORS', 'on' if only else 'off']
                    if policy is not None:
                        args += ['EXCLUDE_ERRORS', policy]
                    result = self.run_file(fixture(), *args)
                    self.assertEqual(len(out.read_bytes().splitlines()), expected)
                    # Selecting one output must not filter another output.
                    self.assertEqual(len([l for l in result.stdout.splitlines() if l.startswith(b'{')]), 6)

    def test_unknown_setting(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = pathlib.Path(tmp) / 'input'
            path.write_bytes(fixture())
            result = subprocess.run([BINARY, '-r', 'txt', str(path), '-X', 'off', '-N', '18999', 'EXCLUDE_ERRORS', 'typo'], capture_output=True, timeout=10)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn(b'Invalid EXCLUDE_ERRORS', result.stderr)


if __name__ == '__main__':
    unittest.main()
