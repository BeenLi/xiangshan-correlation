# 01 仲裁器：固定优先级 / 轮询 / 包锁定 / DWRR

建议用时 30 分钟 · 难度 ★★

## 背景

仲裁器是所有共享资源（总线、crossbar 输出口、cache 端口、发射槽）的入口。
GPU 模型里 L1 的 miss 队列、NoC 路由器、warp 调度器本质上都是仲裁器加一点策略。

```
 req[0] ──▶┐
 req[1] ──▶│  Arbiter  ├──▶ grant (下标，或 -1 表示没人请求)
 req[2] ──▶│  (状态: ptr / owner / deficit)
 req[3] ──▶┘
```

## 要求

请求用位掩码 `uint64_t req` 表示，第 i 位为 1 表示请求者 i 在请求。返回被授权的下标，无请求返回 -1。

```cpp
// A. 固定优先级：下标越小优先级越高。纯组合，无状态。
class FixedPriorityArbiter {
public:
    explicit FixedPriorityArbiter(int n);
    int arbitrate(uint64_t req) const;
};

// B. 轮询（round-robin）：被授权者在下一拍变成最低优先级。
//    复位后请求者 0 优先级最高。没有授权时状态不变。
class RoundRobinArbiter {
public:
    explicit RoundRobinArbiter(int n);
    int arbitrate(uint64_t req);   // 每拍调用一次，相当于一个时钟沿
};

// C. 带包锁定的轮询：一个包占多拍，last[i]=1 表示请求者 i 本拍发的是包的最后一拍。
//    某请求者一旦被授权且这一拍不是 last，就锁定给它，直到它的 last 拍被授权为止。
//    锁定期间 owner 本拍没请求（包中间的气泡），也不能让给别人，返回 -1。
class LockingRRArbiter {
public:
    explicit LockingRRArbiter(int n);
    int arbitrate(uint64_t req, uint64_t last);
};

// D. DWRR（Deficit Weighted Round Robin）：按包长计费的加权轮询。
//    每个请求者有权重 quantum[i] 和一个 deficit 计数器（复位为 0）。len[i] 是请求者 i 队头包的长度，
//    只在 req 第 i 位为 1 时有效。每次调用选出一个包（不管包要发几拍）。
//    规则（指针 ptr 复位指向 0）：
//      1. req 全 0：返回 -1，状态不变。
//      2. 指针指向的 i 没请求：deficit[i] 清零，指针移到下一个。
//      3. 指针刚移到 i（一次"访问"开始）：deficit[i] += quantum[i]。一次访问只加一次。
//      4. len[i] <= deficit[i]：授权 i，deficit[i] -= len[i]，指针不动（下次调用继续访问 i）。
//      5. 否则结束对 i 的访问，余额保留，指针移到下一个，回到第 2 步。
class DWRRArbiter {
public:
    explicit DWRRArbiter(std::vector<int> quantum);  // n = quantum.size()
    int arbitrate(uint64_t req, const std::vector<int>& len);
    int deficit(int i) const;                        // 供测试检查
};
```

## 测试向量

A：`n=4`

| req | grant |
|-----|-------|
| 0b1010 | 1 |
| 0b1000 | 3 |
| 0b0000 | -1 |
| 0b1111 | 0 |

B：`n=4`，按拍依次调用

| 拍 | req | grant |
|----|-----|-------|
| 0 | 0b1111 | 0 |
| 1 | 0b1111 | 1 |
| 2 | 0b0101 | 2 |
| 3 | 0b0101 | 0 |
| 4 | 0b1000 | 3 |
| 5 | 0b0000 | -1 |
| 6 | 0b0011 | 0 |

另外：4 个请求者一直请求，1000 拍内每个人恰好被授权 250 次。

C：`n=3`，请求者 0 发一个 3 拍的包，1 和 2 各发 1 拍的包

| 拍 | req | last | grant |
|----|-----|------|-------|
| 0 | 0b111 | 0b110 | 0 |
| 1 | 0b111 | 0b110 | 0 |
| 2 | 0b111 | 0b111 | 0 |
| 3 | 0b110 | 0b110 | 1 |
| 4 | 0b100 | 0b100 | 2 |
| 5 | 0b000 | 0b000 | -1 |

D：`quantum = {4, 2, 2}`，队头包长固定为 `len = {3, 3, 1}`，按拍依次调用。deficit 列是调用后的值。

| 拍 | req | grant | deficit {0, 1, 2} |
|----|-----|-------|-------------------|
| 0 | 0b111 | 0 | 1, 0, 0 |
| 1 | 0b111 | 2 | 1, 2, 1 |
| 2 | 0b111 | 2 | 1, 2, 0 |
| 3 | 0b111 | 0 | 2, 2, 0 |
| 4 | 0b101 | 2 | 2, 0, 1 |
| 5 | 0b101 | 2 | 2, 0, 0 |
| 6 | 0b101 | 0 | 3, 0, 0 |
| 7 | 0b111 | 0 | 0, 0, 0 |
| 8 | 0b111 | 2 | 0, 2, 1 |
| 9 | 0b111 | 2 | 0, 2, 0 |
| 10 | 0b000 | -1 | 0, 2, 0 |
| 11 | 0b111 | 0 | 1, 2, 0 |

另外：
- `quantum = {1, 2, 3}`，包长都为 1，三个请求者一直请求：600 次授权里分别拿到 100 / 200 / 300 次。
- `quantum = {4, 2, 2}`，包长 `{3, 3, 1}`，一直请求：10000 次授权后发出的总长度为 10002 / 4998 / 5000，约 2:1:1。

## 追问

1. 不用循环，怎么用位运算在 O(1) 里取最低位的 1？RTL 里轮询仲裁器的"双固定优先级"结构是什么样的？
2. 轮询指针有两种更新方式："指向被授权者 + 1"和"每拍 + 1"。哪种公平？哪种会饿死？
3. 加权轮询（WRR）怎么改？要求请求者 i 在长期里拿到 w_i / Σw 的带宽。
4. 矩阵仲裁器（matrix arbiter）和最近最少授权（LRG）是什么关系？N 大时面积如何？
5. 如果 `arbitrate` 的结果可能"授权了但下游没接"（例如 crossbar 里输出授权了输入，但输入选了别的输出），指针应该在什么时候更新？
6. DWRR 和按包计数的 WRR 比，解决了什么问题？为什么空闲的请求者要把 deficit 清零？
7. quantum 小于最大包长时，D 的一次调用可能要扫好几圈才能授权。硬件里一拍做不完怎么办？quantum 该怎么选？
8. D 是"一次调用发一个包"。如果每拍只能发一个 flit、包占多拍，怎么和 C 的包锁定结合？deficit 在什么时候扣？
