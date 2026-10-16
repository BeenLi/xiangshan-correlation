#!/bin/bash
# Generate SimPoint checkpoints for the workload-builder linux/coremark image.
C=$(cd "$(dirname "$0")/.." && pwd)
export NEMU_HOME=$C/NEMU-cpt                  # NEMU master, riscv64-xs-cpt_defconfig
export CHECKPOINT_OUTPUT_BASE=$C/checkpoints
cd $C/minjie-playground
python3 scripts/checkpoint/generate_checkpoint.py \
  --input-path $C/workload-builder/build/linux-workloads/coremark/fw_payload.bin \
  --name coremark --archive-id coremark-linux-20m --interval 20000000
echo "GEN_EXIT=$?"
