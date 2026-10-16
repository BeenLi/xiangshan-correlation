// 09 GPU warp 调度器 + 记分板（scoreboard）：LRR 与 GTO
// 编译：g++ -std=c++17 -O2 -Wall -Wextra 09_warp_scheduler.cpp && ./a.out
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

struct WInst {
    int dst;               // -1 表示不写
    std::vector<int> srcs;
    int latency;           // 结果在 issue + latency 拍可用（那一拍就能被依赖者发射）
};

enum class Policy { LRR, GTO };

struct Warp {
    std::vector<WInst> prog;
    size_t pc = 0;
    std::vector<int> regReadyAt = std::vector<int>(32, 0);  // 记分板：每个寄存器结果可用的周期
    bool done() const { return pc >= prog.size(); }
};

class Scheduler {
public:
    Scheduler(std::vector<Warp> warps, Policy p) : w_(std::move(warps)), policy_(p) {}

    // 记分板检查：RAW（源还没写回）和 WAW（目的寄存器还有在飞的写）都不能发射
    bool canIssue(const Warp& w, int t) const {
        if (w.done()) return false;
        const WInst& i = w.prog[w.pc];
        for (int s : i.srcs) if (w.regReadyAt[s] > t) return false;
        if (i.dst >= 0 && w.regReadyAt[i.dst] > t) return false;
        return true;
    }

    int pick(int t) const {
        int n = (int)w_.size();
        if (policy_ == Policy::LRR) {
            // 从上次发射的下一个 warp 开始轮询
            for (int k = 1; k <= n; ++k) {
                int i = (last_ + k) % n;
                if (canIssue(w_[i], t)) return i;
            }
        } else {
            // GTO：上次的 warp 还能发就继续发；否则选最老的（这里用 warp 编号代表启动顺序）
            if (last_ >= 0 && canIssue(w_[last_], t)) return last_;
            for (int i = 0; i < n; ++i) if (canIssue(w_[i], t)) return i;
        }
        return -1;
    }

    // 返回发射序列 "W<warp>.<pc>@<cycle>"
    std::vector<std::string> run() {
        std::vector<std::string> trace;
        for (int t = 0; t < 1000; ++t) {
            bool all = true;
            for (auto& w : w_) all &= w.done();
            if (all) break;
            int i = pick(t);
            if (i < 0) continue;  // 本拍空发射（所有 warp 都在等记分板）
            Warp& w = w_[i];
            const WInst& in = w.prog[w.pc];
            if (in.dst >= 0) w.regReadyAt[in.dst] = t + in.latency;
            trace.push_back("W" + std::to_string(i) + "." + std::to_string(w.pc) + "@" + std::to_string(t));
            ++w.pc;
            last_ = i;
        }
        return trace;
    }

private:
    std::vector<Warp> w_;
    Policy policy_;
    int last_ = -1;
};

static void check(Policy p, const std::vector<std::string>& expect) {
    // 每个 warp 跑同一段程序：load → 依赖它的 add → 独立的 add
    std::vector<WInst> prog = {
        {1, {0}, 4},  // r1 = ld [r0]   延迟 4
        {2, {1}, 1},  // r2 = r1 + 1    依赖 r1
        {3, {0}, 1},  // r3 = r0 + 1    独立，但按序发射，只能排在上一条后面
    };
    Scheduler s({Warp{prog}, Warp{prog}}, p);
    auto tr = s.run();
    printf("%s: ", p == Policy::LRR ? "LRR" : "GTO");
    for (auto& x : tr) printf("%s ", x.c_str());
    printf("\n");
    assert(tr == expect);
}

int main() {
    check(Policy::LRR, {"W0.0@0", "W1.0@1", "W0.1@4", "W1.1@5", "W0.2@6", "W1.2@7"});
    check(Policy::GTO, {"W0.0@0", "W1.0@1", "W0.1@4", "W0.2@5", "W1.1@6", "W1.2@7"});
    printf("09_warp_scheduler: all tests passed\n");
    return 0;
}
