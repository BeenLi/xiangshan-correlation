# 09 Warp 调度器与记分板

建议用时 30 分钟 · 难度 ★★

## 背景

GPU 的 SM 子分区（sub-core）每拍从若干 warp 里挑一条就绪指令发射。每个 warp 内部按序发射，
用记分板（scoreboard）挡住还没写回的寄存器；warp 之间靠调度策略交错来掩盖延迟。

```
   warp 0: pc ─▶ [ld r1] [add r2,r1] [add r3,r0]      记分板（每个 warp 一份）
   warp 1: pc ─▶ [ld r1] [add r2,r1] [add r3,r0]        regReadyAt[r] = 结果可用的周期
        │              │
        ▼              ▼
   ┌───────────────────────────────┐
   │ 就绪判断：srcs 和 dst 都已可用   │  RAW：源寄存器还没写回
   │ 调度策略：LRR / GTO            │  WAW：目的寄存器还有在飞的写
   └───────────────┬───────────────┘
                   ▼
             每拍最多发射 1 条
```

- **LRR**（loose round robin）：从上一次发射的 warp 的下一个开始轮询，选第一个就绪的。
- **GTO**（greedy-then-oldest）：上一次发射的 warp 还能发就继续发它；否则选最老的就绪 warp（本题用 warp 编号代表年龄，编号小更老）。

## 要求

```cpp
struct WInst { int dst; std::vector<int> srcs; int latency; };   // dst=-1 表示不写
enum class Policy { LRR, GTO };
class Scheduler {
public:
    Scheduler(std::vector<Warp> warps, Policy p);
    std::vector<std::string> run();   // 返回发射序列，形如 "W<warp>.<pc>@<cycle>"
};
```

- 第 t 拍发射、延迟为 L 的指令，结果在第 t+L 拍可用（那一拍依赖者就能发射）。
- 每个 warp 32 个寄存器，初始都可用。
- 初始"上一次发射的 warp"为 -1，LRR 第一拍从 warp 0 开始。

## 测试向量

两个 warp，跑同一段程序：

```
 0: r1 = ld [r0]     latency 4
 1: r2 = r1 + 1      latency 1    依赖 r1
 2: r3 = r0 + 1      latency 1    独立，但按序发射，只能排在 1 之后
```

| 策略 | 发射序列 |
|------|----------|
| LRR | W0.0@0 W1.0@1 W0.1@4 W1.1@5 W0.2@6 W1.2@7 |
| GTO | W0.0@0 W1.0@1 W0.1@4 W0.2@5 W1.1@6 W1.2@7 |

## 追问

1. GTO 相对 LRR 的优势一般体现在 cache 局部性上，为什么？什么负载下 LRR 更好？
2. 两级调度（two-level / 活跃池 + 等待池）解决什么问题？
3. 记分板只记"是否在飞"（1 bit）和记"什么时候可用"（时间戳）有什么区别？可变延迟的 load 怎么处理？
4. Volta 以后 NVIDIA 用编译器插入的控制码（stall cycles、依赖屏障）代替部分硬件记分板，模型里怎么体现？
5. 加上寄存器堆 bank 冲突、操作数收集器（operand collector）后，"发射"和"执行"之间要加什么结构？
