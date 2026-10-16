# SimPoint 检查点：生成与恢复

本文说明本工作区里 SimPoint 检查点怎样生成、里面装了什么，以及 GEM5 和 RTL 怎样从检查点恢复运行。文中的版本见 `README.md`，检查点归档的命名规则见 `AGENTS.md`，difftest 的原理见 `Docs/difftest.md`。

## 1. 为什么要用检查点

一个 SPEC 测试项通常有几千亿到上万亿条指令。GEM5 每秒只能模拟几万条，RTL 的 EMU 更慢，从头跑完是不现实的。SimPoint 的思路是：
1. 把程序切成等长的片段，本工作区用 2000 万条指令一段。
2. 统计每段执行了哪些基本块，把行为相似的片段归成一类。
3. 每类挑一个片段作代表，权重等于这一类片段数占总数的比例。
4. 只详细仿真这些代表片段，再按权重把结果加起来，近似整个程序的表现。

检查点就是每个代表片段开始之前的完整机器状态。有了它，GEM5 和 RTL 可以直接从那一刻开始运行，不用从程序开头跑起。

## 2. 整体流程

```
workload-builder            NEMU-cpt              SimPoint             NEMU-cpt
┌──────────────┐   ┌──────────────────┐   ┌──────────────┐   ┌──────────────────┐
│ 打包 Linux    │──>│ 剖析：统计每段的  │──>│ 聚类：选代表  │──>│ 取检查点：在每个  │
│ 启动镜像      │   │ 基本块向量        │   │ 片段和权重    │   │ 代表片段前停下存盘 │
└──────────────┘   └──────────────────┘   └──────────────┘   └──────────────────┘
 fw_payload.bin      simpoint_bbv.gz        simpoints0          *_memory_.zstd
                                             weights0
                                                                       │
                                     ┌─────────────────────────────────┴───────┐
                                     v                                         v
                              GEM5 kmhv2.py 恢复                         RTL EMU 恢复
```

后三步由 `minjie-playground/scripts/checkpoint/generate_checkpoint.py` 一次完成。CoreMark 的封装脚本是 `scripts/gen_coremark_cpt.sh`。

## 3. 生成

### 3.1 打包启动镜像

```bash
cd $CMODEL_ROOT/workload-builder && make linux/coremark -j32
```

产物是 `build/linux-workloads/coremark/fw_payload.bin`，内存布局如下：

| 地址 | 内容 |
|---|---|
| `0x80000000` 起的 1 MB | LibCheckpointAlpha 恢复程序，版本为 workload-builder 子模块锁定的 `5169da7` |
| `0x80100000` 起 | OpenSBI、Linux 内核和 initramfs，CoreMark 在 initramfs 里 |

镜像的启动脚本通过 `nemu-exec` 运行 CoreMark，源码在 `workload-builder/br2-external/package/nemu-exec/main.c`。它先 fork 出子进程，子进程在执行 CoreMark 之前发出两个 NEMU 陷阱指令：
- `0x100`：关闭定时器中断，让运行过程可重复。
- `0x101`：通知 NEMU 从这里开始计数。

所以剖析和取检查点的指令计数都从 CoreMark 开始，不包括 Linux 的启动过程。

SPEC 需要官方 ISO，例如：

```bash
make linux/spec2006 BENCH=astar INPUT=biglakes SPEC2006_ISO=/path/to/cpu2006.iso -j32
```

### 3.2 剖析

脚本调用 `NEMU-cpt/build/riscv64-nemu-interpreter`，关键参数是：

```
<fw_payload.bin> -D <归档目录> -C profiling -w <负载名> -b --simpoint-profile --cpt-interval 20000000 --checkpoint-format zstd
```

NEMU 完整运行一遍程序，每 2000 万条指令输出一个基本块向量，结果是 `profiling/<负载名>/simpoint_bbv.gz`。CoreMark 共 24.3 亿条指令，NEMU 用了约 30 秒。

### 3.3 聚类

脚本调用 `NEMU-cpt/resource/simpoint/simpoint_repo/bin/simpoint`：

```
-loadFVFile simpoint_bbv.gz -inputVectorsGzipped -maxK 30 -numInitSeeds 2 -iters 1000 -seedkm 171236 -seedproj 193132
-saveSimpoints simpoints0 -saveSimpointWeights weights0
```

- `maxK 30`：最多分 30 类。SimPoint 会在 1 到 30 类之间按 BIC 指标选最合适的类数，CoreMark 选出了 11 类。
- 两个随机种子是固定的，所以同样的输入每次得到同样的结果。
- `simpoints0` 每行是"片段编号 类别编号"，`weights0` 每行是"权重 类别编号"。

### 3.4 取检查点

NEMU 再运行一遍程序，用 `-S <聚类目录>` 读入代表片段，在对应位置停下来保存状态。

