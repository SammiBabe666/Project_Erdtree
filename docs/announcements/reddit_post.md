# Reddit post: erdtree v0.5

**Where:** r/op1. Make it a video post with `video/erdtree_v05_demo.mp4` (30 seconds), then paste the body below in
Markdown mode. Add `erdtree_screen_preview.png` (all the knob states) in the first comment. A short version for
r/Eldenring is at the bottom. `reddit/erdtree_demo.mp4` is the old v0.1 video: don't use it.

**Before posting:**

- Replace the three `LINK` placeholders in the body with wherever you host `erdtree.knl`, `erdtree-presets.zip` and
  the source (`erdtree-source.zip`). In the r/Eldenring version, `LINK` is the link to your r/op1 post.
- If you can, film 10 seconds of erdtree on your own OP-1 with your phone and add it. The demo video is rendered on a
  computer, and people trust a clip from the real device more.

---

## Title (pick one)

1. erdtree: an Elden Ring orchestra synth engine for the original OP-1 (Workshop .knl, 8 presets, free)
2. I turned the Erdtree into an OP-1 synth engine. Brass comes in on orange, strings on green
3. erdtree v0.5: a "conduct the orchestra" synth for the Hundekraft OP-1 Workshop, with an animated Erdtree screen

---

## Body

**erdtree** is a new synth engine for the original OP-1, packed as a `.knl` plugin for the
[Hundekraft OP-1 Workshop](https://hundekraft.github.io/op1-workshop/) (3.0.x). It comes with eight presets.

The idea is a small orchestra living under the Erdtree. The four knobs aren't four timbre controls. Each of the
three colored knobs brings in its own section, generated separately, so you conduct the mix while you play. Hold a
chord and start with lonely strings. Swell the brass in over them, then let the choir arrive.

### The knobs

| Knob | Section | What it does |
|---|---|---|
| **Orange** | BRASS | The Brassophone horn. More brass is louder *and* brighter, with a little lip scoop into each note. |
| **Green** | STRINGS | Two detuned violins and celli an octave down. A bowed swell opens the tone, and vibrato arrives once the note settles. |
| **Blue** | CHOIR | Spectral "aah" voices with a slow breath and a shimmer on top. |
| **White** | SPACE | Distance: a wider section, a darker tone and a slower bloom. |

The section levels mix with headroom, so all three at full don't clip. SPACE isn't a reverb. For the full cathedral,
put a reverb on the synth FX.

### The screen

The screen is a simplified Erdtree over the ruins of Leyndell, in the classic OP-1 line style. Every knob animates
part of it:

- **Orange:** shafts of light break through under the crown.
- **Green:** the crown grows and sways.
- **Blue:** golden leaves fall.
- **White:** the stars multiply and twinkle.

### The presets

| Preset | Sound | FX |
|---|---|---|
| first grace | The default: warm strings, some brass, a touch of choir | spring |
| golden order | Fanfare: brass in front of the strings, quick attack | spring |
| malenia lament | Strings alone, far away and dark, slow swell | spring |
| erdtree dreams | Choir and strings pad, very slow swell, long tail | delay |
| starscourge | Low brass, an octave down | grid |
| leyndell hall | The full orchestra in a big room | spring |
| roundtable | The choir, close up | spring |
| royal stab | Short brass stabs | delay |

They only use the OP-1's stock effects, so they don't need any other plugin.

### Status

This is v0.5, built with the Workshop 3.0.3 SDK and its packer.

- **Tested on an OP-1:** it loads through the Workshop, the knobs and the screen work, the presets show up, and
  chords play cleanly on the speaker. While playing, the resource monitor read 38 % audio load, with the synth voices
  at 16 %.
- Earlier test builds crackled on chords. v0.5 fixed that by reading far less memory per note, with the same sound.
- **Checked on a PC:** every note at the knob extremes runs with memory and overflow checks, and the OP-1 build, run
  in a Blackfin simulator, gives exactly the same samples as the PC build.
- **Not tested yet:** headphones, and how the stock effects suit each preset. Tell me what you hear.

The video is rendered from the plugin's own code on a computer, with reverb added afterwards. It isn't a recording
from the OP-1.

### Install

1. Open the Workshop with an OP-1 already running Workshop 3.0.
2. Under **Your plugins**, add `erdtree.knl`, then press **Update OP-1**.
3. Send it, then press T1 on the OP-1.
4. After the restart, open synth, press Shift + T1, and choose **erdtree**.

The Workshop warns that plugins run as firmware code. Only install files from people you trust, and keep the
original file.

**Presets:**

1. Put the OP-1 in disk mode: Shift + COM, then T3.
2. Unzip `erdtree-presets.zip` and copy the `erdtree` folder from its `synth` folder into the OP-1's `synth/`
   folder, so the files end up in `synth/erdtree/`. Then eject.
3. In synth mode, hold Shift and press a sound key. Turn blue to the `erdtree` folder and green to a preset.

The OP-1 keeps 300 user synth presets in total and silently drops any over that, so check you have room for eight.

**Download:** [erdtree.knl](LINK) · [presets](LINK)
**Source:** [GPL-3.0](LINK). It includes the screen generator, the preset maker and the PC test harness.

### Credits

- The brass voice and its phase-warp table come from **Brassophone** by nvmsocool, in pcppcp's
  [op1r-src](https://github.com/pcppcp/op1r-src).
- **op1r** is by pcppcp.
- The **Workshop** and its kennel SDK are by Hundekraft.

Elden Ring and the Erdtree belong to FromSoftware / Bandai Namco. This is an unofficial fan project.

I'd love feedback on how the strings and choir sit together, and on which presets you'd add. If you make sounds with
it, post them!

---

## Short version for r/Eldenring

**Title:** I turned the Erdtree into a synthesizer engine for the Teenage Engineering OP-1

**Body:**

A fan-made synth engine for the original OP-1. Each knob brings in a section of a little orchestra: brass, strings
and a choir, plus a "space" knob. The screen is the Erdtree over Leyndell in the OP-1's line style. Brass makes
light shafts break through the crown, strings sway the branches, the choir drops golden leaves, and space makes the
stars twinkle. It comes with presets like golden order, malenia lament and roundtable.

The video is rendered from the plugin code on a computer. It runs on the real OP-1 too. Details and download are in
r/op1: LINK
