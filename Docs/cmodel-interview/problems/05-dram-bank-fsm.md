# 05 状态机：DRAM bank 控制器

建议用时 30 分钟 · 难度 ★★

## 背景

带时序约束的状态机是 C-model 面试的高频题。DRAM bank 的行缓冲（row buffer）既有状态又有多种时序参数，
很适合考察"状态转移 + 计时器 + 每拍最多一条命令"。

```
                  ACT (行 r)                     计时 tRCD 到期
     ┌──────┐  ─────────────────▶ ┌────────────┐ ─────────────▶ ┌──────────┐
     │ IDLE │                      │ ACTIVATING │                │  ACTIVE  │──┐ RD（队头命中打开的行，
     │行关闭│ ◀─────────────────── └────────────┘                │ 行 r 打开 │◀─┘    且距上次 RD >= tBL）
     └──────┘  计时 tRP 到期     ┌──────────────┐  PRE           └──────────┘
          ▲  ───────────────────│ PRECHARGING  │◀────────────────────┘
          └─────────────────────└──────────────┘  队头要别的行，且距 ACT >= tRAS
```

时序参数（单位：拍）：

| 参数 | 值 | 含义 |
|------|----|------|
| tRCD | 3 | ACT 到 RD |
| tCL  | 3 | RD 到第一拍数据 |
| tBL  | 2 | 突发数据拍数；也是两条 RD 之间的最小间隔 |
| tRP  | 3 | PRE 到下一次 ACT |
| tRAS | 6 | ACT 到 PRE |

## 要求

单 bank、开页策略（读完不主动关行）、请求按到达顺序 FCFS 处理，只有读请求。

```cpp
struct Req { int id; int arrive; int row; int done; Kind kind; };   // done = 最后一拍数据返回的周期
enum class Kind { MISS, HIT, CONFLICT };

class BankCtrl {
public:
    explicit BankCtrl(Timing tm);
    void enqueue(Req* r);   // 第 arrive 拍入队，同拍就能被处理
    void tick(int t);
};
```

每拍的顺序：

1. 计时到期的状态转移：`ACTIVATING` 在 `t >= ACT拍 + tRCD` 时变 `ACTIVE`；`PRECHARGING` 在 `t >= PRE拍 + tRP` 时变 `IDLE`。
2. 按转移后的状态，针对队头请求**最多发一条命令**（ACT / RD / PRE）。转移和发命令可以在同一拍。
3. RD 在第 t 拍发出，请求的 `done = t + tCL + tBL`，请求出队。
4. 请求**第一次成为队头**时按当时的 bank 状态分类：行关闭 → MISS；打开的行就是它要的 → HIT；否则 → CONFLICT。

## 测试向量

请求（到达拍, 行）：`(0, 5) (1, 5) (2, 7) (3, 7) (10, 5)`

| 请求 | 行 | 分类 | done |
|------|----|------|------|
| 0 | 5 | MISS | 8 |
| 1 | 5 | HIT | 10 |
| 2 | 7 | CONFLICT | 17 |
| 3 | 7 | HIT | 19 |
| 4 | 5 | CONFLICT | 26 |

命令序列：`0:ACT5  3:RD  5:RD  6:PRE  9:ACT7  12:RD  14:RD  15:PRE  18:ACT5  21:RD`

## 追问

1. 第 6 拍的 PRE 和第 15 拍的 PRE 分别被哪个约束卡住（或没卡住）？
2. 改成 FR-FCFS（行命中优先）调度，本例的 done 会怎么变？FR-FCFS 有什么饿死风险，怎么缓解？
3. 关页策略（每次 RD 后自动预充，RDA）在什么访问模式下更好？
4. 多 bank 时，哪些约束是 bank 内的（tRCD、tRP、tRAS），哪些是 rank / channel 级共享的（tRRD、tFAW、数据总线、tWTR）？模型结构怎么分层？
5. 刷新（tREFI / tRFC）怎么插进这个状态机？
6. 这里用了"显式状态 + 时间戳"的写法，和"每拍递减计数器"的写法相比，在仿真速度上有什么区别？
