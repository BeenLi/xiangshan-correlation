# 03 基于 credit 的流控

建议用时 30 分钟 · 难度 ★★

## 背景

片上网络和 GPU 内部各级之间普遍用 credit 流控：发送端手里有多少 credit，就说明接收端还有多少空位，
所以**接收缓冲永远不会溢出**，也不需要 ready 信号跨长线组合传播。

```
           ┌─────────── 前向链路，延迟 Lf ───────────┐
           │                                         ▼
  ┌────────┴───────┐                       ┌──────────────────┐
  │ Sender          │                       │ Receiver          │
  │ credits = B     │                       │ 缓冲深度 B         │──▶ consumer (ready(t) 可能为假 = 反压)
  └────────▲───────┘                       └────────┬─────────┘
           │                                         │ 每消费一个，回一个 credit
           └─────────── credit 回程，延迟 Lc ◀───────┘
```

## 要求

写一个函数，模拟 `cycles` 拍，发送端**一直有数据**要发：

```cpp
struct Result {
    uint64_t delivered;      // 接收端消费掉的 flit 数
    uint32_t maxOccupancy;   // 接收缓冲最大占用
};
Result simulate(uint32_t B, uint32_t Lf, uint32_t Lc, uint64_t cycles,
                const std::function<bool(uint64_t)>& consumerReady);
```

每拍内的顺序（必须遵守）：

1. **到达**：第 t 拍到期的 flit 进入接收缓冲；到期的 credit 加回发送端计数器。
2. **消费**：接收缓冲非空且 `consumerReady(t)`，消费一个，并在本拍发出一个 credit（第 `t+Lc` 拍到达）。
3. **发送**：`credits > 0` 就发一个 flit（第 `t+Lf` 拍到达），`credits--`。

复位时 `credits = B`。在函数里用 `assert` 保证接收缓冲占用从不超过 B。

## 测试向量

（a）`Lf = Lc = 2`，消费者一直 ready，跑 10000 拍，吞吐 = delivered / cycles：

| B | 1 | 2 | 3 | 4 | 5 | 6 |
|---|---|---|---|---|---|---|
| 吞吐 | 0.25 | 0.50 | 0.75 | 1.00 | 1.00 | 1.00 |

（b）`B=4, Lf=Lc=2`，消费者只在偶数拍 ready：吞吐 0.5，缓冲占用不超过 4。

（c）`B=3, Lf=5, Lc=1`，消费者在第 200~299 拍不 ready：最大占用正好是 3，不丢数据、不溢出。

## 追问

1. 推导：满吞吐需要的最小缓冲深度是多少？用题目的拍内顺序，答案是 `Lf + Lc`；
   如果"到达的 flit 下一拍才能被消费"或者"credit 到达后下一拍才能用"，答案各变成多少？
2. 为什么说 credit 流控把"反压"从组合路径变成了时序路径？和 valid/ready 相比优缺点是什么？
3. 多个虚通道（VC）共享一条物理链路时，credit 要按什么粒度维护？
4. 如果接收端一次消费会释放不定数量的空间（例如变长包），credit 怎么设计？
5. 怎么在模型里检测 credit 泄漏（credit 丢了导致吞吐慢慢下降）？
