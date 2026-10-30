# erdtree changelog

Oct 22, 2026 · @Sammi

## At a glance

erdtree went from v0.1 to v0.5 on 21 October 2026. v0.5 is the current build and the first that plays chords cleanly on the OP-1. erdtree is an Elden Ring-inspired orchestra engine for the original OP-1, packed as a `.knl` plugin for the Hundekraft OP-1 Workshop 3.0.x. Times below are UTC, when each build was handed over.

| Version | Released | Headline | On the OP-1 | File size |
| --- | --- | --- | --- | --- |
| v0.5 | 21 Oct, 20:56 | Chord crackle fixed: 4 KB of tables instead of 32 KB | Clean chords; audio load 38 % | 51,992 bytes |
| v0.4 | 21 Oct, 20:30 | 3 dB quieter, 17 % fewer cycles | Still crackled on chords | 64,152 bytes |
| v0.3 | 21 Oct, 20:11 | Same sound, 31 % fewer cycles | Knobs and presets worked; chords crackled | 63,556 bytes |
| v0.2 | 21 Oct, 19:44 | Eight presets, new default sound | Knobs stopped changing the sound; presets missing | 46,736 bytes |
| v0.1 | 21 Oct, 18:42 (screen redrawn 18:51) | First build: three sections, space knob, Erdtree screen | Worked | 46,736 bytes |

## v0.5 (current) · 21 Oct 2026, 20:56

v0.5 fixes the crackle on chords by reading far less memory per note, with the same sound and level as v0.4. On Sammi's OP-1 chords now play cleanly on the speaker.

**Fixed**

- Chords of two or more notes no longer crackle on the OP-1 speaker. v0.3 and v0.4 crackled even with the effect off, while Wavetable, which is louder, played the same chords cleanly.
- Most likely cause: each voice read 32 KB of tables at scattered points on every sample. That is more than the Blackfin's data cache holds, so reads waited on SDRAM and pushed the firmware's own data out of the cache. This was modelled, not measured on the device.

**Changed**

- Two small tables of erdtree's own replace the big ones: a sine (`erd_sine`) and the choir wave (`choir_wave`), 1,024 steps each plus a guard point, 4 KB together.
- Reads from them are interpolated, which is more accurate than the nearest-step lookups they replace.
- The firmware's 16 KB sine table is no longer read for audio. The slow LFOs (vibrato, breath, shimmer) use `erd_sine` too.
- The strings and the choir now run in one loop that writes the output directly. The brass loop adds to it with saturation.
- The per-voice block buffers in memory are gone. BSS is back to 224 bytes, from 1,248.
- The file is 12 KB smaller: 51,992 bytes.
- The output stays 3 dB below v0.3, as in v0.4. A build 9 dB below v0.3 was prepared but never shipped, once the Wavetable test showed loudness wasn't the cause.

**Measured**

- In a cache model of a held chord, v0.4 missed 178 to 221 cache lines per voice-block with a 16 KB cache, and 68 to 92 with 32 KB. v0.5 and Wavetable missed none.
- On the PC, every preset matches v0.4 within 0.35 dB in every octave band, with the same peak and RMS level.
- In the Blackfin simulator, v0.5 runs 22,100 cycles per voice-block, 10 % more instructions than v0.4. The simulator doesn't count memory waits, which is what v0.5 removes.
- The Blackfin build, run in the simulator, gives exactly the same samples as the PC build (hash `493b1e71`).
- On the OP-1, the Resource monitor read 38 % audio load while playing: instruments 16 %, effects 0 %, the rest of the firmware 22 %, peak marker near 60 %.

**Added**

- `harness/cache_model.py`: the cache model above.
- `harness/cycles/match.c`: the PC against Blackfin sample hash, also run by `harness/cycles/run.sh`.
- `harness/film.c` and `harness/film.py`: the 30-second demo video.
- README: the v0.5 status, the hardware result, and a troubleshooting entry for crackling chords.

## v0.4 · 21 Oct 2026, 20:30

v0.4 turned the output down 3 dB and cut the processing by 17 %, aiming at the crackle reported on v0.3. It still crackled on chords, which ruled out loudness.

**Changed**

- The output is 3 dB lower than v0.3.
- Each section (strings, brass, choir) runs its own loop over the 128-sample block and adds into a shared block buffer. The loops carry no per-sample branches. Apart from the trim, the samples match v0.3 bit for bit.
- The block buffers added 1 KB of memory (BSS 1,248 bytes). v0.5 removed them again.
- Knob values below 0, which an LFO can produce, are read as 0.

