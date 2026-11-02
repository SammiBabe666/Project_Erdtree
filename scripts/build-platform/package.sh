#!/bin/sh
# Makes the downloads from the built files: scripts/build-platform/package.sh [opus|hundekraft]   (both by default)
#   downloads/erdtree-opus.zip        platforms/opus/build/erdtree.opmod, platforms/opus/presets/*/*.aif, its INSTALL
#   downloads/erdtree-hundekraft.zip  platforms/hundekraft/build/erdtree.knl, platforms/hundekraft/presets/*.aif, its
#                                     INSTALL
# Each zip takes its own platform's files only. It checks the presets first (scripts/validate-presets). The files'
# dates are set to one fixed time, so the same files give the same zip.
set -e
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
WHICH=${1:-opus hundekraft}
STAMP='2026-11-01 00:00'
for P in $WHICH; do
    python3 -B "$ROOT/scripts/validate-presets/validate.py" "$P"
    T=$(mktemp -d); D=$T/erdtree
    mkdir -p "$D/presets/synth/erdtree"
    case $P in
    opus)
        mkdir -p "$D/Opus 2"; cp "$ROOT/platforms/opus/build/erdtree.opmod" "$D/Opus 2/"
        for f in "$ROOT"/platforms/opus/presets/*/*.aif; do [ -e "$f" ] && cp "$f" "$D/presets/synth/erdtree/"; done ;;
    hundekraft)
        mkdir -p "$D/Hundekraft Workshop"; cp "$ROOT/platforms/hundekraft/build/erdtree.knl" "$D/Hundekraft Workshop/"
        cp "$ROOT"/platforms/hundekraft/presets/*.aif "$D/presets/synth/erdtree/" ;;
    *) echo "opus or hundekraft"; exit 2 ;;
    esac
    [ -n "$(ls "$D/presets/synth/erdtree")" ] || rm -r "$D/presets"
    cp "$ROOT/platforms/$P/INSTALL.txt" "$ROOT/LICENSE" "$ROOT/NOTICE" "$D/"
    find "$D" -exec touch -h -d "$STAMP" {} +
    mkdir -p "$ROOT/downloads"; rm -f "$ROOT/downloads/erdtree-$P.zip"
    (cd "$T" && zip -q -r -X "$ROOT/downloads/erdtree-$P.zip" erdtree)
    rm -rf "$T"
    unzip -l "$ROOT/downloads/erdtree-$P.zip"
done
