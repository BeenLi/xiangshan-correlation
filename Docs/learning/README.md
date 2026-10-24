> 本目录是 xiangshan-correlation 工作区的学习文档，随仓库保存。Obsidian 侧只保留索引。


# xiangshan-correlation 学习计划：用六大思想读一个 Cmodel 与 RTL 的差距

> 这是 CPU → GPU → NPU 三平台学习的第一站。学习对象不是"香山"这个抽象名词，而是本仓库这个 `xiangshan-correlation` 工作区：一份昆明湖 V2 RTL 和一份号称按它校准过的 XS-GEM5，在同一条指令流上跑出 1.35% 的 IPC 差距。**差距本身就是教材**：每一个对不上的计数器，都是两个模型对同一个体系结构思想的两种实现在打架。
>
> 坐标系来自 [six-ideas-in-CA](https://tnm.engin.umich.edu/)；教科书锚点在 Computer Architecture A Quantitative Approach；LSU 的概念底座沿用 LSU 核心概念 - 访存的一生与六大机制、gem5 load 流水线建模 - TimeBuffer 与时序保真、TLB 私有与页表共享 - sfence 与 IPI shootdown。
>
> 存放规则：能进仓库 `Docs/` 的资料随 GitHub 走，本笔记只做索引和计划。旧的 xiangshan_lsu_learning 写的是 myDevbox 上的 kmhv3 环境，环境部分已被本篇取代，概念路线仍有效。

---

## 0. 工作区现状快照（2026-10-20 勘察，只读）

仓库 16 GB，git 单仓库拍平了所有组件，基线标签 `kmhv2-baseline`。

| 组件 | 版本 | 已构建？ | 备注 |
| --- | --- | --- | --- |
| XS-GEM5 | `KMH-V2` 标签 d11fc1cb55 + 本地 1 提交 | 是，`gem5.opt` 9 月 23 日 | 配置入口 [`configs/example/kmhv2.py`](../../GEM5-XS/configs/example/kmhv2.py)，[`kmh_align=True`](../../GEM5-XS/configs/example/kmhv2.py#L30) |
| XiangShan RTL | `kunminghu-v2` f86f50b83 | 是，`build/emu` 9 月 23 日 | Verilator 5.050，8 线程，`WITH_DRAMSIM3=1` |
| NEMU（两份） | gem5 参考 d6fb8ed4；RTL 用 ready-to-run 预编译 so | 是 | difftest 的 REF，两边**故意不同版本** |
| NEMU-cpt + SimPoint | master 3d9f9fb8 | 是 | 生成检查点 |
| workload-builder | b7c321d，Buildroot 2026.08 | 是 | Linux 版 CoreMark 已打包 |
| gem5_data_proc | graphmaker 80316ce | .venv 已装 | top-down 解析、加权合成 |

已有数据：CoreMark 11 个 SimPoint 检查点（`coremark-linux-20m`），只跑了 78 号（权重 16.9%）。结果与分解已写在仓库 [`Docs/checkpoint.md` §5.5](../checkpoint.md#55-已验证的结果)，摘要如下。

| | GEM5 kmhv2 | RTL EMU |
| --- | --- | --- |
| 采样段 IPC | 2.942 | 2.903 |
| 分支误预测重定向 | 11,364 | 11,476 |
| L1D 缺失 / DRAM 读 | 0 / 258 | 1 / — |
| 仿真耗时 | 约 25 分钟 | 约 6 小时（未绑 NUMA） |

Top-down 分解（派发宽度 6）：RTL 前端多出约 108 万 slot，被 GEM5 在错误推测上多出的 36 万和后端多出的 41 万部分抵消，净差 1.35%。工作集全在 L1，**这个点只能校准核内流水线，校准不了存储层次**。

机器状态：96 核，503 GB 内存已用 391 GB。RTL 一个检查点 6 小时，是整个学习计划**最紧的约束**，排期全围绕它。

### 本次勘察核实的三件事

1. **核参数两边确实对齐**。`kmhv2.py` 默认实例化 [`XiangshanCore`](../../GEM5-XS/configs/common/xiangshan.py#L27)，取 [`BaseO3CPU.py`](../../GEM5-XS/src/cpu/o3/BaseO3CPU.py#L132) 的默认值：decode 6 / rename 6 / commit 8 / ROB 160 / LQ 72 / SQ 56，与 RTL [`Parameters.scala`](../../XiangShan/src/main/scala/xiangshan/Parameters.scala#L149) 逐项相同。[`configs/common/xiangshan.py`](../../GEM5-XS/configs/common/xiangshan.py#L30) 里 ROB 150 / LQ 48 的那组是 `XiangshanECore`，只在 [`--xiangshan-ecore`](../../GEM5-XS/configs/example/kmhv2.py#L37) 时启用，2.4 GHz 也只在那个分支里，README 说的 3 GHz 成立。
2. **寄存器缓存是一个未对齐点的候选**。RTL [`backend/regcache/`](../../XiangShan/src/main/scala/xiangshan/backend/regcache/RegCache.scala#L27) 有完整的 RegCache（tag 表、age 计时器），GEM5 `o3/` 里只有 [`issue_queue.hh`](../../GEM5-XS/src/cpu/o3/issue_queue.hh#L311) 用一个 LRU 集合近似它，没有独立模块。这是"Locality 用在寄存器堆上"的一处两边实现不同。
3. **`ITLBMissBubble` 的归因可能是统计口径问题**。RTL 采样段这项有 92 万 slot，但 PTW 访问为 0。[`IFU.scala:170`](../../XiangShan/src/main/scala/xiangshan/frontend/IFU.scala#L170) 附近：这个 reason 在 IFU 的 stage 1 由 `itlbMissBubble` 这根线打上，然后随 `topdown_stages` 流水传下去，"只由时钟驱动、不看 valid-ready"。所以它更像"取指级暂时不能出活"的兜底分类，而非真正的 TLB 缺失。这就是计划里的第一个动手任务。

---

## 1. 组织视角：差距 = 两个模型对同一思想的两种实现

[six-ideas-in-CA](https://tnm.engin.umich.edu/) §7 把 "branch predictor + speculative OoO" 当一行看。这里要把粒度拆到子系统，而且每个子系统都有**两份代码**可以对照读：RTL（Chisel）说"硬件用什么兑现"，GEM5（C++）说"该发生什么、第几拍"。

| 思想 | 香山 RTL 落在哪 | XS-GEM5 落在哪 | 已知或疑似差距 | 教科书锚点 |
| --- | --- | --- | --- | --- |
| Prediction | [`frontend/`](../../XiangShan/src/main/scala/xiangshan/frontend/BPU.scala)：Tage、SC、ITTAGE、RAS、FTB、FauFTB、NewFtq | [`cpu/pred/btb/`](../../GEM5-XS/src/cpu/pred/btb/decoupled_bpred.cc)（DecoupledBPUWithBTB）、[`pred/ftb/`](../../GEM5-XS/src/cpu/pred/ftb/decoupled_bpred.cc)，[`o3/fetch.md`](../../GEM5-XS/src/cpu/o3/fetch.md) | 误预测次数只差 1%，但前端 slot 差 108 万：预测**内容**对齐了，预测**时序**（几级延迟、几个空泡）没完全对齐 | H&P Ch3 §3.3、附录 C |
| Speculation | [`backend/rob/`](../../XiangShan/src/main/scala/xiangshan/backend/rob/Rob.scala#L57)、[`ctrlblock/`](../../XiangShan/src/main/scala/xiangshan/backend/ctrlblock/RedirectGenerator.scala) 的 redirect，`mem/mdp/` 访存依赖预测 | [`o3/commit.cc`](../../GEM5-XS/src/cpu/o3/commit.cc#L669)、[`store_set.cc`](../../GEM5-XS/src/cpu/o3/store_set.cc#L42)（[`kmh_align 时关掉 storeSet 训练`](../../GEM5-XS/configs/common/xiangshan.py#L341)） | GEM5 错误推测 slot 多 36 万；top-down 两边归因逻辑不同 | H&P Ch3 §3.6 |
| Indirection | [`backend/rename/`](../../XiangShan/src/main/scala/xiangshan/backend/rename/Rename.scala#L40)、`regfile/`；`cache/mmu/`（TLB、PTW） | [`o3/rename_map.cc`](../../GEM5-XS/src/cpu/o3/rename_map.cc#L132)、[`free_list.cc`](../../GEM5-XS/src/cpu/o3/free_list.cc#L41) | difftest 本身就是一层 REF/DUT 的名字与实现解耦，[`Docs/difftest.md` §4](../difftest.md) 讲 REF 为什么要被"告知" | H&P Ch3 §3.4、Ch2 §2.4 |
| Parallelism | [`backend/issue/`](../../XiangShan/src/main/scala/xiangshan/backend/issue/IssueQueue.scala#L82)、[`dispatch/`](../../XiangShan/src/main/scala/xiangshan/backend/dispatch/NewDispatch.scala#L49)、`exu/` | [`o3/issue_queue.cc`](../../GEM5-XS/src/cpu/o3/issue_queue.cc)、[`inst_queue.cc`](../../GEM5-XS/src/cpu/o3/inst_queue.cc#L106)、[`FUScheduler.py`](../../GEM5-XS/configs/common/FUScheduler.py#L105)（KunminghuScheduler） | 后端 slot GEM5 多 41 万，可能是发射队列模型、功能单元延迟表的差 | H&P Ch3 §3.7、§3.8 |
| Locality | [`backend/regcache/`](../../XiangShan/src/main/scala/xiangshan/backend/regcache/RegCache.scala#L27)、`cache/dcache/`、`mem/prefetch/`、`mem/sbuffer/`、coupledL2 | [`o3/regfile.cc`](../../GEM5-XS/src/cpu/o3/regfile.cc#L53)（regcache 只在 issue_queue 里近似）、[`CacheConfig.py`](../../GEM5-XS/configs/common/CacheConfig.py#L308)（`do_fast_writeline`、`pipe_latency=3`）、[`PrefetcherConfig.py`](../../GEM5-XS/configs/common/PrefetcherConfig.py#L34) | RegCache 两边不对称；coupledL2 在 GEM5 标签之后升级过两次；CoreMark 点测不出这一层 | H&P Ch2、附录 B |
| Specialization | 预测器、ROB、LSQ 这些"专用结构"本身；`kmh_align` 这组开关 | 同左 | `kmh_align=True` 关掉了 GEM5 自己的一些优化（temporal prefetch、fast writeline、store pf train）来迁就 RTL，这组开关就是一份"Cmodel 为了像 RTL 放弃了什么"的清单 | H&P Ch7 的反向：通用核里的专用部件 |

一句话的读法：**Prediction 和 Speculation 看前端与 redirect，Indirection 看 rename 与 difftest，Parallelism 看 issue，Locality 看 regcache 与 cache 层次**。每读一个子系统，就问"这个思想在 RTL 和 Cmodel 里各自长什么样，差在哪一拍"。

---

## 2. 排期：三步走（2026-10-20 按你的反馈重排）

你的背景：乱序核原理和 LSU 很熟，前端不熟；课本之外，香山新加的特性和**流水级划分里的时序考量**是薄弱点。所以顺序改成"先把机器本身看懂，再校准看它的尺子，最后才用尺子量差距"。RTL 仿真 6 小时一个点的约束不变，前两步都不需要新仿真。

### 第一步：香山流水线微架构，RTL 与 gem5 对照（前端为重）

目标：画出昆明湖 V2 从 BPU 到 commit 的每一级，标出每级在 RTL 里是哪个模块、哪根 fire 信号，在 gem5 里是哪个 stage、哪个 delay 参数；课本没有的部分（解耦前端、FTQ、FTB、三级 BPU 覆盖、IBuffer 分 bank、RegCache、ROB 压缩、issue 的 og0/og1 两拍）单独标注。

- [x] 总览笔记：[01 香山流水线微架构总览 - RTL 与 gem5 对照](01-pipeline-overview.md)。只放地图：昆明湖整体架构图、前端和后端的级划分对照图、待核实清单，细节分篇的链接也在总览里。
- [x] 前端精读 1（2026-10-20，结论在 [01-2 三级预测流水与 override](01-2-bpu-override.md) 的"精读结论"）：BPU 三级的 override 机制。RTL 看 [`BPU.scala`](../../XiangShan/src/main/scala/xiangshan/frontend/BPU.scala#L704) 的 `s2_redirect`、`s3_redirect` 与 [`overrideBubble`](../../XiangShan/src/main/scala/xiangshan/frontend/BPU.scala#L1088)；gem5 看 [`pred/ftb/decoupled_bpred.cc`](../../GEM5-XS/src/cpu/pred/ftb/decoupled_bpred.cc#L798) 的空泡处理。问题：两边各自在哪一拍发现"后级推翻前级"，补几个空泡。
- [x] 前端精读 2（2026-10-24，结论在 [01-1 解耦前端：FTQ 与 FTB](01-1-ftq-decoupled-frontend.md)）：FTQ 的四个指针（bpuPtr、ifuPtr、pfPtr、commPtr，另有 ifuWbPtr、robCommPtr）各自推进条件。gem5 的对应物是 FSQ（[`decoupled_bpred.cc`](../../GEM5-XS/src/cpu/pred/ftb/decoupled_bpred.cc#L1817)），不是同名的 [`fetch_target_queue.cc`](../../GEM5-XS/src/cpu/pred/ftb/fetch_target_queue.cc#L15)。问题：取指落后预测多少、谁反压谁。
- [ ] 前端精读 3：IFU f0 到 f3 各拍做什么（f1 算 PC、f2 等 ICache 回数据加异常合并、f3 预译码加 MMIO 状态机），`f3_fire = toIbuffer.fire`。gem5 的 fetch 把这几拍压成 `fetchToDecodeDelay = 2`。这是第一个"级划分的时序考量"样本。
- [ ] 前端精读 4：ICache 与 ITLB。RTL 看 [`ICache.scala`](../../XiangShan/src/main/scala/xiangshan/frontend/icache/ICache.scala#L590) 的 mainPipe、missUnit、IPrefetch 和 [`Frontend.scala`](../../XiangShan/src/main/scala/xiangshan/frontend/Frontend.scala#L172) 的 ITLB；gem5 看 fetch 怎么发 L1I 请求、ITLB 缺失怎么处理。问题：命中、ICache 缺失、ITLB 缺失三种情况各要几拍，两边是否一致；gem5 有没有 FDIP。
- [ ] 后端走读：decode（含 fusion）→ rename（CompressUnit、snapshot、move 消除）→ NewDispatch → IssueQueue（enq、select、og0 读寄存器、og1 旁路）→ exu → wb → Rob。只看每级的 valid-ready 和重定向恢复，不深挖 LSU（已有 LSU 核心概念 - 访存的一生与六大机制）。
- [ ] 同步做仓库 `Docs/cmodel-interview` 的 04 流水线 stall/flush 与 08 事件队列，手感对应上面两种建模方式。

### 第二步：Top-down 计数器的统计语义是否对齐

目标：先读 [Top-Down Method](https://doi.org/10.1109/ISPASS.2014.6844459)（ISPASS'14）抓住四条原则：分类点在 issue、以 slot 为粒度、bad speculation 是顶层闸门、只有兄弟节点可比。然后拿这四条逐一审两边的实现。

- [ ] RTL：[`package.scala:948`](../../XiangShan/src/main/scala/xiangshan/package.scala#L948) 的 `TopDownCounters` 枚举全表，追每个 reason 在哪个模块、哪一拍打标，重点是 IFU 里"只由时钟驱动、不看 valid-ready"的 `topdown_stages` 传递方式。
- [ ] gem5：`o3/` 里对应的 stall 分类（`IcacheStall`、`BpStall`、`FTQBubble`、`FetchFragStall`、`SquashStall` 等）在哪个 stage 计数，分类点是不是同一个"issue 点"。
- [ ] 产出：两边 top-down 口径对照表（放仓库 `Docs/`），每行标"语义一致 / 名字同义义不同 / 一边没有"。`ITLBMissBubble` 92 万 slot 却 0 次 PTW 这件事，预期在这一步得到解释。

### 第三步：78 号点 1.35% 的来源

有了第一步的流水线图和第二步的口径表，再回到 [`Docs/checkpoint.md` §5.5](../checkpoint.md#55-已验证的结果) 的分解：前端 RTL 多 108 万 slot、错误推测 gem5 多 36 万、后端 gem5 多 41 万。

- [ ] 用 gem5 做单变量实验（25 分钟一轮）验证第一步发现的级划分差异，看前端 slot 能否向 RTL 靠拢。
- [ ] RTL 侧后台跑完剩余 10 个 CoreMark 检查点（先查负载、绑 NUMA），把"一个点"变成"加权整体"。
- [ ] 存储层次留到 SPEC（需要 ISO）；没有 ISO 之前用 gapbs 代替。

---

## 3. 与三平台路线的衔接

CPU 这一站结束时，希望手里有三样东西：一张"六大思想 × 子系统 × 两份实现"的对照表，一份 Cmodel 和 RTL 校准的工作方法（检查点、difftest、top-down 对账），以及 11 道周期级建模自测题的手感。后两样直接带到 GPU（vault 里的 `tiny-gpu`，自测题 09/10/11 已经是 GPU 题）和 NPU（`tiny-NPU`）。[six-ideas-in-CA](https://tnm.engin.umich.edu/) §7 那张表里 GPU 和 TPU 各自"刻意舍弃"的 Prediction 和 Speculation，正好是 CPU 这一站学得最深的两项，对比会很直接。

---

## 4. 索引：资料在哪

| 内容 | 位置 |
| --- | --- |
| 工作区总览、锁定版本、使用方法 | 仓库 [`README.md`](../../README.md) |
| difftest 调用约定、REF 为什么要被告知 | 仓库 [`Docs/difftest.md`](../difftest.md) |
| 检查点生成、恢复、78 号点的全部数据 | 仓库 [`Docs/checkpoint.md`](../checkpoint.md) |
| 周期级建模自测题 | 仓库 `Docs/cmodel-interview/` |
| 78 号点两边 top-down 原始列 | 仓库 `runs/analysis/coremark_78/` |
| 思想坐标系 | [six-ideas-in-CA](https://tnm.engin.umich.edu/) |
| 教科书 | Computer Architecture A Quantitative Approach |
| LSU 概念与 gem5 建模 | LSU 核心概念 - 访存的一生与六大机制、gem5 load 流水线建模 - TimeBuffer 与时序保真 |