**保存位置。** 第 N 号片段的检查点保存在第 `N × 间隔 − 预热长度` 条指令处，预热长度默认等于间隔。计算见 `NEMU-cpt/src/checkpoint/serializer.cpp` 的 `instrsCouldTakeCpt`。以 78 号片段为例：

| 阶段 | 指令范围 | 对应片段 |
|---|---|---|
| 保存检查点 | 第 15.4 亿条 | 77 号片段的起点 |
| 预热 | 15.4 亿到 15.6 亿 | 77 号 |
| 采样 | 15.6 亿到 15.8 亿 | 78 号，即选中的代表 |

预热执行的是紧挨着的前一段代码，让缓存、分支预测器和 TLB 的状态接近真实运行。预热长度可以用 NEMU 的 `--warmup-interval` 单独设置。

**边界情况。** 0 号片段前面没有别的片段，检查点只能保存在第 0 条指令处。结果预热的是 0 号片段本身，采样的是 1 号片段。这类片段权重通常很小，CoreMark 里只占 0.85%。

### 3.5 产物目录

| 路径 | 内容 |
|---|---|
| `checkpoint/<负载>/<片段号>/_<片段号>_<权重>_memory_.zstd` | 检查点本体，zstd 压缩的内存镜像 |
| `checkpoint/checkpoint.lst` | GEM5 批量脚本的输入，每行是"名字 路径 0 0 20 20"，后四项依次是跳过、功能预热、详细预热、采样的指令数，单位百万 |
| `json/<负载>.json`、`json/checkpoints_all.json` | 每个片段的权重和总指令数，供 gem5_data_proc 计算加权结果 |
| `json/checkpoints_cov0.3.json` | 按权重从大到小取片段，直到累计权重达到 30%，用于快速测试 |
| `profiling/`、`cluster/` | 剖析和聚类的中间结果 |
| `logs/` | 三个阶段的日志 |
| `metadata/` | 本次生成的参数 |

## 4. 检查点内部结构

一个检查点就是一份完整的物理内存快照。开头 1 MB 的布局来自 `NEMU-cpt/resource/LibCheckpoint/include/cpt_default_values.h`，下表地址是相对 `0x80000000` 的偏移：

| 偏移 | 内容 |
|---|---|
| `0x00000` | 恢复程序代码。新版约 0x9b8 字节 |
| `0xECDB0` | 魔数 `0xdeadbeef`，表示这是一个检查点 |
| `0xECDB8` | 保存时的 PC |
| `0xECDC0` | 保存时的特权级 |
| `0xECDC8`、`0xECDD0` | `mtime` 和 `mtimecmp` |
| `0xEDDE0` | 32 个通用寄存器 |
| `0xEDEE8` | 32 个浮点寄存器 |
| `0xEDFF0` | CSR 表：按 CSR 编号索引，每项 8 字节。例如 `menvcfg` 编号 `0x30a`，存放在 `0xEDFF0 + 0x30a × 8 = 0xEF840` |
| `0xFDFF8` | 向量寄存器 |
| `0x100000` 起 | OpenSBI、Linux 和用户程序在保存那一刻的真实内存 |

## 5. 恢复

### 5.1 恢复程序怎样工作

GEM5 或 RTL 把整份内存镜像加载进来，从 `0x80000000` 开始执行。这时 CPU 在机器模式下运行恢复程序：
1. 按保存的特权级设置 `mstatus.MPP`。
2. 通过 CLINT 写回 `mtimecmp` 和 `mtime`。CLINT 的地址在编译恢复程序时用 `CONFIG_CLINT_MMIO` 指定，本工作区为 `0x38000000`。源码注释说明 `mtime` 的恢复是不精确的。
3. 从 CSR 表逐个读出 CSR 值，用 `csrw` 写回。
4. 恢复浮点寄存器、向量寄存器和通用寄存器。
5. 让 `mepc` 指向保存的 PC，执行 `mret` 跳回去，同时切换到 `MPP` 指定的特权级。

源码在 `workload-builder/bootloader/LibCheckpointAlpha/src/restore.S`。

`mret` 会改写 `mepc` 和 `mstatus` 的几个位。快照停在用户态或监督态时，这些位不在使用中，改写无害。快照停在机器模式时，它们正在使用，现场会被破坏。所以检查点只能在用户态或监督态保存，这也是检查点工具链只支持 Linux 镜像的原因之一。

### 5.2 两道兼容性约束

1. **恢复程序和生成端要约定同一个布局。** 两边都按第 4 节的表读写。LibCheckpointAlpha 从 `58019e3` 到 `5169da7` 布局没有变。
2. **恢复程序写的每个 CSR，消费端都必须实现。** 恢复程序是在被模拟的 CPU 上执行的，它每写一个 CSR，GEM5 或 RTL 就要模拟这条指令。

