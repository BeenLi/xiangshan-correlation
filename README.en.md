# XiangShan Correlation

[简体中文](README.md) | **English**

A correlation workspace for the XiangShan Kunminghu V2 (KMH-V2) RTL and the XS-GEM5 performance model.

## Layout

```
xiangshan-correlation/
├── README.md
├── AGENTS.md             # Conventions read by coding agents, e.g. checkpoint naming rules
├── Docs/                 # Docs: difftest.md covers difftest, checkpoint.md covers generating and restoring checkpoints, upstream-versions.txt records each component's upstream commit
│   └── learning/         # Study notes: README.md is the study plan, 01-pipeline-overview.md walks through the pipeline (RTL vs gem5), figs/ holds official and hand-drawn figures
├── scripts/              # All of our own scripts, see "Scripts" below
│   └── hooks/            # Repository git hooks; rejects commits of nested git repositories
├── GEM5-XS/              # XS-GEM5, KMH-V2 tag
├── NEMU/                 # NEMU, the gem5 difftest reference model
├── XiangShan/            # XiangShan RTL, kunminghu-v2
├── DRAMsim3/             # OpenXiangShan/DRAMsim3, the memory model for the RTL EMU
├── gem5_data_proc/       # Result processing: parses gem5 and RTL output, computes weighted scores, top-down analysis
├── NEMU-cpt/             # A second copy of NEMU master, used to generate checkpoints
├── workload-builder/     # Packages programs and Linux into images NEMU can boot
├── minjie-playground/    # Only its scripts/checkpoint is used; submodules not pulled
├── checkpoints/          # SimPoint checkpoints, GEM5-specific restorer (LibCheckpointAlpha-gem5-kmhv2)
└── runs/
    ├── gem5-kmhv2/<workload>/
    └── rtl-kmhv2/<workload>/
```

GEM5 and RTL results for the same workload sit side by side under `runs/` with the same workload name, so they are easy to compare in pairs.

## Scripts

Our own scripts all live in `scripts/`, never inside component directories. Scripts locate the repository root from their own location and use no absolute paths.

| Script | Purpose |
|---|---|
| `env_kmhv2.sh` | Common environment, load with `source`; sets `CMODEL_ROOT`, `gem5_home`, `NEMU_HOME`, `NOOP_HOME`, etc. |
| `build_gem5_kmhv2.sh` | Builds GEM5 |
| `build_emu_kmhv2.sh` | Builds the RTL EMU |
| `build_coremark.sh` | Builds a Linux CoreMark image with workload-builder |
| `gen_coremark_cpt.sh` | Generates SimPoint checkpoints for CoreMark |
| `run_rtl_ckpt.sh` | Runs one checkpoint on the EMU |
| `hooks/pre-commit` | Rejects commits of nested git repositories |

## Repository maintenance

Every component is stored in this repository as plain files: no submodules, no upstream tracking. The components' original nested submodules have been flattened, so a clone contains the complete source.

- The first commit is each component's upstream version as-is, tagged `kmhv2-baseline`. `git diff kmhv2-baseline` shows every local change relative to the official KMH-V2.
- The upstream commit of every component and every nested submodule is recorded in `Docs/upstream-versions.txt`; to check upstream history, look up that commit on GitHub.
- Don't run `make init` for XiangShan or workload-builder again, and don't use `git submodule`. Submodules in workload-builder such as gapbs, riscv-tests and nexus-am were not pulled; when needed, clone them into place by hand, delete their `.git`, then commit.
- Build scripts may clone dependencies on their own; for example NEMU clones over the network when a directory under `resource/` is missing. Such directories must have their `.git` removed before committing, otherwise git only records a commit hash. `scripts/hooks/pre-commit` catches this; enable it right after cloning:

```bash
git config core.hooksPath scripts/hooks
```

- Checkpoint memory images (`checkpoints/*/checkpoint/`) and raw simulation output under `runs/` are not committed; see `.gitignore`.

## Pinned versions

