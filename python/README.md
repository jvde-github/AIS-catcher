# aiscat

A fast, complete Python library for decoding AIS NMEA into structured dicts; messages come out compatible with the [AIS-catcher JSON format](https://jvde-github.github.io/AIS-catcher-docs/references/JSON-decoding/).

## Why

aiscat is built for fast, complete decoding of AIS NMEA from Python. Output dicts match the [AIS-catcher JSON format](https://jvde-github.github.io/AIS-catcher-docs/references/JSON-decoding/) field-for-field, so anything you've built around AIS-catcher's documented JSON output works the same when the source becomes a Python iterator.

Coverage is meant to be exhaustive: all standard message types (1–28), multi-part AIVDM reassembly, country-code resolution from MMSI prefix, lookup-table text fields (`status_text`, `shiptype_text`, …) populated from the same tables AIS-catcher uses, and **decoding of binary application messages** — type 6 and 8 ASMs across IMO, IALA, USA, and inland-waterway DAC/FID payloads (AtoN monitoring, meteo/hydro, route info, persons-on-board, and others).

`aiscat` ingests raw NMEA AIVDM/AIVDO (with or without IEC 61162-450 tag blocks), AIS-catcher's JSON envelopes, and the binary packet format emitted by dAISy-catcher and AIS-catcher — all auto-detected from the byte stream. For live decode of a single station (~50 msg/s) any decent AIS library is fast enough; aiscat earns its keep when throughput matters: historical replay, multi-receiver aggregation, batch analysis, and research workloads on millions to billions of messages.

## Quickstart

```bash
pip install aiscat
```

```python
import aiscat

dec = aiscat.Decoder()
dec.feed(b"!AIVDM,1,1,,A,15MgK45P3@G?fl0E`JbR0OwT0@MS,0*4E\r\n")
print(dec.next())
# {'type': 1, 'mmsi': 366730000, 'channel': 'A', 'rxuxtime': 1777739134.027,
#  'lat': 37.803802, 'lon': -122.392532, 'speed': 20.8, 'course': 51.3,
#  'status': 5, 'status_text': 'Moored', ...}
```

## Benchmark

2,000,000 AIS messages (mixed types 1–4), Apple M-series, single thread:

| Tool | Time | msg/s | µs/msg | vs fastest |
|---|---:|---:|---:|---:|
| AIS-catcher CLI (`-r txt -o 5`) | 0.94s | 2,139,000 | 0.47 | 1.00× |
| **aiscat** (this library) | **1.06s** | **1,890,000** | **0.53** | **1.13×** |
| `gpsdecode` (gpsd, BSD-2) — CLI | 3.15s | 635,000 | 1.58 | 3.37× |
| [libais 0.17](https://pypi.org/project/libais/) (C, Apache 2.0) | 5.29s | 378,000 | 2.65 | 5.66× |
| [pyais 3.0.0](https://pypi.org/project/pyais/) (pure Python, MIT) | 22.13s | 90,000 | 11.06 | 23.67× |

aiscat tracks the native AIS-catcher CLI within ~13% despite producing rich Python `dict` objects (the CLI just stringifies JSON to stdout). It's **3× faster than `gpsdecode`**, **5× faster than libais**, and **21× faster than pyais**.

For perspective: a busy AIS shore station produces ~50 msg/s. Throughput matters when you're replaying recordings or aggregating dozens of receivers, not for live decode of a single antenna.

## API

```python
class Decoder:
    def __init__(
        self,
        *,
        format: str = "dictionary",
        country: bool = False,
        stamp: bool = False,
    ) -> None: ...
    def feed(self, data: bytes | bytearray | str) -> int: ...
    def next(self) -> dict | bytes | None: ...
    def pending(self) -> int: ...

def iter_decode(
    chunks: Iterable[bytes | str],
    *,
    format: str = "dictionary",
    country: bool = False,
    stamp: bool = False,
) -> Iterator[dict | bytes]: ...
```

### Decoder options

| Option | Default | Effect |
|---|---|---|
| `format` | `"dictionary"` | Output shape (see table below). |
| `country` | `False` | Adds `country` and `country_code` to every message, derived from the MMSI prefix (ITU-R M.585 MID table). Only meaningful for the `dictionary` and `annotated` formats. |
| `stamp` | `False` | If `True`, always sets `rxuxtime` to the current time. The default honors NMEA tag-block `c:<unix>` timestamps and JSON-input `rxuxtime`/`toa` fields when present, falling back to the current time if neither is provided. |

### Output formats

`format=` selects what `next()` returns. Dict-shaped formats are convenient for in-Python consumption; bytes-shaped formats skip the PyDict allocation and are several times faster — useful when you're forwarding the data to a file, socket, database, or another AIS-catcher instance.

| Format | Returns | Throughput¹ | Equivalent to | Notes |
|---|---|---:|---|---|
| `"dictionary"` *(default)* | `dict` | 1.83M msg/s | AIS-catcher `-o 5` (parsed) | Full decoded fields. |
| `"annotated"` | `dict` | 0.44M msg/s | AIS-catcher `-o 6` (parsed) | Each scalar wrapped as `{value, unit, description, text}`. See [Annotated mode](#annotated-mode). |
| `"json"` | `bytes` | 2.08M msg/s | AIS-catcher `-o 5` | Full decoded JSON, ready to write/send. |
| `"json_nmea"` | `bytes` | 4.68M msg/s | AIS-catcher `-o 3` | Slim JSON envelope wrapping the original NMEA — the relay/passthrough format. |
| `"nmea"` | `bytes` | 6.29M msg/s | AIS-catcher `-n` / `-o 1` | The raw AIVDM/AIVDO line(s). |
| `"nmea_tag"` | `bytes` | 5.02M msg/s | — | NMEA prefixed with an IEC 61162-450 tag block carrying source + timestamp. |
| `"binary"` | `bytes` | 6.09M msg/s | — | AIS-catcher's native 0xac binary packet format (compact, suitable for inter-process transport between AIS-catcher / aiscat instances). |

¹ Apple M-series, single thread, 2M mixed type 1–4 messages.

### Methods

| Method | Returns | Description |
|---|---|---|
| `feed(data)` | `int` | Parses NMEA AIVDM/AIVDO sentences, AIS-catcher JSON envelopes, or 0xac binary packets out of the buffer (auto-detected). Multipart messages are reassembled internally; partial chunks are buffered until completed. Returns the number of decoded messages now waiting. |
| `next()` | `dict \| bytes \| None` | Pops one decoded message — `dict` for `dictionary`/`annotated` modes, `bytes` for the others, `None` if the queue is empty. Drain in a loop after each `feed()`. |
| `pending()` | `int` | Number of decoded messages currently in the queue. |

The queue is unbounded — drain it after each feed, or memory grows.

```python
# Manual streaming pattern (for total control)
import aiscat
dec = aiscat.Decoder()
with open("session.nmea", "rb") as f:
    while chunk := f.read(65536):
        dec.feed(chunk)
        while (msg := dec.next()) is not None:
            handle(msg)
```

## Streams

For the common cases (file / stdin / TCP / UDP), aiscat ships generator helpers that wire up the I/O loop, drain the queue, and yield decoded messages. They take the same `format` / `country` / `stamp` kwargs as `Decoder`.

```python
import aiscat

# File on disk (or any open binary handle: stdin, gzip, ssl-wrapped socket, …)
for msg in aiscat.from_file("session.nmea"):
    print(msg["mmsi"], msg["lat"], msg["lon"])

# stdin
for msg in aiscat.from_stdin(format="json"):
    sys.stdout.buffer.write(msg)

# TCP — connect to a NMEA-over-TCP feed
for msg in aiscat.from_tcp("ais.example.com", 4001):
    print(msg["mmsi"])

# UDP — listen for inbound NMEA datagrams
for msg in aiscat.from_udp(port=4001, format="json_nmea"):
    queue.publish(msg)
```

### Reconnect, retry, errors

Reconnect logic is intentionally not built in — too many policy choices (backoff, max retries, jitter). Compose:

```python
import time
while True:
    try:
        for msg in aiscat.from_tcp(host, port):
            handle(msg)
    except (ConnectionError, OSError):
        time.sleep(5)
```

For `from_udp`, the generator runs until you break out of the loop or the program exits.

## Format examples

The same NMEA sentence — `!AIVDM,1,1,,B,13`e;R001`PNcD2MH48KQq>P0@;5,0*63` — through each format. Lookup-table fields (`status_text`, `shiptype_text`, `aid_type_text`, etc.) are populated from the same tables AIS-catcher's CLI uses. Full field reference: [AIS-catcher JSON decoding docs](https://jvde-github.github.io/AIS-catcher-docs/references/JSON-decoding/).

### `format="dictionary"` (default) — Python `dict`

```python
{'rxuxtime': 1777908449.348, 'channel': 'B', 'type': 1, 'repeat': 0,
 'mmsi': 244009864, 'status': 0, 'status_text': 'Under way using engine',
 'turn_unscaled': 0, 'turn': 0, 'speed': 10.4, 'accuracy': True,
 'lon': 6.701442, 'lat': 51.338295, 'course': 295.1, 'heading': 295,
 'second': 16, 'maneuver': 0, 'power': False, 'raim': False,
 'radio': 66245, 'sync_state': 0, 'slot_timeout': 4, 'slot_number': 709}
```

AIS-catcher's internal meta fields (SDR signal levels, decoder version, the original NMEA echo, etc.) are suppressed since they don't apply to a Python decoder.

### `format="annotated"` — Python `dict` with units, descriptions, lookup text

Each scalar becomes `{value, unit?, description?, text?}` (the optional fields included only when defined for that key):

```python
{'type':   {'value': 1,    'description': 'Message Type', 'text': 'Position report'},
 'status': {'value': 0,    'description': 'Navigation Status', 'text': 'Under way using engine'},
 'speed':  {'value': 10.4, 'unit': 'knots', 'description': 'Speed over Ground (SOG)'},
 'lon':    {'value': 6.701442, 'unit': 'degrees', 'description': 'Longitude'},
 ...}
```

### `format="json"` — full JSON, ready to write/send

```text
b'{"class":"AIS","device":"AIS-catcher","version":68,...,"channel":"B",
   "nmea":["!AIVDM,1,1,,B,13`e;R001`PNcD2MH48KQq>P0@;5,0*63"],...,
   "type":1,"repeat":0,"mmsi":244009864,"status":0,
   "status_text":"Under way using engine",...,"speed":10.400001,...,
   "lon":6.701442,"lat":51.338295,...}'
```

### `format="json_nmea"` — slim JSON envelope wrapping the NMEA

The "passthrough" / relay format — small envelope, the rest is the original NMEA you can hand to another decoder:

```text
b'{"class":"AIS","device":"AIS-catcher","version":68,"channel":"B",
   "repeat":0,"rxuxtime":1777908449.351,"mmsi":244009864,"type":1,
   "nmea":["!AIVDM,1,1,,B,13`e;R001`PNcD2MH48KQq>P0@;5,0*63"]}'
