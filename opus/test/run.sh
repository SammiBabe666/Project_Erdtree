#!/bin/sh
# The port's PC test: opus/test/run.sh OP1R_SRC PLUGIN_KIT (op1r-src for v0.5's host harness, the Opus plugin kit)
set -e
cd "$(dirname "$0")/../.."
OP1R=${1:?op1r-src folder}; KIT=${2:?Opus plugin-kit folder}
OUT=${TMPDIR:-/tmp}/erd_match_test
cc -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=undefined -DOP1_HOST -DERD_HOST \
   -I"$OP1R/synth_sdk" -I"$KIT/sdk" -Iopus/sammi.erdtree \
   -c source/erdtree.c -o "$OUT.v05.o"
cc -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=undefined -DERD_HOST \
   -I"$KIT/sdk" -Iopus/sammi.erdtree -c opus/sammi.erdtree/erdtree.c -o "$OUT.port.o"
cc -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=undefined -DOP1_HOST \
   -I"$OP1R/synth_sdk" -c opus/test/match_test.c -o "$OUT.main.o"
cc -fsanitize=address,undefined "$OUT.main.o" "$OUT.v05.o" "$OUT.port.o" -lm -o "$OUT"
"$OUT"
