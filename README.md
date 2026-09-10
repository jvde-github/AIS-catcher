# AIS-catcher: A multi-platform AIS Receiver

This repository presents the `AIS-catcher` software, a versatile dual-channel AIS receiver that is compatible with a wide range of Software Defined Radios (SDRs). These include RTL-SDR dongles (such as the ShipXplorer AIS dongle and RTL SDR Blog v4), AirSpy (Mini/R2/HF+), HackRF, HydraSDR, SDRPlay, SoapySDR, and file/network input (ZMQ/RTL-TCP/SpyServer). AIS-catcher delivers output in the form of NMEA messages, which can be conveniently displayed on screen or forwarded via UDP/HTTP/TCP. Designed as a lightweight command line utility, AIS-catcher also incorporates a built-in web server for internal use within secure networks. The project home page including several realtime examples can be found at [www.aiscatcher.org](https://www.aiscatcher.org).

<img width="3804" height="1819" alt="image" src="https://github.com/user-attachments/assets/c13b7364-6abe-4df1-a983-ecf4f847f0b3" />

## Purpose

The purpose of `AIS-catcher` is to serve as a platform that encourages the perpetual enhancement of receiver models. We greatly value and appreciate any suggestions, observations, or shared recordings, particularly from setups where the existing models encounter difficulties.

## License

Copyright (C) 2021 - 2026 jvde.github at gmail.com. All rights reserved. Licensed under GNU General Public License v3.0.

## Important Disclaimer
`AIS-catcher` is created for research and educational purposes under the GNU GPL v3 license. It is a hobby project and has not been tested and designed for reliability and correctness. 
You can play with the software but it is the user's responsibility to use it prudently. So, DO NOT rely upon this software in any way including for navigation 
and/or safety of life or property purposes.
There are variations in the legislation concerning radio reception in the different administrations around the world. 
It is your responsibility to determine whether or not your local administration permits the reception and handling of AIS messages from ships. 
It is specifically forbidden to use this software for any illegal purpose whatsoever. 
Only use this software in regions where such use is permitted.

## Plug-and-play solution

Prefer a traditional AIS receiver over an SDR but want the same advantages? We recommend the **dAISy-catcher**, a dual-channel receiver developed with [Wegmatt](https://shop.wegmatt.com/products/daisy-catcher-high-performance-ais-receiver) that combines the benefits of SDR receivers with a dedicated hardware solution: plug-and-play operation, low power consumption, and precise signal processing. It connects directly to AIS-catcher over serial, as a USB device or Raspberry Pi HAT. See the [product page](https://shop.wegmatt.com/products/daisy-catcher-high-performance-ais-receiver) and [manual (PDF)](https://wegmatt.com/files/dAISy-catcher%20AIS%20Receiver%20Manual.pdf).

<img width="2048" height="1365" alt="image" src="https://github.com/user-attachments/assets/d79f51ec-857d-4767-9c28-9cb8ebe4f2e5" />

## Feature overview: Input -> Output

![image](https://github.com/user-attachments/assets/6677b833-bd2c-4338-babe-3817d6a7c3ea)

## The aiscatcher.org community

To join, ensure you're on the latest version, visit [www.aiscatcher.org](https://www.aiscatcher.org), and [add](https://www.aiscatcher.org/addstation) your station. Upon registration, you'll receive a personal sharing key. Simply run AIS-catcher on the command line with "-X" followed by your sharing key to share your station's raw AIS data with the community hub. This activates a "Community Feed" in your station's web viewer, accessible under map layers and some other features.


## Places

Ports, berths, anchorages and custom areas live in an editable place catalogue:
a directory of GeoJSON files. Point the standalone viewer at one with
`-N 8100 PLACES /path/to/places`; a managed installation keeps a `places`
directory beside its configuration and edits it from the Places tab. Ships in
a place, visits and port-bound traffic follow from the catalogue. See
[docs/places.md](docs/places.md) for the file format and rules.

## Links

- Documentation: [here](https://jvde-github.github.io/AIS-catcher-docs/)
- Installation: [here](https://jvde-github.github.io/AIS-catcher-docs/installation/overview)
- What is New? [here](https://jvde-github.github.io/AIS-catcher-docs/what-is-new/)
- Python binding (`aiscat`): [PyPI](https://pypi.org/project/aiscat/) · [source & docs](python/)
- Forum: [here](https://github.com/jvde-github/AIS-catcher/discussions)
- Bug Reports: [here](https://github.com/jvde-github/AIS-catcher/issues)


### Flagged messages and filters

The Statistics tab counts flagged messages and each error category, including
messages excluded from vessel updates. Error flags are independent of filtering:

```sh
AIS-catcher -N 8100 FILTER on EXCLUDE_ERRORS undersized,checksum  # exclude undersized and bad-checksum reports
AIS-catcher -N 8100 FILTER on EXCLUDE_ERRORS all                 # also exclude oversized
AIS-catcher -N 8100 FILTER on EXCLUDE_ERRORS none                # include flagged messages
AIS-catcher -N 8100 FILTER on ONLY_ERRORS on EXCLUDE_ERRORS none # only flagged messages
```

These options belong to the shared message filter and work for map, file, network
and database outputs. They take effect only with `FILTER on`; with filtering off,
quality flags alone do not exclude messages. The default is `EXCLUDE_ERRORS none`, so enabling
the filter alone does not exclude flagged messages.

For example, `-f errors.nmea FILTER on ONLY_ERRORS on EXCLUDE_ERRORS none` saves
only flagged messages. The web-server JSON setting is `"exclude_errors": "undersized,checksum"`.
Values are comma-separated, case-insensitive names: `undersized`, `oversized`,
`checksum`, or the standalone values `all` and `none`. Exclusions prevent all
vessel-state and track updates; diagnostic outputs and error counters remain
available. Use `undersized,checksum` to retain oversized reports from feeds that append
nonstandard trailers.

`ONLY_ERRORS` defaults to `off`. With filtering enabled it rejects unflagged
messages; `EXCLUDE_ERRORS` still applies. Map statistics still count messages
rejected by `EXCLUDE_ERRORS`; messages rejected by other filters, including
`ONLY_ERRORS`, are not counted. Other outputs forward only included messages.

Exact-length checks cover fixed-length types 1–5, 9, 11, 18, 19, 23, 27 and 28.
Messages below their per-type minimum are rejected; zero-length messages remain valid.
Unparsable sentences,
unknown types and payloads too short to identify a sender are still rejected.
MMSI zero is now rejected at validation for every output, rather than only by the map.
`CRC_CHECK on` remains an explicit ingress rejection option: messages rejected
there cannot reach viewer statistics. Leave it off to count checksum failures
while excluding them from the map.

JSON `quality` and the binary quality word use identical flags: checksum
`0x0200`, undersized `0x0400`, oversized `0x0800`. Reception-quality marks use
bits 0–5; updating them must preserve the error mask `0x0E00`.
Only `quality` carries diagnostic
flags; the legacy JSON `error` field is no longer read or emitted. Plain NMEA
cannot carry these flags when its checksum is regenerated.
Statistics backups retain the counters (format v4); older v1–v3 statistics can
still be loaded.
