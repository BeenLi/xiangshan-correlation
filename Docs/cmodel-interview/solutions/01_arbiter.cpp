// 01 仲裁器：固定优先级 / 轮询 / 带包锁定的轮询
// 编译：g++ -std=c++17 -O2 -Wall -Wextra 01_arbiter.cpp && ./a.out
#include <cassert>
#include <cstdint>
#include <cstdio>

// 取最低位的 1 的下标；x 为 0 时返回 -1
static int lowestSetBit(uint64_t x) {
    if (x == 0) return -1;
    return __builtin_ctzll(x);  // 等价于 log2(x & -x)
}

// ---------------- A. 固定优先级：下标越小优先级越高（纯组合逻辑，无状态） ----------------
class FixedPriorityArbiter {
public:
    explicit FixedPriorityArbiter(int n) : n_(n) { assert(n > 0 && n <= 64); }
    int arbitrate(uint64_t req) const { return lowestSetBit(req & mask()); }

private:
    uint64_t mask() const { return n_ == 64 ? ~0ULL : ((1ULL << n_) - 1); }
    int n_;
};

// ---------------- B. 轮询：被授权者下一拍变成最低优先级 ----------------
// 实现：ptr_ 指向当前最高优先级的请求者。
//   masked = req 中 >= ptr_ 的部分；masked 非 0 就在 masked 里做固定优先级，
//   否则回绕，在整个 req 里做固定优先级。这就是 RTL 里常见的"双固定优先级仲裁器"写法。
class RoundRobinArbiter {
public:
    explicit RoundRobinArbiter(int n) : n_(n) { assert(n > 0 && n < 64); }

    // 每拍调用一次：返回授权下标，同时更新指针（相当于时钟沿）
    int arbitrate(uint64_t req) {
        int g = peek(req);
        if (g >= 0) ptr_ = (g + 1) % n_;  // 只有真正授权了才移动指针
        return g;
    }

    // 只做组合判断、不改状态，供上层"先看结果再决定是否提交"使用
    int peek(uint64_t req) const {
        req &= (1ULL << n_) - 1;
        uint64_t masked = req & ~((1ULL << ptr_) - 1);
        return masked ? lowestSetBit(masked) : lowestSetBit(req);
    }

    void accept(int g) { ptr_ = (g + 1) % n_; }  // 供 crossbar 等需要两阶段提交的场景使用
    int ptr() const { return ptr_; }

private:
    int n_;
    int ptr_ = 0;
};

// ---------------- C. 带包锁定的轮询：一个多拍的包传完（last=1）之前不切换 ----------------
class LockingRRArbiter {
public:
    explicit LockingRRArbiter(int n) : rr_(n) {}

    // req[i]=1 表示请求者 i 本拍有一拍数据要发；last[i]=1 表示这一拍是它当前包的最后一拍
    int arbitrate(uint64_t req, uint64_t last) {
        int g;
        if (owner_ >= 0) {
            // 锁定期间只服务 owner；owner 本拍没数据（气泡）时也不让给别人
            g = (req >> owner_) & 1 ? owner_ : -1;
        } else {
            g = rr_.arbitrate(req);
        }
        if (g >= 0) owner_ = ((last >> g) & 1) ? -1 : g;
        return g;
    }

private:
    RoundRobinArbiter rr_;
    int owner_ = -1;
};

int main() {
    // A
    FixedPriorityArbiter fp(4);
    assert(fp.arbitrate(0b1010) == 1);
    assert(fp.arbitrate(0b1000) == 3);
    assert(fp.arbitrate(0b0000) == -1);
    assert(fp.arbitrate(0b1111) == 0);

    // B：题目给出的 7 拍序列
    RoundRobinArbiter rr(4);
    const uint64_t reqs[] = {0b1111, 0b1111, 0b0101, 0b0101, 0b1000, 0b0000, 0b0011};
    const int expect[] = {0, 1, 2, 0, 3, -1, 0};
    for (int t = 0; t < 7; ++t) {
        int g = rr.arbitrate(reqs[t]);
        printf("RR  cycle %d req=0x%llx grant=%d ptr->%d\n", t, (unsigned long long)reqs[t], g, rr.ptr());
        assert(g == expect[t]);
    }

    // 公平性：4 个请求者一直请求，每个人 1000 拍里正好拿到 250 次
    RoundRobinArbiter rr2(4);
    int cnt[4] = {};
    for (int t = 0; t < 1000; ++t) cnt[rr2.arbitrate(0b1111)]++;
    for (int i = 0; i < 4; ++i) assert(cnt[i] == 250);

    // C：请求者 0 发 3 拍的包，请求者 1、2 各发 1 拍的包
    LockingRRArbiter lk(3);
    struct { uint64_t req, last; int expect; } seq[] = {
        {0b111, 0b110, 0},  // 0 开始发包（非最后一拍）→ 锁定 0
        {0b111, 0b110, 0},  // 仍是 0
        {0b111, 0b111, 0},  // 0 的最后一拍 → 解锁
        {0b110, 0b110, 1},  // 指针在 1 → 授权 1，单拍包
        {0b100, 0b100, 2},
        {0b000, 0b000, -1},
    };
    for (int t = 0; t < 6; ++t) {
        int g = lk.arbitrate(seq[t].req, seq[t].last);
        printf("LCK cycle %d grant=%d\n", t, g);
        assert(g == seq[t].expect);
    }
    printf("01_arbiter: all tests passed\n");
    return 0;
}
