# Overview

erdtree is a synth engine for the original OP-1 (firmware 246) that plays like a small orchestra: a choir, a string
section and a brass horn in every voice, with a fourth control for distance. It comes with an animated tree on its
T1 page, each encoder moving its own part of the picture.

It was written first for op1r and the Hundekraft Workshop (v0.5, `platforms/hundekraft/`) and then ported to Opus 2
(0.6.1, `platforms/opus/`). Both builds make the same sound; each has its own plugin file and its own presets.

- How the sound is made: `shared/design-notes/engine.md`
- Which file goes on which firmware, and what differs: `docs/platform-compatibility.md`
- The presets, measured: `docs/preset-guide.md`
- Building, testing and the repository's rules: `docs/development.md`
- The Opus port and the eight Opus presets, what was done and what is still open: `docs/port-report.md`
- History: `docs/changelog.md`
