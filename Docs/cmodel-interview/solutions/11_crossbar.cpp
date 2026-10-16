// 11 N×M crossbar：输入 FIFO（HOL 阻塞） vs. VOQ + 单轮 iSLIP 匹配，可选输出端反压
// 编译：g++ -std=c++17 -O2 -Wall -Wextra 11_crossbar.cpp && ./a.out
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <functional>
#include <string>
#include <vector>

struct Pkt { std::string name; int dst; };
struct Delivery { std::string name; int out; int cycle; };

// 轮询指针：从 ptr 开始找第一个满足条件的下标（同 01 题的 RoundRobinArbiter，只是写成函数形式）
static int rrPick(int n, int ptr, const std::function<bool(int)>& ok) {
    for (int k = 0; k < n; ++k) {
        int i = (ptr + k) % n;
        if (ok(i)) return i;
    }
    return -1;
}

using OutReady = std::function<bool(int out, int cycle)>;

// ---------------- A. 每个输入一个 FIFO：只有队头能参与仲裁 ----------------
std::vector<Delivery> runFifo(std::vector<std::deque<Pkt>> in, int M, const OutReady& ready) {
    int N = (int)in.size();
    std::vector<int> gptr(M, 0);
    std::vector<Delivery> out;
    for (int t = 0; t < 1000; ++t) {
        bool any = false;
        for (auto& q : in) any |= !q.empty();
        if (!any) break;
        // 先根据本拍开始时的队头算出所有授权，再统一出队（两阶段，避免同拍连续出队）
        std::vector<int> grant(M, -1);
        for (int o = 0; o < M; ++o) {
            if (!ready(o, t)) continue;  // 下游反压：该输出本拍不授权
            grant[o] = rrPick(N, gptr[o], [&](int i) { return !in[i].empty() && in[i].front().dst == o; });
        }
        for (int o = 0; o < M; ++o) {
            int i = grant[o];
            if (i < 0) continue;
            out.push_back({in[i].front().name, o, t});
            in[i].pop_front();
            gptr[o] = (i + 1) % N;
        }
    }
    return out;
}

// ---------------- B. VOQ：每个输入为每个输出各维护一个队列，单轮 iSLIP ----------------
//   1) Request：输入 i 对每个非空 voq[i][o] 发请求
//   2) Grant：  输出 o 从 gptr[o] 开始轮询，选一个请求它的输入
//   3) Accept： 输入 i 从 aptr[i] 开始轮询，在给它授权的输出里选一个
//   只有被 accept 的那对才更新指针（gptr[o]=i+1, aptr[i]=o+1）——这是 iSLIP 不饿死、能去同步的关键
std::vector<Delivery> runVoq(const std::vector<std::deque<Pkt>>& inFifo, int M, const OutReady& ready) {
    int N = (int)inFifo.size();
    std::vector<std::vector<std::deque<Pkt>>> voq(N, std::vector<std::deque<Pkt>>(M));
    for (int i = 0; i < N; ++i) for (auto& p : inFifo[i]) voq[i][p.dst].push_back(p);
    std::vector<int> gptr(M, 0), aptr(N, 0);
    std::vector<Delivery> out;
    for (int t = 0; t < 1000; ++t) {
        bool any = false;
        for (auto& v : voq) for (auto& q : v) any |= !q.empty();
        if (!any) break;
        std::vector<int> grant(M, -1);
        for (int o = 0; o < M; ++o) {
            if (!ready(o, t)) continue;
            grant[o] = rrPick(N, gptr[o], [&](int i) { return !voq[i][o].empty(); });
        }
        for (int i = 0; i < N; ++i) {
            int o = rrPick(M, aptr[i], [&](int oo) { return grant[oo] == i; });
            if (o < 0) continue;
            out.push_back({voq[i][o].front().name, o, t});
            voq[i][o].pop_front();
            gptr[o] = (i + 1) % N;
            aptr[i] = (o + 1) % M;
        }
    }
    return out;
}

static int finish(const std::vector<Delivery>& d) {
    int last = -1;
    for (auto& x : d) last = std::max(last, x.cycle);
    return last;
}

static void dump(const char* tag, const std::vector<Delivery>& d) {
    printf("%-10s", tag);
    for (auto& x : d) printf(" %s->o%d@%d", x.name.c_str(), x.out, x.cycle);
    printf("   (last @%d)\n", finish(d));
}

int main() {
    // in0: a(→0) b(→0)；in1: c(→0) d(→1) e(→1) f(→1)
    std::vector<std::deque<Pkt>> in = {
        {{"a", 0}, {"b", 0}},
        {{"c", 0}, {"d", 1}, {"e", 1}, {"f", 1}},
    };
    auto always = [](int, int) { return true; };

    auto f = runFifo(in, 2, always);
    dump("FIFO", f);
    assert(finish(f) == 4);  // d、e、f 被 c 堵在后面：HOL 阻塞

    auto v = runVoq(in, 2, always);
    dump("VOQ+iSLIP", v);
    assert(finish(v) == 3);  // 输出 1 从第 0 拍起就有活干

    // 反压：输出 0 在第 0、1 拍不 ready
    auto stall = [](int o, int t) { return !(o == 0 && t < 2); };
    auto fs = runFifo(in, 2, stall);
    dump("FIFO+bp", fs);
    assert(finish(fs) == 6);  // 输出 0 晚两拍开工，in1 的队头 c 也晚两拍走，d/e/f 全被拖后
    auto vs = runVoq(in, 2, stall);
    dump("VOQ+bp", vs);
    assert(finish(vs) == 4);  // 输出 1 不受影响：d、e、f 在第 0~2 拍就走了；输出 0 第 2~4 拍
    printf("11_crossbar: all tests passed\n");
    return 0;
}