**Measured**

- In the Blackfin simulator, 17 % fewer cycles per voice than v0.3.
- On the PC, a held two-note chord on each engine's own presets peaks about where EVA and Oxbow do, and 2 to 4 dB below Supersaw and Wavetable.
- erdtree has less low bass than most Workshop engines, so the low end was ruled out as the cause.

**On the OP-1**

- The first grace preset still clipped on chords on the speaker, also with the effect off.
- The same chord on Wavetable, at the same volume, played cleanly. That pointed away from level and toward how erdtree used the chip, which led to v0.5.

**Docs**

- The Reddit post still had the wrong keys for the Resource monitor. Corrected to Shift + help, then T1, as the README since v0.3.

## v0.3 · 21 Oct 2026, 20:11

v0.3 kept the sound and cut the processing by 31 %. It brought the knobs back on the OP-1, but chords crackled on the speaker.

**Why**

- On v0.2 the knobs stopped changing the sound while the screen still showed the Erdtree. The engine code was the same as v0.1, so the most likely cause was the OP-1 switching erdtree to its fallback Iter engine for processing load. That was never confirmed on the device.

**Changed**

- The sample loop uses fast 16 x 16 multiplies. v0.2 did about 20 slow 32-bit multiplies per sample; v0.3 does one. On the Blackfin this takes one line of inline assembly, as the compiler otherwise picks the slow multiply.
- The choir reads one precomputed wave (harmonics 1 to 4) instead of four sine lookups. The shimmer harmonic keeps its own lookup, as its level moves with a slow LFO.
- The brass warp table from Brassophone is stored by column (`warp_cols`, made by the new `gen_erd_tables.py`), so a held note reads a few cache lines instead of rows 64 apart. `erd_tables.h` replaces `warp_table.h`.
- The PC tests run the new loop under the same memory and overflow checks, with the Blackfin multiplies stood in by plain C.
- The file grew to 63,556 bytes, mostly the 16 KB choir wave. v0.5 shrank it again.

**Measured**

- In the Blackfin simulator, 31 % fewer cycles per voice than v0.2.
- On the PC, all eight presets match v0.2 within 0.1 dB in every octave band, at the same level.

**Added**

- `harness/cycles`: a cycle bench that runs erdtree and Supersaw in a Blackfin simulator built from GDB 9.2.
- README: the Resource monitor keys corrected to Shift + help, then T1.

**On the OP-1**

- The knobs and the presets worked, and Sammi found the sound good.
- Chords of two or more notes crackled on the speaker, and the sound seemed too loud. v0.4 and v0.5 followed from this.

## v0.2 · 21 Oct 2026, 19:44

v0.2 added eight presets and made first grace the default sound. The engine and screen code were the same as v0.1.

**Added**

- Eight presets: first grace, golden order, malenia lament, erdtree dreams, starscourge, leyndell hall, roundtable and royal stab.
- `presets.py` writes them as OP-1 patch files, laid out like the Workshop's own presets. Each names its engine `erdtree`, so the Workshop finds it by name; without erdtree the sounds fall back to Iter.
- They only use the stock spring, delay and grid effects, and every value stays inside the ranges of the OP-1's factory presets.
- Their levels are matched within about 1 dB on a held chord (PC render).

**Changed**

- The starting knobs (blue, green, white, orange) moved from 9000, 22000, 12000, 14000 to 10000, 26000, 12000, 16000, the first grace values.

**On the OP-1**

- The knobs stopped changing the sound, while the screen still showed the Erdtree.
- The presets didn't show up.

**Follow-ups to v0.2 (19:57 to 20:01)**

- The preset pack was rebuilt as a `synth/erdtree` folder, like the Workshop's own pack, with an `INSTALL.txt`. The presets then showed up.
- README: a Troubleshooting section for the Iter fallback and for missing presets.
- README: the preset limit corrected. The OP-1 keeps 300 user synth presets and silently drops any past that; it doesn't refuse to start.

## v0.1 · 21 Oct 2026, 18:42 (screen redrawn 18:51)

v0.1 was the first build: an orchestra engine where each knob brings in its own section, with an animated Erdtree screen. It worked on the OP-1.

**Engine**

