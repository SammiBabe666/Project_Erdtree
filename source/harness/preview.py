#!/usr/bin/env python3
"""Knob-state preview grid of the erdtree screen (real screen code, mocked firmware calls), in the layout of the
earlier bloodmoon preview: 2 columns, a caption under each frame. Uses op1r-src's harness (render.py, screen_harness.c).

    python3 harness/preview.py OP1R_SRC/synth_sdk out.png
The erdtree folder must be reachable as OP1R_SRC/synth_sdk/plugins/erdtree (a symlink is fine).
Copyright (C) 2026 Sammi (Project ERD). GPL-3.0-or-later with the OP-1 firmware permission (see LICENSE and NOTICE).
"""
import json, os, subprocess, sys
import xml.etree.ElementTree as ET
sdk, out = sys.argv[1], sys.argv[2]
sys.path.insert(0, os.path.join(sdk, 'harness'))
import render, cairo

D = [9000, 22000, 12000, 14000]
# (caption, knobs blue/green/white/orange, frames at 60 fps)
STATES = [
    (f'defaults ({", ".join(map(str, D))})', D, 150),
    ('all min (0, 0, 0, 0)', [0, 0, 0, 0], 150),
    ('all max (32767, 32767, 32767, 32767)', [32767] * 4, 150),
    ('BRASS (orange) max, rest min: light shafts break through (0, 0, 0, 32767)', [0, 0, 0, 32767], 150),
    ('STRINGS (green) max: crown full size and swaying (0, 32767, 0, 0)', [0, 32767, 0, 0], 95),
    ('CHOIR (blue) max: leaves fall from the crown (32767, 0, 0, 9000)', [32767, 0, 0, 9000], 150),
    ('SPACE (white) max: stars multiply and twinkle (9000, 0, 32767, 14000)', [9000, 0, 32767, 14000], 150),
    ('SPACE max, other moment (9000, 0, 32767, 14000)', [9000, 0, 32767, 14000], 211),
]
e, exe, _ = render.build('erdtree')
svg = ET.parse(os.path.join(sdk, e['screen']['svg'])).getroot()
S, PAD, CAP = 2, 20, 28
W, H = 320 * S, 160 * S
surf = cairo.ImageSurface(cairo.FORMAT_RGB24, 2 * W + 3 * PAD - 8, 4 * (H + CAP + PAD) + PAD - 10)
ctx = cairo.Context(surf); ctx.set_source_rgb(0.133, 0.133, 0.133); ctx.paint()
for n, (cap, k, frames) in enumerate(STATES):
    fr = json.loads(subprocess.run([exe, *map(str, k), '1', str(frames), '-1'], check=True, capture_output=True, text=True).stdout)
    x0, y0 = PAD + (n % 2) * (W + PAD - 8), PAD + (n // 2) * (H + CAP + PAD)
    ctx.save(); ctx.translate(x0, y0); ctx.rectangle(0, 0, W, H); ctx.clip()
    ctx.set_source_rgb(0, 0, 0); ctx.paint(); ctx.scale(S, S)
    render.STYLES.clear(); render.STYLES.update(fr.get('styles', {}))
    render.draw_svg(ctx, svg, fr['nodes'], set(fr.get('hidden', ())))
    for x1, y1, x2, y2, w, c in fr['lines']:
        ctx.set_source_rgb(*(v / 255 for v in c[:3])); ctx.set_line_width(w / 1024); ctx.set_line_cap(cairo.LINE_CAP_BUTT)
        ctx.move_to(x1 / 1024, y1 / 1024); ctx.line_to(x2 / 1024, y2 / 1024); ctx.stroke()
    ctx.restore()
    ctx.set_source_rgb(0.7, 0.7, 0.7); ctx.select_font_face('DejaVu Sans'); ctx.set_font_size(11)
    ctx.move_to(x0, y0 + H + 16); ctx.show_text(cap)
surf.write_to_png(out); print('->', out)
