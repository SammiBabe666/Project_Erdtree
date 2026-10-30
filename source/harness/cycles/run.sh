#!/bin/sh
# Build and run the cycle bench and the host match check: run.sh KENNEL_SYNTH_SDK TOOLCHAIN_DIR BFIN_SIM_RUN
#   KENNEL_SYNTH_SDK  ext/kennel/synth_sdk of the Workshop SDK (for op1_plugin.h and plugins/supersaw)
#   TOOLCHAIN_DIR     holds bfin-gcc/bin and bfin/bin
#   BFIN_SIM_RUN      GDB's sim/bfin/run (configure --target=bfin-elf --disable-gdb; make all-sim)
set -e
SDK=$1; TC=$2; RUN=$3; HERE=$(cd "$(dirname "$0")" && pwd); OUT=$(mktemp -d)
GCC=$TC/bfin-gcc/bin/bfin-elf-gcc; export PATH=$TC/bfin/bin:$PATH
CF="-O2 -mcpu=bf524 -ffreestanding -fno-builtin -nostdlib -fno-common -Wall -I$SDK -I$HERE/../.."
$GCC $CF -c "$HERE/../../erdtree.c" -o $OUT/erdtree.o
$GCC $CF -c "$SDK/plugins/supersaw/supersaw.c" -o $OUT/supersaw.o
$GCC $CF -c "$HERE/bench.c" -o $OUT/bench.o
$GCC -mcpu=bf524 -c "$HERE/crt0.S" -o $OUT/crt0.o
$GCC -mcpu=bf524 -nostdlib -T "$HERE/link.ld" $OUT/crt0.o $OUT/bench.o $OUT/erdtree.o $OUT/supersaw.o \
    "$($GCC -mcpu=bf524 -print-libgcc-file-name)" -o $OUT/bench.elf
"$RUN" --model bf524 --memory-size 32m --memory-region 0xff800000,0x8000 $OUT/bench.elf
# match.c: the hash must equal the host build's (cc -DOP1_HOST -I<op1r-src>/synth_sdk -I../.. match.c ../../erdtree.c)
$GCC $CF -c "$HERE/match.c" -o $OUT/match.o
$GCC -mcpu=bf524 -nostdlib -T "$HERE/link.ld" $OUT/crt0.o $OUT/match.o $OUT/erdtree.o \
    "$($GCC -mcpu=bf524 -print-libgcc-file-name)" -o $OUT/match.elf
"$RUN" --model bf524 --memory-size 32m --memory-region 0xff800000,0x8000 $OUT/match.elf
rm -rf $OUT
