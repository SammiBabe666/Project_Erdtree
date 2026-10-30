# erdtree (Project ERD v0.5)

An Elden Ring-inspired synth engine for the original OP-1, packed as a kennel plugin (`erdtree.knl`) for the
Hundekraft OP-1 Workshop 3.0.x. It plays like a small orchestra under the Erdtree. Each knob brings in its own
section, and each section is generated separately, so you conduct the mix rather than brighten one oscillator.

## Controls

| Knob | Section | What it does |
|---|---|---|
| Blue | CHOIR | Spectral voices: harmonics 1 to 6 of a slightly sharp tone, with a slow breath and a shimmer on the top harmonics. |
| Green | STRINGS | A section of three saws: two detuned violins and celli an octave down. The bow swells in and opens the tone, and vibrato arrives once the note settles. |
| White | SPACE | Distance: wider section detune, a darker tone, a slower bloom on the strings and a deeper choir breath. |
| Orange | BRASS | The Brassophone horn (phase-warped sine). More brass is louder and brighter, and each note gets a short lip scoop. |

- The three section levels use a squared curve, so the first part of each knob fades a section in gently.
- Turning blue, green and orange all to zero gives silence: no section is playing.
- The levels are summed with headroom. All three at full stay below clipping: a single voice peaked at -6.5 dBFS
  across every note and knob corner in the host test.