- Built on the code structure of op1r-src commit `358d7e6` (Brassophone), packed as a `.knl` file for the Hundekraft Workshop 3.0.3.
- Orange, brass: the Brassophone horn, a phase-warped sine whose timbre follows the envelope. More brass is louder and brighter, with a short lip scoop into each note.
- Green, strings: two detuned violin saws and celli an octave down. A bowed swell opens the tone, and vibrato arrives once the note settles.
- Blue, choir: harmonics 1 to 6 of a slightly sharp tone, with a slow breath and a shimmer on the top harmonic.
- White, space: a wider section detune, a darker tone, a slower bloom and a deeper choir breath. It isn't a reverb.
- Section levels follow a squared knob curve, and the mix is scaled down when their sum passes 1.5 x full scale, so all three at full don't clip.

**Screen**

- The Erdtree over the walls of Leyndell, in OP-1 line style. The 18:42 build drew it after Sammi's blood-moon screen, with a halo that rose on orange.
- Redrawn at 18:51 after Sammi's reference shot: a taller trunk of fibres, a crown that forks under a wide scalloped canopy, and ruins with arcades and towers at its foot.
- Orange makes shafts of light break through under the crown (replacing the halo), green grows and sways the crown, blue drops golden leaves, and white multiplies the stars.

**Checked**

- The Blackfin compiler was built from source. As a check, it rebuilt the Workshop's own `fmbell.knl` and `supersaw.knl` byte for byte.
- The `.knl` passed the SDK's loader, format and SVG checks.
- Every MIDI note at the knob corners ran clean under memory and overflow checks on the PC.

**On the OP-1**

- The engine worked. Sammi then asked for presets, which became v0.2.

## Beyond the engine

Alongside the builds, the project gained a preset pack, demo media and ready-to-paste posts. All files are in the project's `erdtree` folder; times are 21 Oct 2026, UTC.

| Time | File | What it is |
| --- | --- | --- |
| 21:23 | `reddit/reddit_post.md`, `reddit/discord_post.md` | Reddit post rewritten for v0.5, and a new Discord post |
| 21:16 | `video/erdtree_v05_demo.mp4` | 30-second demo of v0.5: knobs one by one, then four presets; rendered on a computer |
| 20:56 | `older/erdtree-v0.4.knl` | v0.4 kept as a fallback |
| 20:01 | `erdtree-presets.zip`, `erdtree-presets/` | Preset pack as a `synth/erdtree` folder, with `INSTALL.txt` |
| 19:44 | `erdtree_preset_tour.wav` | Each v0.2 preset as a held chord, PC render with a stand-in reverb |
| 18:57 | `reddit/reddit_post.md`, `reddit/erdtree_demo.mp4` | First Reddit post (v0.1) and its 23-second video |
| 18:51 | `erdtree_screen_preview.png` | Grid of the redrawn screen at different knob settings |
| 18:42 | `erdtree_demo_dry.wav`, `erdtree_demo_hall.wav` | v0.1 demo: strings, brass, choir, then space; dry and with a stand-in reverb |

The source (`erdtree-source.zip` and `source/`) was refreshed with every build.

## Known gaps

These are the things that have not been checked on the OP-1 yet, or were only worked out on a computer.

- **Headphones.** v0.5 has only been heard on the speaker. The -3 dB trim leaves headroom, but nobody has listened on headphones or through the line out.
- **Stock effects per preset.** Each preset picks spring, delay or grid. How well each effect suits its preset has not been judged on the device.
- **Why the v0.2 knobs went dead.** v0.3 fixed it, and the most likely cause is the loader's fallback when the engine ran over its budget, but this was never confirmed on the OP-1.
- **Why v0.3 and v0.4 crackled.** The cause, cache misses from large scattered table reads, comes from a cache model on the computer, not a measurement on the OP-1. The v0.5 fix working on the device supports it.
- **The simulator ignores memory waits.** Cycle counts from the Blackfin simulator don't include time spent waiting for SDRAM, so they understate the real cost. The resource monitor reading (38 % audio, 16 % instruments) is the real figure.
- **Older media predate v0.5.** `erdtree_demo_dry.wav`, `erdtree_demo_hall.wav` and `reddit/erdtree_demo.mp4` are v0.1, and `erdtree_preset_tour.wav` is v0.2. Use `video/erdtree_v05_demo.mp4` for anything current.
