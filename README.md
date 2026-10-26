# XiangShan Correlation

**简体中文** | [English](README.en.md)

XiangShan 昆明湖 V2 RTL 与 XS-GEM5 性能模型的 correlation 工作区。

## 目录结构

```
xiangshan-correlation/
├── README.md
├── AGENTS.md             # 代理工具读取的约定，如检查点命名规则
├── Docs/                 # 说明文档：difftest.md 讲 difftest，checkpoint.md 讲检查点的生成与恢复，upstream-versions.txt 记录各组件的上游提交
│   └── learning/         # 学习文档：README.md 是学习计划，01-pipeline-overview.md 讲流水线（RTL 对 gem5），figs/ 放官方图与自绘图
├── scripts/              # 所有自写脚本，见下文“脚本”
│   └── hooks/            # 仓库的 git hook，拒绝提交嵌套的 git 仓库
├── GEM5-XS/              # XS-GEM5，KMH-V2 标签
├── NEMU/                 # NEMU，gem5 difftest 参考模型
├── XiangShan/            # XiangShan RTL，kunminghu-v2
├── DRAMsim3/             # OpenXiangShan/DRAMsim3，RTL EMU 的内存模型
├── gem5_data_proc/       # 结果处理工具，解析 gem5 和 RTL 输出、算加权分数、top-down 分析
├── NEMU-cpt/             # NEMU master 的另一份源码，用于生成检查点
├── workload-builder/     # 把程序和 Linux 打包成 NEMU 可启动的镜像
├── minjie-playground/    # 只用其中 scripts/checkpoint，未拉子模块
├── checkpoints/          # SimPoint 检查点、GEM5 专用恢复程序(LibCheckpointAlpha-gem5-kmhv2)
└── runs/
    ├── gem5-kmhv2/<workload>/
    └── rtl-kmhv2/<workload>/
```

GEM5 和 RTL 在同一负载上的结果，按相同的负载名并排放在 `runs/` 下，方便成对对比。

## 脚本

自己写的脚本统一放在 `scripts/`，不放进各组件目录。脚本按自身位置找到仓库根目录，不写绝对路径。

| 脚本 | 作用 |
|---|---|
| `env_kmhv2.sh` | 统一环境，用 `source` 加载，设置 `CMODEL_ROOT`、`gem5_home`、`NEMU_HOME`、`NOOP_HOME` 等 |
| `build_gem5_kmhv2.sh` | 编译 GEM5 |
| `build_emu_kmhv2.sh` | 编译 RTL 的 EMU |
| `build_coremark.sh` | 用 workload-builder 构建 Linux 版 CoreMark 镜像 |
| `gen_coremark_cpt.sh` | 生成 CoreMark 的 SimPoint 检查点 |
| `run_rtl_ckpt.sh` | 在 EMU 上跑一个检查点 |
| `hooks/pre-commit` | 拒绝提交嵌套的 git 仓库 |

## 仓库维护

所有组件都以普通文件的形式放在本仓库里，不用子模块，也不跟踪上游。各组件原来的嵌套子模块已经拍平，克隆下来就是完整源码。

- 第一个提交是各组件的上游原样版本，打了标签 `kmhv2-baseline`。`git diff kmhv2-baseline` 能看到相对官方 KMH-V2 的全部本地改动。
- 每个组件、每个嵌套子模块对应的上游提交记在 `Docs/upstream-versions.txt`，要查上游历史就去 GitHub 看对应提交。
- 不要再运行 XiangShan 和 workload-builder 的 `make init`，也不要用 `git submodule`。workload-builder 里 gapbs、riscv-tests、nexus-am 等子模块当时没有拉取，需要时手动 clone 到对应位置，删掉其中的 `.git` 再提交。
- 构建脚本可能自动 clone 依赖，例如 NEMU 的 `resource/` 下缺目录时会联网 clone。这类目录要删掉 `.git` 才能提交，否则 git 只会记下一个提交号。`scripts/hooks/pre-commit` 会拦住这种情况，新克隆仓库后先启用它：

```bash
git config core.hooksPath scripts/hooks
```

- 检查点内存镜像（`checkpoints/*/checkpoint/`）和 `runs/` 下的原始仿真输出不进仓库，见 `.gitignore`。

## 锁定版本