- SPACE is not a reverb. A synth engine renders each voice separately, and the stock envelope cuts each voice at
  release, so a real hall tail has to come from an effect. For the cathedral sound, put a reverb on the synth FX
  slot (op1r's reverb, or a stock one).
- The stock T2 envelope still shapes the volume. The brass timbre follows it too, as in Brassophone.
- Defaults (blue, green, white, orange): 10000, 26000, 12000, 16000, the same as the "first grace" preset. That is
  mostly strings, some brass, a touch of choir, and a little space.

## Screen

The screen is a simplified Erdtree over the ruins of Leyndell, drawn in the OP-1 line style after the in-game view.
It has a tall trunk of fibres, a crown that fans out into fine branches under a wide scalloped canopy, and broken
walls, arcades and towers at its foot. The lines are thin blue, green, white and red on black, with a star field.
Each knob animates its own part of the drawing:

- **Orange (brass):** shafts of light break through under the crown, more and longer with more brass.
- **Green (strings):** the crown grows from 86 % to full size and sways.
- **Blue (choir):** golden leaves fall from the crown, from none up to 16.
- **White (space):** stars multiply and twinkle.

See `erdtree_screen_preview.png` for the knob-state grid.

## Presets

Eight sounds in `presets/`, made by `presets.py`. Knobs are listed as choir, strings, space, brass.

| Preset | Knobs | Character | FX |
|---|---|---|---|
| first grace | 10000, 26000, 12000, 16000 | The default: warm strings, some brass, a touch of choir | spring |
| golden order | 8000, 20000, 9000, 30000 | Fanfare: brass in front of the strings, quick attack | spring |
| malenia lament | 0, 26000, 26000, 0 | Strings alone, far away and dark, slow swell | spring |
| erdtree dreams | 30000, 22000, 30000, 4000 | Choir and strings pad, very slow swell, long tail | delay |
| starscourge | 0, 10000, 6000, 32767 | Low brass, an octave down | grid |
| leyndell hall | 18000, 24000, 18000, 20000 | The full orchestra in a big room | spring |
| roundtable | 32767, 6000, 8000, 0 | The choir, close up | spring |
| royal stab | 0, 8000, 4000, 30000 | Short brass stabs | delay |

- They use stock effects only, so they need no other plugin.
- Every value is inside the ranges the OP-1's factory presets use. The firmware imports a patch with values outside
  them as a plain sample.
- `knobs[4]` holds `0x5300`. The Workshop core finds erdtree by the type name `"erdtree"`, as it does for its own file
  engines. Without erdtree installed, the sounds fall back to iter.
- The levels are matched within about 1 dB on a held chord (host render).

## Install

1. Open the [Hundekraft Workshop](https://hundekraft.github.io/op1-workshop/) with an OP-1 that already runs
   Workshop 3.0.
2. Add `erdtree.knl` under **Your plugins**, then press **Update OP-1**.
3. Read the review, send, and press T1 on the OP-1.
4. After the restart, open synth, press Shift + T1, and choose `erdtree`.

To add the presets:

1. Check how many user synth presets the OP-1 already holds. Workshop 3.0 allows 300 in total, and when the sync
   goes over that it silently drops the presets past the limit. These add eight.
2. Put the OP-1 in disk mode: Shift + COM, then T3.
3. Unzip `erdtree-presets.zip`. Inside is a `synth` folder holding the `erdtree` folder, the same layout as the
   Workshop's preset pack. Copy that `erdtree` folder into the OP-1 disk's `synth/` folder, so the files end up
   directly in `synth/erdtree/`, for example `synth/erdtree/first grace.aif`. Then eject.
4. The OP-1 restarts by itself, syncs the new files and waits for a key press.
5. To find them: in synth mode, hold Shift and press a sound key (1 to 8). Turn blue to the `erdtree` folder and green
   to the preset.

A patch whose file name already exists on the OP-1 may keep its old sound after the sync. Rename the file to replace
it.

## Troubleshooting

- **The knobs don't change the sound, or the screen shows the Iter lab (four gauges and bubbles) instead of the
  Erdtree.** erdtree isn't running, so the OP-1 plays its fallback Iter engine. This happens when the plugin was
  skipped at startup (the OP-1 says "plugin files not loaded") or bypassed for audio load (it says "erdtree muted: too
  slow" or "erdtree bypassed: audio overload"). Restart the OP-1. The Resource monitor's plugin page (blue) shows the
  loaded, failed and skipped counts. If the bypass comes back, play fewer notes at once and report which preset and
  effect it happened with.
- **Chords crackle.** Check the Resource monitor (Shift + help, then T1) while you hold the chord, and compare it with
  the same chord on Wavetable. A much higher audio load on erdtree means it is still too heavy for the OP-1; report
  the reading and the preset.
- **The presets don't show up.** Check that the files are in `synth/erdtree/`, one folder deep, and that the OP-1
  wasn't already near its 300-preset limit.

## Build

You need the matching Workshop SDK: `hundekraft-3.0.3-source.tar.gz` from the Workshop's `source/` folder, with
SHA-256 `2f745e38…26995b1d7`. You also need Blackfin GCC 13.3.0 and binutils. Copy this folder to
`ext/kennel/synth_sdk/plugins/erdtree/` and run:

```sh
python3 tools/kennel/pack.py synth_sdk/plugins/erdtree -o build/erdtree.knl --toolchain /path/to/toolchain
```

The toolchain directory holds `bfin-gcc/bin` and `bfin/bin`. Built that way, the toolchain used here reproduces the
Workshop's own `fmbell.knl` and `supersaw.knl` byte for byte.

Two other scripts regenerate generated files:

- `python3 gen_svg.py > erdtree.svg` regenerates the screen art.
- `python3 gen_erd_tables.py > erd_tables.h` regenerates the brass warp table (Brassophone's, stored by column), the
  sine and the choir wave. `gen_warp_table.py` is Brassophone's original generator, kept for reference.
- `python3 presets.py` regenerates `presets/*.aif`.

## Tests

These run on a PC, using [op1r-src](https://github.com/pcppcp/op1r-src)'s host harness, with this folder linked as
`synth_sdk/plugins/erdtree`:

| Test | Command |
|---|---|
| Phrase render with sanitizers | `python3 harness/audio.py erdtree --knobs 0,32767,0,32767` |
| Every MIDI note × knob corners under ASan/UBSan, vtable word untouched | `cc -fsanitize=address,undefined -fno-sanitize-recover=undefined -DOP1_HOST -I<op1r-src>/synth_sdk -I.. harness/range_test.c erdtree.c -lm` |
| Four-voice conducted demo (strings in, brass in, choir in, space up) | `harness/demo.c` (same flags) |
| Screen preview grid | `python3 harness/preview.py <op1r-src>/synth_sdk out.png` |
| Preset tour: each preset as a held chord, rough ADSR, stand-in reverb | `harness/audition.c` (same flags), then `python3 harness/audition.py ./audition out.wav` |
| Cycles per voice-block in the Blackfin simulator, against supersaw, and the target build's sample hash | `harness/cycles/run.sh <kennel>/synth_sdk <toolchain> <gdb>/sim/bfin/run` |
| The PC build's sample hash (must equal the target's) | `cc -DOP1_HOST -I<op1r-src>/synth_sdk -I. harness/cycles/match.c erdtree.c` |
| Data-cache misses of the table reads, v0.4 against v0.5 and Wavetable | `python3 harness/cache_model.py` |
| The 30-second demo film (knobs one by one, then four presets) | `harness/film.c` (same flags, plus `erdscreen.c`; `./a.out audio.raw > frames.jsonl`), then `python3 harness/film.py <op1r-src>/synth_sdk frames.jsonl audio.raw erdtree.svg out.mp4` |
| The preset tour film (each preset playing a phrase written for it, with its knobs and what it is) | `harness/tour.c` (same flags, plus `erdscreen.c`), then `python3 harness/tour.py <op1r-src>/synth_sdk ./a.out erdtree.svg out.mp4` |

## Status of v0.5

- **Hardware:**
  - v0.1 ran on an OP-1 through the Workshop.
  - On v0.2 the knobs stopped changing the sound while the screen still showed the Erdtree. The engine code was
    identical to v0.1, so the OP-1 most likely switched erdtree to its fallback Iter engine for processing load (see
    Troubleshooting).
  - v0.3 and v0.4 played on the OP-1 with working knobs and presets, but chords of two or more notes crackled on the
    speaker, also with the effect off. Wavetable, which is louder, played the same chords cleanly. So the level was
    not the cause.
  - v0.5 runs well on the OP-1: no crackle on chords. The Resource monitor read audio 38 % while playing (erdtree's
    instruments 16 %, effects 0 %, the rest of the firmware 22 %), with its peak marker near 60 %.
- **Since v0.4:** v0.5 reads far less memory per sample. The sound and the level are the same.
  - Up to v0.4 each voice read three tables at scattered points every sample: the firmware's 16 KB sine table twice
    (brass, choir shimmer) and its own 16 KB choir wave once. 32 KB read that way doesn't fit the Blackfin's data
    cache, so many of those reads have to wait for SDRAM, and they push the firmware's own data out of the cache
    too. v0.5 no longer reads them, and its chords play cleanly on the device. The cache model
    (`harness/cache_model.py`) counts the missed cache lines per voice-block on a held chord:

    | Data cache | v0.4 | v0.5 | Wavetable |
    |---|---|---|---|
    | 16 KB | 178 to 221 | 0 | 0 |
    | 32 KB | 68 to 92 | 0 | 0 |

  - v0.5 reads two 2 KB tables of its own (a sine and the choir wave, 1024 steps each, interpolated), plus one
    130-byte column of the brass warp table as before. It no longer stores block buffers in memory: the strings and
    the choir run in one loop that writes the output, and the brass adds to it.
  - The interpolated tables are more accurate than the nearest-step lookups they replace. On a held chord, every
    preset matches v0.4 within 0.35 dB in every octave band, with the same peak and RMS level.
  - The Blackfin build gives exactly the same samples as the PC build the tests use (the hash in `harness/cycles`).
- **Earlier changes:**
  - v0.4 lowered the output by 3 dB and split the sections into separate loops.
  - v0.3 replaced about 20 slow 32-bit multiplies per sample with fast 16 x 16 ones and stored the warp table by
    column.
- **Processing, in the Blackfin instruction simulator** (built from GDB 9.2), per voice with all three sections and
  per 128-sample block:

  | Version | Cycles per voice | Six voices, share of the block |
  |---|---|---|
  | v0.2 | 35,200 | 18 % |
  | v0.3 | 24,200 | 12 % |
  | v0.4 | 20,100 | 10 % |
  | v0.5 | 22,100 | 11 % |
  | supersaw, for comparison | 10,900 | 6 % |

  The simulator counts instructions only. It doesn't count the time a cache miss waits for SDRAM, which is what v0.5
  removes, so on the device v0.5 should cost less than v0.4 even though it runs 10 % more instructions.
- **Level:** with each engine's own presets on a held two-note chord (host render), erdtree peaks about where EVA and
  Oxbow do, and 2 to 4 dB below supersaw and wavetable. With six voices it peaks lower than all of them.
- **Checked off the device:** the `.knl` passes the Workshop SDK's reference loader, format tests and SVG checks.
  Every MIDI note at the knob corners runs clean under ASan/UBSan. The preset files have the same chunk layout as the
  Workshop's own presets, and their values are inside the factory ranges.
- **Not yet checked on the device:**
  - Chords on headphones (v0.5 was checked on the speaker).
  - How the stock spring, delay and grid suit each sound.

## Credits and license

- **Brass voice and phase-warp table:** from Brassophone by nick (nvmsocool), op1r-src commit `358d7e6`. Since v0.3
  erdtree stores the same table by column (`gen_erd_tables.py`).
- **Plugin API, host harness and kennel packer:** from op1r (pcppcp) and Hundekraft.
- **Engine, screen and artwork:** Project ERD.

GPL-3.0-or-later with the OP-1 firmware permission (see `LICENSE` and `NOTICE`). Elden Ring and the Erdtree belong
to FromSoftware and Bandai Namco. This is an unofficial fan tribute.