新版恢复程序会写 `senvcfg` 和 stateen 系列寄存器。V2 RTL 实现了它们，GEM5 的 KMH-V2 标签没有。所以两边使用同一份检查点内存镜像，但恢复程序不同：

| 消费端 | 恢复程序 | 覆盖方式 |
|---|---|---|
| RTL EMU | 检查点自带的新版，`5169da7` | 不覆盖 |
| GEM5 kmhv2 | `checkpoints/restorers/LibCheckpointAlpha-gem5-kmhv2`，基于 `58019e3` 去掉 `senvcfg`，不含 stateen | 通过 `GCB_RESTORER` 覆盖前 0x1000 字节 |

GEM5 专用恢复程序的编译方法：

```bash
cd $CMODEL_ROOT/checkpoints/restorers/LibCheckpointAlpha-gem5-kmhv2
export PATH=$CMODEL_ROOT/workload-builder/build/buildroot/output/host/bin:$PATH
CFLAGS="-DCONFIG_CLINT_MMIO=0x38000000 -DCONFIG_DRAM_BASE=0x80000000" make CROSS_COMPILE=riscv64-linux-
```

### 5.3 在 RTL 上恢复

```bash
$CMODEL_ROOT/scripts/run_rtl_ckpt.sh <ckpt.zstd> <输出目录>
```

脚本实际执行的是：

```bash
numactl --cpunodebind=<节点> --membind=<节点> $NOOP_HOME/build/emu -i <ckpt.zstd> --diff $NOOP_HOME/ready-to-run/riscv64-nemu-interpreter-so -W 20000000 -I 40000000
```

- EMU 的线程和内存都绑在同一个 NUMA 节点上，做法与 XiangShan 的 `scripts/xiangshan.py --numa` 相同。Verilator 的多线程仿真每个周期都要在线程之间同步，线程分散在不同节点上会明显变慢。这台服务器每个节点有 12 个逻辑核，放得下 8 个仿真线程。
- 默认先采样 1 秒各节点的 CPU 空闲率，在空闲内存不少于 16 GB 的节点里选最空闲的。可以用环境变量 `NUMA_NODE` 指定节点，用 `MIN_FREE_MB` 调整内存门槛。选中的节点记录在输出目录的 `run_info.txt` 里。
- `-W` 是预热指令数，`-I` 是最多执行的指令数，与 XiangShan 性能测试脚本一致。
- EMU 能直接读 zstd 文件。
- 如需换恢复程序，用 `-r <gcpt.bin>`，默认覆盖前 0xe00 字节，可以用 `--overwrite-nbytes` 调整，上限 0xf00。

### 5.4 在 GEM5 上恢复

```bash
source $CMODEL_ROOT/scripts/env_kmhv2.sh
export GCB_RESTORER=$CMODEL_ROOT/checkpoints/restorers/LibCheckpointAlpha-gem5-kmhv2/build/gcpt.bin
$gem5_home/build/RISCV/gem5.opt -d m5out $gem5_home/configs/example/kmhv2.py --generic-rv-cpt=<ckpt.zstd> --gcpt-restorer-size-limit=0x1000
```

- 标量检查点必须设置 `GCB_RESTORER`，否则 GEM5 直接报错退出。设成空字符串表示不覆盖，使用检查点自带的恢复程序。
- GEM5 默认只覆盖恢复程序文件的前 0x700 字节，而 GEM5 专用恢复程序有 0x8b4 字节。`--gcpt-restorer-size-limit` 是本地提交（原 `kmhv2-align` 分支的 `5b9124ed72`）加入的参数。已核实检查点在 0x9b8 到 0x1000 之间全为零，状态数据从 0xECDB0 开始，所以覆盖 0x1000 字节是安全的。
- GEM5 默认预热 2000 万条、总共 4000 万条，与检查点约定一致，不用额外传参。
- `m5out/stats.txt` 里有两段统计，第一段是预热，第二段是采样。对比时用第二段。

### 5.5 已验证的结果

CoreMark 的 78 号检查点，权重 16.9%。两边都先预热 2000 万条指令，再采样 2000 万条。采样段的数字取自第二个统计块：

- GEM5：`m5out/stats.txt` 里的第 2 个 `Begin Simulation Statistics` 块。
- RTL：`simulator_err.txt` 里 `time=13873653` 那一次 `[PERF ]` 输出。`-W` 预热结束时，计数器会先输出一次，然后清零。

RTL 结束时在 `simulator_out.txt` 打印的 `instrCnt`、`cycleCnt` 和 `IPC` 是预热加采样的累计值，分别是 40,000,008、13,873,648 和 2.883，不能拿来和 GEM5 的采样段比。