```

### `format="nmea"` — bare AIVDM line(s)

```text
b'!AIVDM,1,1,,B,13`e;R001`PNcD2MH48KQq>P0@;5,0*63\n'
```

For multipart messages, all reassembled fragments are concatenated (each terminated with `\n`).

### `format="nmea_tag"` — NMEA prefixed with an IEC 61162-450 tag block

```text
b'\\s:s0,c:1777908449.352*53\\!AIVDM,1,1,,B,13`e;R001`PNcD2MH48KQq>P0@;5,0*63\r\n'
```

### `format="binary"` — AIS-catcher native 0xac packet

37-byte compact binary frame, suitable for inter-process transport between AIS-catcher / aiscat instances:

```text
b'\xac\x00\x00\x00\x06\x50\xff\x91\x91\x18\x11\x42\x00\xa8\x04\x3a\x2d\x2e\x20\x00\x06\x88\x1e\xad\xad\x40\x9d\x60\x42\x1b\x87\x93\xa0\x01\x02\xc5\x0a'
```

## Pretty-printed output

See [`examples/pretty_print.py`](examples/pretty_print.py) for a complete consumer that uses `format="annotated"` to render a readable table:

```text
$ python examples/pretty_print.py '!AIVDM,1,1,,B,13`e;R001`PNcD2MH48KQq>P0@;5,0*63'
Message 1 — Position report
!AIVDM,1,1,,B,13`e;R001`PNcD2MH48KQq>P0@;5,0*63

