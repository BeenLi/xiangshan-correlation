# 08 离散事件队列（gem5 风格）

建议用时 30 分钟 · 难度 ★★

## 背景

gem5 不是每拍遍历所有模块，而是一个按时间排序的事件队列：模块把"下一次要做的事"调度进去，
仿真主循环不断取出最早的事件执行。没有事件的时间段直接跳过，这是它比纯周期驱动快的原因。

```
 EventQueue（按 (when, priority, seq) 排序）

   ┌────────────┬────────────┬────────────┬────────────┐
   │ E2  @5  p0 │ E3 @10 p-1 │ E1 @10 p0  │ E5 @12 p0  │ ...
   └────────────┴────────────┴────────────┴────────────┘
        ▲ 取出最早的，curTick = 5，执行回调；回调里可以再 schedule 新事件

 ClockedObject 的写法：
   tick() { 干活; schedule(&tickEvent, curTick + period); }
```

## 要求

```cpp
using Tick = uint64_t;
class Event {
public:
    Event(std::string name, std::function<void()> fn, int pri = 0);  // pri 越小越先执行
    bool scheduled() const;
    Tick when() const;
};
class EventQueue {
public:
    Tick curTick() const;
    void schedule(Event* e, Tick when);     // 不能调度到过去，不能重复调度
    void deschedule(Event* e);
    void reschedule(Event* e, Tick when);   // 已调度则先取消
    void serviceUntil(Tick until);          // 执行所有 when <= until 的事件，之后 curTick = until
};
```

- 排序：`when` 小的先；同 `when` 比 `pri`，小的先；再相同按调度（插入）顺序。
- `deschedule` 要做到 O(log n)，不能线性扫描。
- 回调里允许调度新事件，包括重新调度自己。

再写一个 `ClockedCounter`：构造时给周期 `period`，`start(t)` 后每 `period` 拍执行一次 `tick()`，
`tick()` 里记日志并重新调度自己；`stop()` 取消下一次 tick。

## 测试向量

（a）E1@10 p0，E2@5 p0，E3@10 p-1，E4@10 p0（在 E1 之后调度），E5@7 p0；
然后 `deschedule(E4)`，`reschedule(E5, 12)`，`serviceUntil(100)`。
执行顺序：`E2 E3 E1 E5`。

（b）A 周期 3、B 周期 5，都从 0 开始（A 先 start）。`serviceUntil(10)` 后停掉 B，再 `serviceUntil(20)`。
日志前 7 项：`A@0 B@0 A@3 B@5 A@6 A@9 B@10`；A 共 7 次（0~18），B 共 3 次。

## 追问

1. 为什么需要 `priority`？举一个 gem5 里同一 tick 两个事件顺序不同会导致结果不同的例子。
2. 用 `std::priority_queue` 实现时，`deschedule` 怎么办？（惰性删除 / 版本号）各有什么坑？
3. gem5 的事件队列实际是什么数据结构？为什么那样设计？
4. 一个周期驱动模型（每拍调用所有模块的 `tick`）怎么改成事件驱动的？哪些模块适合"空闲时不调度自己"？
5. 多线程并行仿真时，事件队列怎么拆分？时间同步怎么做（quantum）？