| 组件 | 远端仓库 | 分支或标签 | 提交 | 说明 |
|---|---|---|---|---|
| XS-GEM5 | https://github.com/OpenXiangShan/GEM5 | `KMH-V2` 标签 | `d11fc1cb55` | 标签说明为 "XS-GEM5 calibrated with KMH-V2"，配置用 `configs/example/kmhv2.py`。本地另有一个提交（原 `5b9124ed72`），加了 `--gcpt-restorer-size-limit` 参数 |
| DRAMsim3（GEM5 用） | https://github.com/umd-memsys/DRAMsim3 | `master` | `2981759` | 原由 GEM5 的 `init.sh` 拉取，现已直接放在 `GEM5-XS/ext/dramsim3/DRAMsim3`，不用再跑 `init.sh` |
| XiangShan RTL | https://github.com/OpenXiangShan/XiangShan | `kunminghu-v2` | `f86f50b83` | 标签之后的提交基本都是功能修复 |
| DRAMsim3（RTL 用） | https://github.com/OpenXiangShan/DRAMsim3 | `cosim-kmh`（仓库默认分支） | `1d2a9bf` | 用 `cmake -D COSIM=1` 编译，EMU 读取其中的 `configs/XiangShan.ini` |
| NEMU（gem5 参考模型） | https://github.com/OpenXiangShan/NEMU | master 上的本地分支 `kmhv2-ref` | `d6fb8ed4` | 与 GEM5 标签同为 2026-01-22，`riscv64-gem5-ref_defconfig` |
| NEMU（RTL 参考模型） | https://github.com/OpenXiangShan/NEMU | `kunminghu-v2`（不在 master 上） | `b9801f05` | 不需要自己编。用的是预编译好的 `riscv64-nemu-interpreter-so`，由 NEMU 官方 CI 于 2026-08-26 用 `riscv64-xs-ref_defconfig` 编译，放在 XiangShan 的子模块 `XiangShan/ready-to-run`（https://github.com/OpenXiangShan/ready-to-run ，子模块锁定在 `5a63554d`）里 |
| NEMU（生成检查点） | https://github.com/OpenXiangShan/NEMU | master，工作目录 `NEMU-cpt`，本地分支 `cpt-master` | `3d9f9fb8` | `riscv64-xs-cpt_defconfig`，SimPoint 也在这里编译 |
| workload-builder | https://github.com/OpenXiangShan/workload-builder | `main` | `b7c321d` | Buildroot 2026.08 (构建linux-based FS workload) |
| LibCheckpointAlpha（检查点内嵌，RTL 用） | https://github.com/OpenXiangShan/LibCheckpointAlpha ，作为 workload-builder 的子模块(workload-builder/bootloader/LibCheckpointAlpha) | workload-builder 子模块锁定的版本 | `5169da7` | 会恢复 senvcfg 和 stateen 系列寄存器 |
| LibCheckpointAlpha（GEM5 专用恢复程序） | 基于 https://github.com/OpenXiangShan/LibCheckpointAlpha ，本地目录 `checkpoints/restorers/LibCheckpointAlpha-gem5-kmhv2` | `58019e3` 加一个本地提交 | 原 `5b2b891`，基于 `58019e3` | 去掉了 senvcfg，也不含 stateen，只恢复 KMH-V2 标签的 GEM5 实现了的 CSR |
| minjie-playground | https://github.com/OpenXiangShan/minjie-playground | `master` | `2af747e` | 提供 `scripts/checkpoint/generate_checkpoint.py`，未拉子模块 |
| gem5_data_proc | https://github.com/OpenXiangShan/gem5_data_proc | `graphmaker`（仓库默认分支） | `80316ce` | 2026-09-21 |

两个 NEMU 来自同一份源码，但提交和编译配置都不同。原则是每个被测模型(Cmodel/RTLModel)搭配与其 difftest 接口(检查哪些字段,什么时候检查等)同期的 NEMU：GEM5 的 difftest 代码冻结在 2026-01-22，RTL 的 difftest 子模块与 ready-to-run 由 XiangShan 团队成对升级。

V2 RTL 在 GEM5 标签之后，有几处改动可能影响对比：
- coupledL2 在 2026-08-11 和 2026-09-16 各升级过一次。
- TopDown 的内存停顿归因在 2026-07-30 修过。
- CHI 异步桥深度从 4 改成了 8。

## 内存子系统对齐

两边都用 DRAMsim3，且 DRAM 参数一致，这是 IPC 可比的前提：
- GEM5 的 `kmhv2.py` 默认 `--mem-type=DRAMsim3`，配置文件是 `GEM5-XS/ext/dramsim3/xiangshan_configs/xiangshan_DDR4_8Gb_x8_3200_2ch.ini`。
- RTL 的 EMU 用 `WITH_DRAMSIM3=1` 编译，配置文件是 `DRAMsim3/configs/XiangShan.ini`。
- 两个配置文件的 DRAM 时序参数逐项相同。RTL 的文件多了 `cpu_freq=3000` 和 `dram_freq=1600` 两行，是联合仿真换算时钟用的。GEM5 的 CPU 时钟周期是 333 ps，也是 3 GHz。

