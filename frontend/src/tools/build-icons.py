#!/usr/bin/env python3
"""Draws the map's sprite sheet: icons-tint.png and icons-tint-2x.png in frontend/src, and
the copy inlined in frontend/packages/ui/css/sprites.css, from the shapes below. Every cell is a
white body with a grey outline, coloured where it is drawn (markers.js, sprites.css).
The cell layout is a contract with core/sprites.js CELLS: a cell moved here moves there.

    pip install cairosvg pillow
    python3 frontend/src/tools/build-icons.py
"""
import base64, os, re
import cairosvg

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.normpath(os.path.join(HERE, ".."))                       # frontend/src
CSS = os.path.normpath(os.path.join(HERE, "..", "..", "packages", "ui", "css", "sprites.css"))
FILL, LINE = "#ffffff", "#555555"
# (name, cell size, body, extra filled parts, stroke-only details, hint)
SHIPS = [
    ("arrow", "M12.23 3.66Q15.93 10.96 17.48 18.99Q18 20.5 16.62 19.69Q14.68 18.90 13.04 17.60Q12 17 10.96 17.60Q9.32 18.90 7.38 19.69Q6 20.5 6.52 18.99Q8.07 10.96 11.77 3.66Q12 3 12.23 3.66Z", "", "", "moving vessel"),
    ("dot", "M6.5 12A5.5 5.5 0 1 1 17.5 12A5.5 5.5 0 1 1 6.5 12Z", "", "", "stationary vessel"),
    ("diamond", "M13.13 3.63L20.37 10.87Q21.5 12 20.37 13.13L13.13 20.37Q12 21.5 10.87 20.37L3.63 13.13Q2.5 12 3.63 10.87L10.87 3.63Q12 2.5 13.13 3.63ZM12 10a2 2 0 1 0 0 4a2 2 0 1 0 0-4Z", "", "", "AtoN / base station / SART"),
]
PLANES = [
    ("airliner", "M12 1.5C13 1.5 13.3 3 13.3 4.5V9L22 13.6V15.2L13.3 12.6L13.1 18L16 20V21.3L12 20.4L8 21.3V20L10.9 18L10.7 12.6L2 15.2V13.6L10.7 9V4.5C10.7 3 11 1.5 12 1.5Z", "M16.2 10.4a.8.8 0 0 1 1.6 0V13H16.2ZM6.2 10.4a.8.8 0 0 1 1.6 0V13H6.2Z", "", "A3 A4"),
    ("widebody", "M12 .8C13.2 .8 13.6 2.6 13.6 4.6V9L23.2 14.2V16L13.6 13.2L13.4 18.6L17 20.7V22.1L12 21.1L7 22.1V20.7L10.6 18.6L10.4 13.2L.8 16V14.2L10.4 9V4.6C10.4 2.6 10.8 .8 12 .8Z", "M17.3 11.2a1 1 0 0 1 2 0V13.8H17.3ZM4.7 11.2a1 1 0 0 1 2 0V13.8H4.7Z", "", "A5"),
    ("bizjet", "M12 3C12.8 3 13 4.2 13 5.5V10.2L19.5 14V15.2L13 13.8L12.9 17.6L15.6 19.6V20.8L12 20L8.4 20.8V19.6L11.1 17.6L11 13.8L4.5 15.2V14L11 10.2V5.5C11 4.2 11.2 3 12 3Z", "M13.2 14.8a.7.7 0 0 1 1.4 0V17.4H13.2ZM9.4 14.8a.7.7 0 0 1 1.4 0V17.4H9.4Z", "", "A2"),
    ("light", "M12 4.2C12.9 4.2 13 5.2 13 6.2V8.2L21.5 8.4V10.8L13 11L12.7 16.8L15.6 17.4V19.2L12 18.9L8.4 19.2V17.4L11.3 16.8L11 11L2.5 10.8V8.4L11 8.2V6.2C11 5.2 11.1 4.2 12 4.2Z", "", "M9.6 3.4H14.4", "A1 B4"),
    ("fighter", "M12 1.2L13 4.6L13.2 9.4L20.6 15.6V17.4L13.4 16.2L13.3 18.6L15.8 20.6V21.8L12.6 21.2L12 22.4L11.4 21.2L8.2 21.8V20.6L10.7 18.6L10.6 16.2L3.4 17.4V15.6L10.8 9.4L11 4.6Z", "", "", "A6"),
    ("helicopter", "M12 5.4C13.9 5.4 15 7.2 15 9.6C15 12 13.9 13.6 12.7 13.9V20.2H11.3V13.9C10.1 13.6 9 12 9 9.6C9 7.2 10.1 5.4 12 5.4Z", "M9.6 19.4H14.4V20.8H9.6Z", "", "A7"),
    ("glider", "M12 6C12.6 6 12.7 7 12.7 8V9.4L23.5 10V11.2L12.7 11.4L12.4 19L14.6 19.4V20.6L12 20.4L9.4 20.6V19.4L11.6 19L11.3 11.4L.5 11.2V10L11.3 9.4V8C11.3 7 11.4 6 12 6Z", "", "", "B1"),
    ("balloon", "M5.2 9.3A6.8 6.8 0 1 1 18.8 9.3C18.8 12.6 15.2 15 13 16.2H11C8.8 15 5.2 12.6 5.2 9.3Z", "M10.6 18.2H13.4V21H10.6Z", "M11 16.4L10.9 18.2M13 16.4L13.1 18.2", "B2 (no rotation)"),
    ("uav", "M12 6.6L13.3 9.2L14 10.6L13.4 12L14 13.4L12.9 14.4L12 13.8L11.1 14.4L10 13.4L10.6 12L10 10.6L10.7 9.2Z", "M3.8 6.6a2.7 2.7 0 1 0 5.4 0a2.7 2.7 0 1 0 -5.4 0ZM14.8 6.6a2.7 2.7 0 1 0 5.4 0a2.7 2.7 0 1 0 -5.4 0ZM3.8 17.4a2.7 2.7 0 1 0 5.4 0a2.7 2.7 0 1 0 -5.4 0ZM14.8 17.4a2.7 2.7 0 1 0 5.4 0a2.7 2.7 0 1 0 -5.4 0Z", "M8.4 8.4L10.4 10.4M15.6 8.4L13.6 10.4M8.4 15.6L10.4 13.6M15.6 15.6L13.6 13.6", "B6"),
    ("vehicle", "M8.3 7H15.7A1.3 1.3 0 0 1 17 8.3V15.7A1.3 1.3 0 0 1 15.7 17H8.3A1.3 1.3 0 0 1 7 15.7V8.3A1.3 1.3 0 0 1 8.3 7Z", "", "", "C1 C2"),
    ("obstacle", "M12 3.6L20.4 19H3.6Z", "", "M12 9.4V13.8M12 16.2V16.4", "C3-C7 (no rotation)"),
    ("unknown", "M11 3H13V8.5L21 12V14L13 12.2V18L15.6 19.8V21L12 20.2L8.4 21V19.8L11 18V12.2L3 14V12L11 8.5Z", "", "", "A0 B0 B3 B7 / none"),
]

