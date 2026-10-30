#!/usr/bin/env python3
"""Data-cache model: how many cache lines erdtree's table reads miss per voice-block, v0.4 against v0.5, with
Wavetable (the Workshop's engine, clean on chords) for comparison.

The OP-1's Blackfin keeps recently read SDRAM in a small L1 data cache (here: one 16 KB bank or two, 2-way set
associative, 32-byte lines, LRU). Up to v0.4 erdtree read the firmware's 16 KB sine table twice per sample (brass,
choir shimmer) and its own 16 KB choir wave once, each at a point that moves by tens to hundreds of bytes per
sample. v0.5 reads two 2 KB tables (erd_sine, choir_wave, interpolated). The warp table is read one 130-byte column
at a time in both. The model replays the table addresses of a held chord (one voice per note, C major from C4) and
counts misses after a warm-up. It leaves out the firmware's own memory traffic, which only adds misses, and the
time a miss takes on the device (an SDRAM line fill, tens of cycles), so read it as a comparison.

    python3 harness/cache_model.py
Copyright (C) 2026 Sammi (Project ERD). GPL-3.0-or-later with the OP-1 firmware permission (see LICENSE and NOTICE).
"""
import os
import sys
import numpy as np

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
import gen_erd_tables as G

FW_SINE = 0x0101E5E8                      # OP1_SINE_TABLE, 8192 x int16
WARP = 0x02990000                         # the plugin's .rodata in the kennel arena: warp_cols, then the waves
V04_CHOIR = WARP + 257 * 65 * 2           # v0.4: choir_wave[8192]
V05_SINE = WARP + 257 * 65 * 2            # v0.5: erd_sine[1025], choir_wave[1025]
V05_CHOIR = V05_SINE + 1025 * 2
V04_BSS = (V04_CHOIR + 16384 + 31) & ~31  # v0.4 block buffers mix[128], choir phase[128]
WT = 0x029A0000                           # Wavetable's wt_waves[8][257]
CHORD = [261.63, 329.63, 392.0, 523.25, 659.25, 783.99]
COLS = [[G.q15(G.warp(s / 63, t * G.MAX_TIMBRE / G.COLUMNS)) for s in range(64)] + [32767] for t in range(257)]


class Cache:
    def __init__(self, banks):
        self.banks, self.sets, self.miss = banks, {}, 0

    def read(self, a):
        line = a >> 5
        ways = self.sets.setdefault((line & 255, (a >> 14) & 1 if self.banks == 2 else 0), [])
        if line in ways:
            ways.remove(line)
        else:
            self.miss += 1
            if len(ways) == 2:
                ways.pop()
        ways.insert(0, line)


def erdtree(version, f, st):
    """the table addresses one voice reads in one block, in program order (brass timbre column 150)"""
    inc = int(f * 2 ** 32 / 44100)
    n = np.arange(1, 129, dtype=np.int64)
    p2 = (st['p2'] + n * int(inc * 1.006)) & 0xFFFFFFFF   # detuned violin phase (the choir's)
    pb = (st['pb'] + n * inc) & 0xFFFFFFFF                # brass phase
    st['p2'], st['pb'] = int(p2[-1]), int(pb[-1])
    col = WARP + 150 * 130
    r = (pb >> 26).astype(int)
    a = np.array([COLS[150][k] for k in r]); b = np.array([COLS[150][k + 1] for k in r])
    wph = a + (((b - a) * ((pb & 0x3FFFFFF) >> 11)) >> 15)  # warped phase, 0..32767
    h6 = (p2 * 6) & 0xFFFFFFFF
    out = []
    if version == 'v0.4':   # strings loop (stores), brass loop, choir loop; block buffers in .bss
        mix, chp = V04_BSS, V04_BSS + 512
        for k in range(128):
            out += [chp + 4 * k, mix + 4 * k]
        for k in range(128):
            out += [col + 2 * r[k], col + 2 * r[k] + 2, FW_SINE + 2 * (wph[k] >> 2), mix + 4 * k]
        for k in range(128):
            out += [chp + 4 * k, V04_CHOIR + 2 * (p2[k] >> 19), FW_SINE + 2 * (h6[k] >> 19), mix + 4 * k]
    else:                   # strings and choir loop, then brass loop; interpolated reads of the small tables
        for k in range(128):
            c, s = p2[k] >> 22, h6[k] >> 22
            out += [V05_CHOIR + 2 * c, V05_CHOIR + 2 * c + 2, V05_SINE + 2 * s, V05_SINE + 2 * s + 2]
        for k in range(128):
            s = wph[k] >> 5
            out += [col + 2 * r[k], col + 2 * r[k] + 2, V05_SINE + 2 * s, V05_SINE + 2 * s + 2]
    return out


def wavetable(f, st):
    inc = int(f * 2 ** 32 / 44100)
    n = np.arange(1, 129, dtype=np.int64)
    out, wa = [], WT + 3 * 514                            # saw -> square
    p1 = (st['p1'] + n * int(inc * 0.997)) & 0xFFFFFFFF
    p2 = (st['p2'] + n * int(inc * 1.003)) & 0xFFFFFFFF
    st['p1'], st['p2'] = int(p1[-1]), int(p2[-1])
    for k in range(128):
        for p in (p1[k], p2[k]):
            i = p >> 24
            out += [wa + 2 * i, wa + 2 * i + 2, wa + 514 + 2 * i, wa + 516 + 2 * i]
    return out


def misses(engine, voices, banks, blocks=40, warm=10):
    c = Cache(banks)
    st = [{'p2': 0x55555555 * (v + 1) & 0xFFFFFFFF, 'pb': 0, 'p1': 0} for v in range(voices)]
    for blk in range(blocks):
        if blk == warm:
            c.miss = 0
        for v in range(voices):
            for a in (wavetable(CHORD[v], st[v]) if engine == 'wavetable' else erdtree(engine, CHORD[v], st[v])):
                c.read(a)
    return c.miss / ((blocks - warm) * voices)


if __name__ == '__main__':
    for banks in (1, 2):
        print(f'{16 * banks} KB data cache: missed lines per voice-block (1 / 2 / 4 / 6 voices)')
        for engine in ('v0.4', 'v0.5', 'wavetable'):
            name = 'wavetable' if engine == 'wavetable' else 'erdtree ' + engine
            print(f'  {name:12s}' + ''.join(f'{misses(engine, nv, banks):8.1f}' for nv in (1, 2, 4, 6)))