不开 DRAMsim3 编译的 EMU 用的是固定延迟内存，不能用来做 correlation。

## 使用方法

```bash
source /home/data/wanli/Cmodel_projects/xiangshan-correlation/scripts/env_kmhv2.sh
```

编译 GEM5：

```bash
cd $gem5_home && $CMODEL_ROOT/scripts/build_gem5_kmhv2.sh 2>&1 | tee build.log
```

编译 NEMU 参考模型：

```bash
export PATH=/home/data/wanli/.local/deps/nemu-tools/root/usr/bin:$PATH
export BISON_PKGDATADIR=/home/data/wanli/.local/deps/nemu-tools/root/usr/share/bison
cd $NEMU_HOME && make riscv64-gem5-ref_defconfig && make -j48
```

编译 RTL 的 EMU，参数与 XiangShan 性能测试 CI 一致，但不做 PGO，也不编波形支持：

```bash
cd $NOOP_HOME && $CMODEL_ROOT/scripts/build_emu_kmhv2.sh 2>&1 | tee build_emu.log
```

在 GEM5 上跑 coremark，difftest 默认开启：

```bash
mkdir -p $CMODEL_ROOT/runs/gem5-kmhv2/coremark && cd $CMODEL_ROOT/runs/gem5-kmhv2/coremark
$gem5_home/build/RISCV/gem5.opt -d m5out $gem5_home/configs/example/kmhv2.py --raw-cpt --generic-rv-cpt=$CMODEL_ROOT/XiangShan/ready-to-run/coremark-2-iteration.bin
```

跑 SimPoint 检查点时，GEM5 和 RTL 要用同一个检查点、同样的预热和采样长度。恢复程序两边版本不同，见下文“恢复程序：GEM5 和 RTL 用不同版本”。

gem5_data_proc 的依赖装在它目录下的 `.venv` 虚拟环境里，已写进根目录的 `.gitignore`。仓库 requirements.txt 锁定的 numpy 1.22 不支持 Python 3.12，所以装的是不锁版本的新版本：numpy 2.5.3、pandas 3.0.6、scipy 1.18.1、matplotlib 3.11.2、PyYAML 6.0.3，另加 pytest。

```bash
source $CMODEL_ROOT/gem5_data_proc/.venv/bin/activate
```

运行时会出现大量 SyntaxWarning，是 Python 3.12 对正则写法的提示，不影响结果，可以加 `-W ignore` 屏蔽。

仓库自带测试在 `80316ce` 上有 2 个失败，都在 `tests/test_rtl_missing_stats.py`。原因是这次提交删掉了 `batch.py` 里的 `Manager`，但没有更新测试。上一个提交在同一环境下 34 个测试全部通过，所以不是依赖问题。去掉测试里对 `Manager` 的过时替换后，这 2 个测试在 `80316ce` 上也都通过，说明功能本身没有问题，实际使用不受影响。

`batch.py` 只处理 SimPoint 检查点的结果，要求 stats.txt 里有预热和正式测量两段统计。直接跑完整程序的结果，比如 coremark，会被过滤掉。

## SimPoint 检查点

生成流程分两步，都不需要 root：

```bash
cd $CMODEL_ROOT/workload-builder && make linux/coremark -j32
```

```bash
$CMODEL_ROOT/scripts/gen_coremark_cpt.sh
```

第二步用 NEMU-cpt 做剖析、SimPoint 聚类和取检查点，结果在 `checkpoints/<archive-id>/`。其中 `checkpoint/checkpoint.lst` 是 GEM5 批量脚本的格式，`json/` 是 gem5_data_proc 用的权重文件。间隔为 2000 万条指令，两边都用预热 2000 万条、采样 2000 万条。

SPEC 需要官方 ISO：`make linux/spec2006 BENCH=<bench> INPUT=<input> SPEC2006_ISO=/path/to/cpu2006.iso`。

### 恢复程序：GEM5 和 RTL 用不同版本

检查点里嵌入的恢复程序会恢复生成端保存的每个 CSR，消费端必须全部实现。新版 LibCheckpointAlpha 会恢复 senvcfg 和 stateen 系列寄存器：
- V2 RTL 实现了这些寄存器，直接用检查点自带的恢复程序。
- GEM5 的 KMH-V2 标签没有实现 senvcfg 和任何 stateen 寄存器，写入会卡死或触发非法指令。

所以 GEM5 用 `checkpoints/restorers/LibCheckpointAlpha-gem5-kmhv2` 覆盖恢复程序。它基于 `58019e3`，即加入 stateen 之前的版本，本地提交又去掉了 senvcfg。编译参数与 workload-builder 相同：CLINT 地址 `0x38000000`，DRAM 起始 `0x80000000`。两边的检查点内存镜像是同一份，只有前 4 KB 的恢复代码不同。

