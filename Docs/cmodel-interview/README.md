# 体系结构建模手撕代码自测题（C++ 周期级模型）

面向 GPU / CPU 体系结构建模岗位（例如 NVIDIA GPU Architecture Modeling）的白板/在线编程自测。
每道题都是面试里常见的"给你 30~45 分钟，用 C++ 写一个周期级模型"的形式。

```
cmodel-interview/
├── README.md          本文件：用法、通用约定、评分标准
├── Makefile           make 编译并运行全部参考答案
├── problems/          题目（只看这里，先自己写）
└── solutions/         参考答案（.cpp 可直接编译运行，README.md 是讲解和追问答案）
```

## 怎么用

1. 打开 `problems/NN-*.md`，按题目给的接口和约定自己写，**不要先看 solutions/**。
2. 用题目末尾的"测试向量"自测：把期望值写成 `assert`，跑通再说。
3. 计时：基础题 20~30 分钟，综合题 40~45 分钟（见下表）。
4. 写完对照 `solutions/NN_*.cpp` 和 `solutions/README.md` 里的讲解，重点看"周期对不对齐""握手顺序"。
5. 最后过一遍"追问"，口头回答，这部分在面试里通常比代码本身更拉开差距。

运行全部参考答案：

```bash
cd Docs/cmodel-interview && make        # 编译并运行 11 个参考答案，全部打印 all tests passed
make clean
```

| # | 题目 | 考点 | 建议用时 | 难度 |
|---|------|------|---------|------|
| 01 | [仲裁器](problems/01-arbiter.md) | 固定优先级、轮询、包锁定、位运算技巧 | 20 min | ★ |
| 02 | [环形 FIFO 与延迟队列](problems/02-fifo-delay-queue.md) | 指针回绕、满空判断、定延迟建模 | 20 min | ★ |
| 03 | [Credit 流控](problems/03-credit-flow-control.md) | credit 往返、缓冲深度与吞吐、反压 | 30 min | ★★ |
| 04 | [五级流水线](problems/04-pipeline-stall-flush.md) | valid/ready、stall、flush、反压、两阶段更新 | 45 min | ★★★ |
| 05 | [DRAM bank 状态机](problems/05-dram-bank-fsm.md) | FSM、时序约束 tRCD/tCL/tRP/tRAS | 30 min | ★★ |
| 06 | [组相联 Cache](problems/06-set-assoc-cache.md) | 地址拆分、LRU、写回写分配、sector cache | 30 min | ★★ |
| 07 | [MSHR 非阻塞 Cache](problems/07-mshr.md) | 初级/次级缺失、合并、资源满反压 | 30 min | ★★ |
| 08 | [离散事件队列](problems/08-event-queue.md) | gem5 EventQueue、优先级、自调度时钟 | 30 min | ★★ |
| 09 | [Warp 调度器与记分板](problems/09-warp-scheduler.md) | RAW/WAW、LRR vs GTO | 30 min | ★★ |
| 10 | [合并访存与 bank conflict](problems/10-coalescing-bank-conflict.md) | 128B 行 / 32B sector、32 bank、广播、padding | 20 min | ★ |
| 11 | [Crossbar](problems/11-crossbar.md) | HOL 阻塞、VOQ、iSLIP、输出反压 | 45 min | ★★★ |

建议顺序：01 → 02 → 03 → 04 是一条线（仲裁、队列、流控、流水线是所有周期级模型的积木）；
05、06、07 是存储系统；08 是仿真器框架；09、10、11 是 GPU 特色题。

## 通用约定（所有题都按这个来，否则周期数对不上）

### 1. 时间

- 周期从 0 开始。`tick(t)` 表示"第 t 拍发生的事"。
- **延迟 L 的含义**：第 t 拍发出的东西，在第 `t + L` 拍**可以被使用**（那一拍的逻辑里就能看到）。
  延迟 1 = 下一拍可用，相当于中间隔了一级寄存器。
- 同一拍内多个事件的先后顺序，每道题都会写明，例如"先处理到达，再消费，再发送"。

### 2. 两阶段更新（evaluate / commit）

周期级模型最常见的 bug 是"同一拍里数据穿过了两级"。写法上有两种安全做法：

```
做法 A：读旧写新                         做法 B：从下游往上游更新
  next = f(cur)   // 只读 cur             for stage = last .. first:
  cur  = next     // 拍末统一提交             if 下游有空: 下游 = 本级, 本级 = 空
```

做法 A 和 RTL 的 `always @(posedge clk)` 一一对应，更推荐。04 题的参考答案用的是 A。

### 3. valid / ready 握手

```
          valid ──────────▶
 上游                           下游
          ◀────────── ready
          data  ──────────▶

 fire = valid && ready    // 只有这一拍才真正传输
```

- `ready` 只能依赖下游状态，不能依赖本拍的 `valid`，否则会出组合环。
- 流水线里 `ready_in[i] = !valid[i] || fire_out[i]`：本级空，或本级这拍会走掉，就能收新的。
  这条 ready 链从最后一级组合地传到第一级，是关键路径，追问里常让你用 skid buffer 打断它。

### 4. 代码规范

- C++17，只用标准库。`g++ -std=c++17 -Wall -Wextra` 不能有警告。
- 每题写一个 `main()`，用 `assert` 检查题目给的测试向量。
- 先写接口和数据结构，再写 `tick`，最后补测试。面试里先把"每拍的顺序"讲清楚再动手。

## 自评标准

| 维度 | 及格 | 优秀 |
|------|------|------|
| 正确性 | 测试向量全过 | 能自己补边界用例（空、满、回绕、同拍进出） |
| 周期语义 | 能说清每拍的事件顺序 | 主动讨论"同拍 push/pop 是否允许"、延迟算法的两种口径 |
| 结构 | 状态和组合逻辑分开 | 接口能复用（例如 01 的仲裁器直接用在 11 题） |
| 追问 | 答出主要思路 | 能联系真实硬件（RTL 实现代价、gem5/GPGPU-Sim 里对应的类） |
