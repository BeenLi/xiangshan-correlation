# 11 Crossbar：HOL 阻塞、VOQ 与 iSLIP

建议用时 45 分钟 · 难度 ★★★

## 背景

GPU 里 SM 到 L2 slice 之间、NoC 路由器内部都是 crossbar。每拍每个输入最多送出一个包，每个输出最多收一个包。

```
 A. 输入 FIFO：只有队头能参加仲裁

   in0: [ b→0 | a→0 ] ─┐        ┌─▶ out0（RR 仲裁）
                       ├─ XBAR ─┤
   in1: [ f→1 | e→1 | d→1 | c→0 ] ─┘        └─▶ out1（RR 仲裁）
                                   ▲
                       c 去 out0 被挡住时，后面的 d/e/f 明明 out1 空着也走不了 → HOL 阻塞

 B. VOQ：每个输入为每个输出各开一个队列

   in0: voq[0][0] = [a b]   voq[0][1] = []
   in1: voq[1][0] = [c]     voq[1][1] = [d e f]

   单轮 iSLIP：
     1. Request：输入 i 对每个非空的 voq[i][o] 向输出 o 发请求
     2. Grant：  输出 o 从 gptr[o] 开始轮询，在请求它的输入里选一个
     3. Accept： 输入 i 从 aptr[i] 开始轮询，在授权它的输出里选一个
     只有被 accept 的 (i, o) 才传输，也只有它们才更新指针：gptr[o] = i+1，aptr[i] = o+1
```

## 要求

```cpp
struct Pkt { std::string name; int dst; };
struct Delivery { std::string name; int out; int cycle; };
using OutReady = std::function<bool(int out, int cycle)>;   // 下游反压：为假时该输出本拍不授权

std::vector<Delivery> runFifo(std::vector<std::deque<Pkt>> in, int M, const OutReady& ready);
std::vector<Delivery> runVoq(const std::vector<std::deque<Pkt>>& in, int M, const OutReady& ready);
```

- 所有包第 0 拍已在输入端。指针初值都是 0。
- 必须两阶段：先根据本拍开始时的队列状态算出所有授权，再统一出队。
- A 里每个输出用轮询仲裁（可以直接复用 01 题），被授权后 `gptr[o] = i + 1`。

## 测试向量

输入：`in0 = [a→0, b→0]`，`in1 = [c→0, d→1, e→1, f→1]`，2 个输出。

| 情形 | 送达（包→输出@拍） | 最后一拍 |
|------|-------------------|----------|
| FIFO，无反压 | a→o0@0 c→o0@1 b→o0@2 d→o1@2 e→o1@3 f→o1@4 | 4 |
| VOQ，无反压 | a→o0@0 d→o1@0 c→o0@1 b→o0@2 e→o1@2 f→o1@3 | 3 |
| FIFO，out0 在第 0、1 拍不 ready | a→o0@2 c→o0@3 b→o0@4 d→o1@4 e→o1@5 f→o1@6 | 6 |
| VOQ，out0 在第 0、1 拍不 ready | d→o1@0 e→o1@1 a→o0@2 f→o1@2 c→o0@3 b→o0@4 | 4 |

## 追问

1. 均匀随机流量下，输入 FIFO 的 crossbar 饱和吞吐约为多少？（经典结论约 58.6%）为什么？
2. VOQ 无反压时第 1 拍只匹配了 in1→out0，但 {in0→out0, in1→out1} 能同时匹配两对。iSLIP 多轮迭代能找到它吗？
   最大匹配（maximum）和极大匹配（maximal）有什么区别？多轮迭代时指针只在第几轮更新，为什么？
3. 为什么 iSLIP 规定"只有 accept 了才更新 grant 指针"？如果授权就更新会发生什么？
4. VOQ 需要 N×M 个队列，面积太大时有什么折中？（共享缓冲、有限个 VOQ、输出排队 + 加速比）
5. 输出端反压在真实 NoC 里通常用什么实现？把本题的 `ready` 换成 03 题的 credit，模型怎么改？
