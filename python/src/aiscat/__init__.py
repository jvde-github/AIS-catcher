"""aiscat — fast Python bindings for AIS-catcher's NMEA-to-JSON AIS decoder."""

from ._core import Decoder
from .types import AISMessage

__version__ = "0.68.6"
__all__ = ["Decoder", "iter_decode", "AISMessage"]


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
