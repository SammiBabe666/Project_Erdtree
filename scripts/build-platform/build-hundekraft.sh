#!/bin/sh
# Builds the Hundekraft Workshop plugin: scripts/build-platform/build-hundekraft.sh KENNEL TOOLCHAIN
#   KENNEL     the Workshop SDK's ext/kennel folder, from hundekraft-3.0.3-source.tar.gz (the Workshop's source/
#              folder; SHA-256 2f745e38...26995b1d7)
#   TOOLCHAIN  holds bfin-gcc/bin and bfin/bin (Blackfin GCC 13.3.0 and binutils)
# Copies platforms/hundekraft/src into the SDK as synth_sdk/plugins/erdtree (replacing an earlier copy there), packs
# it, and writes platforms/hundekraft/build/erdtree.knl and its .sha256 only.
set -e
KENNEL=${1:?the Workshop SDK ext/kennel folder}; TC=${2:?the toolchain folder}
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
SRC=$ROOT/platforms/hundekraft/src; OUT=$ROOT/platforms/hundekraft/build
rm -rf "$KENNEL/synth_sdk/plugins/erdtree"
cp -r "$SRC" "$KENNEL/synth_sdk/plugins/erdtree"
cd "$KENNEL"
python3 tools/kennel/pack.py synth_sdk/plugins/erdtree -o build/erdtree.knl --toolchain "$TC"
mkdir -p "$OUT"
cp build/erdtree.knl "$OUT/erdtree.knl"
cd "$OUT" && sha256sum erdtree.knl > erdtree.knl.sha256 && cat erdtree.knl.sha256
