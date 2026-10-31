# erdtree for Opus 2

erdtree as a plugin for [Opus 2](https://github.com/LizardMan245/opus), LizardMan's plugin manager for the original
OP-1 (firmware 246). The sound is v0.5's, the same engine as the Hundekraft `.knl`, ported to Opus 2's engine service.
It installs as one file, `erdtree.opmod`, in the OP-1's `mods` folder: no reflashing and no Workshop update.

- **Plugin:** `release/opus/erdtree.opmod` (id `sammi.erdtree`, version 0.6.0, needs Opus core 2.6 or later).
- **Source:** `opus/sammi.erdtree/`, a plugin folder for the Opus plugin kit.

**Status: built and checked on a PC only, not yet tried on an OP-1.** It builds without warnings with the kit's own
build, and the PC tests below pass. Nothing has played on a device yet.

## Controls

The same as v0.5: blue choir, green strings, white space, orange brass. The main [README](../README.md#what-the-encoders-do)
has the encoder guide, with a screenshot for each encoder and the demo video (`media/opus/`). The plugin takes no
extra keys or encoders: on T1 the four encoders turn the four knobs as on any stock engine, and the T2 envelope and T4
LFO are the OS's.

## Install

1. **Developer mode** (once, while you test it): open Opus's menu (hold synth + drum + tape + mixer), press T1 for the
   system settings, and switch developer mode on.
2. **Disk mode:** press shift + COM, then T3, and connect the OP-1. Copy `erdtree.opmod` into the disk's `mods`
   folder. Eject, then switch the OP-1 off and on.
3. **The mods list:** Opus's menu, then T2. Turn blue to erdtree's row. It should say "loaded".
4. **Play it:** in synth mode, press shift + T1 and choose `erdtree` from the engine list.

**Presets:** the v0.5 preset pack (`release/erdtree-presets.zip`) works as before. Copy its `erdtree` folder into the
disk's `synth/` folder. Those sounds carry `"type": "erdtree"`, which the plugin claims, so they load as this engine.
Sounds you saved with erdtree on op1r or the Workshop load the same way.

**If something goes wrong:** holding Stop while switching the OP-1 on starts it with no plugins. Then delete the file
from the `mods` folder in disk mode.

## What changed from the .knl

The DSP is v0.5's, line for line. The differences are in how it talks to the OS:

- **Voices:** each voice's state is the plugin's own memory (36 bytes, in the core's fast memory while it lasts). On op1r it was
  the OS's `iter` voice object. Each voice now knows its number from the core, where v0.5 worked it out from its
  memory address.
- **Pitch:** it arrives as an argument to `render`. This is the same 128 phase steps v0.5 read from the firmware.
- **Velocity:** Opus's OS multiplies every engine's output by the envelope, which already includes the velocity.
  v0.5 multiplied by the velocity a second time, so with MIDI velocity a soft note is now a little louder than it
  was. At full velocity, which is what the OP-1's own keys play, the sound is the same.
- **Brass timbre:** it still follows the stock envelope (T2). The plugin reads the OS's envelope gains for the voice
  being rendered, at the firmware address `op1fw.h` documents for them (`fw_voice_envelope`, 0xFF801D50). This
  read-only access to one firmware address, made only inside `render`, is the plugin's one deep part. The Opus API has
  no call for it.
- **Screen:** T1 is drawn with the core's page calls instead of an OS screen. The still parts (the trunk, the ground,
  the ruins) are drawn once from SVG art the plugin carries and writes to its folder, and their pixels are kept. Every
  frame, the crown, stars, light shafts and leaves are drawn over them with the core's smoothed lines. The animation
  is v0.5's. The crown, leaves and arcade windows, and the light shafts, take your encoder colours (green, blue and
  orange) from Opus's colour themes. The trunk, the ruins and the stars stay white.
  `erdtree_opus_page_preview.png` shows six knob states.
- **CPU figure:** `cpu.txt` holds an estimate of 160,000 cycles a block, about 28 % of the audio block. It is six
  voices at v0.5's simulator figure, plus a margin for cache misses. It has not been measured on an OP-1 yet. Opus
  switches off any plugin that goes 25 % over its figure, except in developer mode, so measure it before relying on it
  outside developer mode. To measure it: turn developer mode on, play six-note chords on "leyndell hall" for a while,
  then open erdtree's details in the mods list (orange push on its row). Put the "audio: worst N" figure into
  `cpu.txt` as `cycles N` and build again.

## Build

You need the Opus plugin kit (`plugin-kit/` in the opus repository) and the Blackfin compiler (GCC 13.3.0 for
`bfin-elf`). The kit's `docs/getting-started.md` explains how to build the compiler.

```sh
export OP1_BFIN_TOOLCHAIN=/path/to/toolchain        # the folder holding bin/bfin-elf-gcc
python3 plugin_tool.py build /path/to/Project_Erdtree/opus/sammi.erdtree
python3 plugin_tool.py show build/erdtree.opmod
```

The `.opmod` in `release/opus/` was built with the plugin kit at opus commit `d9b0d2e`, using GCC 13.3.0 and binutils
2.42 (the kit names 2.38).

Generated files:

- `python3 opus/gen_opus_art.py > opus/sammi.erdtree/erd_art.h` generates the still art and the crown's lines from
  `source/gen_svg.py`.
- `opus/sammi.erdtree/erd_tables.h` is a copy of `source/erd_tables.h` (`source/gen_erd_tables.py`).

## Tests (PC)

| Test | What it checks | Command |
|---|---|---|
| Sound match | Renders the port and v0.5 side by side for every MIDI note at 125 knob corners, across the envelope's attack, sustain and release (590 M samples), under ASan and UBSan. They differ by at most 3 in 32,768, from v0.5's full-velocity gain of 32766 / 32767. | `opus/test/run.sh OP1R_SRC PLUGIN_KIT` |
| Page preview | Draws the T1 page at six knob states over the still art. | see `opus/test/page_preview.c` |
| Screenshots and video | Runs the page at 30 frames a second and the engine through a stand-in for the OS's voice loop, along a knob script, and makes `media/opus/`'s screenshots and demo video. | see `opus/test/opus_video.c` and `opus_video.py` |
| Art check | `plugin_tool.py svg` on the still art: 0 errors, 0 warnings. | |

The sound match uses [op1r-src](https://github.com/pcppcp/op1r-src)'s host harness for v0.5. Nothing here runs the
Blackfin build itself: the kit's emulator isn't published yet.

## Before sharing it

Go through the kit's `docs/sharing.md` once it has run on an OP-1. Measure its CPU figure, try it with the other
plugins you use, and add a `checked device YYYY-MM-DD ...` line to `manifest.txt`.