| Component | Remote | Branch or tag | Commit | Notes |
|---|---|---|---|---|
| XS-GEM5 | https://github.com/OpenXiangShan/GEM5 | `KMH-V2` tag | `d11fc1cb55` | The tag message reads "XS-GEM5 calibrated with KMH-V2"; configure with `configs/example/kmhv2.py`. One extra local commit (originally `5b9124ed72`) adds the `--gcpt-restorer-size-limit` option |
| DRAMsim3 (for GEM5) | https://github.com/umd-memsys/DRAMsim3 | `master` | `2981759` | Originally pulled by GEM5's `init.sh`, now placed directly at `GEM5-XS/ext/dramsim3/DRAMsim3`; no need to run `init.sh` |
| XiangShan RTL | https://github.com/OpenXiangShan/XiangShan | `kunminghu-v2` | `f86f50b83` | Commits after the tag are almost all bug fixes |
| DRAMsim3 (for RTL) | https://github.com/OpenXiangShan/DRAMsim3 | `cosim-kmh` (repository default branch) | `1d2a9bf` | Built with `cmake -D COSIM=1`; the EMU reads its `configs/XiangShan.ini` |
| NEMU (gem5 reference model) | https://github.com/OpenXiangShan/NEMU | local branch `kmhv2-ref` on master | `d6fb8ed4` | Same date as the GEM5 tag, 2026-01-22; `riscv64-gem5-ref_defconfig` |
| NEMU (RTL reference model) | https://github.com/OpenXiangShan/NEMU | `kunminghu-v2` (not on master) | `b9801f05` | No need to build it yourself. Uses the prebuilt `riscv64-nemu-interpreter-so`, built by NEMU's official CI on 2026-08-26 with `riscv64-xs-ref_defconfig` and shipped in XiangShan's submodule `XiangShan/ready-to-run` (https://github.com/OpenXiangShan/ready-to-run , submodule pinned at `5a63554d`) |
| NEMU (checkpoint generation) | https://github.com/OpenXiangShan/NEMU | master, working directory `NEMU-cpt`, local branch `cpt-master` | `3d9f9fb8` | `riscv64-xs-cpt_defconfig`; SimPoint is built here too |
| workload-builder | https://github.com/OpenXiangShan/workload-builder | `main` | `b7c321d` | Buildroot 2026.08 (builds Linux-based FS workloads) |
| LibCheckpointAlpha (embedded in checkpoints, for RTL) | https://github.com/OpenXiangShan/LibCheckpointAlpha , as a workload-builder submodule (workload-builder/bootloader/LibCheckpointAlpha) | Version pinned by the workload-builder submodule | `5169da7` | Restores senvcfg and the stateen registers |
| LibCheckpointAlpha (GEM5-specific restorer) | Based on https://github.com/OpenXiangShan/LibCheckpointAlpha , local directory `checkpoints/restorers/LibCheckpointAlpha-gem5-kmhv2` | `58019e3` plus one local commit | Originally `5b2b891`, based on `58019e3` | Drops senvcfg and has no stateen; restores only the CSRs implemented by GEM5 at the KMH-V2 tag |
| minjie-playground | https://github.com/OpenXiangShan/minjie-playground | `master` | `2af747e` | Provides `scripts/checkpoint/generate_checkpoint.py`; submodules not pulled |
| gem5_data_proc | https://github.com/OpenXiangShan/gem5_data_proc | `graphmaker` (repository default branch) | `80316ce` | 2026-09-21 |

The two NEMUs come from the same source but differ in commit and build config. The rule is that each model under test (C model / RTL model) is paired with a NEMU from the same period as its difftest interface (which fields are checked, when they are checked, etc.): GEM5's difftest code is frozen at 2026-01-22, while the RTL's difftest submodule and ready-to-run are upgraded together by the XiangShan team.

Changes to the V2 RTL after the GEM5 tag that may affect the comparison:
- coupledL2 was upgraded twice, on 2026-08-11 and 2026-09-16.
- TopDown's memory-stall attribution was fixed on 2026-07-30.
- The CHI async bridge depth went from 4 to 8.

## Memory subsystem alignment

Both sides use DRAMsim3 with identical DRAM parameters, which is a precondition for comparable IPC:
- GEM5's `kmhv2.py` defaults to `--mem-type=DRAMsim3`, with config file `GEM5-XS/ext/dramsim3/xiangshan_configs/xiangshan_DDR4_8Gb_x8_3200_2ch.ini`.
- The RTL EMU is built with `WITH_DRAMSIM3=1`, with config file `DRAMsim3/configs/XiangShan.ini`.
- The DRAM timing parameters in the two config files match item by item. The RTL file has two extra lines, `cpu_freq=3000` and `dram_freq=1600`, used for clock conversion in co-simulation. GEM5's CPU clock period is 333 ps, also 3 GHz.

An EMU built without DRAMsim3 uses fixed-latency memory and can't be used for correlation.

