#!/bin/bash
# Build XS-GEM5 (KMH-V2) with locally extracted sqlite3 dev headers.
cd "$(dirname "$0")"
S=/home/data/wanli/.local/deps/sqlite3-dev/root/usr
export CCFLAGS_EXTRA="-I$S/include"
export LINKFLAGS_EXTRA="-L$S/lib/x86_64-linux-gnu"
scons build/RISCV/gem5.opt --linker=gold -j${JOBS:-48}
echo "SCONS_EXIT=$?"
