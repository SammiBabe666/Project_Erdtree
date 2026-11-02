# Presets

Each firmware has its own presets. Use the ones for the firmware your OP-1 runs: the other set does not load.

## Opus 2: the dark-fantasy collection

In `platforms/opus/presets/`, one folder each. What each is meant to be, and which part of the engine carries it:
`shared/preset-specifications/dark-fantasy-collection.md`.

| Name | Choir | Strings | Space | Brass | Envelope (A, D, S, R) | Effect | Octave |
|---|---|---|---|---|---|---|---|
| crown of ash | 9000 | 20000 | 14000 | 28000 | 1800, 11000, 29000, 11000 | spring | 0 |
| the hollow belfry | 24000 | 0 | 30000 | 32767 | 64, 9000, 0, 12000 | delay | 0 |
| thornwake | 0 | 32767 | 0 | 19000 | 64, 3000, 13000, 1400 | grid | 0 |
| vesperglass | 30000 | 0 | 6000 | 0 | 3500, 14000, 22000, 13000 | delay | +1 |
| the sunken choir | 32767 | 12000 | 32767 | 0 | 8000, 16000, 32767, 15000 | spring | 0 |
| grave of giants | 0 | 12000 | 22000 | 29000 | 1500, 12000, 30000, 10000 | grid | -1 |
| moonless vigil | 9000 | 20000 | 32767 | 0 | 9000, 16000, 24000, 14000 | spring | 0 |
| the unwritten god | 32767 | 6000 | 32767 | 24000 | 12000, 16000, 30000, 16000 | delay | -1 |

The effect's four parameters are in each `sound.json`. The LFO is off in all eight.

**Status:** the values are final for now, but the `.aif` files are not written yet. They need erdtree's Opus number
and the layout of Opus's `"opus"` member, which the plugin kit does not document; one erdtree sound saved on an OP-1
with Opus shows both. Until then, set a sound up by hand from the table above.

### Measured on the PC

`platforms/opus/tests/preset_audition.py` plays each preset's test phrase (a held low C, a held C minor chord, a fast
high run, four overlapping legato notes, then the tail) on the Opus build, with a stand-in for the OS's envelope and
no effect. Renders, through a stand-in effect, are in `platforms/opus/assets/presets/`.

| Name | Peak (dBFS) | Chord (dBFS RMS) | Phrase (dBFS RMS) | Silent notes | Block-edge steps | Brightness (Hz) | Speaks in (s) |
|---|---|---|---|---|---|---|---|
| crown of ash | -3.4 | -16.2 | -18.1 | 0 | 1.01 | 2491 | 0.10 |
| the hollow belfry | -3.9 | -24.5 | -21.0 | 0 | 1.01 | 1425 | 0.01 |
| thornwake | -7.4 | -19.1 | -23.0 | 0 | 1.02 | 3364 | 0.05 |
| vesperglass | -6.3 | -18.8 | -21.2 | 0 | 0.99 | 1433 | 0.44 |
| the sunken choir | -5.0 | -17.8 | -21.3 | 0 | 1.00 | 1277 | 1.36 |
| grave of giants | -3.1 | -17.1 | -19.2 | 0 | 1.01 | 1364 | 0.08 |
| moonless vigil | -9.7 | -22.2 | -26.0 | 0 | 1.01 | 2137 | 1.76 |
| the unwritten god | -3.6 | -19.3 | -22.1 | 0 | 1.01 | 632 | 2.85 |

- **Level:** the sustained sounds sit within 5 dB of each other on the phrase (-18 to -23). Moonless vigil is about
  4 dB under the rest on purpose. The hollow belfry and thornwake have no or little sustain, so their chord figure is
  lower while their peaks match the others.
- **Clicks:** a step at the engine's block edges no bigger than elsewhere (1.0) means no zipper or click from
  per-block control changes.
- **Silence:** every test note sounds in every preset.
- These are numbers, not listening. The renders were not judged by ear here, and the OP-1's own envelope curves and
  effects differ from the stand-ins.

### CPU of each preset

GDB's Blackfin simulator, six voices on one block (instructions only), from `platforms/opus/tests/cycles`:

| Name | Cycles a block | Share of the 580,525-cycle block |
|---|---|---|
| crown of ash | 131,238 | 23 % |
| the hollow belfry | 105,822 | 18 % |
| thornwake | 94,266 | 16 % |
| vesperglass | 54,870 | 9 % |
| the sunken choir | 81,348 | 14 % |
| grave of giants | 94,266 | 16 % |
| moonless vigil | 81,348 | 14 % |
| the unwritten god | 132,300 | 23 % |

The plugin declares 200,000 (its `cpu.txt`), above the heaviest preset with room for memory waits.

### Hearing the differences

Play the same thing on each, in this order, and the eight pull apart:

1. **One low C, held for four seconds.** Grave of giants and the unwritten god are an octave down: one heavy and
   quick, the other dark and swelling for seconds. Vesperglass is an octave up and glassy.
2. **A C minor chord, held.** Crown of ash (bright, full orchestra, quick) against the sunken choir (dark, breathing,
   slow) against moonless vigil (soft, slowest of the sustained ones to arrive).
3. **Short staccato notes up high.** Thornwake snaps and stops; the hollow belfry strikes and rings on; the slow
   presets barely speak, which is the point.
4. **Overlapping legato notes in the middle.** The choir's breath (sunken choir, vesperglass) against the bow swell
   and brass scoop (crown of ash, grave of giants).

`platforms/opus/assets/presets/tour.mp3` is that phrase on all eight in order.

### Testing on the OP-1

Not done yet. When the `.aif` files exist, for each preset:

- [ ] It shows in the browser under `erdtree` and loads as erdtree (the T1 page shows the tree, not iter).
- [ ] Its knobs, envelope, effect and octave are the table's.
- [ ] Low, middle and high notes all sound; no clicks at note on or off; chords of six notes play cleanly.
- [ ] Switching between the eight quickly, while a note holds, does not glitch or go silent.
- [ ] The mods list still says "loaded" after a minute of six-note chords, with developer mode off, and its
      "audio: worst N" stays under 200,000.
- [ ] Levels feel even, with moonless vigil quieter on purpose.

## Hundekraft Workshop

In `platforms/hundekraft/presets/`, made by `presets.py` there: first grace, golden order, malenia lament, erdtree
dreams, starscourge, leyndell hall, roundtable, royal stab. Their values and characters are in
`platforms/hundekraft/README.md`. They load on the Workshop and on op1r, not on Opus.
