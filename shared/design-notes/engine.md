# How erdtree makes its sound

This is the same on every platform: both builds run the same synthesis, sample for sample (within 3 in 32,768: see
`platforms/opus/tests/match_test.c`). Only how each firmware calls it differs (`docs/platform-compatibility.md`).

erdtree is a small orchestra in one voice. Each voice renders three sections and mixes them. Four controls conduct
the mix rather than shape one oscillator.

| Control | Section | How it is made |
|---|---|---|
| Blue | Choir | Harmonics 1 to 6 of a slightly sharp tone, read from a 1,024-step wave table and interpolated. A slow "breath" tremolo and a shimmer on the top harmonic, both deeper with more space, and a one-pole low-pass that darkens with space. It rides the detuned violin's phase. |
| Green | Strings | Three saws: two violins detuned 5 to 18 cents apart (wider with space) and celli an octave down. A bow swell: the level rises from 40 % and the tone opens over the note's first 0.2 to 0.95 s (slower with space). Vibrato arrives in the second half of that swell. One-pole low-pass per voice. |
| White | Space | Not a section and not a reverb: distance. It widens the string detune, darkens the strings and the choir, slows the bow swell, deepens the choir's breath and shimmer, and slightly darkens the brass. |
| Orange | Brass | Brassophone's horn (by nick, nvmsocool): a sine read through a phase-warp table of 257 timbre columns. More brass picks brighter columns, and so does the OS's envelope as it rises: the tone follows T2. A short lip scoop (a pitch dip that lands 8 times faster than the bow swell) and vibrato late in the note. |

- The three levels use a squared curve, so the first part of each knob fades a section in gently. All three at zero
  is silence.
- When the three levels add up to more than the headroom allows, they are scaled together, so turning everything up
  never clips one voice.
- The OS does the rest around each voice: the envelope (T2), the velocity, the LFO (T4), the mix and the effect (T3).
- What erdtree does not have: noise, inharmonic partials (true bells), filters with resonance, oscillator sync or FM.
  A preset that sounds bell-like or metallic gets there with the brass's bright attack decaying through the envelope,
  not with an inharmonic source.

## What each control costs

Measured in GDB's Blackfin simulator, six voices, one 128-sample block (instructions only: it does not count waits
for memory). The OP-1's block is 580,525 cycles.

| Sections on | Cycles a block, six voices |
|---|---|
| none (levels at zero; the phases keep running) | 17,904 |
| strings only | 44,376 |
| choir only | 54,870 |
| brass only | 67,794 |
| all three | 132,300 |

A section at zero costs nothing but its phase. So a preset's CPU depends on which sections it uses, not on how loud
they are.
