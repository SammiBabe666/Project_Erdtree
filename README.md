# Project_Erdtree
A custom Elden Ring engine for the OP-1

**erdtree** is an Elden Ring-inspired synth engine for the original OP-1: choir (blue), strings (green), space
(white) and brass (orange), with an animated Erdtree screen. Full details, controls, presets and build steps are in
[source/README.md](source/README.md).

## Layout

| Folder | What |
|---|---|
| `source/` | The engine's C source, generated tables and art, preset generator, host test harness |
| `release/` | The built `erdtree.knl` (v0.5, Hundekraft Workshop 3.0.x) with its SHA-256, the preset pack, the changelog; `older/` has v0.4 |
| `media/` | Demo audio, preset tour, demo videos and the screen preview |
| `social/` | Reddit and Discord announcement drafts |

`source/erdtree.svg` is the output of `python3 source/gen_svg.py`.
