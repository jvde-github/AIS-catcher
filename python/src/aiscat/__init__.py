"""aiscat — fast Python bindings for AIS-catcher's NMEA-to-JSON AIS decoder."""

from ._core import Decoder
from .types import AISMessage

__version__ = "0.68.0"
__all__ = ["Decoder", "iter_decode", "AISMessage"]


def iter_decode(chunks):
    """Yield decoded AIS messages (as dicts) from an iterable of bytes/str chunks."""
    dec = Decoder()
    for chunk in chunks:
        dec.feed(chunk)
        msg = dec.next()
        while msg is not None:
            yield msg
            msg = dec.next()
