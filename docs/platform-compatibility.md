# Platforms: which file goes where

erdtree runs on the original OP-1 (firmware 246) under two different firmware mods. They are not interchangeable:
each has its own plugin file **and its own preset files**.

| | Hundekraft Workshop 3.0.x | Opus 2 |
|---|---|---|
| Folder | `platforms/hundekraft/` | `platforms/opus/` |
| Download | `downloads/erdtree-hundekraft.zip` | `downloads/erdtree-opus.zip` |
| Plugin file | `erdtree.knl` (v0.5), added in the Workshop's web page, then "Update OP-1" | `erdtree.opmod` (0.6.1), copied into the disk's `mods` folder |
| Needs | Workshop 3.0.x | Opus core 2.8 or later |
| Presets | `platforms/hundekraft/presets/`: 8 sounds (first grace, golden order, ...) | `platforms/opus/presets/`: 8 different sounds (crown of ash, the hollow belfry, ...) |
| Preset identity | `"type": "erdtree"`, knob 4 = 0x5300; the Workshop core finds the engine by its type name | The OS's `iter`, knob 4 = erdtree's Opus number, an `"opus"` member naming the plugin |
| Works on the other? | No: Hundekraft presets do not load on Opus (LizardMan, Opus's author, 2026-11-01) | No: Opus presets and the `.opmod` need Opus |
| Engine API | kennel/op1r engine (`op1_engine_t`): voices are the OS's `iter` objects, the engine finds its voice and velocity from the object's address | Opus's engine service (`struct opus_engine`): its own 36-byte voice state, `init` / `note_on` / `release` / `render` |
| Render | 128 samples at 88.2 kHz a voice-block | the same |
| Velocity | applied by the engine (v0.5) | applied by the OS (the port leaves it out) |
| Envelope | the OS's T2, read for the brass timbre | the same, read from the same place (`fw_voice_envelope`) |
| Tables | in the firmware's data memory | sine and choir wave copied into L1 fast memory, warp table into the audio pool (0.6.1) |
| Screen | an OS screen from `erdtree.svg` (`erdscreen.c`) | a page drawn with Opus's page API (`erd_page.c`), redrawn only when something changes, with a `screen` setting (animated or still) |
| CPU guard | the Workshop mutes or bypasses an overloaded plugin | Opus switches a plugin's voices off when it runs 25 % over its declared figure in 8 of 64 blocks (unless developer mode is on); erdtree declares 200,000 cycles |
| On a device | v0.5 runs well: audio 38 % on the Resource monitor while playing | 0.6.0 loaded but was silent (most likely the CPU guard). 0.6.1 is the fix and has not been confirmed on a device yet |

## What is the same on both

The sound. `platforms/opus/tests/match_test.c` renders the Opus port and the Hundekraft source side by side for every
MIDI note at 125 knob corners across the envelope (590 million samples): they differ by at most 3 in 32,768.

## What is shared in the repository

Only what is truly platform-independent, and nothing that a firmware loads:

- `shared/design-notes/`: how the engine makes its sound.
- `shared/preset-specifications/`: what each preset in a collection is meant to sound like.
- Two generators that both builds use live in the Hundekraft source, which came first, and the Opus build reads
  them from there: `platforms/hundekraft/src/gen_svg.py` (the screen's drawing; the Opus page's art is generated
  from it) and `platforms/hundekraft/src/gen_erd_tables.py` (the tables; the Opus plugin folder keeps a copy of
  `erd_tables.h` because the plugin kit builds one folder, and `scripts/validate-presets/validate.py` fails if the
  copy drifts).
- The DSP code is in each platform's own `src/`, because the two firmware APIs differ. The match test is what keeps
  them from drifting apart: change one, and run it.
