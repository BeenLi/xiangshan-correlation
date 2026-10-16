// 02 环形 FIFO + 定延迟队列（DelayQueue）
// 编译：g++ -std=c++17 -O2 -Wall -Wextra 02_fifo_delay.cpp && ./a.out
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <vector>

// ---------------- A. 环形 FIFO ----------------
// 读写指针各多带一位"圈数位"：指针范围 [0, 2N)，
//   空：rd == wr；满：两者差 N。这样不需要额外的 count，和 RTL 写法一一对应。
template <typename T>
class RingFifo {
public:
    explicit RingFifo(uint32_t cap) : buf_(cap), cap_(cap) { assert(cap > 0); }
    bool empty() const { return rd_ == wr_; }
    bool full() const { return size() == cap_; }
    uint32_t size() const { return (wr_ + 2 * cap_ - rd_) % (2 * cap_); }

    bool push(const T& v) {
        if (full()) return false;
        buf_[wr_ % cap_] = v;
        wr_ = (wr_ + 1) % (2 * cap_);
        return true;
    }
    const T& front() const { assert(!empty()); return buf_[rd_ % cap_]; }
    void pop() { assert(!empty()); rd_ = (rd_ + 1) % (2 * cap_); }

private:
    std::vector<T> buf_;
    uint32_t cap_;
    uint32_t rd_ = 0, wr_ = 0;
};

// ---------------- B. 定延迟队列 ----------------
// push 发生在 now，数据在 now + latency 拍起可见（ready）。有容量上限，按序出队。
// 每个元素带一个 readyCycle 时间戳；因为延迟固定，时间戳天然单调，队头最早就绪。
template <typename T>
class DelayQueue {
public:
    DelayQueue(uint32_t latency, uint32_t cap) : lat_(latency), q_(cap) {}
    bool canPush() const { return !q_.full(); }
    void push(const T& v, uint64_t now) {
        bool ok = q_.push({v, now + lat_});
        assert(ok);
        (void)ok;
    }
    bool ready(uint64_t now) const { return !q_.empty() && q_.front().readyAt <= now; }
    T pop(uint64_t now) {
        assert(ready(now));
        T v = q_.front().v;
        q_.pop();
        return v;
    }
    uint32_t size() const { return q_.size(); }

private:
    struct Entry { T v; uint64_t readyAt; };
    uint32_t lat_;
    RingFifo<Entry> q_;
};

int main() {
    // A：容量 3 的 FIFO，交替 push/pop 让指针多次回绕
    RingFifo<int> f(3);
    assert(f.empty() && !f.full());
    assert(f.push(1) && f.push(2) && f.push(3));
    assert(f.full() && !f.push(4));
    assert(f.front() == 1); f.pop();
    assert(f.push(4));
    int expect = 2;
    for (int i = 0; i < 100; ++i) {  // 稳态：每次 pop 一个 push 一个，检查顺序
        assert(f.front() == expect); f.pop();
        assert(f.push(expect + 3));
        ++expect;
    }
    assert(f.size() == 3);

    // B：延迟 3、容量 2。每拍顺序：先出队（消费者），再入队（生产者）。
    //    生产者每拍都想发一个新编号；队满就等。
    DelayQueue<int> dq(3, 2);
    int next = 0;
    std::vector<int> pushCycle, popCycle;
    for (uint64_t t = 0; t < 12; ++t) {
        if (dq.ready(t)) { dq.pop(t); popCycle.push_back((int)t); }
        if (dq.canPush()) { dq.push(next++, t); pushCycle.push_back((int)t); }
    }
    // 期望：t0 推 0，t1 推 1，t2/t3 队满；t3 弹 0 后同拍推 2；t4 弹 1 推 3 ……
    const std::vector<int> expPush = {0, 1, 3, 4, 6, 7, 9, 10};
    const std::vector<int> expPop = {3, 4, 6, 7, 9, 10};
    for (int c : pushCycle) printf("push@%d ", c);
    printf("\n");
    for (int c : popCycle) printf("pop@%d ", c);
    printf("\n");
    assert(pushCycle == expPush);
    assert(popCycle == expPop);
    // 吞吐 = 容量 / 延迟 = 2/3，正是 credit 题要讲的"缓冲深度 < 往返延迟会掉吞吐"
    printf("02_fifo_delay: all tests passed\n");
    return 0;
}
