#!/bin/bash
# Build XiangShan kunminghu-v2 EMU for performance correlation.
# Mirrors the perf CI (DRAMsim3 on, EMU_THREADS=8 for Verilator >= 5.050), minus PGO and wave tracing.
source "$(dirname "$0")/env_kmhv2.sh"
cd $NOOP_HOME
make emu -j${JOBS:-32} CONFIG=DefaultConfig EMU_THREADS=8 WITH_DRAMSIM3=1 WITH_CHISELDB=0
echo "MAKE_EXIT=$?"
