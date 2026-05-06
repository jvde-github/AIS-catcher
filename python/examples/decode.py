#!/usr/bin/env python3
"""Single-shot decoding via aiscat.decode().

For streaming or batch input, use Decoder / iter_decode / from_file instead.
"""
import aiscat


# Single-part message
msg = aiscat.decode("!AIVDM,1,1,,B,15MgK45P3@G?fl0E`JbR0OwT0@MS,0*4E")
print(msg["type"], msg["mmsi"], msg["lat"], msg["lon"])

# Multipart — pass each fragment as a positional argument
msg = aiscat.decode(
    "!AIVDM,2,1,4,A,55O0W7`00001L@gCWGA2uItLth@DqtL5@F22220j1h742t0Ht0000000,0*08",
    "!AIVDM,2,2,4,A,000000000000000,2*20",
)
print(msg["type"], msg["mmsi"], msg.get("shipname"))

# Bytes input also works
msg = aiscat.decode(b"!AIVDM,1,1,,A,13HOI:0P0000VOHLCnHQKwvL05Ip,0*23")
print(msg)

# Switch the output shape: annotated wraps each scalar with units & descriptions
msg = aiscat.decode(
    "!AIVDM,1,1,,B,15MgK45P3@G?fl0E`JbR0OwT0@MS,0*4E",
    format="annotated",
)
print(msg["lat"])  # {'value': 37.803802, 'unit': 'degrees', 'description': 'Latitude'}

# Country-code resolution from MMSI
msg = aiscat.decode(
    "!AIVDM,1,1,,B,13`e;R001`PNcD2MH48KQq>P0@;5,0*63",
    country=True,
)
print(msg["mmsi"], msg["country"], msg["country_code"])
