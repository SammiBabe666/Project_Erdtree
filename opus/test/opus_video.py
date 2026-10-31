#!/usr/bin/env python3
"""The Opus port's demo video and display screenshots, from opus_video.c's frames (the port's own page code) and
sound. The still art (gen_opus_art.py) is rasterised once by a headless Chromium (Playwright, through Node); every line
the page draws is painted over it at 2x and scaled down.

    python3 opus/test/opus_video.py video FRAMES.txt AUDIO.raw OUT.mp4     1280 x 720, 30 fps, H.264 + AAC
    python3 opus/test/opus_video.py still FRAMES.txt OUT.png               the OP-1's screen alone, 960 x 480
Needs Pillow, Node with Playwright and Chromium, and ffmpeg (video).
"""
import math
import os
import subprocess
import sys
import tempfile

from PIL import Image, ImageDraw, ImageFont

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import gen_opus_art  # noqa: E402

W, H, FPS = 1280, 720, 30
SS = 2                                                  # supersampling
KNOBS = [('choir', 'blue', (0x4D, 0x9E, 0xFF)), ('strings', 'green', (0x00, 0xED, 0x95)),
         ('space', 'white', (0xAD, 0xAD, 0xFF)), ('brass', 'orange', (0xFF, 0x3A, 0x5D))]
SCENES = [(0, 4, 'green: the strings swell in, the crown grows and sways'),
          (4, 8, 'orange: the brass comes in, shafts of light break through'),
          (8, 12, 'blue: the choir joins, leaves fall from the crown'), (12, 16, 'white: the space opens up, the stars come out'),
          (16, 99, 'erdtree for Opus 2  ·  an OP-1 synth engine')]
FONT = '/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf'
FONT_B = '/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf'


def still_png(scale, path):
    """The still art (in the stock colours, as the page recolours it) at 320 x 160 x scale, black behind it."""
    svg = gen_opus_art.static_svg().replace(gen_opus_art.ART_WHITE, '#FFFFFF').replace(gen_opus_art.ART_BLUE, '#4D9EFF')
    with tempfile.TemporaryDirectory() as d:
        sp = os.path.join(d, 'still.html')
        with open(sp, 'w') as f:
            f.write(f'<html><body style="margin:0;background:#000">'
                    f'<div style="width:{320 * scale}px;height:{160 * scale}px">'
                    f'{svg.replace("<svg ", f"<svg style=\"width:{320 * scale}px;height:{160 * scale}px\" ", 1)}'
                    f'</div></body></html>')
        js = os.path.join(d, 'shot.js')
        with open(js, 'w') as f:
            f.write("const { chromium } = require('playwright');(async () => {const b = await chromium.launch();"
                    f"const p = await b.newPage({{ viewport: {{ width: {320 * scale}, height: {160 * scale} }} }});"
                    f"await p.goto('file://{sp}');await p.screenshot({{ path: '{path}' }});await b.close();}})();")
        subprocess.run(['node', js], check=True)


def read_frames(path):
    frames = []
    for line in open(path):
        p = line.split()
        if p[0] == 'F':
            frames.append(([int(v) for v in p[1:5]], []))
        elif p[0] == 'L' and frames:
            frames[-1][1].append(([int(v) / 256 for v in p[1:5]], int(p[5]) / 256, p[6]))
    return frames


def screen(base, lines, scale):
    """One frame of the OP-1's screen at 320 x 160 x scale: the still art and the page's lines."""
    s = scale * SS
    img = base.copy()
    d = ImageDraw.Draw(img)
    px = s * 320 / 376                                  # a panel pixel (rev 2) in this image's pixels
    for (x0, y0, x1, y1), w, col in lines:
        c = tuple(int(col[i:i + 2], 16) for i in (0, 2, 4))
        wd = max(1.0, w * px)
        a, b = (x0 * s, y0 * s), (x1 * s, y1 * s)
        if a != b:
            d.line([a, b], fill=c, width=int(round(wd)))
        r = wd / 2
        for x, y in (a, b):
            d.ellipse([x - r, y - r, x + r, y + r], fill=c)
    return img.resize((320 * scale, 160 * scale), Image.LANCZOS)


def knob(d, x, y, name, enc, col, v, f1, f2):
    d.ellipse([x - 26, y - 26, x + 26, y + 26], outline=col, width=4)
    a = math.radians(-225 + 270 * v / 32767)
    d.line([(x + 9 * math.cos(a), y + 9 * math.sin(a)), (x + 21 * math.cos(a), y + 21 * math.sin(a))], fill=col, width=4)
    d.text((x + 40, y - 22), name, font=f1, fill=(225, 225, 225))
    d.text((x + 40, y + 4), f'{enc}  {round(100 * v / 32767)} %', font=f2, fill=(140, 140, 145))


def video(frames_path, audio, out):
    frames = read_frames(frames_path)
    scale = 3
    with tempfile.TemporaryDirectory() as d:
        sp = os.path.join(d, 'still.png')
        still_png(scale * SS, sp)
        base = Image.open(sp).convert('RGB')
        f1, f2, f3 = ImageFont.truetype(FONT_B, 20), ImageFont.truetype(FONT, 16), ImageFont.truetype(FONT, 24)
        sx, sy = (W - 320 * scale) // 2, 24
        for i, (k, lines) in enumerate(frames):
            img = Image.new('RGB', (W, H), (17, 17, 19))
            dr = ImageDraw.Draw(img)
            dr.rounded_rectangle([sx - 8, sy - 8, sx + 320 * scale + 8, sy + 160 * scale + 8], 10, fill=(0, 0, 0),
                                 outline=(60, 60, 64), width=2)
            img.paste(screen(base, lines, scale), (sx, sy))
            t = i / FPS
            cap = next(c for a, b, c in SCENES if a <= t < b)
            w = dr.textlength(cap, font=f3)
            dr.text(((W - w) / 2, sy + 160 * scale + 22), cap, font=f3, fill=(235, 235, 235))
            for j, (name, enc, col) in enumerate(KNOBS):
                knob(dr, 150 + j * 290, H - 62, name, enc, col, k[j], f1, f2)
            img.save(os.path.join(d, f'f{i:05d}.png'))
        subprocess.run(['ffmpeg', '-y', '-loglevel', 'error', '-framerate', str(FPS), '-i', os.path.join(d, 'f%05d.png'),
                        '-f', 's16le', '-ar', '44100', '-ac', '1', '-i', audio, '-c:v', 'libx264', '-pix_fmt', 'yuv420p',
                        '-crf', '20', '-c:a', 'aac', '-b:a', '160k', '-shortest', out], check=True)


def still(frames_path, out):
    scale = 3
    k, lines = read_frames(frames_path)[-1]
    with tempfile.TemporaryDirectory() as d:
        sp = os.path.join(d, 'still.png')
        still_png(scale * SS, sp)
        screen(Image.open(sp).convert('RGB'), lines, scale).save(out)


if __name__ == '__main__':
    if sys.argv[1] == 'video':
        video(*sys.argv[2:5])
    else:
        still(*sys.argv[2:4])