## Usage

```bash
source /home/data/wanli/Cmodel_projects/xiangshan-correlation/scripts/env_kmhv2.sh
```

Build GEM5:

```bash
cd $gem5_home && $CMODEL_ROOT/scripts/build_gem5_kmhv2.sh 2>&1 | tee build.log
```

Build the NEMU reference model:

```bash
export PATH=/home/data/wanli/.local/deps/nemu-tools/root/usr/bin:$PATH
export BISON_PKGDATADIR=/home/data/wanli/.local/deps/nemu-tools/root/usr/share/bison
cd $NEMU_HOME && make riscv64-gem5-ref_defconfig && make -j48
```

Build the RTL EMU, with the same options as XiangShan's performance-test CI but without PGO or waveform support:

```bash
cd $NOOP_HOME && $CMODEL_ROOT/scripts/build_emu_kmhv2.sh 2>&1 | tee build_emu.log
```

Run coremark on GEM5, with difftest on by default:

```bash
mkdir -p $CMODEL_ROOT/runs/gem5-kmhv2/coremark && cd $CMODEL_ROOT/runs/gem5-kmhv2/coremark
$gem5_home/build/RISCV/gem5.opt -d m5out $gem5_home/configs/example/kmhv2.py --raw-cpt --generic-rv-cpt=$CMODEL_ROOT/XiangShan/ready-to-run/coremark-2-iteration.bin
```

When running SimPoint checkpoints, GEM5 and RTL must use the same checkpoint and the same warmup and sample lengths. The two sides use different restorer versions; see "Restorers: GEM5 and RTL use different versions" below.

gem5_data_proc's dependencies are installed in a `.venv` virtual environment in its directory, already listed in the root `.gitignore`. The numpy 1.22 pinned in its requirements.txt doesn't support Python 3.12, so unpinned newer versions are installed: numpy 2.5.3, pandas 3.0.6, scipy 1.18.1, matplotlib 3.11.2, PyYAML 6.0.3, plus pytest.

```bash
source $CMODEL_ROOT/gem5_data_proc/.venv/bin/activate
```

Running it prints many SyntaxWarnings: these are Python 3.12 warnings about regex escapes, they don't affect results, and `-W ignore` silences them.

The repository's own tests have 2 failures at `80316ce`, both in `tests/test_rtl_missing_stats.py`. The cause is that this commit removed `Manager` from `batch.py` without updating the tests. At the previous commit all 34 tests pass in the same environment, so it isn't a dependency problem. After removing the stale `Manager` patching from the tests, both tests pass at `80316ce` too, so the functionality itself is fine and real use is unaffected.

`batch.py` only handles SimPoint checkpoint results and expects stats.txt to contain two stat dumps, warmup and measurement. Results from running a whole program directly, such as coremark, are filtered out.

## SimPoint checkpoints

Generation takes two steps, neither needs root:

```bash
cd $CMODEL_ROOT/workload-builder && make linux/coremark -j32
```

```bash
$CMODEL_ROOT/scripts/gen_coremark_cpt.sh
```

The second step uses NEMU-cpt for profiling, SimPoint clustering and taking checkpoints, with results in `checkpoints/<archive-id>/`. There, `checkpoint/checkpoint.lst` is in the format of the GEM5 batch scripts and `json/` holds the weight files used by gem5_data_proc. The interval is 20 million instructions, and both sides warm up for 20 million and sample 20 million instructions.

SPEC needs the official ISO: `make linux/spec2006 BENCH=<bench> INPUT=<input> SPEC2006_ISO=/path/to/cpu2006.iso`.

### Restorers: GEM5 and RTL use different versions

The restorer embedded in a checkpoint restores every CSR saved by the producer, and the consumer must implement all of them. Newer LibCheckpointAlpha restores senvcfg and the stateen registers:
- The V2 RTL implements these registers and uses the restorer shipped in the checkpoint directly.
- GEM5 at the KMH-V2 tag implements neither senvcfg nor any stateen register; writing them hangs or raises an illegal-instruction exception.

So GEM5 overrides the restorer with `checkpoints/restorers/LibCheckpointAlpha-gem5-kmhv2`. It is based on `58019e3`, the version before stateen was added, and a local commit also drops senvcfg. Build options match workload-builder: CLINT at `0x38000000`, DRAM base `0x80000000`. Both sides share the same checkpoint memory image; only the first 4 KB of restorer code differs.

