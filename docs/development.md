# Development

## Layout

```text
Project_Erdtree/
├── README.md                  the platform index
├── LICENSE, NOTICE
├── downloads/                 what users download: one zip per platform, made by scripts/build-platform/package.sh
├── docs/                      overview, platform compatibility, preset guide, this page, the changelog, the port
│                              report, the announcement drafts
├── shared/
│   ├── design-notes/          how the engine makes its sound (platform-independent)
│   └── preset-specifications/ what each collection's sounds are meant to be (no preset files)
├── platforms/
│   ├── hundekraft/            the Hundekraft Workshop build
│   │   ├── README.md, INSTALL.txt
│   │   ├── src/               the engine (op1r/kennel API), its screen, the table and art generators
│   │   ├── presets/           presets.py and the eight Hundekraft .aif patches it writes
│   │   ├── tests/             PC tests (op1r-src's harness) and the Blackfin cycle bench
│   │   ├── assets/            demo audio and video, the screen preview
│   │   └── build/             erdtree.knl and its SHA-256 (older/: v0.4)
│   └── opus/                  the Opus 2 build
│       ├── README.md, INSTALL.txt
│       ├── src/               sammi.erdtree/ (the plugin folder the kit builds) and the art generator
│       ├── presets/           presets.py, and one folder per preset: sound.json (+ its .aif once written)
│       ├── tests/             match test, preset audition and stress, page preview, video, cycle bench
│       ├── assets/            screenshots, demo video, the presets' renders
│       └── build/             erdtree.opmod and its SHA-256
└── scripts/
    ├── validate-presets/      validate.py: checks both platforms' presets
    └── build-platform/        build-opus.sh, build-hundekraft.sh, package.sh
```

## Rules

- **A platform's files stay in its folder.** A preset file, a plugin build or an install note for one firmware never
  goes in another's folder or zip. `package.sh` takes each zip from its own platform's folder only, and
  `validate.py` fails if a Hundekraft patch turns up among the Opus presets.
- **One source for each value.** An Opus preset's values are its folder's `sound.json`; its `.aif` is written from
  it. A Hundekraft preset's values are in `platforms/hundekraft/presets/presets.py`; its `.aif` is written from it.
  `validate.py` checks that every `.aif` is exactly what its writer makes.
- **Names:** folders lower case with hyphens (`hollow-belfry`); preset names as the OP-1 shows them, lower case
  letters and spaces (`the hollow belfry`), at most 23 characters for Opus (14 for the Hundekraft set, the longest
  the Workshop's own presets use).
- **Built files** (`build/`, `downloads/`) are committed on purpose: they are what users install, and the repository
  is where they download them. Everything else generated (WAV renders, `__pycache__`, test binaries) stays out.
- **Change the engine on one platform, then on the other, then run the match test** (`platforms/opus/tests/run.sh`).

## Build each platform on its own

Each build reads only its own `src/` and writes only its own `build/`.

```sh
# Opus 2: needs the Opus repository's plugin-kit/ and the bfin-elf toolchain (the kit's docs/getting-started.md)
scripts/build-platform/build-opus.sh /path/to/opus/plugin-kit /path/to/bfin-toolchain

# Hundekraft Workshop: needs the Workshop SDK (hundekraft-3.0.3-source.tar.gz, its ext/kennel) and the toolchain
scripts/build-platform/build-hundekraft.sh /path/to/hundekraft/ext/kennel /path/to/toolchain

# the downloads (validates the presets first)
scripts/build-platform/package.sh opus
scripts/build-platform/package.sh hundekraft
```

## Validate and test each platform on its own

```sh
python3 scripts/validate-presets/validate.py opus
python3 scripts/validate-presets/validate.py hundekraft

# Opus (from platforms/opus/)
python3 presets/presets.py check
tests/run.sh /path/to/op1r-src /path/to/opus/plugin-kit          # sound match against the Hundekraft engine
cc -O1 -fsanitize=address,undefined -fno-sanitize-recover=undefined -DERD_HOST -I/path/to/opus/plugin-kit/sdk \
   -Isrc/sammi.erdtree tests/preset_audition.c src/sammi.erdtree/erdtree.c -lm -o preset_audition
./preset_audition stress 1                                        # also 2, 3 ...
python3 tests/preset_audition.py ./preset_audition /tmp/renders   # needs numpy and scipy
tests/cycles/run.sh /path/to/opus/plugin-kit /path/to/bfin-toolchain /path/to/gdb/sim/bfin/run

# Hundekraft (from platforms/hundekraft/tests/)
cc -fsanitize=address,undefined -fno-sanitize-recover=undefined -DOP1_HOST -I/path/to/op1r-src/synth_sdk \
   -I../src range_test.c ../src/erdtree.c -lm -o range_test && ./range_test
python3 cache_model.py
```

## Adding things

- **A preset to the Opus collection:** make a folder under `platforms/opus/presets/` with a `sound.json` like the
  others, add its folder name to `ORDER` in `presets.py`, run `validate.py`, render it with `preset_audition.py`, and
  describe it in `shared/preset-specifications/`.
- **A new collection:** a new specification page in `shared/preset-specifications/`, then its folders under each
  platform's `presets/` (a subfolder per collection once there is more than one).
- **A new platform:** `platforms/<name>/` with the same five folders, its own `INSTALL.txt`, a case in
  `package.sh`, a section in `validate.py`, a row in `docs/platform-compatibility.md` and the root README.
