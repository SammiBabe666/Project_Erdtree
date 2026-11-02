#!/usr/bin/env python3
"""erdtree's Opus presets: the eight sounds of the dark-fantasy collection, as data, and the writer of their files.

    python3 presets.py check           checks every value (scripts/validate-presets runs this and more)
    python3 presets.py aif             writes each preset folder's .aif (needs OPUS_FORMAT below to be filled in)

These are Opus presets only. The Hundekraft Workshop presets live in platforms/hundekraft/presets/ and are a
different file: they name the engine by "type": "erdtree", and on Opus they do not load (LizardMan, Opus's author,
Opus Discord, 2026-11-01).

What each value is (all of them erdtree's real controls; nothing here is a feature the engine lacks):
    knobs   blue choir, green strings, white space, orange brass, 0-32767 (the engine's T1 page)
    adsr    the OS's T2 envelope: attack, decay, sustain, release. erdtree's brass timbre follows it too.
    fx      the OS's synth effect: spring, delay or grid, four parameters, inside the factory presets' ranges
    octave  the OS's octave, -1 to 1
The OS's LFO stays off: its parameters' factory ranges are not known for any type but the tremolo default, and a
value outside them makes the firmware import the file as a plain sample.

Copyright (C) 2026 Sammi (Project ERD). GPL-3.0-or-later with the OP-1 firmware permission (see LICENSE and NOTICE).
"""
import json
import math
import struct
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent

# The factory presets' ranges (measured in the Hundekraft Workshop source, tools/op1r_presets.py): the OS imports a
# file with a value outside them as a plain sample.
RANGE_KNOB = (0, 32767)
RANGE_ADSR = [(64, 16320), (64, 16320), (0, 32767), (64, 16320)]
RANGE_FX = {
    "delay": [(3392, 11200), (4969, 32767), (0, 16384), (1332, 32767)],
    "spring": [(1344, 16448), (9920, 16448), (0, 16384), (2047, 26120)],
    "grid": [(1344, 12824), (1344, 13752), (13392, 28165), (6854, 32767)],
}
RANGE_OCTAVE = (-1, 1)
NAME_MAX = 23          # the OS keeps a sound's name up to 23 characters (plugin kit, docs/api.md, Engines)

# How Opus saves an erdtree sound. Not in the plugin kit's documentation: read from a sound saved on an OP-1 with
# Opus and erdtree 0.6.1. Until then the .aif files are not written (presets.py aif refuses).
OPUS_FORMAT = None
# OPUS_FORMAT = {"type": "iter", "mark": 0x0000, "opus": {...}}

# The collection's order (the tour, the docs). Each preset is its own folder holding sound.json, the one source of
# its values, and, once written, its .aif. A folder not named here is an error (scripts/validate-presets says so).
ORDER = ["crown-of-ash", "hollow-belfry", "thornwake", "vesperglass", "sunken-choir", "grave-of-giants",
         "moonless-vigil", "unwritten-god"]


def load(slug):
    """A preset folder's sound.json as (slug, name, what, knobs, adsr, (fx type, params), octave)."""
    j = json.loads((HERE / slug / "sound.json").read_text())
    k = j["knobs"]
    return (slug, j["name"], j["description"], [k["choir"], k["strings"], k["space"], k["brass"]], j["adsr"],
            (j["fx"]["type"], j["fx"]["params"]), j["octave"])


PRESETS = [load(s) for s in ORDER]


