#!/usr/bin/env python3
"""Preset tour: each preset of presets.py as a held chord (audition.c), one after another, through a stand-in reverb
(not the OP-1's spring / delay / grid), as one stereo WAV. Prints where each preset starts and its peak.
    python3 harness/audition.py AUDITION_EXE out.wav
Copyright (C) 2026 Sammi (Project ERD). GPL-3.0-or-later with the OP-1 firmware permission (see LICENSE and NOTICE).
"""
import subprocess, sys, wave
from pathlib import Path
import numpy as np
from scipy.signal import lfilter
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from presets import PRESETS
exe, out = sys.argv[1], sys.argv[2]


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


parts, t = [], 0.0
for name, knobs, adsr, fx, octave in PRESETS:
    raw = subprocess.run([exe, *map(str, knobs + adsr + [octave])], check=True, capture_output=True).stdout
    x = np.frombuffer(raw, dtype='<i2').astype(float) / 32768
    print(f'{t:5.1f} s  {name:15s} peak {20 * np.log10(np.abs(x).max()):6.1f} dBFS (dry, before the reverb)')
    parts.append(x); t += len(x) / 44100
x = np.concatenate(parts + [np.zeros(44100)])
y = np.stack([hall(x, 0), hall(x, 23)], 1)
y = y / np.abs(y).max() * 0.89
with wave.open(out, 'wb') as w:
    w.setnchannels(2); w.setsampwidth(2); w.setframerate(44100)
    w.writeframes((y * 32767).astype('<i2').tobytes())
print('->', out)
