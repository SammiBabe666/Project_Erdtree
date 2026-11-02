#!/bin/sh
# Builds the Opus 2 plugin: scripts/build-platform/build-opus.sh PLUGIN_KIT TOOLCHAIN
#   PLUGIN_KIT  the Opus repository's plugin-kit folder (https://github.com/LizardMan245/opus)
#   TOOLCHAIN   the bfin-elf toolchain (bin/bfin-elf-gcc), as the kit's docs/getting-started.md builds it
# Reads platforms/opus/src/sammi.erdtree only; writes platforms/opus/build/erdtree.opmod and its .sha256 only.
set -e
KIT=${1:?the Opus plugin-kit folder}; TC=${2:?the bfin-elf toolchain folder}
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
SRC=$ROOT/platforms/opus/src/sammi.erdtree; OUT=$ROOT/platforms/opus/build
cd "$KIT"
OP1_BFIN_TOOLCHAIN=$TC python3 plugin_tool.py build "$SRC"
python3 plugin_tool.py show build/erdtree.opmod
mkdir -p "$OUT"
cp build/erdtree.opmod "$OUT/erdtree.opmod"
cd "$OUT" && sha256sum erdtree.opmod > erdtree.opmod.sha256 && cat erdtree.opmod.sha256
