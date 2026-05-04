"""aiscat — fast Python bindings for AIS-catcher's NMEA-to-JSON AIS decoder."""

import os
import socket
import sys

from ._core import Decoder
from .types import AISMessage

__version__ = "0.68.7"
__all__ = [
    "Decoder",
    "iter_decode",
    "from_file",
    "from_stdin",
    "from_tcp",
    "from_udp",
    "AISMessage",
]


def iter_decode(chunks, *, format="dictionary", country=False, stamp=False):
    """Yield decoded AIS messages from an iterable of bytes/str chunks.

    ``format`` selects the output shape:

    - ``"dictionary"`` (default) — yield Python ``dict`` with full decoded fields
    - ``"annotated"`` — Python ``dict`` with each scalar wrapped as ``{value, unit, description, text}``
    - ``"json"`` — ``bytes``, full JSON serialization (AIS-catcher ``-o 5``)
    - ``"json_nmea"`` — ``bytes``, JSON envelope wrapping the NMEA (``-o 3``)
    - ``"nmea"`` — ``bytes``, the bare AIVDM/AIVDO line(s)
    - ``"nmea_tag"`` — ``bytes``, NMEA prefixed with an IEC 61162-450 tag block
    - ``"binary"`` — ``bytes``, AIS-catcher 0xac binary packet

    See ``Decoder`` for ``country`` and ``stamp``.
    """
    dec = Decoder(format=format, country=country, stamp=stamp)
    for chunk in chunks:
        dec.feed(chunk)
        msg = dec.next()
        while msg is not None:
            yield msg
            msg = dec.next()


def from_file(path, *, format="dictionary", country=False, stamp=False, chunk=65536):
    """Yield decoded AIS messages from a file path or open binary stream.

    ``path`` may be a filesystem path or an already-open binary file (e.g.
    ``sys.stdin.buffer``, a ``gzip.open`` handle, an ``ssl`` socket file).
    """
    own = isinstance(path, (str, os.PathLike))
    f = open(path, "rb") if own else path
    dec = Decoder(format=format, country=country, stamp=stamp)
    try:
        while True:
            data = f.read(chunk)
            if not data:
                break
            dec.feed(data)
            msg = dec.next()
            while msg is not None:
                yield msg
                msg = dec.next()
    finally:
        if own:
            f.close()


def from_stdin(**kwargs):
    """Yield decoded AIS messages from stdin. Accepts the same kwargs as ``from_file``."""
    return from_file(sys.stdin.buffer, **kwargs)


def from_tcp(host, port, *, format="dictionary", country=False, stamp=False, timeout=None):
    """Connect to a TCP source and yield decoded messages until the peer closes.

    Reconnect-on-disconnect is intentionally not built in — wrap the call in
    your own retry loop if you need it.
    """
    dec = Decoder(format=format, country=country, stamp=stamp)
    with socket.create_connection((host, port), timeout=timeout) as sock:
        while True:
            data = sock.recv(65536)
            if not data:
                break
            dec.feed(data)
            msg = dec.next()
            while msg is not None:
                yield msg
                msg = dec.next()


def from_udp(host="0.0.0.0", port=4001, *, format="dictionary", country=False, stamp=False):
    """Listen on a UDP port and yield decoded messages.

    Runs until the generator is closed (e.g. by ``break``-ing out of the for-loop,
    Ctrl-C, or ``.close()`` on the generator).
    """
    dec = Decoder(format=format, country=country, stamp=stamp)
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind((host, port))
    try:
        while True:
            data, _ = sock.recvfrom(65536)
            dec.feed(data)
            msg = dec.next()
            while msg is not None:
                yield msg
                msg = dec.next()
    finally:
        sock.close()
