# Environment for XS-GEM5 KMH-V2 (tag KMH-V2, d11fc1cb55) + NEMU ref (master d6fb8ed4)
export CMODEL_ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
export gem5_home=$CMODEL_ROOT/GEM5-XS
export NEMU_HOME=$CMODEL_ROOT/NEMU
export GCBV_REF_SO=$NEMU_HOME/build/riscv64-nemu-interpreter-so
export GCB_RESTORER=""   # not needed for --raw-cpt; set to gcpt.bin before running SimPoint checkpoints

# XiangShan RTL EMU (kunminghu-v2) toolchain
export NOOP_HOME=$CMODEL_ROOT/XiangShan
export DRAMSIM3_HOME=$CMODEL_ROOT/DRAMsim3          # OpenXiangShan/DRAMsim3, built with -D COSIM=1
export PATH=/home/data/wanli/.local/verilator-5.050/bin:/home/data/wanli/.local/mill-0.12.3/bin:$PATH
