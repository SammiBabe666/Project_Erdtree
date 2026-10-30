#!/usr/bin/env python3
"""Demo video for erdtree: the screen as the real screen code draws it (frames from video.c), the four knobs, a caption
per scene, and the demo audio (demo.c, through a stand-in reverb). 1280 x 720, 30 fps, H.264 + AAC.

    python3 harness/video.py OP1R_SRC/synth_sdk frames.jsonl erdtree.svg audio.wav out.mp4
Copyright (C) 2026 Sammi (Project ERD). GPL-3.0-or-later with the OP-1 firmware permission (see LICENSE and NOTICE).
"""
import json, math, os, subprocess, sys, tempfile
import xml.etree.ElementTree as ET
sdk, frames_path, svg_path, audio, out = sys.argv[1:6]
sys.path.insert(0, os.path.join(sdk, 'harness'))
import render, cairo

W, H, FPS = 1280, 720, 30
S = 3.25                                            # OP-1 screen scale
SX, SY = (W - 320 * S) / 2, 22
KNOBS = [('CHOIR', (0x4D, 0x9E, 0xFF)), ('STRINGS', (0x00, 0xED, 0x95)), ('SPACE', (0xFF, 0xFF, 0xFF)),
         ('BRASS', (0xFF, 0x3A, 0x5D))]
SCENES = [(0, 4, 'green: the strings swell in'), (4, 8, 'orange: the brass comes in'),
          (8, 12, 'blue: the choir joins'), (12, 16, 'white: the space opens up'),
          (16, 99, 'erdtree  ·  an OP-1 synth engine')]
svg = ET.parse(svg_path).getroot()
rgb = lambda c: tuple(v / 255 for v in c)


def text(ctx, s, x, y, size, col, bold=False, anchor='left'):
    ctx.select_font_face('DejaVu Sans', 0, 1 if bold else 0); ctx.set_font_size(size)
    w = ctx.text_extents(s).x_advance
    ctx.move_to(x - (w / 2 if anchor == 'center' else w if anchor == 'right' else 0), y)
    ctx.set_source_rgb(*col); ctx.show_text(s)


def knob(ctx, x, y, name, col, v):
    ctx.set_line_width(3); ctx.set_source_rgb(*rgb(col))
    ctx.new_sub_path(); ctx.arc(x, y, 24, 0, 2 * math.pi); ctx.stroke()
    a = math.radians(-225 + 270 * v / 32767)        # -135..+135 degrees from straight up
    ctx.move_to(x + 8 * math.cos(a), y + 8 * math.sin(a)); ctx.line_to(x + 20 * math.cos(a), y + 20 * math.sin(a)); ctx.stroke()
    text(ctx, name, x + 36, y - 2, 15, (0.85, 0.85, 0.85), bold=True)
    text(ctx, f'{round(100 * v / 32767)} %', x + 36, y + 17, 14, (0.55, 0.55, 0.57))


def frame(fr, n, path):
    surf = cairo.ImageSurface(cairo.FORMAT_RGB24, W, H); ctx = cairo.Context(surf)
    ctx.set_source_rgb(0.04, 0.04, 0.045); ctx.paint()
    # the display
    ctx.save(); ctx.translate(SX, SY); ctx.rectangle(0, 0, 320 * S, 160 * S); ctx.clip()
    ctx.set_source_rgb(0, 0, 0); ctx.paint(); ctx.scale(S, S)
    render.draw_svg(ctx, svg, fr['nodes'])
    for x1, y1, x2, y2, w, r, g, b in fr['lines']:
        ctx.set_source_rgb(r / 255, g / 255, b / 255); ctx.set_line_width(w / 1024); ctx.set_line_cap(cairo.LINE_CAP_BUTT)
        ctx.move_to(x1 / 1024, y1 / 1024); ctx.line_to(x2 / 1024, y2 / 1024); ctx.stroke()
    ctx.restore()
    ctx.set_source_rgb(0.2, 0.2, 0.22); ctx.set_line_width(2); ctx.rectangle(SX - 1, SY - 1, 320 * S + 2, 160 * S + 2); ctx.stroke()
    # caption, knobs, footnote
    s = n / FPS
    cap = next(c for a, b, c in SCENES if a <= s < b)
    text(ctx, cap, W / 2, SY + 160 * S + 44, 24, (1, 1, 1), bold=True, anchor='center')
    for i, ((name, col), v) in enumerate(zip(KNOBS, fr['k'])):
        knob(ctx, 250 + i * 230, 652, name, col, v)
    text(ctx, 'screen and audio rendered from the plugin code on a PC (reverb added) · not an OP-1 recording',
         W / 2, H - 10, 12, (0.42, 0.42, 0.45), anchor='center')
    surf.write_to_png(path)


with tempfile.TemporaryDirectory() as d:
    for n, line in enumerate(open(frames_path)):
        frame(json.loads(line), n, os.path.join(d, f'{n:04d}.png'))
    subprocess.run(['ffmpeg', '-y', '-loglevel', 'error', '-framerate', str(FPS), '-i', os.path.join(d, '%04d.png'),
                    '-i', audio, '-c:v', 'libx264', '-pix_fmt', 'yuv420p', '-crf', '18', '-preset', 'slow',
                    '-c:a', 'aac', '-b:a', '192k', '-shortest', '-movflags', '+faststart', out], check=True)
print('->', out)