GEM5 默认只覆盖恢复程序文件的前 0x700 字节，而这个恢复程序有 0x8b4 字节。本地提交（原 `kmhv2-align` 分支的 `5b9124ed72`）加了 `--gcpt-restorer-size-limit` 参数。已核实检查点在 0x9b8 到 0x1000 之间全为零，状态数据从 0xecdb0 开始，所以设成 0x1000 是安全的。

```bash
export GCB_RESTORER=$CMODEL_ROOT/checkpoints/restorers/LibCheckpointAlpha-gem5-kmhv2/build/gcpt.bin
$gem5_home/build/RISCV/gem5.opt -d m5out $gem5_home/configs/example/kmhv2.py --generic-rv-cpt=<ckpt.zstd> --gcpt-restorer-size-limit=0x1000
```

RTL 侧用 `scripts/run_rtl_ckpt.sh <ckpt.zstd> <out_dir>`，它调用 EMU 并加上 `-W 20000000 -I 40000000`。

## 已知结论：coremark 怎样用才能做 correlation

裸机版 coremark 不能直接用，Linux 版的 SimPoint 检查点可以用，但只能校准核内流水线。

### 裸机版：MMIO 延迟会盖过核的差异

ready-to-run 里的 coremark 是裸机版，会打印约 400 个字符。每个字符是一次串口 MMIO 读加一次 MMIO 写，这两类访问会串行执行，延迟完全暴露。两边的 MMIO 路径都只是仿真环境，不代表真实芯片：

| | GEM5 kmhv2 | RTL EMU |
|---|---|---|
| MMIO 读次数 | 415 | 422 |
| 单次 MMIO 读延迟 | 约 724 周期，来自两次 50 ns 桥延迟加 100 ns 串口延迟 | 约 95 周期 |
| 总周期数 | 860,479 | 312,622 |
| IPC | 0.771 | 2.123 |

仅 MMIO 读一项就让 GEM5 多花约 30 万周期。裸机版只用来确认两边的链路能跑通。

### Linux 版检查点：可以用来校准核内流水线

在 `coremark-linux-20m` 的 78 号检查点上，两边都预热 2000 万条、采样 2000 万条。详细结果见 `Docs/checkpoint.md` 第 5.5 节：

| | GEM5 kmhv2 | RTL EMU |
|---|---|---|
| 采样段 IPC | 2.942 | 2.903 |
| 分支误预测重定向 | 11,364 | 11,476 |

GEM5 的 IPC 比 RTL 高 1.35%。能对上的关键不是程序跑得长，而是采样窗口里没有 MMIO，GEM5 的串口和 IO 总线在采样段里没有任何访问。原因有三点：

- Linux 版会把 coremark 迭代很多轮，打印集中在结尾。
- SimPoint 选的是计算阶段的区间。
- 检查点跳过了启动过程。

它的局限：

- **只覆盖核内**：工作集全在 L1 里，采样段 L1D 只缺失 1 次，DRAM 读只有 258 次。所以它只能校准流水线和分支预测，不能校准 cache 层次和内存。内存子系统要用 SPEC 这类工作集更大的负载。
- **只验证了一个点**：目前只跑了 1 个检查点，权重 16.9%。要得到 coremark 整体的误差，需要把 11 个检查点都跑完再按权重合成。

## 服务器环境说明

服务器没有 sudo 权限，缺少的依赖已从系统 apt 源下载，解压到用户目录，没有改动系统：
- `~/.local/deps/sqlite3-dev`：sqlite3 头文件，与系统运行库同为 3.45.1，GEM5 编译需要。
- `~/.local/deps/nemu-tools`：bison、flex、libfl-dev 和 help2man，NEMU 的配置工具和 Verilator 编译需要。
- `~/.local/verilator-5.050`：Verilator v5.050，从源码编译，编译脚本是 `~/.local/src/build_verilator.sh`。XiangShan CI 对 5.050 及以上版本使用 8 个仿真线程。
- `~/.local/deps/nemu-tools` 里还有 SDL2 和 readline 的开发头文件，NEMU-cpt 编译需要。编译时设置 `C_INCLUDE_PATH` 为该目录下的 `usr/include` 和 `usr/include/x86_64-linux-gnu`，`LIBRARY_PATH` 为 `usr/lib/x86_64-linux-gnu`。
- `~/.local/mill-0.12.3`：mill 0.12.3，从 Maven 中央仓库下载，SHA1 已校验。GitHub 发布页从服务器下载很慢，不要用。

GEM5 从环境变量 `CCFLAGS_EXTRA` 和 `LINKFLAGS_EXTRA` 读取额外编译参数，不从 scons 命令行读取。`scripts/build_gem5_kmhv2.sh` 已处理好这一点。
