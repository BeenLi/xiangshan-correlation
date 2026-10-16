#!/bin/bash
# Usage: run_rtl_ckpt.sh <checkpoint.zstd> <out_dir>
# Runs one SimPoint checkpoint on the XiangShan EMU with difftest,
# 20M warmup + 20M sample (same window as gem5 kmhv2 defaults).
#
# The EMU is bound to a single NUMA node (CPUs and memory), like
# XiangShan's scripts/xiangshan.py --numa. Verilator threads sync every
# cycle, so spreading them across nodes slows simulation down a lot.
#   NUMA_NODE=<n>    force a node instead of auto-selecting
#   MIN_FREE_MB=<m>  minimum free memory for auto-selection (default 16384)
source "$(dirname "$0")/env_kmhv2.sh"
CK=$1; D=$2
if [ -z "$CK" ] || [ -z "$D" ]; then
  echo "usage: $0 <checkpoint.zstd> <out_dir>" >&2; exit 2
fi

pick_numa_node() {
  # Pick the node with the most idle CPU time over a 1 s sample,
  # among nodes that have at least MIN_FREE_MB of free memory.
  python3 - "${MIN_FREE_MB:-16384}" <<'PY'
import re, subprocess, sys, time
min_free = int(sys.argv[1])
hw = subprocess.run(["numactl", "--hardware"], capture_output=True, text=True).stdout
cpus, free = {}, {}
for line in hw.splitlines():
    m = re.match(r"node (\d+) cpus: (.*)", line)
    if m: cpus[int(m.group(1))] = [int(c) for c in m.group(2).split()]
    m = re.match(r"node (\d+) free: (\d+) MB", line)
    if m: free[int(m.group(1))] = int(m.group(2))
def snap():
    out = {}
    for line in open("/proc/stat"):
        m = re.match(r"cpu(\d+) (.*)", line)
        if m:
            v = [int(x) for x in m.group(2).split()]
            out[int(m.group(1))] = (v[3] + v[4], sum(v))   # idle+iowait, total
    return out
a = snap(); time.sleep(1); b = snap()
best, best_idle = None, -1.0
for node, cl in cpus.items():
    if free.get(node, 0) < min_free:
        continue
    idle = sum((b[c][0] - a[c][0]) / max(1, b[c][1] - a[c][1]) for c in cl)
    if idle > best_idle:
        best, best_idle = node, idle
if best is None:
    sys.exit("no NUMA node with enough free memory")
print(best, round(best_idle, 1), len(cpus[best]), free[best])
PY
}

if [ -n "$NUMA_NODE" ]; then
  NODE=$NUMA_NODE; NODE_INFO="forced"
else
  read -r NODE IDLE NCPU FREE < <(pick_numa_node) || { echo "NUMA selection failed" >&2; exit 1; }
  NODE_INFO="auto: ${IDLE}/${NCPU} idle CPUs, ${FREE} MB free"
fi

mkdir -p $D && cd $D
ulimit -s $((32*1024))
echo "NUMA_NODE=$NODE ($NODE_INFO)" > run_info.txt
start=$(date +%s)
numactl --cpunodebind=$NODE --membind=$NODE \
  $NOOP_HOME/build/emu -i $CK --diff $NOOP_HOME/ready-to-run/riscv64-nemu-interpreter-so \
  -W 20000000 -I 40000000 > simulator_out.txt 2> simulator_err.txt
echo "EMU_EXIT=$? time=$(( $(date +%s)-start ))s" >> simulator_out.txt
