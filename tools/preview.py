#!/usr/bin/env python3
# Picture of the panel with Rack's own component graphics and a sample screen.
#   python3 tools/preview.py out.png [path/to/Rack/res/ComponentLibrary]
import os, sys, re, base64, subprocess
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else "preview.png")
LIB = sys.argv[2] if len(sys.argv) > 2 else os.path.expanduser("~/Rack-src/res/ComponentLibrary")
svg = open(os.path.join(HERE, "..", "res", "SoundSystem.svg")).read()
L = {}
src = open(os.path.join(HERE, "..", "res", "make_panel.py")).read()
exec(src.split("# ---- LAYOUT")[1].split("CONTROL =")[0].split("\n", 1)[1], L)
W = 101.6
PX = 25.4 / 75.0
g = []

def comp(name, x, y, angle=0.0):
    s = open(os.path.join(LIB, name + ".svg")).read()
    m = re.search(r'width="([0-9.]+)(px|mm)?"', s); n = re.search(r'height="([0-9.]+)(px|mm)?"', s)
    w = float(m.group(1)) * (1 if m.group(2) == "mm" else PX); h = float(n.group(1)) * (1 if n.group(2) == "mm" else PX)
    data = base64.b64encode(s.encode()).decode()
    g.append(f'<g transform="rotate({angle} {x} {y})"><image href="data:image/svg+xml;base64,{data}" x="{x - w/2}" y="{y - h/2}" width="{w}" height="{h}"/></g>')

def knob(base, x, y, v):
    if os.path.exists(os.path.join(LIB, base + "_bg.svg")):
        comp(base + "_bg", x, y)
    comp(base, x, y, -135 + 270 * v)

knob("RoundLargeBlackKnob", L["SYS_X"], L["BIG_Y"], 0 / 7)
knob("RoundHugeBlackKnob", L["PUSH_X"], L["BIG_Y"], 0.5)
knob("RoundLargeBlackKnob", L["DIST_X"], L["BIG_Y"], 0.35)
for x, v in zip(L["COL5"], (0.5, 0.5, 0.5, 0.3, 1.0)):
    knob("RoundBlackKnob", x, L["ROW2_Y"], v)
for x in L["COL5"]: comp("PJ301M", x, L["CV_Y"])
C6 = L["COL6"]
for i in (0, 1, 3, 4, 5): comp("PJ301M", C6[i], L["IO_Y"])
comp("VCVBezel", C6[2], L["IO_Y"])
for x in (5.08, W - 2 * 5.08):
    for y in (0.0, 128.5 - 5.08):
        comp("ScrewSilver", x + 2.54, y + 2.54)

# sample screen
dx, dy, dw, dh = L["DISP_X"], L["DISP_Y"], L["DISP_W"], L["DISP_H"]
mono = "font-family:'DejaVu Sans Mono',monospace;font-weight:bold"
g.append(f'<text x="{dx + 2.4}" y="{dy + 6.2}" font-size="4.3" fill="#F0E6D2" style="{mono}">1 DUB HALL</text>')
g.append(f'<text x="{dx + 2.4}" y="{dy + 11.4}" font-size="2.6" fill="#AAA091" style="{mono}">scoop bins, valve amps, a hall shaking</text>')
g.append(f'<text x="{dx + 2.4}" y="{dy + 16.4}" font-size="2.6" fill="#F5AA3C" style="{mono}">7.1 m from the stack</text>')
g.append(f'<text x="{dx + dw - 17}" y="{dy + 16.4}" font-size="2.6" text-anchor="end" fill="#736A60" style="{mono}">room 1.7 s</text>')
sx, sw = dx + dw - 14.5, 12.0
y0, sh = dy + 1.6, dh - 3.2
binH = sh * 0.3
for b in range(2):
    by = y0 + sh - binH * (b + 1)
    g.append(f'<rect x="{sx}" y="{by}" width="{sw}" height="{binH - 0.4}" fill="#342F2B"/>')
    g.append(f'<circle cx="{sx + sw/2}" cy="{by + binH/2 - 0.2}" r="{binH*0.34*1.2}" fill="#181615" stroke="#C08A40" stroke-width="0.35"/>')
midH = sh * 0.24; my = y0 + sh - 2 * binH - midH
g.append(f'<rect x="{sx}" y="{my}" width="{sw}" height="{midH - 0.4}" fill="#342F2B"/>')
g.append(f'<path d="M{sx+1.2} {my+0.8} L{sx+sw-1.2} {my+0.8} L{sx+sw/2+1} {my+midH-1.4} L{sx+sw/2-1} {my+midH-1.4} Z" fill="#3C3226"/>')
for i in range(3):
    g.append(f'<circle cx="{sx + sw*(0.2+0.3*i)}" cy="{my - 2.4}" r="0.9" fill="#8A6A3A"/>')

svg = svg.replace("</svg>", "\n".join(g) + "\n</svg>")
tmp = OUT + ".svg"; open(tmp, "w").write(svg)
pw, ph = 508, 643
open(tmp + ".html", "w").write(f'<html><body style="margin:0;background:#111"><img src="{os.path.basename(tmp)}" style="width:{pw}px;height:{ph}px"></body></html>')
js = f"""const {{chromium}} = require('playwright');
(async () => {{ const b = await chromium.launch(); const p = await b.newPage({{viewport: {{width: {pw}, height: {ph}}}, deviceScaleFactor: 2}});
await p.goto('file://{tmp}.html'); await p.waitForTimeout(400);
await p.screenshot({{path: '{OUT}'}}); await b.close(); }})();"""
open(OUT + ".js", "w").write(js)
subprocess.run(["node", OUT + ".js"], check=True, cwd="/tmp")
os.remove(OUT + ".js"); os.remove(tmp); os.remove(tmp + ".html")
print("wrote", OUT)
