import concurrent.futures
import json
import os
import subprocess
import sys
import sysconfig
import textwrap

import aiscat


SAMPLE_A = b"!AIVDM,1,1,,A,15MgK45P3@G?fl0E`JbR0OwT0@MS,0*4E\r\n"
SAMPLE_B = b"!AIVDM,1,1,,B,177KQJ5000G?tO`K>RA1wUbN0TKH,0*5C\r\n"
SAMPLE_TYPE5 = (
    b"!AIVDM,2,1,4,A,55O0W7`00001L@gCWGA2uItLth@DqtL5@F22220j1h742t0Ht0000000,0*08\r\n"
    b"!AIVDM,2,2,4,A,000000000000000,2*20\r\n"
)
SAMPLE_TYPE26_MAX = (
    b"!AIVDM,4,1,7,A,J1mg=5AEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE,0*69\r\n"
    b"!AIVDM,4,2,7,A,EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE,0*17\r\n"
    b"!AIVDM,4,3,7,A,EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE,0*16\r\n"
    b"!AIVDM,4,4,7,A,EEEEE@4SA@,4*76\r\n"
)
# The same 1068 armored bits with zero fill bits declared; checksum adjusted.
SAMPLE_TYPE26_TOO_LONG = SAMPLE_TYPE26_MAX.replace(b",4*76\r\n", b",0*72\r\n")


def unescape_binary_packet(packet):
    decoded = bytearray()
    index = 0
    while index < len(packet):
        value = packet[index]
        if value == 0xAD:
            index += 1
            value = {0xAD: 0xAD, 0xAE: 0x0A, 0xAF: 0x0D}[packet[index]]
        decoded.append(value)
        index += 1
    return bytes(decoded)


def test_decode_type1():
    dec = aiscat.Decoder()
    n = dec.feed(SAMPLE_A)
    assert n >= 1, f"expected >= 1 pending, got {n}"
    msg = dec.next()
    assert msg is not None
    assert msg.get("type") == 1
    assert msg.get("mmsi") == 366730000
    assert abs(msg["lat"] - 37.803802) < 1e-3
    assert abs(msg["lon"] - (-122.392532)) < 1e-3
    # Meta envelope should be stripped
    for stripped in ("class", "device", "version", "scaled", "signalpower", "ppm", "nmea"):
        assert stripped not in msg, f"expected {stripped!r} to be stripped"
    print(msg)


def test_decode_two_sentences():
    dec = aiscat.Decoder()
    dec.feed(SAMPLE_A + SAMPLE_B)
    msgs = []
    m = dec.next()
    while m is not None:
        msgs.append(m)
        m = dec.next()
    assert len(msgs) == 2, f"expected 2 messages, got {len(msgs)}"
    print(f"decoded {len(msgs)} messages")
    for m in msgs:
        print(" ", m)


def test_decode_max_length_type26():
    dec = aiscat.Decoder()
    assert dec.feed(SAMPLE_TYPE26_MAX) == 1
    msg = dec.next()
    assert msg is not None
    assert msg["type"] == 26
    assert msg["mmsi"] == 123456789
    assert msg["radio"] == 0x12345

    binary_dec = aiscat.Decoder(format="binary")
    assert binary_dec.feed(SAMPLE_TYPE26_MAX) == 1
    encoded_packet = binary_dec.next()
    packet = unescape_binary_packet(encoded_packet)
    assert packet[:2] == b"\xac\x00"
    assert int.from_bytes(packet[12:14], "big") == 1064

    binary_roundtrip = aiscat.Decoder()
    assert binary_roundtrip.feed(encoded_packet) == 1
    roundtrip_msg = binary_roundtrip.next()
    assert roundtrip_msg["type"] == 26
    assert roundtrip_msg["radio"] == 0x12345

    oversized_dec = aiscat.Decoder()
    assert oversized_dec.feed(SAMPLE_TYPE26_TOO_LONG) == 0
    assert oversized_dec.next() is None


# Free-threading: independent Decoder instances must be usable from different
# threads. The Py_GIL_DISABLED-specific assertions self-gate, so these run
# (and trivially pass) on GIL builds too.

def test_import_does_not_enable_gil():
    if not sysconfig.get_config_var("Py_GIL_DISABLED"):
        return

    code = """
import sys
import aiscat
assert not sys._is_gil_enabled()
"""
    env = os.environ.copy()
    env.pop("PYTHON_GIL", None)
    subprocess.run([sys.executable, "-c", textwrap.dedent(code)], check=True, env=env)


def test_independent_decoders_from_multiple_threads():
    if sysconfig.get_config_var("Py_GIL_DISABLED"):
        assert not sys._is_gil_enabled()

    def worker(iterations):
        dec = aiscat.Decoder()
        count = 0
        for i in range(iterations):
            dec.feed(SAMPLE_A if i % 2 == 0 else SAMPLE_B)
            while True:
                msg = dec.next()
                if msg is None:
                    break
                assert msg["type"] == 1
                count += 1
        return count

    iterations = 1000
    workers = 4
    with concurrent.futures.ThreadPoolExecutor(max_workers=workers) as executor:
        counts = list(executor.map(worker, [iterations] * workers))

    assert counts == [iterations] * workers


def test_multifragment_decoders_from_multiple_threads():
    def worker(iterations):
        dec = aiscat.Decoder()
        count = 0
        for _ in range(iterations):
            dec.feed(SAMPLE_TYPE5)
            msg = dec.next()
            assert msg is not None
            assert msg["type"] == 5
            assert dec.next() is None
            count += 1
        return count

    iterations = 250
    workers = 4
    with concurrent.futures.ThreadPoolExecutor(max_workers=workers) as executor:
        counts = list(executor.map(worker, [iterations] * workers))

    assert counts == [iterations] * workers


def test_json_decoders_from_multiple_threads():
    def worker(iterations):
        dec = aiscat.Decoder(format="json")
        count = 0
        for _ in range(iterations):
            dec.feed(SAMPLE_A)
            msg = dec.next()
            assert msg is not None
            assert json.loads(msg)["type"] == 1
            assert dec.next() is None
            count += 1
        return count

    iterations = 250
    workers = 4
    with concurrent.futures.ThreadPoolExecutor(max_workers=workers) as executor:
        counts = list(executor.map(worker, [iterations] * workers))

    assert counts == [iterations] * workers


def test_nmea_tag_decoders_from_multiple_threads():
    def worker(iterations):
        dec = aiscat.Decoder(format="nmea_tag")
        count = 0
        for _ in range(iterations):
            dec.feed(SAMPLE_TYPE5)
            msg = dec.next()
            assert msg is not None
            assert msg.count(b"!AIVDM") == 2
            assert b"g:1-2-" in msg
            assert b"g:2-2-" in msg
            assert dec.next() is None
            count += 1
        return count

    iterations = 250
    workers = 4
    with concurrent.futures.ThreadPoolExecutor(max_workers=workers) as executor:
        counts = list(executor.map(worker, [iterations] * workers))

    assert counts == [iterations] * workers


if __name__ == "__main__":
    test_decode_type1()
    test_decode_two_sentences()
    test_decode_max_length_type26()
    test_import_does_not_enable_gil()
    test_independent_decoders_from_multiple_threads()
    test_multifragment_decoders_from_multiple_threads()
    test_json_decoders_from_multiple_threads()
    test_nmea_tag_decoders_from_multiple_threads()
    print("OK")
