# 06 组相联 Cache：LRU、写回写分配、sector cache

建议用时 30 分钟 · 难度 ★★

## 背景

```
 地址拆分（本题 A：容量 128B、2 路、16B 行 → 4 组）

   63                           6 5     4 3       0
  ┌──────────────────────────────┬───────┬─────────┐
  │             tag              │ index │ offset  │
  └──────────────────────────────┴───────┴─────────┘
                                   2 bit    4 bit

  set 0: [ way0: V D tag lastUse ] [ way1: V D tag lastUse ]
  set 1: [ ...                   ] [ ...                   ]
  ...
```

NVIDIA GPU 的 L1/L2 是 **sector cache**：tag 按 128B 行分配，但数据按 32B sector 填充，
每个 sector 有自己的有效位。这样一次 warp 访存只碰到一个 sector 时，只需要搬 32B。

```
  一行 128B = 4 个 sector
  ┌──────────┬──────────┬──────────┬──────────┐
  │ sector 0 │ sector 1 │ sector 2 │ sector 3 │   sectorMask = 0b0011 表示 0、1 号 sector 有效
  └──────────┴──────────┴──────────┴──────────┘
```

## 要求

A. 功能级 cache（不建模时序）：

```cpp
struct AccessResult { bool hit; bool writeback; uint64_t wbAddr; };
class Cache {
public:
    Cache(uint64_t sizeBytes, int assoc, int lineBytes);   // 都是 2 的幂
    AccessResult access(uint64_t addr, bool isWrite);
    uint64_t hits, misses, writebacks;
};
```

- 写回 + 写分配：写缺失也要分配行，然后标脏。
- 替换：优先无效行；否则真 LRU。
- 踢出脏行时返回被写回行的**首地址**（用 tag 和 set 拼回来）。

B. 只读 sector cache：

```cpp
class SectorCache {
public:
    SectorCache(int sets, int assoc, int lineBytes, int sectorBytes);
    void read(uint64_t addr);
    uint64_t hit, sectorMiss, lineMiss, bytesFetched;
};
```

- tag 命中且 sector 有效 → `hit`。
- tag 命中但 sector 无效 → `sectorMiss`，只取这一个 sector（`bytesFetched += sectorBytes`），不换行。
- tag 不命中 → `lineMiss`，按 LRU 换行，新行只有这一个 sector 有效。
- 组号 = (addr / lineBytes) % sets。

## 测试向量

A：`Cache(128, 2, 16)`

| # | 访问 | 结果 | 写回 |
|---|------|------|------|
| 1 | R 0x000 | miss | |
| 2 | W 0x040 | miss | |
| 3 | R 0x004 | hit | |
| 4 | R 0x080 | miss | 0x040 |
| 5 | W 0x000 | hit | |
| 6 | R 0x0C0 | miss | |
| 7 | R 0x010 | miss | |
| 8 | R 0x040 | miss | 0x000 |

结束时 hits=2，misses=6，writebacks=2。

B：`SectorCache(2, 2, 128, 32)`，依次读 `0x000 0x020 0x010 0x100 0x200 0x030 0x0A0`

结束时 hit=1，sectorMiss=1，lineMiss=5，bytesFetched=192。

## 追问

1. 真 LRU 在 8 路、16 路时的硬件代价是多少？写出 tree-PLRU 的更新和选择逻辑，并举一个它和真 LRU 选得不一样的例子。
2. 写直达 + 写不分配适合什么场景？GPU 的 L1 为什么常用写直达（或者直接不缓存全局写）？
3. 如果要建模时序：命中延迟、MSHR、填充端口冲突、tag 和数据阵列串行还是并行访问，分别加在哪里？
4. sector cache 相对"直接用 32B 行"有什么好处？（提示：tag 面积、空间局部性预取）
5. 物理索引虚拟标签（VIPT）的 L1，组数 × 行大小为什么不能超过页大小？
