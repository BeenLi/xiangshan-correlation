> 本文是 xiangshan-correlation 工作区的学习文档，随仓库保存；图在 `figs/`，来源见 [`figs/README.md`](figs/README.md)。文中的 `文件:行号` 都是相对本仓库的链接，在 VS Code 打开工程后点击可直接跳到对应代码行，GitHub 上同样可点。


# 00 两种写法：gem5 的建模手法与 RTL 的实现写法

> 读 01 到 03 时反复碰到的写法，集中在这里讲一次，各篇只链过来。每一项先讲 RTL 怎么写，再讲 gem5 怎么建模。动手练习见仓库 [`Docs/cmodel-interview`](../cmodel-interview/README.md) 的 04 流水线 stall/flush 与 08 事件队列。框架已搭，内容待写。标 ✅ 的是直接从代码或配置读出的事实，标 ❓ 的是待核实。

---

## 0. 读法约定：RTL 写的是实现，gem5 写的是行为和延迟

RTL 和 gem5 描述的是同一条流水线，但写法不同。

- **RTL 写的是实现**：每一拍都靠一个真实的寄存器来实现。读 RTL 要弄清"哪根信号在第几拍有效，是哪个寄存器把它从上一拍传过来的"。
- **gem5 写的是行为和延迟**：只说每一级做什么、相邻两级之间隔几拍，不建模寄存器。两级之间用 `TimeBuffer` 连接：上游写 `getWire(0)`（[`decode.cc:207`](../../GEM5-XS/src/cpu/o3/decode.cc#L207)），下游读 `getWire(-delay)`（[`decode.cc:216`](../../GEM5-XS/src/cpu/o3/decode.cc#L216) 的 `fetchQueue->getWire(-fetchToDecodeDelay)`），每拍结束时统一 `advance()`（[`cpu.cc:586`](../../GEM5-XS/src/cpu/o3/cpu.cc#L586)）✅。所以 `delay = N` 就相当于 RTL 里两级之间隔着 N 个寄存器。

对照着读时分两步：

1. **先在 RTL 里数寄存器。** RTL 里的"一级"有两种来源，和拍数的关系不一样：
   - **寄存器边界**：模块内部的流水寄存器，例如 IFU 的 f0→f3、BPU 的 s0→s3。每过一个这样的边界就要花一拍。
   - **模块边界**：两个 Scala 模块之间的接口。端口本身只是连线，要不要花一拍，取决于接口处有没有寄存器。例如 Rename 读 RAT 的请求在 Decode 级就已经发出，跨了模块却不多花一拍。

   所以，模块边界不一定是拍数的边界，一个模块内部也可能有好几拍。
2. **再到 gem5 里找对应的延迟参数。** RTL 模块内部的那几拍，gem5 一般不单独分级，而是合并成一个参数来表示，例如 BPU 各级对应每个预测器的 `numDelay`。IFU 的 f0→f3 加上 IBuffer 这一段，在 gem5 里只有 `fetchToDecodeDelay = 2` 加一个 fetchQueue；ICache 命中时从发请求到进 Decode，RTL 4 拍、gem5 3 拍，差在 ICache 命中延迟 ✅，见 [01-3](01-3-ifu.md)。RTL 两个模块之间的边界，在 gem5 里对应一个 stage 加一个 TimeBuffer 位置，例如 `decodeToRenameDelay = 1`。

比较时，看的是一段路径的总拍数，不是级名能不能一一对上。总拍数对不上的地方就是 correlation 要重点检查的地方。例如 RTL 有独立的 [`NewDispatch`](../../XiangShan/src/main/scala/xiangshan/backend/dispatch/NewDispatch.scala#L49) 模块，而 gem5 的 [`enableDispatchStage`](../../GEM5-XS/src/cpu/o3/BaseO3CPU.py#L145) 默认为 False，[`kmhv2.py`](../../GEM5-XS/configs/example/kmhv2.py) 也没有打开它（见 [01 总览](01-0-pipeline-overview.md) §0 的后端图）。

---

## 1. 对照总表

| 主题 | RTL 写法 | gem5 手法 |
| --- | --- | --- |
| 两级之间隔几拍 | 级间寄存器（RegNext、RegEnable、PipelineConnect） | TimeBuffer：上游写 `getWire(0)`，下游读 `getWire(-delay)` |
| 一拍之内谁先谁后 | 组合逻辑与时钟沿；valid-ready 握手、同拍反向的 ready 链 | tick 与事件队列：各级在一个 tick 里的执行顺序、事件优先级 |
| 冲刷怎么传播 | flush / redirect 信号与 `robIdx.needFlush` | squash 经 TimeBuffer 往前级传 |
| 推翻前级的预测 | 先写后改：s1 结果先进 FTQ，后级推翻时回拉指针、冲掉 IFU | 同一拍全部算完，再用 `numOverrideBubbles` 补空泡 |
| 参数从哪来 | `Parameters.scala` 与 Config 层层覆盖 | `kmhv2.py` → `xiangshan.py` → `BaseO3CPU.py` 默认值 |

---

## 2. 两级之间隔几拍

（待写）

## 3. 一拍之内谁先谁后

（待写）

## 4. 冲刷怎么传播

（待写）

## 5. 推翻前级的预测

（待写）

## 6. 参数从哪来

（待写）
