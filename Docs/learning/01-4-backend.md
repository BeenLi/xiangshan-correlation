> 本文是 xiangshan-correlation 工作区的学习文档，随仓库保存；图在 `figs/`，来源见 [`figs/README.md`](figs/README.md)。文中的 `文件:行号` 都是相对本仓库的链接，点击可直接跳到对应代码行。


# 01-4 后端：课本之外的五件事（走读级，待精读）

> 属于 [01 香山流水线微架构总览：RTL 与 gem5 对照](01-pipeline-overview.md)，读法约定和全流水对照图见总览。标 ✅ 的是直接从代码或配置读出的事实，标 ❓ 的是待核实，🆕 是课本（Computer Architecture A Quantitative Approach Ch3）没有的东西。

| 🆕 特性 | RTL 位置 | gem5 对应 | 一句话 |
| --- | --- | --- | --- |
| 宏融合 | [`decode/FusionDecoder.scala`](../../XiangShan/src/main/scala/xiangshan/backend/decode/FusionDecoder.scala#L538)，`DecodeStage` 有 `fusion` 向量 | [`o3/decode.cc`](../../GEM5-XS/src/cpu/o3/decode.cc#L99) 有 fusion 逻辑 ✅，规则是否同一套 ❓ | 把两条相邻指令合成一个 uop，影响 IPC 的分母 |
| ROB 压缩 | [`rename/CompressUnit.scala`](../../XiangShan/src/main/scala/xiangshan/backend/rename/CompressUnit.scala#L38)，`needRobFlags`、`instrSizes` | [`RobCompressPolicy = 'kmhv2'`](../../GEM5-XS/src/cpu/o3/BaseO3CPU.py#L223)，`CROB_instPerGroup = 6` ✅ | 多条指令共用一个 ROB 项，160 项的"有效容量"大于 160 |
| 重命名快照 | [`rename/Snapshot.scala`](../../XiangShan/src/main/scala/xiangshan/backend/rename/Snapshot.scala#L26)，`io.snpt` | ❓ gem5 用 [`robWalkPolicy = 'Replay'`](../../GEM5-XS/src/cpu/o3/BaseO3CPU.py#L158)，`squashWidth = 8` | 分支误预测恢复 RAT 的两种路子：快照 vs 走 ROB |
| move 消除 | [`Rename.scala`](../../XiangShan/src/main/scala/xiangshan/backend/rename/Rename.scala#L260) 的 `isMove`，不分配物理寄存器 | ❓ | 课本提过，但 RTL 里 walk 时还要特殊处理 `walkIsMove` |
| RegCache | [`backend/regcache/`](../../XiangShan/src/main/scala/xiangshan/backend/regcache/RegCache.scala#L27)，[`IntRegCacheSize = 16`](../../XiangShan/src/main/scala/xiangshan/Parameters.scala#L208)，DataPath 的 [`s1_RCReadData`](../../XiangShan/src/main/scala/xiangshan/backend/datapath/DataPath.scala#L469) | [`issue_queue.hh`](../../GEM5-XS/src/cpu/o3/issue_queue.hh#L311) 里一个整数专用的 LRU 集合 `regCache`，命中时允许 src1 借用 src0 的读端口（[`TX dynamic port optimization`](../../GEM5-XS/src/cpu/o3/issue_queue.cc#L591)）✅ | Locality 用在寄存器堆上；gem5 用一个 LRU 集合近似，只影响读端口分配，不建数据通路 ❓ 容量是否也是 16 |

发射到执行的两拍 ✅：香山 IssueQueue 出队后不是直接到 EXU，而是经过 **og0**（读寄存器堆或 RegCache）和 **og1**（旁路网络、立即数）两级，`og0Cancel`、`og1Cancel` 可以在这两拍取消已发射的 uop（例如 load 唤醒失败）。[`IssueQueue.scala:52`](../../XiangShan/src/main/scala/xiangshan/backend/issue/IssueQueue.scala#L52) 的 `og0Resp`、`og1Resp`、`og2Resp`（向量才有）就是这个。gem5 的 [`issue_queue.cc`](../../GEM5-XS/src/cpu/o3/issue_queue.cc) 怎么表达这两拍和取消，是后端精读的第一个问题。

一个漂亮的对齐证据 ✅：gem5 [`FUScheduler.py`](../../GEM5-XS/configs/common/FUScheduler.py#L105) 的 `KunminghuScheduler` 把每个发射端口的寄存器读端口号（`IntRD(0,0)`、`IntRD(6,1)` 这类）写得和 RTL [`Parameters.scala:400`](../../XiangShan/src/main/scala/xiangshan/Parameters.scala#L400) 的 `ExeUnitParams` 一模一样。Cmodel 不只是在模仿延迟，还在模仿读端口的争用结构。
