#!/bin/sh
# The port's cycle bench in GDB's Blackfin simulator: run.sh PLUGIN_KIT TOOLCHAIN BFIN_SIM_RUN
#   PLUGIN_KIT  the Opus plugin kit (sdk/opus_plugin.h); TOOLCHAIN holds bin/bfin-elf-gcc; BFIN_SIM_RUN GDB's sim/bfin/run
# The engine is built with ERD_BENCH: the stock envelope comes from a buffer here instead of the OS's.
set -e
KIT=$1; TC=$2; RUN=$3; HERE=$(cd "$(dirname "$0")" && pwd); SRC=$HERE/../../src/sammi.erdtree; OUT=$(mktemp -d)
GCC=$TC/bin/bfin-elf-gcc
CF="-O2 -mcpu=bf524-any -ffreestanding -fno-builtin -nostdlib -Wall -Wextra -I$KIT/sdk -I$SRC -DERD_BENCH"
$GCC $CF -c "$SRC/erdtree.c" -o $OUT/erdtree.o
python3 -B - "$HERE/../../presets" > $OUT/bench_presets.h <<'PY'
import json, sys
from pathlib import Path
sys.path.insert(0, sys.argv[1]); from presets import PRESETS
print("static const struct { const char *name; short k[4]; } bench_presets[] = {")
for p in PRESETS:
    print('    { "%-12s", { %s } },' % (p[0][:12], ", ".join(map(str, p[3]))))
print("};")
PY
$GCC $CF -I$OUT -c "$HERE/bench.c" -o $OUT/bench.o
$GCC -mcpu=bf524-any -c "$HERE/crt0.S" -o $OUT/crt0.o
$GCC -mcpu=bf524-any -nostdlib -T "$HERE/link.ld" $OUT/crt0.o $OUT/bench.o $OUT/erdtree.o -lgcc -o $OUT/bench.elf
"$RUN" --model bf524 --memory-size 32m $OUT/bench.elf
rm -rf $OUT
