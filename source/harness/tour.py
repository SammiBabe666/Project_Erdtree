#!/usr/bin/env python3
"""The preset tour film: each preset of presets.py in turn, playing a short phrase written for it, with its knobs on
screen and a caption saying what it is and which OP-1 effect it uses. tour.c renders the engine and the screen from
the timeline below; the audio goes through stand-ins for the effects (film.py's reverb on everything, and an echo on
the delay presets), not the OP-1's spring / delay / grid. Layout and encoding are film.py's.

    cc -DOP1_HOST -I<op1r-src>/synth_sdk -I.. harness/tour.c erdtree.c erdscreen.c -lm -o tour
    python3 harness/tour.py OP1R_SRC/synth_sdk ./tour erdtree.svg out.mp4
Copyright (C) 2026 Sammi (Project ERD). GPL-3.0-or-later with the OP-1 firmware permission (see LICENSE and NOTICE).
"""
import json, os, subprocess, sys, tempfile
from pathlib import Path
import numpy as np
from scipy.signal import lfilter
sys.path.insert(0, str(Path(__file__).resolve().parent)); sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import film
from presets import PRESETS

FOOT = ('erdtree v0.5 · rendered from the plugin code on a computer · stand-in reverb and delay, not the OP-1\'s '
        'effects · not recorded on an OP-1')
TAIL = 1.2                                          # after the last preset

# name: what it is, seconds on screen, and its phrase as (on, off, notes) in seconds from the preset's start
Cm, Ab, Bb, G, C = (48, 55, 63), (44, 51, 60), (46, 53, 62), (43, 50, 59), (48, 55, 64)
TOUR = {
    'first grace': ('the default: warm strings, some brass, a touch of choir', 5.0,
                    [(0.2, 2.4, Cm), (2.5, 4.3, Ab)]),
    'golden order': ('fanfare: brass in front of the strings, quick attack', 5.0,
                     [(0.2, 0.5, C), (0.6, 0.75, C), (0.8, 0.95, C), (1.05, 1.95, Ab), (2.05, 2.95, Bb),
                      (3.05, 4.4, C)]),
    'malenia lament': ('strings alone, far away and dark, slow swell', 6.0,
                       [(0.2, 4.9, (50, 57)), (0.2, 1.8, (65,)), (1.8, 3.3, (64,)), (3.3, 4.9, (62,))]),
    'erdtree dreams': ('choir and strings pad, very slow swell, long tail', 6.0,
                       [(0.2, 4.6, (51, 55, 58, 62)), (2.2, 4.6, (65,))]),
    'starscourge': ('low brass, an octave down', 5.0,
                    [(0.2, 0.7, (48, 55)), (0.8, 1.05, (48, 55)), (1.1, 1.35, (48, 55)), (1.4, 2.2, (44, 51)),
                     (2.3, 2.8, (48, 55)), (2.9, 3.8, (49, 56)), (3.9, 4.5, (48, 55))]),
    'leyndell hall': ('the full orchestra in a big room', 5.5,
                      [(0.2, 1.8, Cm), (1.9, 3.3, Ab), (3.4, 4.9, G)]),
    'roundtable': ('the choir, close up', 5.0,
                   [(0.2, 2.4, (56, 60, 63)), (2.5, 4.4, (55, 58, 63))]),
    'royal stab': ('short brass stabs', 5.0,
                   [(0.2 + b * 0.25, 0.2 + b * 0.25 + 0.18, ch) for b, ch in
                    [(0, Cm), (2, Cm), (3, Cm), (5, Ab), (7, Bb), (9, Cm), (11, Cm), (12, Cm), (14, Ab), (16, G)]]),
}


def timeline():
    scenes, notes, t = [], [], 0.0
    for name, knobs, adsr, fx, octave in PRESETS:
        desc, dur, phrase = TOUR[name]
        scenes.append((t, dur, name, desc, fx[0], knobs, adsr, octave))
        notes += [(t + on, t + off, n) for on, off, ch in phrase for n in ch]
        t += dur
    return scenes, sorted(notes), t + TAIL


def echo(x, sr, secs=0.36, fb=0.38, mix=0.45):
    d = int(secs * sr)
    b = np.zeros(d + 1); b[d] = mix
    a = np.zeros(d + 1); a[0] = 1; a[d] = -fb
    return lfilter(b, a, x)


def main():
    sdk, exe, svg_path, out = sys.argv[1:5]
    film.load(sdk, svg_path)
    scenes, notes, length = timeline()
    spec = [f'L {length:.3f}', f'S {len(scenes)}']
    spec += [f'{s[0]:.3f} {" ".join(map(str, s[5]))} {" ".join(map(str, s[6]))} {s[7]}' for s in scenes]
    spec += [f'N {len(notes)}'] + [f'{on:.3f} {off:.3f} {n}' for on, off, n in notes]
    with tempfile.TemporaryDirectory() as d:
        raw = os.path.join(d, 'audio.raw')
        frames = subprocess.run([exe, raw], input='\n'.join(spec) + '\n', capture_output=True, text=True, check=True).stdout
        x = np.fromfile(raw, dtype='<i2').astype(float) / 32768
    sr = film.SR
    send = np.zeros_like(x)                                     # the delay presets' share of the dry signal
    for start, dur, name, desc, fx, *_ in scenes:
        if fx == 'delay':
            send[int(start * sr):int((start + dur) * sr)] = x[int(start * sr):int((start + dur) * sr)]
    x = x + echo(send, sr)
    for start, dur, name, *_ in scenes:
        seg = x[int(start * sr):int((start + dur) * sr)]
        print(f'{start:5.1f} s  {name:15s} peak {20 * np.log10(np.abs(seg).max()):6.1f} dBFS (before the reverb)')
    y = np.stack([film.hall(x, 0), film.hall(x, 23)], 1)

    def captioned():
        for line in frames.splitlines():
            fr = json.loads(line)
            _, _, name, desc, fx, *_ = scenes[fr['scene']]
            fr['caption'] = f'{fr["scene"] + 1} / {len(scenes)}   {name}'
            fr['sub'] = f'{desc} · OP-1 effect: {fx}'
            yield fr
    film.encode(captioned(), y, out, FOOT)


if __name__ == '__main__':
    main()
