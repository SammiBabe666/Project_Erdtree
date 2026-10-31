#!/usr/bin/env python3
"""Draw the port's T1 page at a few knob states into one SVG (a grid): the still art from gen_opus_art.py, recoloured
as the page does with the stock colours, and every line the page draws, from the page_preview program's output.
    python3 opus/test/page_preview.py ./page_preview > preview.svg
"""
import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
import gen_opus_art  # noqa: E402

STATES = [('all at zero', (0, 0, 0, 0)), ('first grace', (10000, 26000, 12000, 16000)),
          ('roundtable: choir', (32767, 6000, 8000, 0)), ('starscourge: brass', (0, 10000, 6000, 32767)),
          ('erdtree dreams', (30000, 22000, 30000, 4000)), ('all at full', (32767, 32767, 32767, 32767))]


def frame(prog, knobs, ms):
    out = subprocess.run([prog] + [str(k) for k in knobs] + [str(ms)], capture_output=True, text=True, check=True).stdout
    last = out.split('FRAME\n')[-1]
    parts = []
    for line in last.splitlines():
        _, x0, y0, x1, y1, w, col = line.split()
        x0, y0, x1, y1 = (int(v) / 256 for v in (x0, y0, x1, y1))
        parts.append(f'<line x1="{x0:.2f}" y1="{y0:.2f}" x2="{x1:.2f}" y2="{y1:.2f}" stroke="#{col}" '
                     f'stroke-width="{int(w) / 256:.2f}" stroke-linecap="round"/>')
    return ''.join(parts)


def main():
    prog = sys.argv[1]
    still = gen_opus_art.static_svg()
    still = still[still.index('>') + 1:still.rindex('</svg>')]
    still = still.replace(gen_opus_art.ART_WHITE, '#FFFFFF').replace(gen_opus_art.ART_BLUE, '#4D9EFF')
    cells = []
    for i, (name, knobs) in enumerate(STATES):
        x, y = (i % 2) * 330, (i // 2) * 182
        cells.append(f'<g transform="translate({x},{y})"><rect width="320" height="160" fill="#000"/>{still}'
                     f'{frame(prog, knobs, 2300 + 700 * i)}'
                     f'<text x="4" y="172" fill="#ccc" font-family="sans-serif" font-size="10">{name}: '
                     f'{" ".join(str(k) for k in knobs)}</text></g>')
    print(f'<svg xmlns="http://www.w3.org/2000/svg" width="650" height="546" viewBox="0 0 650 546">'
          f'<rect width="650" height="546" fill="#111"/>{"".join(cells)}</svg>')


if __name__ == '__main__':
    main()
