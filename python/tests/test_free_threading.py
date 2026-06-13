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
    test_import_does_not_enable_gil()
    test_independent_decoders_from_multiple_threads()
    test_multifragment_decoders_from_multiple_threads()
    test_json_decoders_from_multiple_threads()
    test_nmea_tag_decoders_from_multiple_threads()
    print("OK")