Field          Value                                     Unit                Description
-------------  ----------------------------------------  ------------------  -------------------------------------------------------------------------
rxuxtime       1777805849.119 (2026-05-03 10:57:29 UTC)                      Host receive time (Unix epoch s).
channel        B                                                             VHF channel (A or B).
type           1 (Position report)                                           Message Type
repeat         0                                                             Repeat indicator (0..3; 3=do not repeat).
mmsi           244009864                                                     MMSI
country        Netherlands                                                   Flag country name derived from MMSI MID.
country_code   NL                                                            ISO-3166 alpha-2 country code derived from MMSI MID.
status         0 (Under way using engine)                                    Navigation Status
status_text    Under way using engine                                        Navigation status text.
turn_unscaled  0                                                             Raw ROT field (-128..127; 128=N/A).
turn           0                                         degrees per minute  Rate of Turn (ROT)
speed          10.400001                                 knots               Speed over Ground (SOG)
accuracy       true                                                          Position accuracy (1=DGPS <10m; 0=GNSS >10m).
lon            6.701442                                  degrees             Longitude
lat            51.338295                                 degrees             Latitude
course         295.100006                                degrees             Course over Ground (COG)
heading        295                                       degrees             True Heading (HDG)
second         16                                                            UTC second (0..59; 60=N/A; 61=manual; 62=dead reckoning; 63=inoperative).
maneuver       0 (Not available (default))                                   Maneuver indicator.
power          false                                                         Transmit power flag (type 22; 0=high power, 1=low power).
raim           false                                                         RAIM flag (EPFD integrity monitoring in use).
radio          66245                                                         Radio status (19-bit SOTDMA/ITDMA state).
sync_state     0 (UTC direct)                                                TDMA sync state.
slot_timeout   4                                                             Frames until new slot (0=next).
slot_number    709                                                           TDMA slot number used.
```

## Type hints

Decoded messages are plain dicts at runtime — zero overhead. For static type-checking and IDE autocomplete, `aiscat.types` provides one `TypedDict` per AIS message type (1–28) plus an `AISMessage` union:

```python
from aiscat import Decoder, AISMessage

def handle(msg: AISMessage) -> None:
    if msg["type"] == 1:
        # mypy narrows to Type1; knows msg["lat"] is float
        print(msg["mmsi"], msg["lat"], msg["lon"])
```

## Licence

**GPLv3**, inherited from AIS-catcher. Linking aiscat into a closed-source application makes that application GPLv3.

## Building from source

Requires CMake ≥ 3.15, a C++11 compiler, and Python ≥ 3.9 with development headers.

```bash
git clone https://github.com/jvde-github/AIS-catcher
cd AIS-catcher/python
pip install -e .
```

The build pulls a curated subset of AIS-catcher's source (`Marine/`, `JSON/`, `Library/`, `Utilities/`) — about 8K lines of C++ — and compiles them together with the binding into `_core.cpython-*.so`. Build takes ~10 seconds.

## Status

 The decoder itself is mature — it's the AIS-catcher decoder, battle-tested in shore-station and SDR deployments. The Python surface is new and may evolve — feedback welcome.

Versioning tracks AIS-catcher's: `0.68.x` uses AIS-catcher v0.68's decoder. Patch releases bump independently for binding-only changes.
