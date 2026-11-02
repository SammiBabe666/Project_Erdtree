#!/usr/bin/env python3
"""Render and measure the eight Opus presets: each plays preset_audition.c's test phrase (a low held note, a held
minor chord, a fast high run, overlapping legato notes, the tail) through a stand-in effect, not the OP-1's own
spring, delay or grid.

    python3 tests/preset_audition.py ./preset_audition OUT_DIR

Writes OUT_DIR/<folder>.wav for each preset and OUT_DIR/tour.wav (all eight, in order), and prints for each:
    peak     the dry phrase's highest sample, dBFS
    chord    the held chord's RMS level, dBFS
    phrase   the whole phrase's RMS level, dBFS (with chord, what the presets are balanced on)
    silent   test notes during which the output stayed below -60 dBFS (should be none)
    edges    how much bigger the sample-to-sample step is at the engine's block edges than elsewhere (a zipper or
             click at a block edge shows as well above 1.0)
    centre   the held chord's spectral centre, Hz: dark (low) to bright (high)
    rise     seconds from the chord's start to 90 % of its loudest: how fast the sound speaks
What this checks is the numbers. Listening to the files is a separate step: say which one was done.

Copyright (C) 2026 Sammi (Project ERD). GPL-3.0-or-later with the OP-1 firmware permission (see LICENSE and NOTICE).
"""
import subprocess
import sys
import wave
from pathlib import Path

import numpy as np
from scipy.signal import lfilter

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "presets"))
from presets import PRESETS  # noqa: E402

SR = 44100
NOTES = [(0.0, 2.0), (2.4, 3.0), (6.2, 0.15), (6.4, 0.15), (6.6, 0.15), (6.8, 0.15), (7.0, 0.4),
         (8.2, 0.65), (8.7, 0.65), (9.2, 0.65), (9.7, 1.2)]
CHORD = (2.9, 5.4)


def comb(s, d, g, damp=0.3):
    a = np.zeros(d + 1); a[0] = 1; a[d] = -g
    return lfilter([1 - damp], [1, -damp], lfilter([1], a, s))


def allpass(s, d, g=0.5):
    b = np.zeros(d + 1); b[0] = -g; b[d] = 1
    a = np.zeros(d + 1); a[0] = 1; a[d] = -g
    return lfilter(b, a, s)


def hall(x, spread, size=0.84):
    w = sum(comb(x, d + spread, size) for d in (1557, 1617, 1491, 1422, 1277, 1356, 1188, 1116)) / 8
    for d in (556, 441, 341, 225):
        w = allpass(w, d + spread)
    return 0.6 * x + 0.5 * w


def echo(x, secs, fb):
    d = int(secs * SR)
    a = np.zeros(d + 1); a[0] = 1; a[d] = -fb
    return 0.7 * x + 0.45 * lfilter([0] * d + [1], a, x)


def effect(x, fx):
    kind, p = fx
    if kind == "delay":            # stand-in: time from blue, feedback from green (roughly)
        t = 0.15 + 0.45 * (p[0] - 3392) / (11200 - 3392)
        fb = 0.3 + 0.35 * (p[1] - 4969) / (32767 - 4969)
        return np.stack([echo(x, t, fb), echo(x, t * 1.07, fb)], 1)
    size = 0.80 if kind == "spring" else 0.86
    return np.stack([hall(x, 0, size), hall(x, 23, size)], 1)


def db(v):
    return 20 * np.log10(max(v, 1e-9))


def main():
    exe, out = sys.argv[1], Path(sys.argv[2])
    out.mkdir(parents=True, exist_ok=True)
    tour = []
    print(f"{'preset':20s} {'peak':>7s} {'chord':>7s} {'phrase':>7s} {'silent':>6s} {'edges':>6s} {'centre':>7s} {'rise':>5s}")
    for slug, name, what, knobs, adsr, fx, octave in PRESETS:
        raw = subprocess.run([exe, "play", *map(str, knobs + adsr + [octave])], check=True,
                             capture_output=True).stdout
        x = np.frombuffer(raw, dtype="<i2").astype(float) / 32768
        peak = db(np.abs(x).max())
        c = x[int(CHORD[0] * SR):int(CHORD[1] * SR)]
        chord = db(np.sqrt(np.mean(c ** 2)))
        a = x[:int(11.0 * SR)]
        phrase = db(np.sqrt(np.mean(a ** 2)))
        spec = np.abs(np.fft.rfft(c * np.hanning(len(c))))
        centre = (spec * np.fft.rfftfreq(len(c), 1 / SR)).sum() / max(spec.sum(), 1e-12)
        ch = x[int(2.4 * SR):int(5.4 * SR)] ** 2
        env = np.sqrt(np.convolve(ch, np.ones(441) / 441, "same"))
        rise = np.argmax(env >= 0.9 * env.max()) / SR
        silent = sum(1 for s, l in NOTES
                     if np.sqrt(np.mean(x[int(s * SR):int((s + l) * SR) + 2048] ** 2)) < 10 ** (-60 / 20))
        d = np.abs(np.diff(x))
        idx = np.arange(len(d))
        edge = d[(idx % 64) == 63]          # the step from a block's last sample to the next block's first
        inner = d[(idx % 64) != 63]
        edges = edge.mean() / max(inner.mean(), 1e-12)
        print(f"{name:20s} {peak:7.1f} {chord:7.1f} {phrase:7.1f} {silent:6d} {edges:6.2f} {centre:7.0f} {rise:5.2f}")
        y = effect(x, fx)
        y = y / max(1.0, np.abs(y).max() / 0.98)
        with wave.open(str(out / f"{slug}.wav"), "wb") as w:
            w.setnchannels(2); w.setsampwidth(2); w.setframerate(SR)
            w.writeframes((y * 32767).astype("<i2").tobytes())
        tour.append(y)
        tour.append(np.zeros((SR // 2, 2)))
    t = np.concatenate(tour)
    with wave.open(str(out / "tour.wav"), "wb") as w:
        w.setnchannels(2); w.setsampwidth(2); w.setframerate(SR)
        w.writeframes((t * 32767).astype("<i2").tobytes())


if __name__ == "__main__":
    main()
