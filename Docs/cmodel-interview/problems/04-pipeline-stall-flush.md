# 04 五级流水线：stall / flush / 反压

建议用时 45 分钟 · 难度 ★★★

## 背景

这是周期级建模的"母题"。三种让流水线停下来的机制要同时处理：

| 机制 | 来源 | 方向 | 效果 |
|------|------|------|------|
| stall | ID 检测到 load-use 相关 | 本级不走，下游插气泡 | 上游跟着停 |
| 反压 | MEM 的存储器本拍忙 | 从下游往上游传 | 所有上游停，**已有的气泡可以被挤掉** |
| flush | EX 发现分支误预测 | 从 EX 往上游 | 冲掉 IF、ID，pc 重定向 |

```
            ready_in[ID]      ready_in[EX]      ready_in[MEM]     ready_in[WB]=1
        ◀──────────────── ◀──────────────── ◀──────────────── ◀────────────────
  ┌────┐   fire[IF]  ┌────┐  fire[ID]  ┌────┐  fire[EX]  ┌─────┐  fire[MEM] ┌────┐
  │ IF │ ──────────▶ │ ID │ ─────────▶ │ EX │ ─────────▶ │ MEM │ ─────────▶ │ WB │──▶ 提交
  └────┘             └────┘     ▲      └────┘     │      └─────┘            └────┘
     ▲                          │ load-use        │ 误预测                ▲
     │                          │ hazard          ▼                       │
     └──────── pc 重定向 ◀──────┴───────── flush IF、ID              memBusy(t)

  ready_in[i] = !valid[i] || fire[i]        // 本级空，或本级本拍会走
  fire[i]     = valid[i] && 本级无内部停顿 && ready_in[i+1]
```

## 要求

```cpp
enum class Kind { ALU, LOAD, BRANCH };
struct Inst {
    int id;           // 程序里的下标
    Kind kind;
    int dst;          // -1 表示不写寄存器
    int src1, src2;   // -1 表示不读
    bool mispredict;  // BRANCH 专用：预测成不跳，实际跳到 target
    int target;
};

class Pipeline {
public:
    Pipeline(std::vector<Inst> prog, std::set<int> memBusyCycles);
    // 跑到程序取完、流水线排空，返回每条指令离开 WB 的周期；被冲掉的为 -1
    std::vector<int> run(bool verbose);
};
```

规则（必须遵守，否则周期对不上）：

1. 每级最多一条指令。第 t 拍"取指"会把 `prog[pc]` 放进 IF，所以它在第 t+1 拍才出现在 IF 里。第 0 拍流水线是空的。
2. WB 每拍都把里面的指令提交掉。
3. MEM：如果里面是 LOAD 且 `t ∈ memBusyCycles`，本拍不走（反压）。
4. load-use：ID 的指令读的寄存器等于**本拍开始时** EX 里那条 LOAD 的 dst，则 ID 本拍不走。其余相关都靠前递解决，不停。
5. 分支在**离开 EX 的那一拍**解析。误预测：本拍 IF、ID 里的指令作废；本拍就从 `target` 取指（所以代价是 2 个气泡）。
6. 某一级不走时，它原来的指令留在原地；某一级走了但上游没东西进来，它变空（气泡）。
7. 必须用"读旧写新"的两阶段写法：先只读当前状态算出所有 `fire`，再一次性生成下一拍状态。

## 测试向量

```
 0: ALU    r1 = r0 + r0
 1: LOAD   r2 = [r1]
 2: ALU    r3 = r2 + r1        ← 依赖 1 的 load 结果
 3: BRANCH beq r3              ← 误预测，实际跳到 6
 4: ALU    r4 = r1 + r1        ← 错误路径
 5: ALU    r5 = r1 + r1        ← 错误路径
 6: LOAD   r6 = [r3]
 7: ALU    r7 = r1 + r1
 memBusyCycles = {11, 12}
```

期望的提交周期：`{5, 6, 8, 9, -1, -1, 14, 15}`。

期望的流水线占用表（每行是该拍**开始时**各级里的指令）：

```
cycle |  IF  ID  EX MEM  WB | 本拍事件
    0 |   -   -   -   -   - |
    1 |   0   -   -   -   - |
    2 |   1   0   -   -   - |
    3 |   2   1   0   -   - |
    4 |   3   2   1   0   - | load-use stall
    5 |   3   2   -   1   0 |
    6 |   4   3   2   -   1 |
    7 |   5   4   3   2   - | 误预测 → flush IF/ID
    8 |   6   -   -   3   2 |
    9 |   7   6   -   -   3 |
   10 |   -   7   6   -   - |
   11 |   -   -   7   6   - | MEM 忙 → 反压
   12 |   -   -   7   6   - | MEM 忙 → 反压
   13 |   -   -   7   6   - |
   14 |   -   -   -   7   6 |
   15 |   -   -   -   -   7 |
```

## 追问

1. 如果用"从 WB 往 IF 逐级更新"的单阶段写法，load-use 检测会出什么错？为什么规则 4 强调"本拍开始时"？
2. 全局 stall（任何一级停，全流水线一起停）和逐级 valid/ready（气泡可以被挤掉）各有什么优缺点？假设第 11 拍 MEM 反压时 EX 是空的、ID 里有指令，两种写法下 ID 的指令分别会怎样？
3. `ready` 从 WB 一路组合传到 IF，级数多时是关键路径。skid buffer 怎么打断它？写出一个两项 skid buffer 的 valid/ready 逻辑。
4. 分支在 EX 解析代价是 2 拍。如果把解析提前到 ID、或者在流水线停顿时也允许分支解析，模型要怎么改？
5. 乱序核里 flush 不能简单"冲掉上游所有级"，要按什么来判断谁更年轻？
