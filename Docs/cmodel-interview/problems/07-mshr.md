# 07 非阻塞 Cache 的 MSHR

建议用时 30 分钟 · 难度 ★★

## 背景

GPU 能用大量 warp 掩盖访存延迟，前提是 L1 是非阻塞的：缺失不停流水线，而是记进 MSHR
（Miss Status Holding Register），多个访问同一行的缺失合并成一次下层请求。

```
             request(id, line)
                    │
                    ▼
            ┌───────────────┐  命中 ──────────────────────▶ 响应 @ t + hitLat
            │   tag 查询    │
            └───────┬───────┘
                    │ 缺失
                    ▼
     ┌──────────────────────────────────┐
     │ MSHR 表（numMshr 项）              │
     │  ┌───────┬──────┬─────────────┐   │   有同一行的项 → 次级缺失：追加 target（target 满则拒收）
     │  │ valid │ line │ targets[≤K] │   │   没有 → 初级缺失：分配新项，向下层发请求（MSHR 满则拒收）
     │  └───────┴──────┴─────────────┘   │
     └────────────────┬─────────────────┘
                      │ 第 t + memLat 拍回填
                      ▼
          安装该行，释放 MSHR，给所有 target 回响应（都在回填这一拍）
```

## 要求

```cpp
struct Resp { int id; int cycle; };
class NonBlockingCache {
public:
    NonBlockingCache(int numMshr, int maxTargets, int hitLat, int memLat);
    void processFills(int t);                       // 每拍最先调用
    bool request(int id, uint64_t line, int t);     // 返回 false = 本拍拒收（反压）
    std::vector<Resp> resps;
};
```

- tag 阵列简化为无限容量（装进来就不会被踢），本题只关注 MSHR。
- 回填和本拍新请求同拍时，**先回填后处理请求**（所以回填那一拍同一行的请求直接命中）。
- 上游驱动：按顺序发请求，每拍最多发一个；被拒收就下一拍重发同一个（按序、阻塞式）。

## 测试向量

`numMshr=2, maxTargets=2, hitLat=1, memLat=10`，请求序列（行号）：`A A A B C D B`（A=0, B=1, C=2, D=3）

| 请求 | 行 | 被接收的拍 | 响应拍 | 说明 |
|------|----|-----------|--------|------|
| 0 | A | 0 | 10 | 初级缺失 |
| 1 | A | 1 | 10 | 次级缺失，合并 |
| 2 | A | 10 | 11 | target 满，等到回填后命中 |
| 3 | B | 11 | 21 | |
| 4 | C | 12 | 22 | MSHR 用满 |
| 5 | D | 21 | 31 | 等 B 回填释放 MSHR |
| 6 | B | 22 | 23 | 命中 |

## 追问

1. 请求 2 被卡了 8 拍，并且把后面所有请求都堵住了。有哪些办法缓解？（更多 target、拒收后让请求去 replay 队列、允许乱序接收……）
2. GPU 里一条 warp 的访存会拆成多个 sector 请求，MSHR 合并应该按行还是按 sector？
3. 为什么所有 target 都在回填那一拍响应不现实？真实硬件里响应怎么排队？对模型精度影响多大？
4. 如果下层返回顺序和请求顺序不同，MSHR 怎么匹配回填？
5. MSHR 数量、target 数量、warp 数量和访存延迟之间满足什么关系才能把带宽打满？（Little's law）
