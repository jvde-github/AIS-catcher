import aiscat


def test_decode_type1():
    dec = aiscat.Decoder()
    n = dec.feed(b"!AIVDM,1,1,,A,15MgK45P3@G?fl0E`JbR0OwT0@MS,0*4E\r\n")
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
    dec.feed(
        b"!AIVDM,1,1,,A,15MgK45P3@G?fl0E`JbR0OwT0@MS,0*4E\r\n"
        b"!AIVDM,1,1,,B,177KQJ5000G?tO`K>RA1wUbN0TKH,0*5C\r\n"
    )
    msgs = []
    m = dec.next()
    while m is not None:
        msgs.append(m)
        m = dec.next()
    assert len(msgs) == 2, f"expected 2 messages, got {len(msgs)}"
    print(f"decoded {len(msgs)} messages")
    for m in msgs:
        print(" ", m)


if __name__ == "__main__":
    test_decode_type1()
    test_decode_two_sentences()
    print("OK")
