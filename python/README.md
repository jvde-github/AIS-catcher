# aiscat

A fast, complete Python library for decoding AIS NMEA into structured dicts. Built on the [AIS-catcher](https://github.com/jvde-github/AIS-catcher) decoder; messages come out compatible with the [AIS-catcher JSON format](https://jvde-github.github.io/AIS-catcher-docs/references/JSON-decoding/).

## Why

aiscat is built for fast, complete decoding of AIS NMEA from Python. Output dicts match the [AIS-catcher JSON format](https://jvde-github.github.io/AIS-catcher-docs/references/JSON-decoding/) field-for-field, so anything you've built around AIS-catcher's documented JSON output works the same when the source becomes a Python iterator.

Coverage is meant to be exhaustive: all standard message types (1–28), multi-part AIVDM reassembly, country-code resolution from MMSI prefix, lookup-table text fields (`status_text`, `shiptype_text`, …) populated from the same tables AIS-catcher uses, and **decoding of binary application messages** — type 6 and 8 ASMs across IMO, IALA, USA, and inland-waterway DAC/FID payloads (AtoN monitoring, meteo/hydro, route info, persons-on-board, and others).

For live decode of a single station (~50 msg/s) any decent AIS library is fast enough. aiscat earns its keep when throughput matters: replay of historical recordings, multi-receiver fan-in, batch analysis, and research workloads on millions to billions of messages.

**Tradeoffs.** aiscat is a C++ extension, so `pip install` either picks up a pre-built wheel for your platform or falls back to a source build that needs CMake and a C++11 compiler. The API is deliberately small — feed bytes, get dicts — without configuration knobs or custom output formats. And the licence is GPLv3 (inherited from AIS-catcher), so linking aiscat into a closed-source application makes that application GPL too; see the Licence section below for permissive alternatives.

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
| AIS-catcher CLI (`-r txt -o 5`) | 1.04s | 1,922,000 | 0.52 | 1.00× |
| **aiscat** (this library) | **1.05s** | **1,906,000** | **0.52** | **1.01×** |
| `gpsdecode` (gpsd, BSD-2) — CLI | 3.14s | 636,000 | 1.57 | 3.02× |
| [libais 0.17](https://pypi.org/project/libais/) (C, Apache 2.0) | 5.33s | 375,000 | 2.67 | 5.12× |
| [pyais 3.0.0](https://pypi.org/project/pyais/) (pure Python, MIT) | 22.36s | 89,000 | 11.18 | 21.49× |

aiscat sits within 1% of the native AIS-catcher CLI despite producing rich Python `dict` objects (the CLI just stringifies JSON to stdout). It's **3× faster than `gpsdecode`**, **5× faster than libais**, and **21× faster than pyais**.

The full benchmark — including the test-fixture generator — is in [benchmarks/](benchmarks/). Types 1–4 (single-line position reports / base station reports) are used because libais's stream API does not reassemble multi-part AIVDM groups; on a mixed corpus including type 5 (multi-part static data), libais silently drops every type-5 message. AIS-catcher, gpsdecode, and pyais handle multi-part correctly.

For perspective: a busy AIS shore station produces ~50 msg/s. Throughput matters when you're replaying recordings or aggregating dozens of receivers, not for live decode of a single antenna.

## API

```python
class Decoder:
    def __init__(self) -> None: ...
    def feed(self, data: bytes | bytearray | str) -> int: ...
    def next(self) -> dict | None: ...
    def pending(self) -> int: ...

def iter_decode(chunks: Iterable[bytes | str]) -> Iterator[dict]: ...
```

- `feed(data)` parses NMEA AIVDM/AIVDO sentences out of the buffer. Multipart messages are reassembled internally; partial chunks are buffered until completed. Returns the number of decoded messages waiting.
- `next()` pops one decoded message as a `dict`, or `None` if the queue is empty. Pop in a loop after each `feed()`.
- The queue is unbounded — drain it after each feed, or memory grows.

```python
# Streaming pattern
import aiscat
dec = aiscat.Decoder()
with open("session.nmea", "rb") as f:
    while chunk := f.read(65536):
        dec.feed(chunk)
        while (msg := dec.next()) is not None:
            handle(msg)
```

## Output format

Each decoded message is a `dict` containing the AIS protocol fields plus a small reception envelope. AIS-catcher's internal "meta" fields that don't apply to a Python decoder (SDR signal levels, decoder version, the original NMEA echo, etc.) are suppressed.

Lookup-table values are decoded for you — `status` comes through as both the raw integer and `status_text` (`"Moored"`, `"Under way using engine"`, …); same for `shiptype_text`, `aid_type_text`, etc.

For the full field reference — every key, its unit, and the lookup tables — see the [AIS-catcher JSON decoding documentation](https://jvde-github.github.io/AIS-catcher-docs/references/JSON-decoding/).

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

If you need a permissive licence (BSD/MIT/Apache), use one of:

- [pyais](https://pypi.org/project/pyais/) — pure-Python, MIT, the de facto standard for Python AIS work
- [libais](https://pypi.org/project/libais/) — older C extension, Apache 2.0

aiscat exists for users who *want* AIS-catcher's decoding (multipart handling, lookup tables, channel A/B disambiguation, country tables, message-type 6/8 ASMs) and can live with the licence.

## Building from source

Requires CMake ≥ 3.15, a C++11 compiler, and Python ≥ 3.9 with development headers.

```bash
git clone https://github.com/jvde-github/AIS-catcher
cd AIS-catcher/python
pip install -e .
```

The build pulls a curated subset of AIS-catcher's source (`Marine/`, `JSON/`, `Library/`, `Utilities/`) — about 8K lines of C++ — and compiles them together with the binding into `_core.cpython-*.so`. Build takes ~10 seconds.

## Status

Pre-1.0. The decoder itself is mature — it's the AIS-catcher decoder, battle-tested in shore-station and SDR deployments. The Python surface is new and may evolve — feedback welcome.

Versioning tracks AIS-catcher's: `0.68.x` uses AIS-catcher v0.68's decoder. Patch releases bump independently for binding-only changes.
