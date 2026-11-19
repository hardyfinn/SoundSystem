#!/usr/bin/env python3
# Draws res/SoundSystem.svg. Rack can't draw SVG <text>, so labels become paths.
# Positions are in mm; the LAYOUT block numbers are shared with src/SoundSystem.cpp.
#   pip install fonttools ; python3 res/make_panel.py
import os
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen

HERE = os.path.dirname(os.path.abspath(__file__))
FONT_BOLD = os.environ.get("PANEL_FONT_BOLD", "/usr/share/fonts/opentype/inter/Inter-Bold.otf")
FONT_MED = os.environ.get("PANEL_FONT_MED", "/usr/share/fonts/opentype/inter/Inter-Medium.otf")
FONT_BLACK = os.environ.get("PANEL_FONT_BLACK", "/usr/share/fonts/opentype/inter/Inter-Black.otf")

W, H = 101.6, 128.5  # 20 HP

BG = "#1E1C1B"
BG_DARK = "#121110"
GRILLE = "#2A2725"
LINE = "#3E3A37"
INK = "#F0E6D2"
SOFT = "#AAA091"
AMBER = "#F5AA3C"
OUTC = "#F0E6D2"

# ---- LAYOUT (same numbers in src/SoundSystem.cpp) ----
DISP_X, DISP_Y, DISP_W, DISP_H = 6.0, 16.0, 89.6, 20.0
SYS_X, BIG_Y = 17.0, 53.0
PUSH_X = 50.8
DIST_X = 84.6
COL5 = [12.7, 31.75, 50.8, 69.85, 88.9]
ROW2_Y = 77.0
CV_Y = 95.0
COL6 = [9.5, 26.0, 42.5, 59.1, 75.6, 92.1]
IO_Y = 113.0

CONTROL = 3.2   # control label size (project standard)
JACK = 2.9      # jack label size (project standard)

fonts = {}


def text(s, x, y, size, font=FONT_BOLD, fill=INK, anchor="middle", spacing=0.04):
    if font not in fonts:
        f = TTFont(font)
        fonts[font] = (f, f.getGlyphSet(), f.getBestCmap(), f["head"].unitsPerEm)
    f, gs, cmap, upm = fonts[font]
    scale = size / upm
    track = spacing * upm
    names = [cmap[ord(c)] for c in s]
    width = sum(gs[n].width + track for n in names) - track
    x0 = {"middle": x - width * scale / 2, "end": x - width * scale}.get(anchor, x)
    pen = SVGPathPen(gs)
    cx = 0.0
    for n in names:
        gs[n].draw(TransformPen(pen, (scale, 0, 0, -scale, x0 + cx * scale, y)))
        cx += gs[n].width + track
    return f'<path fill="{fill}" d="{pen.getCommands()}"/>'


parts = []
add = parts.append
add(f'<rect width="{W}" height="{H}" fill="{BG}"/>')

# speaker-grille dots behind the big knobs
dots = []
y = 40.0
while y < 66.5:
    x = 3.5 + (1.6 if int((y - 40.0) / 2.2) % 2 else 0)
    while x < W - 3.0:
        dots.append(f'M{x:.2f} {y:.2f}h0.01')
        x += 3.2
    y += 2.2
add(f'<path d="{" ".join(dots)}" stroke="{GRILLE}" stroke-width="0.9" stroke-linecap="round"/>')

# title
add(text("SOUND SYSTEM", 5.0, 11.4, 5.2, FONT_BLACK, INK, anchor="start", spacing=0.06))
add(text("big rig, real place", W - 5.0, 11.0, 2.5, FONT_MED, SOFT, anchor="end"))

# screen frame
add(f'<rect x="{DISP_X - 0.6}" y="{DISP_Y - 0.6}" width="{DISP_W + 1.2}" height="{DISP_H + 1.2}" '
    f'rx="1.0" fill="{BG_DARK}" stroke="{LINE}" stroke-width="0.3"/>')

# big knobs (labels sit on a dark tab so they read over the grille)
for (x, lab, off) in ((SYS_X, "SYSTEM", 10.2), (PUSH_X, "PUSH", 12.4), (DIST_X, "DISTANCE", 10.2)):
    add(f'<rect x="{x - 10.5}" y="{BIG_Y - off - 3.6}" width="21" height="4.8" rx="0.8" fill="{BG}"/>')
    add(text(lab, x, BIG_Y - off, CONTROL, fill=AMBER if lab == "PUSH" else INK))
add(f'<rect x="{DIST_X - 11}" y="{BIG_Y + 7.6}" width="22" height="4.2" rx="0.8" fill="{BG}"/>')
add(text("near", DIST_X - 7.0, BIG_Y + 10.6, 2.5, FONT_MED, SOFT))
add(text("far", DIST_X + 7.0, BIG_Y + 10.6, 2.5, FONT_MED, SOFT))
add(f'<rect x="{SYS_X - 9}" y="{BIG_Y + 7.6}" width="18" height="4.2" rx="0.8" fill="{BG}"/>')
add(text("1-8", SYS_X, BIG_Y + 10.6, 2.5, FONT_MED, SOFT))

# second row
for x, n in zip(COL5, ["WEIGHT", "ROOM", "TOPS", "CROWD", "MIX"]):
    add(text(n, x, ROW2_Y - 6.4, CONTROL))

# CV inputs on a dark strip
add(f'<rect x="2.6" y="{CV_Y - 9.2}" width="{W - 5.2}" height="14.4" rx="1.4" fill="{BG_DARK}"/>')
for x, n in zip(COL5, ["SYSTEM", "PUSH", "DIST", "WEIGHT", "ROOM"]):
    add(text(n, x, CV_Y - 5.6, JACK, FONT_BOLD, SOFT))

# audio in, door, audio out
add(f'<rect x="{COL6[0] - 7.6}" y="{IO_Y - 9.2}" width="{COL6[1] - COL6[0] + 15.2}" height="13.6" rx="1.4" fill="{BG_DARK}"/>')
add(text("IN L", COL6[0], IO_Y - 5.6, JACK))
add(text("IN R", COL6[1], IO_Y - 5.6, JACK))
add(text("DOOR", COL6[2], IO_Y - 5.6, JACK, fill=AMBER))
add(text("GATE", COL6[3], IO_Y - 5.6, JACK, fill=AMBER))
add(f'<rect x="{COL6[4] - 7.6}" y="{IO_Y - 9.2}" width="{COL6[5] - COL6[4] + 15.2}" height="13.6" rx="1.4" '
    f'fill="{OUTC}" fill-opacity="0.12"/>')
add(text("OUT L", COL6[4], IO_Y - 5.6, JACK))
add(text("OUT R", COL6[5], IO_Y - 5.6, JACK))

svg = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}mm" height="{H}mm" viewBox="0 0 {W} {H}">\n'
       + "\n".join(p for p in parts if p) + "\n</svg>\n")
open(os.path.join(HERE, "SoundSystem.svg"), "w").write(svg)
print("ok")

# --- Haer maker logo (res/haer_logo.py) on the bottom rail
import sys as _sys
_sys.path.insert(0, HERE)
import haer_logo
haer_logo.add_to_file(os.path.join(HERE, "SoundSystem.svg"), x=W / 2, y=126.0, cap=3.0, bg=BG)