def check(p):
    """The problems with one preset, as text (none: an empty list)."""
    slug, name, what, knobs, adsr, fx, octave = p
    bad = []
    if not (0 < len(name) <= NAME_MAX and set(name) <= set("abcdefghijklmnopqrstuvwxyz ")):
        bad.append(f"name {name!r}: 1 to {NAME_MAX} lowercase letters and spaces")
    if not (slug and set(slug) <= set("abcdefghijklmnopqrstuvwxyz-")):
        bad.append(f"folder {slug!r}: lowercase letters and hyphens")
    if len(knobs) != 4 or any(not RANGE_KNOB[0] <= k <= RANGE_KNOB[1] for k in knobs):
        bad.append(f"knobs {knobs}: four values, 0-32767")
    elif not (knobs[0] or knobs[1] or knobs[3]):
        bad.append("choir, strings and brass all at 0 is silent")
    if len(adsr) != 4 or any(not lo <= v <= hi for v, (lo, hi) in zip(adsr, RANGE_ADSR)):
        bad.append(f"adsr {adsr}: outside {RANGE_ADSR}")
    if fx[0] not in RANGE_FX:
        bad.append(f"fx {fx[0]!r}: spring, delay or grid")
    elif len(fx[1]) != 4 or any(not lo <= v <= hi for v, (lo, hi) in zip(fx[1], RANGE_FX[fx[0]])):
        bad.append(f"fx {fx[0]} {fx[1]}: outside {RANGE_FX[fx[0]]}")
    if not RANGE_OCTAVE[0] <= octave <= RANGE_OCTAVE[1]:
        bad.append(f"octave {octave}")
    return [f"{slug}: {b}" for b in bad]


def sound(p):
    """The sound as the OS keeps it, but for what OPUS_FORMAT says (the type, knob 4, the "opus" member)."""
    slug, name, what, knobs, adsr, fx, octave = p
    s = {
        "adsr": list(adsr) + [2048, 5568, 4000, 4000],
        "fx_active": True,
        "fx_params": list(fx[1]) + [0, 0, 0, 0],
        "fx_type": fx[0],
        "knobs": list(knobs) + [0, 0, 0, 0],
        "lfo_active": False,
        "lfo_params": [16000, 0, 0, 16000, 0, 0, 0, 0],
        "lfo_type": "tremolo",
        "name": name,
        "octave": octave,
        "synth_version": 2,
    }
    if OPUS_FORMAT:
        s["type"] = OPUS_FORMAT["type"]
        s["knobs"][4] = OPUS_FORMAT["mark"]
        if "opus" in OPUS_FORMAT:
            s["opus"] = OPUS_FORMAT["opus"]
    return s


def ieee_extended(x):
    exp = int(math.floor(math.log2(x)))
    return struct.pack(">HQ", exp + 16383, int(x / 2 ** exp * 2 ** 63))


def chunk(cid, body):
    return cid + struct.pack(">I", len(body)) + body + (b"\0" if len(body) & 1 else b"")


def aifc(meta):
    """An OP-1 synth patch: AIFC (16-bit mono, 44.1 kHz, 'sowt'), an APPL chunk of "op-1" + JSON, an empty SSND."""
    comm = struct.pack(">hIh", 1, 0, 16) + ieee_extended(44100) + b"sowt" + b")Signed integer (little-endian) linear PCM"
    js = json.dumps(meta, separators=(",", ":"), sort_keys=True).encode() + b"\n"
    if len(js) & 1:
        js += b" "
    body = (chunk(b"FVER", bytes.fromhex("a2805140")) + chunk(b"COMM", comm) + chunk(b"APPL", b"op-1" + js)
            + chunk(b"SSND", struct.pack(">II", 0, 0)))
    return b"FORM" + struct.pack(">I", len(body) + 4) + b"AIFC" + body


def main():
    what = sys.argv[1] if len(sys.argv) == 2 else ""
    bad = [b for p in PRESETS for b in check(p)]
    if len({p[0] for p in PRESETS}) != len(PRESETS) or len({p[1] for p in PRESETS}) != len(PRESETS):
        bad.append("two presets share a folder or a name")
    if bad or what not in ("check", "aif"):
        sys.exit("\n".join(bad) if bad else __doc__)
    if what == "check":
        print(f"{len(PRESETS)} presets ok")
    else:
        if not OPUS_FORMAT:
            sys.exit("OPUS_FORMAT is not filled in yet: it comes from a sound saved on an OP-1 with Opus")
        for p in PRESETS:
            d = HERE / p[0]
            d.mkdir(exist_ok=True)
            (d / (p[1] + ".aif")).write_bytes(aifc(sound(p)))
        print(f"{len(PRESETS)} presets written")


if __name__ == "__main__":
    main()
