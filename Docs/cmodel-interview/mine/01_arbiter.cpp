#include <cstdio>
#include <cstdint>
#include <cassert>
#include <vector>

// A. 固定优先级：下标越小优先级越高。纯组合，无状态。
inline bool getSpecificBit(uint64_t req, int bit_index) {
    return (req >> bit_index & 0x1);
}
class FixedPriorityArbiter {
    public:
        explicit FixedPriorityArbiter(int n): input_nums(n) 
            {};
        int arbitrate(uint64_t req) const {
            for (int i=0; i < input_nums; i++) {
                if (getSpecificBit(req, i)) {
                    return i;
                }
            }
            return -1;
        }

    private:
        int input_nums;
};

// B. 轮询（round-robin）：被授权者在下一拍变成最低优先级。
//    复位后请求者 0 优先级最高。没有授权时状态不变。
class RoundRobinArbiter {
    public:
        explicit RoundRobinArbiter(int n):rr_pointer(0),input_num(n) 
            {};
        // 每拍调用一次，相当于一个时钟沿
        int arbitrate(uint64_t req) {
            for(int i=0; i<input_num; i++) {
                if (getSpecificBit(req, rr_pointer)) {
                    int result = rr_pointer;
                    rr_pointer = (rr_pointer + 1) % input_num;
                    return result;
                } else {
                    rr_pointer = (rr_pointer + 1) % input_num;
                }
            }
            return -1;
        }

        int ptr() const {return rr_pointer;}
    private:
        int rr_pointer;
        int input_num;
};


// C. 带包锁定的轮询：一个包占多拍，last[i]=1 表示请求者 i 本拍发的是包的最后一拍。
//    某请求者一旦被授权且这一拍不是 last，就锁定给它，直到它的 last 拍被授权为止。
//    锁定期间 owner 本拍没请求（包中间的气泡），也不能让给别人，返回 -1。
class LockingRRArbiter {
    public:
        explicit LockingRRArbiter(int n): input_num(n), rr_pointer(0), 
            lockOwner(-1) {};
        int arbitrate(uint64_t req, uint64_t last){
            if (lockOwner != -1) {
                int result = lockOwner;
                if (getSpecificBit(last, lockOwner)) {
                    lockOwner = -1;
                    rr_pointer = (rr_pointer + 1) % input_num;
                }
                return result;
            } else {
                for(int i=0; i<input_num; i++) {
                    if (getSpecificBit(req, rr_pointer)) {
                        int result = rr_pointer;
                        lockOwner = result;
                        if (getSpecificBit(last, lockOwner)) {
                            lockOwner = -1;
                            rr_pointer = (rr_pointer + 1) % input_num;
                        }
                        return result;
                    } else {
                        rr_pointer = (rr_pointer + 1) % input_num;
                    }
                }
                return -1;
            }
        }
        int ptr() const {return rr_pointer;}
        int owner() const {return lockOwner;}
    
    private:
        int input_num;
        int rr_pointer;
        int lockOwner;
};

// D. DWRR（Deficit Weighted Round Robin）：按包长计费的加权轮询。
//    每个请求者有权重 quantum[i] 和一个 deficit 计数器（复位为 0）。len[i] 是请求者 i 队头包的长度，
//    只在 req 第 i 位为 1 时有效。每次调用选出一个包（不管包要发几拍）。
//    规则（指针 ptr 复位指向 0）：
//      1. req 全 0：返回 -1，状态不变。
//      2. 指针指向的 i 没请求：deficit[i] 清零，指针移到下一个。
//      3. 指针刚移到 i（一次"访问"开始）：deficit[i] += quantum[i]。一次访问只加一次。
//      4. len[i] <= deficit[i]：授权 i，deficit[i] -= len[i]，指针不动（下次调用继续访问 i）。
//      5. 否则结束对 i 的访问，余额保留，指针移到下一个，回到第 2 步。
class DWRRArbiter {
    public:
        explicit DWRRArbiter(std::vector<int> quantum):input_num(quantum.size()),
            cur_pointer(0), first_touch(true),
            _quantum(quantum), _deficit(quantum.size(), 0) {};
        int arbitrate(uint64_t req, const std::vector<int>& len) {
            if (req == 0) {
                return -1;
            }
            for (int i=0; i<input_num; i++) {
                if (getSpecificBit(req, cur_pointer)) {
                    if (first_touch) {
                        _deficit[cur_pointer] += _quantum[cur_pointer];
                    }
                    if (_deficit[cur_pointer] >= len[cur_pointer]) {
                        _deficit[cur_pointer] -= len[cur_pointer];
                        first_touch = false;
                        return cur_pointer;
                    } else {
                        cur_pointer = (cur_pointer + 1) % input_num;
                        first_touch = true;
                    }
                } else {
                    _deficit[cur_pointer] = 0;
                    cur_pointer = (cur_pointer + 1) % input_num;
                    first_touch = true;
                }
            }
            first_touch = true;
            return -1;
        }
        int deficit(int i) const {return _deficit[i];}
    
