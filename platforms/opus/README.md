# erdtree for Opus 2

erdtree as a plugin for [Opus 2](https://github.com/LizardMan245/opus), LizardMan's plugin manager for the original
OP-1 (firmware 246). The sound is v0.5's, the same engine as the Hundekraft `.knl`, ported to Opus 2's engine service.
It installs as one file, `erdtree.opmod`, in the OP-1's `mods` folder: no reflashing and no Workshop update.

- **Plugin:** `build/erdtree.opmod` (id `sammi.erdtree`, version 0.6.1, needs Opus core 2.8 or later).
- **Source:** `src/sammi.erdtree/`, a plugin folder for the Opus plugin kit.
- **Presets:** `presets/`, eight sounds made for Opus, one folder each (see [Presets](#presets)).
- **Install steps for users:** `INSTALL.txt` (it goes into `downloads/erdtree-opus.zip`).

Every path below is relative to this folder, `platforms/opus/`, unless it starts with `platforms/` or `scripts/`.

**Status: 0.6.0 loaded on an OP-1 but made no sound and loaded the CPU heavily (developer mode off). 0.6.1 is the fix
for that, and it has not been tried on a device yet.** It builds without warnings with the kit's own build, and the PC
tests below pass.

### 0.6.1: why 0.6.0 was silent, and what changed

0.6.0 most likely went over its CPU figure. Opus then switches a plugin's voices off for the rest of that start, unless
developer mode is on. Three things made it too heavy:

- **Its tables were in slow memory.** Every voice read the sine and the choir wave (4 KB) at scattered points every
  sample, from ordinary memory. The kit says each 32-byte line of it costs about 107 cycles a block, and twice that
  while the screen is being sent. With six voices that could add more than the whole figure allowed. 0.6.1 copies
  them into fast memory (L1) at the start, and the 33 KB brass table into the audio pool, which the display doesn't
  share.
- **Its screen redrew everything every frame:** about 550 lines, 28 times a second, in the display thread. 0.6.1
  draws the picture about 10 times a second and has the core put the kept frame back in between (`opus_page_still`).
  The new setting **screen** (mods list, blue push on erdtree's row) can be set to `still`: then the picture is drawn
  only when a knob moves.
- **Its CPU figure was tight.** 160,000 cycles left little room for six voices at 132,000 instructions. 0.6.1 states
  200,000 until it is measured.

Also new: an empty `release` handler (the OS's envelope lets the voice go, as before), and the brass's phase skips its
per-sample loop while orange is at zero. The sound is the same: the PC match test against v0.5 gives the same result
as 0.6.0.

### If it is still silent

Switch developer mode on (Opus's menu, T1) and play again. If it sounds then, it is still over its CPU figure: open
its details (orange push on its row in the mods list) and send the "audio: worst N" figure.

## Controls

The same as v0.5: blue choir, green strings, white space, orange brass. The main [README](../README.md#what-the-encoders-do)
has the encoder guide, with a screenshot for each encoder and the demo video (`assets/`). The plugin takes no
extra keys or encoders: on T1 the four encoders turn the four knobs as on any stock engine, and the T2 envelope and T4
LFO are the OS's.

## Install

1. **Developer mode** (once, while you test it): open Opus's menu (hold synth + drum + tape + mixer), press T1 for the
   system settings, and switch developer mode on.
2. **Disk mode:** press shift + COM, then T3, and connect the OP-1. Copy `erdtree.opmod` into the disk's `mods`
   folder. Eject, then switch the OP-1 off and on.
3. **The mods list:** Opus's menu, then T2. Turn blue to erdtree's row. It should say "loaded".
4. **Play it:** in synth mode, press shift + T1 and choose `erdtree` from the engine list.

**Presets:** use the Opus presets in `downloads/erdtree-opus.zip`, not the Hundekraft ones. The Hundekraft presets
carry `"type": "erdtree"`; the plugin claims that type, but on Opus they do not load (LizardMan, 2026-11-01).

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
  the ruins) are drawn once from SVG art the plugin carries and writes to its folder, and their pixels are kept. About
  10 times a second, the crown, stars, light shafts and leaves are drawn over them with the core's smoothed lines, and
  the core puts that frame back in between. The animation
  is v0.5's. The crown, leaves and arcade windows, and the light shafts, take your encoder colours (green, blue and
  orange) from Opus's colour themes. The trunk, the ruins and the stars stay white.
  `erdtree_opus_page_preview.png` shows six knob states.
- **Memory:** the sine and the choir wave are in fast memory (L1) and the brass table in the audio pool (0.6.1).
- **CPU figure:** `cpu.txt` holds an estimate of 200,000 cycles a block, about 34 % of the audio block. Six voices
  with all three sections run 132,300 instructions a block in GDB's Blackfin simulator (`tests/cycles`). It has not been measured on an OP-1 yet. Opus
  switches off any plugin that goes 25 % over its figure, except in developer mode, so measure it before relying on it
  outside developer mode. To measure it: turn developer mode on, play six-note chords on "leyndell hall" for a while,
  then open erdtree's details in the mods list (orange push on its row). Put the "audio: worst N" figure into
  `cpu.txt` as `cycles N` and build again.

## Build

You need the Opus plugin kit (`plugin-kit/` in the opus repository) and the Blackfin compiler (GCC 13.3.0 for
`bfin-elf`). The kit's `docs/getting-started.md` explains how to build the compiler.

```sh
export OP1_BFIN_TOOLCHAIN=/path/to/toolchain        # the folder holding bin/bfin-elf-gcc
python3 plugin_tool.py build /path/to/Project_Erdtree/platforms/opus/src/sammi.erdtree
python3 plugin_tool.py show build/erdtree.opmod
```

`scripts/build-platform/build-opus.sh PLUGIN_KIT TOOLCHAIN` does the same and copies the result into `build/`,
with its SHA-256. The `.opmod` in `build/` was built with the plugin kit at opus commit `d9b0d2e`, using GCC 13.3.0 and binutils
2.42 (the kit names 2.38).

Generated files:

- `python3 src/gen_opus_art.py > src/sammi.erdtree/erd_art.h` generates the still art and the crown's lines from
  `platforms/hundekraft/src/gen_svg.py` (the screen's one drawing, used by both builds).
- `src/sammi.erdtree/erd_tables.h` is a copy of `platforms/hundekraft/src/erd_tables.h`
  (`platforms/hundekraft/src/gen_erd_tables.py`). The plugin kit builds one folder, so the copy stays;
  `scripts/validate-presets/validate.py` fails if the two ever differ.

## Tests (PC)

| Test | What it checks | Command |
|---|---|---|
| Sound match | Renders the port and v0.5 side by side for every MIDI note at 125 knob corners, across the envelope's attack, sustain and release (590 M samples), under ASan and UBSan. They differ by at most 3 in 32,768, from v0.5's full-velocity gain of 32766 / 32767. | `tests/run.sh OP1R_SRC PLUGIN_KIT` |
| Page preview | Draws the T1 page at six knob states over the still art. | see `tests/page_preview.c` |
| Screenshots and video | Runs the page at 30 frames a second and the engine through a stand-in for the OS's voice loop, along a knob script, and makes `assets/`'s screenshots and demo video. | see `tests/opus_video.c` and `opus_video.py` |
| Cycle bench | Six voices, 20 blocks, at six knob settings, in GDB's Blackfin simulator (instructions only, no memory waits): 132,300 cycles a block with all sections, 17,900 with all three levels at zero. | `tests/cycles/run.sh PLUGIN_KIT TOOLCHAIN BFIN_SIM_RUN` |
| Art check | `plugin_tool.py svg` on the still art: 0 errors, 0 warnings. | |
| Preset audition | The renders in `assets/presets/` (MP3). Plays each preset's test phrase (a low held note, a held minor chord, a fast high run, overlapping legato notes, the tail) through a stand-in effect, writes a WAV each and a tour, and measures the peak, the chord's and the phrase's level, silent notes, clicks at block edges, brightness and how fast it speaks. | build `tests/preset_audition.c` (its header), then `python3 tests/preset_audition.py ./preset_audition OUT_DIR` |
| Stress | Knobs jumped to random values and the extremes every block, presets switched, notes started, let go and stolen at random across every MIDI note, voices rebuilt from garbage memory mid-note, envelopes changed: 400,000 blocks a seed under ASan and UBSan. | `./preset_audition stress SEED` |
| Presets | Every value in range, one folder per preset, no Hundekraft patch among them, the files what `presets.py` writes. | `python3 scripts/validate-presets/validate.py opus` (from the repository's top) |

The sound match uses [op1r-src](https://github.com/pcppcp/op1r-src)'s host harness for v0.5. Nothing here runs the
Blackfin build itself: the kit's emulator isn't published yet.

## Presets

Eight sounds made for Opus, one folder each in `presets/`. A folder's `sound.json` is the one source of its values;
`presets/presets.py` reads them, checks them and writes each folder's `.aif`. Details, the measured levels and how to
hear the differences: `docs/preset-guide.md` at the repository's top.

**The `.aif` files are not written yet.** Opus saves an engine's sound as the OS's `iter` with the engine's number in
knob 4 and an `"opus"` member naming the plugin. The kit does not document that number or the member's layout, so
`presets.py` waits for them (`OPUS_FORMAT`), read from one erdtree sound saved on an OP-1 with Opus. Then
`python3 presets/presets.py aif` writes all eight.

## Before sharing it

Go through the kit's `docs/sharing.md` once it has run on an OP-1. Measure its CPU figure, try it with the other
plugins you use, and add a `checked device YYYY-MM-DD ...` line to `manifest.txt`.
