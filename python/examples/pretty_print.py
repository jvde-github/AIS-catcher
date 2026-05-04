#!/usr/bin/env python3
"""Decode an NMEA line and print it as a labelled table.

Usage:
    python pretty_print.py '!AIVDM,1,1,,B,13`e;R001`PNcD2MH48KQq>P0@;5,0*63'
    echo '!AIVDM,...' | python pretty_print.py
"""
import datetime as _dt
import sys
import aiscat


def fmt_epoch(v):
    if not isinstance(v, (int, float)):
        return str(v)
    ts = _dt.datetime.fromtimestamp(v, tz=_dt.timezone.utc)
    return f"{v:.3f} ({ts.strftime('%Y-%m-%d %H:%M:%S UTC')})"


# Per-key value formatters override the default fmt_value.
KEY_FORMATTERS = {
    "rxuxtime": fmt_epoch,
}


def fmt_value(v):
    if isinstance(v, bool):
        return "true" if v else "false"
    if isinstance(v, float):
        # 6 decimal places, strip trailing zeros (but keep "0" for integer-valued floats).
        s = f"{v:.6f}".rstrip("0").rstrip(".")
        return s if s else "0"
    if isinstance(v, (list, dict)):
        return repr(v)
    return str(v)


def render(msg, source_lines=()):
    t = msg.get("type", {})
    if isinstance(t, dict):
        type_id = t.get("value", "?")
        title = t.get("text", "Unknown")
    else:
        type_id, title = t, "Unknown"
    print(f"Message {type_id} — {title}")

    for line in source_lines:
        print(line)

    rows = []
    for key, entry in msg.items():
        if not isinstance(entry, dict) or "value" not in entry:
            continue  # skip non-annotated fields (nmea list, raw envelope, etc.)
        formatter = KEY_FORMATTERS.get(key, fmt_value)
        val = formatter(entry["value"])
        if "text" in entry:
            val = f"{val} ({entry['text']})"
        unit = entry.get("unit", "")
        desc = entry.get("description", "")
        rows.append((key, val, unit, desc))

    if not rows:
        return

    headers = ("Field", "Value", "Unit", "Description")
    widths = [max(len(headers[i]), max(len(r[i]) for r in rows)) for i in range(4)]
    fmt = f"{{:<{widths[0]}}}  {{:<{widths[1]}}}  {{:<{widths[2]}}}  {{:<{widths[3]}}}"
    print()
    print(fmt.format(*headers))
    print(fmt.format(*("-" * w for w in widths)))
    for row in rows:
        print(fmt.format(*row))


def main():
    if len(sys.argv) > 1:
        lines = [sys.argv[1]]
    else:
        lines = [ln.strip() for ln in sys.stdin if ln.strip()]

    # Feed line-by-line so we can attribute fragments to each emitted message.
    # AIVDM multipart sentences span N lines but only the final fragment yields a message.
    dec = aiscat.Decoder(format="annotated", country=True)
    pending = []
    first = True
    for line in lines:
        pending.append(line)
        dec.feed(line + "\n")
        msg = dec.next()
        while msg is not None:
            if not first:
                print("\n" + "=" * 78 + "\n")
            first = False
            render(msg, pending)
            pending = []
            msg = dec.next()


if __name__ == "__main__":
    main()
