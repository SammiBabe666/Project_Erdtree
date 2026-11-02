#!/usr/bin/env python3
"""Checks every preset in the repository, each against its own firmware's rules, and that the two sets never mix.

    python3 scripts/validate-presets/validate.py            both platforms
    python3 scripts/validate-presets/validate.py opus       one of them (opus or hundekraft)

Opus (platforms/opus/presets/):
  - every folder is in presets.py's ORDER and holds a sound.json, and every ORDER entry has its folder;
  - every value is inside the factory ranges (presets.py's check);
  - an .aif in a folder is an OP-1 patch whose sound is that folder's sound.json, written by presets.py (the same
    bytes), and never a Hundekraft one ("type": "erdtree");
  - once presets.py's OPUS_FORMAT is filled in, every folder has its .aif.
Hundekraft (platforms/hundekraft/presets/):
  - every .aif is an OP-1 patch of "type": "erdtree" with its values in the factory ranges, and the same bytes
    presets.py writes.
Both: the Opus build's erd_tables.h is the Hundekraft source's (the tables are generated once, by
platforms/hundekraft/src/gen_erd_tables.py).
Exit status 0 when all is well; each problem is printed.

Copyright (C) 2026 Sammi (Project ERD). GPL-3.0-or-later with the OP-1 firmware permission (see LICENSE and NOTICE).
"""
import importlib.util
import json
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OPUS = ROOT / "platforms" / "opus"
HK = ROOT / "platforms" / "hundekraft"


def module(path, name):
    spec = importlib.util.spec_from_file_location(name, path)
    m = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(m)
    return m


def patch(path):
    """An OP-1 patch file's JSON, or raises ValueError."""
    b = path.read_bytes()
    if b[:4] != b"FORM" or b[8:12] != b"AIFC":
        raise ValueError("not an AIFC file")
    i = 12
    while i + 8 <= len(b):
        cid, n = b[i:i + 4], struct.unpack(">I", b[i + 4:i + 8])[0]
        if cid == b"APPL":
            body = b[i + 8:i + 8 + n]
            if body[:4] != b"op-1":
                raise ValueError("APPL chunk is not op-1's")
            return json.loads(body[4:].decode())
        i += 8 + n + (n & 1)
    raise ValueError("no op-1 APPL chunk")


def in_ranges(m, j):
    bad = []
    if any(not 0 <= k <= 32767 for k in j["knobs"][:4]):
        bad.append(f"knobs {j['knobs'][:4]}")
    if any(not lo <= v <= hi for v, (lo, hi) in zip(j["adsr"][:4], m.RANGE_ADSR)):
        bad.append(f"adsr {j['adsr'][:4]}")
    r = m.RANGE_FX.get(j["fx_type"])
    if r is None or any(not lo <= v <= hi for v, (lo, hi) in zip(j["fx_params"][:4], r)):
        bad.append(f"fx {j['fx_type']} {j['fx_params'][:4]}")
    if j.get("lfo_active"):
        bad.append("the LFO is on (its ranges are not known)")
    return bad


def opus():
    bad = []
    m = module(OPUS / "presets" / "presets.py", "opus_presets")
    folders = sorted(p.name for p in (OPUS / "presets").iterdir() if p.is_dir() and not p.name.startswith("__"))
    for f in folders:
        if f not in m.ORDER:
            bad.append(f"opus: folder {f} is not in presets.py's ORDER")
    for p in m.PRESETS:
        bad += ["opus: " + b for b in m.check(p)]
        aifs = sorted((OPUS / "presets" / p[0]).glob("*.aif"))
        if m.OPUS_FORMAT and not aifs:
            bad.append(f"opus: {p[0]} has no .aif (python3 platforms/opus/presets/presets.py aif)")
        for a in aifs:
            try:
                j = patch(a)
            except ValueError as e:
                bad.append(f"opus: {a.name}: {e}")
                continue
            if j.get("type") == "erdtree":
                bad.append(f"opus: {a.name} is a Hundekraft patch (\"type\": \"erdtree\")")
            if a.name != p[1] + ".aif":
                bad.append(f"opus: {p[0]}/{a.name} should be named {p[1]}.aif")
            bad += [f"opus: {a.name}: {b}" for b in in_ranges(m, j)]
            if m.OPUS_FORMAT and a.read_bytes() != m.aifc(m.sound(p)):
                bad.append(f"opus: {a.name} is not what presets.py writes from {p[0]}/sound.json")
    print(f"opus: {len(m.PRESETS)} presets, {sum(1 for _ in (OPUS / 'presets').glob('*/*.aif'))} .aif files"
          + ("" if m.OPUS_FORMAT else " (OPUS_FORMAT not filled in yet: no .aif files expected)"))
    return bad


def hundekraft():
    bad = []
    m = module(HK / "presets" / "presets.py", "hk_presets")
    aifs = sorted((HK / "presets").glob("*.aif"))
    for a in aifs:
        try:
            j = patch(a)
        except ValueError as e:
            bad.append(f"hundekraft: {a.name}: {e}")
            continue
        if j.get("type") != "erdtree":
            bad.append(f"hundekraft: {a.name} is not an erdtree patch (type {j.get('type')!r})")
        bad += [f"hundekraft: {a.name}: {b}" for b in in_ranges(m, j)]
    for p in m.PRESETS:
        have = HK / "presets" / (p[0] + ".aif")
        if not have.exists() or have.read_bytes() != m.aifc(m.meta(*p)):
            bad.append(f"hundekraft: {p[0]}.aif is not what presets.py writes")
    if len(aifs) != len(m.PRESETS):
        bad.append(f"hundekraft: {len(aifs)} .aif files for {len(m.PRESETS)} presets in presets.py")
    print(f"hundekraft: {len(m.PRESETS)} presets, {len(aifs)} .aif files")
    return bad


def shared():
    a, b = OPUS / "src" / "sammi.erdtree" / "erd_tables.h", HK / "src" / "erd_tables.h"
    return [] if a.read_bytes() == b.read_bytes() else ["the Opus build's erd_tables.h differs from the Hundekraft source's"]


def main():
    which = sys.argv[1:] or ["opus", "hundekraft"]
    bad = []
    for w in which:
        if w not in ("opus", "hundekraft"):
            sys.exit(__doc__)
        bad += opus() if w == "opus" else hundekraft()
    bad += shared()
    for b in bad:
        print("  " + b)
    print("ok" if not bad else f"{len(bad)} problems")
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