| 消费端 | 采样段指令数 | 采样段周期数 | IPC | 仿真耗时 |
|---|---|---|---|---|
| GEM5 kmhv2 | 20,000,000 | 6,798,183 | 2.942 | 约 25 分钟 |
| RTL EMU | 20,000,017 | 6,890,118 | 2.903 | 约 6 小时 9 分钟，即 22,138 秒，这次运行没有绑定 NUMA 节点 |

采样段 GEM5 的 IPC 比 RTL 高 1.35%，周期数少 1.33%。预热段 RTL 的 IPC 是 2.864，GEM5 是 2.837。预热段从冷的 cache 和预测器开始，只作参考。

#### 分项对比

| 指标 | RTL 计数器 | RTL | GEM5 计数器 | GEM5 |
|---|---|---|---|---|
| 预测的分支数 | `ftq: BpInstr` | 3,659,657 | `commit.branches` | 3,659,682 |
| 分支误预测重定向 | `ftq: mispredictRedirect` | 11,476 | `commit.branchMispredicts` | 11,364 |
| L1D 缺失 | `dcache.ldu_*: load_miss` | 1 | `dcache.demandMisses` | 0 |
| L1I 缺失 | `ICacheMissBubble` | 0 | `icache.demandMisses` | 0 |
| DRAM 读请求 | — | — | `mem_ctrls.numReads` | 258 |

这个检查点的工作集全部在 L1 里，分支误预测次数也只差 1%。因此 IPC 的差距来自核内流水线，和内存子系统无关。

#### Top-down 对比

两边都按派发宽度 6 统计 slot，总 slot 数都正好等于 6 × 周期数。

| 类别 | RTL | GEM5 |
|---|---|---|
| 有效派发 | 49.7% | 49.9% |
| 前端 | 6.5% | 4.0% |
| 错误推测 | 0.7% | 1.6% |
| 后端 | 43.0% | 44.6% |

类别是按计数器名字手工对应的：

- **前端**：RTL 取 `Override`、`FtqUpdate`、`FtqFull`、`ITLBMiss`、`BTBMiss`、`FetchFrag`；GEM5 取 `IcacheStall`、`BpStall`、`FTQBubble`、`FetchFragStall`。
- **错误推测**：RTL 取各预测器的 `*MissBubble`、`MemVioRedirect`、`OtherRedirect`、`ControlRecovery`、`MemVioRecovery`；GEM5 取 `SquashStall`、`InstSquashed`、`CommitSquash`。
- **后端**：其余各项。

两边的归因逻辑并不相同，所以这张表只能看趋势，不能逐项精确对账。

差距最大的一项在前端：

- RTL 的 `ITLBMissBubble` 有 922,904 个 slot。但同一时段 PTW 的访问次数是 0，`ICacheMissBubble` 也是 0，所以这不是真正的 TLB 缺失。更可能是取指单元暂时没有准备好，被统计逻辑归到了这一类。
- GEM5 与之对应的 `IcacheStall` 是 460,688 个 slot，同样没有真正的缺失。
- `FetchFrag` 方面，RTL 是 1,357,264 个 slot，GEM5 是 1,105,998 个 slot。

RTL 在前端多出约 108 万个 slot，其中一部分被 GEM5 在错误推测上多出的约 36 万个 slot、在后端多出的约 41 万个 slot 抵消了。最后合起来，就是 IPC 相差 1.35%。

如果要继续校准，第一步是查清楚 RTL 的 `ITLBMissBubble` 在什么条件下计数，再对照 GEM5 的取指模型看能不能对上。这只是一个检查点的结果，不代表 CoreMark 整体的误差。整体误差需要把 11 个检查点都跑完，再按权重合成。

## 6. 常见问题

| 现象 | 原因 | 处理 |
|---|---|---|
| GEM5 在一千多个周期后报提交阶段卡住，ROB 为空 | 恢复程序写了 GEM5 不认识的 CSR，比如 stateen | 换 GEM5 专用恢复程序 |
| GEM5 报 difftest 不一致，PC 在 `0x800003xx` 附近，随后跳到 `0x80100508` | 恢复程序写的 CSR 在 GEM5 里是非法指令，比如 `senvcfg`，GEM5 陷入了 OpenSBI 的异常处理 | 同上。用 `--debug-flags=Exec` 查看最后执行的指令 |
| GEM5 警告恢复程序"is partially loaded" | 恢复程序比覆盖上限大，只写入了一部分 | 加 `--gcpt-restorer-size-limit=0x1000` |
| Buildroot 下载时报连接超时或域名解析失败 | 服务器访问部分国外镜像不稳定 | 一般会自动换镜像重试。频繁失败时，把 GNU 和内核的下载地址换成国内镜像 |
| 打算给裸机程序取检查点 | 裸机程序运行在机器模式，且通常链接在 `0x80000000`，与恢复程序冲突 | 不建议。用 workload-builder 打包成 Linux 镜像 |
