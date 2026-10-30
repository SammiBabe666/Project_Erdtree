#!/usr/bin/env python3
"""The 30-second demo film: the screen as the real screen code draws it, the four knobs and a caption per scene (all
from film.c's frames), and film.c's audio through a stand-in reverb (audition.py's, not the OP-1's spring / delay /
grid). 1280 x 720, 30 fps, H.264 + AAC. The layout is video.py's. tour.py imports the drawing, reverb and encoding.

    python3 harness/film.py OP1R_SRC/synth_sdk frames.jsonl audio.raw erdtree.svg out.mp4
Copyright (C) 2026 Sammi (Project ERD). GPL-3.0-or-later with the OP-1 firmware permission (see LICENSE and NOTICE).
"""
import json, math, os, subprocess, sys, tempfile, wave
import xml.etree.ElementTree as ET
import numpy as np
from scipy.signal import lfilter

W, H, FPS, SR = 1280, 720, 30, 44100
S = 3.25                                            # OP-1 screen scale
SX, SY = (W - 320 * S) / 2, 22
KNOBS = [('CHOIR', (0x4D, 0x9E, 0xFF)), ('STRINGS', (0x00, 0xED, 0x95)), ('SPACE', (0xFF, 0xFF, 0xFF)),
         ('BRASS', (0xFF, 0x3A, 0x5D))]
FOOT = 'erdtree v0.5 · screen and audio rendered from the plugin code on a computer (reverb added) · not recorded on an OP-1'
rgb = lambda c: tuple(v / 255 for v in c)
render = cairo = svg = None


def load(sdk, svg_path):
    """The SDK's SVG renderer (synth_sdk/harness/render.py) and the screen's SVG."""
    global render, cairo, svg
    sys.path.insert(0, os.path.join(sdk, 'harness'))
    import render as r, cairo as c
    render, cairo, svg = r, c, ET.parse(svg_path).getroot()


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


def frame(fr, path, foot=FOOT):
    """One video frame. An optional 'sub' line goes under the caption, which then moves up to make room."""
    surf = cairo.ImageSurface(cairo.FORMAT_RGB24, W, H); ctx = cairo.Context(surf)
    ctx.set_source_rgb(0.04, 0.04, 0.045); ctx.paint()
    ctx.save(); ctx.translate(SX, SY); ctx.rectangle(0, 0, 320 * S, 160 * S); ctx.clip()
    ctx.set_source_rgb(0, 0, 0); ctx.paint(); ctx.scale(S, S)
    render.draw_svg(ctx, svg, fr['nodes'])
    for x1, y1, x2, y2, w, r, g, b in fr['lines']:
        ctx.set_source_rgb(r / 255, g / 255, b / 255); ctx.set_line_width(w / 1024); ctx.set_line_cap(cairo.LINE_CAP_BUTT)
        ctx.move_to(x1 / 1024, y1 / 1024); ctx.line_to(x2 / 1024, y2 / 1024); ctx.stroke()
    ctx.restore()
    ctx.set_source_rgb(0.2, 0.2, 0.22); ctx.set_line_width(2); ctx.rectangle(SX - 1, SY - 1, 320 * S + 2, 160 * S + 2); ctx.stroke()
    if fr.get('sub'):
        text(ctx, fr['caption'], W / 2, SY + 160 * S + 34, 24, (1, 1, 1), bold=True, anchor='center')
        text(ctx, fr['sub'], W / 2, SY + 160 * S + 60, 16, (0.68, 0.68, 0.7), anchor='center')
    else:
        text(ctx, fr['caption'], W / 2, SY + 160 * S + 44, 24, (1, 1, 1), bold=True, anchor='center')
    for i, ((name, col), v) in enumerate(zip(KNOBS, fr['k'])):
        knob(ctx, 250 + i * 230, 652, name, col, v)
    text(ctx, foot, W / 2, H - 10, 12, (0.42, 0.42, 0.45), anchor='center')
    surf.write_to_png(path)


def comb(s, d, g, damp=0.3):
    a = np.zeros(d + 1); a[0] = 1; a[d] = -g
    return lfilter([1 - damp], [1, -damp], lfilter([1], a, s))


def allpass(s, d, g=0.5):
    b = np.zeros(d + 1); b[0] = -g; b[d] = 1
    a = np.zeros(d + 1); a[0] = 1; a[d] = -g
    return lfilter(b, a, s)


def hall(x, spread):
    w = sum(comb(x, d + spread, 0.84) for d in (1557, 1617, 1491, 1422, 1277, 1356, 1188, 1116)) / 8
    for d in (556, 441, 341, 225):
        w = allpass(w, d + spread)
    return 0.6 * x + 0.5 * w


def encode(frames, y, out, foot=FOOT):
    """frames: the frame dicts in order; y: stereo float audio (n x 2), faded out over its last 0.4 s and scaled to a
    0.89 peak here. Writes an H.264 + AAC MP4."""
    fade = int(0.4 * SR)
    y = y.copy(); y[-fade:] *= np.linspace(1, 0, fade)[:, None]           # the reverb tail at the cut
    y = y / np.abs(y).max() * 0.89
    with tempfile.TemporaryDirectory() as d:
        wav = os.path.join(d, 'audio.wav')
        with wave.open(wav, 'wb') as w:
            w.setnchannels(2); w.setsampwidth(2); w.setframerate(SR)
            w.writeframes((y * 32767).astype('<i2').tobytes())
        for n, fr in enumerate(frames):
            frame(fr, os.path.join(d, f'{n:04d}.png'), foot)
        subprocess.run(['ffmpeg', '-y', '-loglevel', 'error', '-framerate', str(FPS), '-i', os.path.join(d, '%04d.png'),
                        '-i', wav, '-c:v', 'libx264', '-pix_fmt', 'yuv420p', '-crf', '18', '-preset', 'slow',
                        '-c:a', 'aac', '-b:a', '192k', '-shortest', '-movflags', '+faststart', out], check=True)
    print('->', out)


def main():
    sdk, frames_path, raw_path, svg_path, out = sys.argv[1:6]
    load(sdk, svg_path)
    x = np.fromfile(raw_path, dtype='<i2').astype(float) / 32768
    encode((json.loads(line) for line in open(frames_path)), np.stack([hall(x, 0), hall(x, 23)], 1), out)


if __name__ == '__main__':
    main()
