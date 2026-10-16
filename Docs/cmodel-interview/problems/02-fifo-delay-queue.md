# 02 环形 FIFO 与定延迟队列

建议用时 20 分钟 · 难度 ★

## 背景

几乎所有模块之间的连线在周期级模型里都写成"带延迟的队列"：
gem5 的 `TimeBuffer`、GPGPU-Sim 的 `fifo_pipeline`、各家 C-model 的 `DelayQueue` 都是这个东西。

```
 A. 环形 FIFO（容量 N=4）

     rd                 wr
     ▼                  ▼
   ┌────┬────┬────┬────┐
   │ d0 │ d1 │ d2 │    │      指针范围 [0, 2N)，多出的最高位是"圈数位"
   └────┴────┴────┴────┘      空：rd == wr
                              满：(wr - rd) mod 2N == N

 B. 定延迟队列（延迟 L=3，容量 2）

   push(x, t) ─▶ [ x | readyAt = t+3 ] [ ... ] ─▶ pop(t')   仅当 t' >= readyAt
```

## 要求

```cpp
template <typename T>
class RingFifo {
public:
    explicit RingFifo(uint32_t cap);      // cap 不要求是 2 的幂
    bool empty() const;
    bool full() const;
    uint32_t size() const;
    bool push(const T& v);                // 满时返回 false
    const T& front() const;
    void pop();
};

template <typename T>
class DelayQueue {                        // 内部用 RingFifo 实现
public:
    DelayQueue(uint32_t latency, uint32_t cap);
    bool canPush() const;
    void push(const T& v, uint64_t now);  // now + latency 拍起可以被 pop
    bool ready(uint64_t now) const;       // 队头已经到期
    T pop(uint64_t now);
};
```

不允许为了判满额外维护一个 `count` 成员（用指针本身判断）。

## 测试向量

A：容量 3。push 1、2、3 后 `full()`，push 4 失败；pop 出 1 后 push 4 成功；
之后循环 100 次"pop 一个、push 一个"，pop 出来的值依次是 2、3、4……（检查指针回绕）。

B：延迟 3、容量 2。每拍**先出队后入队**：消费者在 `ready(t)` 时 pop；生产者在 `canPush()` 时 push 一个新编号。跑 12 拍：

| | 发生的拍 |
|---|---|
| push | 0, 1, 3, 4, 6, 7, 9, 10 |
| pop  | 3, 4, 6, 7, 9, 10 |

稳态吞吐 = 2/3。想一想为什么，这是 03 题的伏笔。

## 追问

1. 不用圈数位，只用 `rd`、`wr` 两个 `[0, N)` 的指针，满和空怎么区分？各有什么代价？
2. 同一拍里队列满、但消费者也在 pop，生产者能不能 push？在模型里和 RTL 里分别怎么实现？这对吞吐有什么影响？
3. 如果延迟不是固定的（例如每个元素自带不同延迟），队头就不一定最早就绪，你会怎么改？（提示：按序 vs 乱序完成）
4. 用 `std::deque` 和自己写环形缓冲，在仿真器性能上有什么区别？
