#!/bin/bash
cd "$(dirname "$0")/../workload-builder"
make linux/coremark -j${JOBS:-32}
echo "MAKE_EXIT=$?"