By default GEM5 only overrides the first 0x700 bytes of the restorer file, but this restorer is 0x8b4 bytes. A local commit (`5b9124ed72` on the former `kmhv2-align` branch) adds the `--gcpt-restorer-size-limit` option. The checkpoint was verified to be all zeros from 0x9b8 to 0x1000, with state data starting at 0xecdb0, so setting it to 0x1000 is safe.

```bash
export GCB_RESTORER=$CMODEL_ROOT/checkpoints/restorers/LibCheckpointAlpha-gem5-kmhv2/build/gcpt.bin
$gem5_home/build/RISCV/gem5.opt -d m5out $gem5_home/configs/example/kmhv2.py --generic-rv-cpt=<ckpt.zstd> --gcpt-restorer-size-limit=0x1000
```

On the RTL side use `scripts/run_rtl_ckpt.sh <ckpt.zstd> <out_dir>`, which calls the EMU with `-W 20000000 -I 40000000`.

## Findings so far: how to use coremark for correlation

Bare-metal coremark can't be used directly. SimPoint checkpoints of the Linux build can, but only to calibrate the in-core pipeline.

### Bare metal: MMIO latency swamps the core differences

The coremark in ready-to-run is bare metal and prints about 400 characters. Each character is one UART MMIO read plus one MMIO write; these accesses execute serially, so their latency is fully exposed. On both sides the MMIO path is only a simulation environment and doesn't represent the real chip:

| | GEM5 kmhv2 | RTL EMU |
|---|---|---|
| MMIO reads | 415 | 422 |
| Latency per MMIO read | About 724 cycles, from two 50 ns bridge delays plus a 100 ns UART delay | About 95 cycles |
| Total cycles | 860,479 | 312,622 |
| IPC | 0.771 | 2.123 |

MMIO reads alone cost GEM5 about 300,000 extra cycles. The bare-metal build is only used to confirm both pipelines run end to end.

### Linux checkpoints: usable to calibrate the in-core pipeline

On checkpoint 78 of `coremark-linux-20m`, both sides warm up 20 million and sample 20 million instructions. Details are in section 5.5 of `Docs/checkpoint.md`:

| | GEM5 kmhv2 | RTL EMU |
|---|---|---|
| Sampled IPC | 2.942 | 2.903 |
| Branch-mispredict redirects | 11,364 | 11,476 |

GEM5's IPC is 1.35% higher than RTL. The key to matching is not a longer run but that the sample window contains no MMIO: GEM5's UART and IO bus see no accesses at all during the sample. There are three reasons:

- The Linux build runs many coremark iterations and prints mostly at the end.
- SimPoint picks intervals from the compute phase.
- The checkpoint skips boot.

Its limits:

- **In-core only**: the working set fits entirely in L1; during the sample L1D misses once and there are only 258 DRAM reads. So it can only calibrate the pipeline and branch prediction, not the cache hierarchy or memory. The memory subsystem needs workloads with larger working sets, such as SPEC.
- **Only one point verified**: so far only 1 checkpoint has been run, weight 16.9%. Getting coremark's overall error requires running all 11 checkpoints and combining them by weight.

## Server environment notes

The server has no sudo access. Missing dependencies were downloaded from the system apt sources and unpacked into the user directory, without touching the system:
- `~/.local/deps/sqlite3-dev`: sqlite3 headers, same 3.45.1 as the system runtime library; needed to build GEM5.
- `~/.local/deps/nemu-tools`: bison, flex, libfl-dev and help2man; needed by NEMU's config tools and the Verilator build.
- `~/.local/verilator-5.050`: Verilator v5.050, built from source with the build script `~/.local/src/build_verilator.sh`. XiangShan CI uses 8 simulation threads for 5.050 and above.
- `~/.local/deps/nemu-tools` also holds the SDL2 and readline development headers needed to build NEMU-cpt. When building, set `C_INCLUDE_PATH` to `usr/include` and `usr/include/x86_64-linux-gnu` under that directory, and `LIBRARY_PATH` to `usr/lib/x86_64-linux-gnu`.
- `~/.local/mill-0.12.3`: mill 0.12.3, downloaded from Maven Central with its SHA1 verified. Downloads from the GitHub releases page are very slow from the server; don't use it.

GEM5 reads extra build flags from the environment variables `CCFLAGS_EXTRA` and `LINKFLAGS_EXTRA`, not from the scons command line. `scripts/build_gem5_kmhv2.sh` already takes care of this.
