// 03 基于 credit 的流控
// 编译：g++ -std=c++17 -O2 -Wall -Wextra 03_credit.cpp && ./a.out
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <functional>

// 一条固定延迟的线（链路）。为了题目自包含，这里用 deque 实现，不限容量
// （链路上同时在飞的数据量天然 <= 延迟拍数，所以不需要容量上限）。
template <typename T>
class Wire {
public:
    explicit Wire(uint32_t lat) : lat_(lat) {}
    void send(const T& v, uint64_t now) { q_.push_back({v, now + lat_}); }
    bool arrive(uint64_t now) const { return !q_.empty() && q_.front().at <= now; }
    T take() { T v = q_.front().v; q_.pop_front(); return v; }

private:
    struct E { T v; uint64_t at; };
    uint32_t lat_;
    std::deque<E> q_;
};

struct Result {
    uint64_t delivered = 0;   // 接收端消费掉的 flit 数
    uint32_t maxOccupancy = 0;  // 接收缓冲的最大占用，必须 <= B
};

// 发送端一直有数据可发。每拍内的顺序（题目约定）：
//   1) 链路到达：flit 进接收缓冲，credit 回到发送端计数器
//   2) 接收端：缓冲非空且下游 ready(t) → 消费一个，同时发回一个 credit
//   3) 发送端：credits > 0 → 发一个 flit，credits--
Result simulate(uint32_t B, uint32_t Lf, uint32_t Lc, uint64_t cycles,
                const std::function<bool(uint64_t)>& consumerReady) {
    Wire<int> fwd(Lf);
    Wire<int> back(Lc);
    uint32_t credits = B;  // 复位时发送端的 credit = 接收端缓冲深度
    std::deque<int> rxBuf;
    Result r;
    int seq = 0;
    for (uint64_t t = 0; t < cycles; ++t) {
        // 1) 到达
        while (fwd.arrive(t)) rxBuf.push_back(fwd.take());
        while (back.arrive(t)) { back.take(); ++credits; }
        assert(rxBuf.size() <= B);  // credit 机制保证永不溢出
        if (rxBuf.size() > r.maxOccupancy) r.maxOccupancy = rxBuf.size();
        // 2) 接收端消费
        if (!rxBuf.empty() && consumerReady(t)) {
            rxBuf.pop_front();
            ++r.delivered;
            back.send(1, t);
        }
        // 3) 发送端
        if (credits > 0) {
            --credits;
            fwd.send(seq++, t);
        }
    }
    return r;
}

int main() {
    const uint64_t N = 10000;
    auto always = [](uint64_t) { return true; };

    // (a) Lf = Lc = 2：credit 往返 RTT = Lf + Lc = 4。B >= 4 才能满吞吐。
    for (uint32_t B = 1; B <= 6; ++B) {
        Result r = simulate(B, 2, 2, N, always);
        double thr = (double)r.delivered / N;
        double expect = B >= 4 ? 1.0 : B / 4.0;
        printf("B=%u Lf=2 Lc=2 throughput=%.3f (expect %.3f) maxOcc=%u\n", B, thr, expect, r.maxOccupancy);
        assert(thr > expect - 0.01 && thr < expect + 0.01);
    }

    // (b) 下游反压：消费者只在偶数拍 ready。缓冲占用不能超过 B（函数里有 assert），吞吐 = 0.5
    Result r = simulate(4, 2, 2, N, [](uint64_t t) { return t % 2 == 0; });
    printf("backpressure: throughput=%.3f maxOcc=%u\n", (double)r.delivered / N, r.maxOccupancy);
    assert(r.maxOccupancy <= 4);
    assert(r.delivered > N / 2 - 10 && r.delivered <= N / 2);

    // (c) 消费者停顿 100 拍后恢复：既不丢数据也不溢出
    r = simulate(3, 5, 1, N, [](uint64_t t) { return t < 200 || t >= 300; });
    printf("long stall: maxOcc=%u\n", r.maxOccupancy);
    assert(r.maxOccupancy == 3);

    printf("03_credit: all tests passed\n");
    return 0;
}
