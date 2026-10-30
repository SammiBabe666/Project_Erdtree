#!/usr/bin/env python3
"""ERDTREE sound presets as OP-1 patch files: python3 presets.py [OUT_DIR] -> OUT_DIR/*.aif
(default: presets/ next to this script)

An OP-1 synth patch is an AIFC file (16-bit mono, 44.1 kHz, 'sowt') with an APPL chunk holding "op-1" + JSON and an
empty SSND chunk (the firmware renders its own preview when it syncs). Saved sounds find erdtree by its type name
("type": "erdtree", docs/preset-identity.md); knobs[4] holds 0x5300 (OP1_ENGINE_MAGIC alone): a file plugin has no
fixed number, kennel rewrites the selector from the type name when erdtree is loaded, and without erdtree the sound
falls back to the stock iter. Same file layout as the Workshop's own presets (e.g. plugins/mbira/presets.py).
Every value stays inside the ranges the factory presets use: values outside them make the firmware import the file
as a plain sample (ranges measured in the Workshop source, tools/op1r_presets.py).
Only stock effects (spring, delay, grid), so the sounds need no other plugin.
Install: copy the presets folder to the OP-1's synth/ folder in disk mode (Shift + COM, T3), as synth/erdtree/.
Copyright (C) 2026 Sammi (Project ERD). GPL-3.0-or-later with the OP-1 firmware permission (see LICENSE and NOTICE).
"""
import json
import math
import struct
import sys
from pathlib import Path

RANGE_ADSR = [(64, 16320), (64, 16320), (0, 32767), (64, 16320)]
RANGE_FX = {
    "delay": [(3392, 11200), (4969, 32767), (0, 16384), (1332, 32767)],
    "spring": [(1344, 16448), (9920, 16448), (0, 16384), (2047, 26120)],
    "grid": [(1344, 12824), (1344, 13752), (13392, 28165), (6854, 32767)],
}
SELECTOR = 0x5300                 # OP1_ENGINE_MAGIC: no fixed number (see above)
NAME_MAX = 14                     # longest name in the Workshop's 152 presets; lowercase letters and spaces only

# name: knobs (CHOIR blue, STRINGS green, SPACE white, BRASS orange), ADSR (attack, decay, sustain, release),
# FX (type, 4 params), octave
PRESETS = [
    ("first grace", [10000, 26000, 12000, 16000], [2500, 12000, 30000, 12000],
     ("spring", [14000, 14000, 2500, 10000]), 0),
    ("golden order", [8000, 20000, 9000, 30000], [600, 10000, 30000, 9000],
     ("spring", [16448, 15000, 2000, 10239]), 0),
    ("malenia lament", [0, 26000, 26000, 0], [7000, 14000, 30000, 15000],
     ("spring", [16448, 16000, 3000, 12000]), 0),
    ("erdtree dreams", [30000, 22000, 30000, 4000], [9000, 16000, 32767, 16000],
     ("delay", [10000, 26000, 6000, 20000]), 0),
    ("starscourge", [0, 10000, 6000, 32767], [1200, 12000, 30000, 10000],
     ("grid", [6000, 8000, 20000, 18000]), -1),
    ("leyndell hall", [18000, 24000, 18000, 20000], [3000, 12000, 30000, 14000],
     ("spring", [16448, 16448, 14000, 24000]), 0),
    ("roundtable", [32767, 6000, 8000, 0], [3000, 12000, 32767, 11000],
     ("spring", [14000, 13000, 3000, 9000]), 0),
    ("royal stab", [0, 8000, 4000, 30000], [64, 3800, 0, 2600],
     ("delay", [8000, 14000, 8000, 12000]), 0),
]


def ieee_extended(x):
    exp = int(math.floor(math.log2(x)))
    return struct.pack(">HQ", exp + 16383, int(x / 2 ** exp * 2 ** 63))


def chunk(cid, body):
    return cid + struct.pack(">I", len(body)) + body + (b"\0" if len(body) & 1 else b"")


def aifc(meta):
    comm = struct.pack(">hIh", 1, 0, 16) + ieee_extended(44100) + b"sowt" + b")Signed integer (little-endian) linear PCM"
    js = json.dumps(meta, separators=(",", ":"), sort_keys=True).encode() + b"\n"
    if len(js) & 1:
        js += b" "
    body = (chunk(b"FVER", bytes.fromhex("a2805140")) + chunk(b"COMM", comm) + chunk(b"APPL", b"op-1" + js)
            + chunk(b"SSND", struct.pack(">II", 0, 0)))
    return b"FORM" + struct.pack(">I", len(body) + 4) + b"AIFC" + body


def check(name, what, values, ranges):
    for i, (v, (lo, hi)) in enumerate(zip(values, ranges)):
        if not lo <= v <= hi:
            raise ValueError(f"{name}: {what}[{i}] = {v} is outside {lo}..{hi}")


def meta(name, knobs, adsr, fx, octave):
    if not (0 < len(name) <= NAME_MAX and set(name) <= set("abcdefghijklmnopqrstuvwxyz ")):
        raise ValueError(f"{name!r}: 1..{NAME_MAX} lowercase letters and spaces")
    check(name, "knobs", knobs, [(0, 32767)] * 4)
    if not (knobs[0] or knobs[1] or knobs[3]):
        raise ValueError(f"{name}: choir, strings and brass all at 0 is silent")
    check(name, "adsr", adsr, RANGE_ADSR)
    check(name, "fx " + fx[0], fx[1], RANGE_FX[fx[0]])
    if not -1 <= octave <= 1:
        raise ValueError(f"{name}: octave {octave}")
    return {
        "adsr": list(adsr) + [2048, 5568, 4000, 4000],
        "fx_active": True,
        "fx_params": list(fx[1]) + [0, 0, 0, 0],
        "fx_type": fx[0],
        "knobs": list(knobs) + [SELECTOR, 0, 0, 0],
        "lfo_active": False,
        "lfo_params": [16000, 0, 0, 16000, 0, 0, 0, 0],
        "lfo_type": "tremolo",
        "name": name,
        "octave": octave,
        "synth_version": 2,
        "type": "erdtree",
    }


def main():
    if len(sys.argv) > 2:
        sys.exit(__doc__)
    out = Path(sys.argv[1]) if len(sys.argv) == 2 else Path(__file__).resolve().parent / "presets"
    out.mkdir(parents=True, exist_ok=True)
    for p in PRESETS:
        (out / (p[0] + ".aif")).write_bytes(aifc(meta(*p)))
    print(f"{len(PRESETS)} presets in {out}")


if __name__ == "__main__":
    main()
