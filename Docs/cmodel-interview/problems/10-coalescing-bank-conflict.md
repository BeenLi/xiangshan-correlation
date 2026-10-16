# 10 合并访存与共享内存 bank conflict

建议用时 20 分钟 · 难度 ★

## 背景

一条 warp 级访存指令有 32 个线程的地址，硬件要把它们变成尽量少的存储事务。

```
 A. 全局内存：按 128B 行分组，行内按 32B sector 计数

   线程 0..31，每个读 4B，stride 4B，起始地址 0：
   0        32       64       96       128
   ├────────┼────────┼────────┼────────┤
   │ T0..T7 │T8..T15 │T16..T23│T24..T31│      1 行，4 个 sector → 4 个 32B 事务
   └────────┴────────┴────────┴────────┘

 B. 共享内存：32 个 bank，每个 bank 4B 宽，bank = (addr / 4) % 32

   bank:   0    1    2    ...   31
   word:   0    1    2    ...   31
           32   33   34   ...   63
   同一 bank 的不同字要串行访问；同一个字是广播，不算冲突。
   需要的周期 = 各 bank 上"不同字"个数的最大值
```

## 要求

```cpp
constexpr int kWarp = 32;
using Addrs = std::array<uint64_t, kWarp>;
struct CoalesceResult { int lines; int sectors; };

CoalesceResult coalesce(const Addrs& a, uint32_t activeMask, int bytesPerThread = 4);
int bankConflictCycles(const Addrs& a, uint32_t activeMask);
```

- 只统计 `activeMask` 里为 1 的线程。
- `coalesce`：一个线程的访问如果跨 sector（未对齐的宽访问），首尾字节所在的 sector 都要算。

## 测试向量

线程 i 的地址 = base + i × stride，全部 4B 访问：

| 情形 | base | stride | mask | lines | sectors | smem 周期 |
|------|------|--------|------|-------|---------|-----------|
| 对齐、连续 | 0 | 4 | 全 1 | 1 | 4 | 1 |
| 步长 8B | 0 | 8 | 全 1 | 2 | 8 | 2 |
| 步长 128B | 0 | 128 | 全 1 | 32 | 32 | 32 |
| 不对齐 | 4 | 4 | 全 1 | 2 | 5 | 1 |
| 全部同址 | 64 | 0 | 全 1 | 1 | 1 | 1 |
| 只有偶数线程 | 0 | 4 | 0x55555555 | 1 | 4 | 1 |
| 步长 132B | 0 | 132 | 全 1 | 32 | 32 | 1 |

## 追问

1. 最后一行为什么没有 bank conflict？这就是矩阵转置里 `__shared__ float tile[32][33]` 多加一列的原因，请解释。
2. 每线程 8B（`double`）或 16B（`float4`）访问时，共享内存按什么粒度分阶段处理？冲突怎么算？
3. 早期 GPU（compute 1.x）和现在的合并规则有什么不同？为什么现在以 32B sector 为粒度？
4. 如果把 `coalesce` 放进周期级模型，它在 LD/ST 单元的哪一级？拆出来的 sector 请求每拍能发几个？
5. 原子操作访问同一地址时，合并和 bank 逻辑有什么不同？
