"""aiscat — fast Python bindings for AIS-catcher's NMEA-to-JSON AIS decoder."""

from ._core import Decoder
from .types import AISMessage

__version__ = "0.68.2"
__all__ = ["Decoder", "iter_decode", "AISMessage"]


def iter_decode(chunks, *, annotated=False):
    """Yield decoded AIS messages (as dicts) from an iterable of bytes/str chunks.

    If ``annotated`` is True, every scalar value is wrapped as
    ``{"value": x, "unit": ..., "description": ..., "text": ...}`` (the latter
    three fields included only when defined for that key).
    """
    dec = Decoder(annotated=annotated)
    for chunk in chunks:
        dec.feed(chunk)
        msg = dec.next()
        while msg is not None:
            yield msg
            msg = dec.next()