UNDER = {"helicopter": "M4.03 2.77L18.83 17.57L19.97 16.43L5.17 1.63ZM19.97 2.77L5.17 17.57L4.03 16.43L18.83 1.63Z"}

def cell(x, y, size, body, parts, acc, under=""):
    sw = 1.2
    s = f'<svg x="{x}" y="{y}" width="{size}" height="{size}" viewBox="0 0 24 24" overflow="visible">'
    if under:
        s += f'<path d="{under}" fill="{FILL}" stroke="{LINE}" stroke-width="0.9" stroke-linejoin="round"/>'
    s += f'<path d="{body}" fill="{FILL}" fill-rule="evenodd" stroke="{LINE}" stroke-width="{sw}" stroke-linejoin="round"/>'
    if parts:
        s += f'<path d="{parts}" fill="{FILL}" stroke="{LINE}" stroke-width="{sw}" stroke-linejoin="round"/>'
    if acc:
        s += f'<path d="{acc}" fill="none" stroke="{LINE}" stroke-width="1.3" stroke-linecap="round"/>'
    return s + "</svg>"

W, H = 12 * 25, 20 + 25
cells = []
for i, (n, b, p, a, hint) in enumerate(SHIPS):
    cells.append(cell(i * 20, 0, 20, b, p, a))
for i, (n, b, p, a, hint) in enumerate(PLANES):
    cells.append(cell(i * 25, 20, 25, b, p, a, UNDER.get(n, "")))

svg = f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}">' + "".join(cells) + "</svg>"
cairosvg.svg2png(bytestring=svg.encode(), write_to=f"{OUT}/icons-tint.png", output_width=W, output_height=H)
cairosvg.svg2png(bytestring=svg.encode(), write_to=f"{OUT}/icons-tint-2x.png", output_width=W * 2, output_height=H * 2)

# the sheet sprites.css carries, so the chips in HTML and the markers on the map never differ
css = open(CSS).read()
data = base64.b64encode(open(f"{OUT}/icons-tint-2x.png", "rb").read()).decode()
css, n = re.subn(r'(--sprite-sheet: url\("data:image/png;base64,)[^"]*(")', lambda m: m.group(1) + data + m.group(2), css)
assert n == 1, "sprites.css has no --sprite-sheet to replace"
open(CSS, "w").write(css)
print(f"{W}x{H}: icons-tint.png, icons-tint-2x.png, sprites.css")