    private:
        int input_num;
        int cur_pointer;
        bool first_touch;
        std::vector<int> _quantum;
        std::vector<int> _deficit;
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
        printf("LCK cycle %d grant=%d owner=%d rrPointer=%d\n", t, g, lk.owner(), lk.ptr());
        assert(g == seq[t].expect);
    }

    // D：quantum = {4, 2, 2}，队头包长固定为 {3, 3, 1}
    DWRRArbiter dw({4, 2, 2});
    const std::vector<int> len = {3, 3, 1};
    struct { uint64_t req; int expect, d0, d1, d2; } dseq[] = {
        {0b111, 0, 1, 0, 0},   // 访问 0：0+4=4，发 3 余 1
        {0b111, 2, 1, 2, 1},   // 0 余 1 不够；访问 1：0+2=2 < 3 跳过（余额留着）；访问 2：发 1 余 1
        {0b111, 2, 1, 2, 0},   // 2 再发一个，余 0
        {0b111, 0, 2, 2, 0},   // 访问 0：1+4=5，发 3 余 2
        {0b101, 2, 2, 0, 1},   // 0 余 2 不够；1 没请求 → deficit 清零；访问 2
        {0b101, 2, 2, 0, 0},
        {0b101, 0, 3, 0, 0},   // 访问 0：2+4=6，发 3 余 3
        {0b111, 0, 0, 0, 0},   // 余 3 够再发一个
        {0b111, 2, 0, 2, 1},   // 1 刚清过零，0+2=2 < 3 又被跳过
        {0b111, 2, 0, 2, 0},
        {0b000, -1, 0, 2, 0},  // 没人请求：状态不变
        {0b111, 0, 1, 2, 0},
    };
    for (int t = 0; t < 12; ++t) {
        int g = dw.arbitrate(dseq[t].req, len);
        printf("DWRR cycle %d grant=%d deficit={%d,%d,%d}\n", t, g, dw.deficit(0), dw.deficit(1), dw.deficit(2));
        assert(g == dseq[t].expect);
        assert(dw.deficit(0) == dseq[t].d0 && dw.deficit(1) == dseq[t].d1 && dw.deficit(2) == dseq[t].d2);
    }

    // 带宽按 quantum 分配：包长都为 1 时，600 次授权里正好 100 / 200 / 300
    DWRRArbiter dw2({1, 2, 3});
    int dcnt[3] = {};
    for (int t = 0; t < 600; ++t) {
        int g = dw2.arbitrate(0b111, {1, 1, 1});
        assert(g >= 0);
        dcnt[g]++;
    }
    assert(dcnt[0] == 100 && dcnt[1] == 200 && dcnt[2] == 300);

    // 包长不同时按字节公平：quantum {4,2,2}、包长 {3,3,1}，10000 次授权后字节数约 2:1:1
    DWRRArbiter dw3({4, 2, 2});
    long bytes[3] = {};
    for (int t = 0; t < 10000; ++t) {
        int g = dw3.arbitrate(0b111, len);
        assert(g >= 0);
        bytes[g] += len[g];
    }
    assert(bytes[0] == 10002 && bytes[1] == 4998 && bytes[2] == 5000);


    printf("01_arbiter: all tests passed\n");
    return 0;
}
