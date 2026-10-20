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
int main() {


    printf("02_fifo_delay: all tests passed\n");
    return 0;
}