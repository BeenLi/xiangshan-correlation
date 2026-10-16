// 05 状态机：单个 DRAM bank 控制器（开页策略、FCFS）
// 编译：g++ -std=c++17 -O2 -Wall -Wextra 05_dram_bank_fsm.cpp && ./a.out
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <vector>

struct Timing {
    int tRCD = 3;  // ACT -> RD
    int tCL = 3;   // RD -> 第一拍数据
    int tBL = 2;   // 突发占用的数据拍数（也作为 RD -> RD 的最小间隔 tCCD）
    int tRP = 3;   // PRE -> ACT
    int tRAS = 6;  // ACT -> PRE
};

enum class State { IDLE, ACTIVATING, ACTIVE, PRECHARGING };
enum class Kind { MISS, HIT, CONFLICT };  // 请求到达队头时：bank 关着 / 行命中 / 行冲突

struct Req {
    int id;
    int arrive;
    int row;
    int done = -1;      // 最后一拍数据返回的周期
    Kind kind = Kind::MISS;
    bool classified = false;
};

class BankCtrl {
public:
    explicit BankCtrl(Timing tm) : tm_(tm) {}
    void enqueue(Req* r) { q_.push_back(r); }
    bool idle() const { return q_.empty(); }

    // 每拍：先做"计时到期"的状态转移，再按当前状态最多发一条命令
    void tick(int t) {
        if (state_ == State::ACTIVATING && t >= readyAt_) state_ = State::ACTIVE;
        if (state_ == State::PRECHARGING && t >= readyAt_) { state_ = State::IDLE; openRow_ = -1; }

        if (q_.empty()) return;
        Req* h = q_.front();
        if (!h->classified) {  // 请求第一次站到队头时，按当时的 bank 状态分类
            h->classified = true;
            if (openRow_ < 0) h->kind = Kind::MISS;
            else h->kind = (h->row == openRow_) ? Kind::HIT : Kind::CONFLICT;
        }

        switch (state_) {
        case State::IDLE:  // 预充完毕，可以激活
            openRow_ = h->row;
            actAt_ = t;
            readyAt_ = t + tm_.tRCD;
            state_ = State::ACTIVATING;
            log(t, "ACT", h->row);
            break;
        case State::ACTIVATING:  // 等 tRCD
        case State::PRECHARGING:  // 等 tRP
            break;
        case State::ACTIVE:
            if (h->row == openRow_) {
                if (t >= nextRd_) {
                    h->done = t + tm_.tCL + tm_.tBL;
                    nextRd_ = t + tm_.tBL;
                    q_.pop_front();
                    log(t, "RD", h->row);
                }
            } else if (t >= actAt_ + tm_.tRAS) {  // 行冲突：满足 tRAS 才能关行
                state_ = State::PRECHARGING;
                readyAt_ = t + tm_.tRP;
                log(t, "PRE", openRow_);
            }
            break;
        }
    }

private:
    void log(int t, const char* cmd, int row) { printf("  cycle %2d: %-3s row %d\n", t, cmd, row); }

    Timing tm_;
    State state_ = State::IDLE;
    int openRow_ = -1;
    int actAt_ = 0, readyAt_ = 0, nextRd_ = 0;
    std::deque<Req*> q_;
};

int main() {
    std::vector<Req> reqs = {{0, 0, 5}, {1, 1, 5}, {2, 2, 7}, {3, 3, 7}, {4, 10, 5}};
    BankCtrl bank(Timing{});
    size_t next = 0;
    for (int t = 0; t < 100; ++t) {
        while (next < reqs.size() && reqs[next].arrive == t) bank.enqueue(&reqs[next++]);
        bank.tick(t);
        if (next == reqs.size() && bank.idle()) break;
    }
    const int expDone[] = {8, 10, 17, 19, 26};
    const Kind expKind[] = {Kind::MISS, Kind::HIT, Kind::CONFLICT, Kind::HIT, Kind::CONFLICT};
    const char* kname[] = {"miss", "hit", "conflict"};
    for (auto& r : reqs) {
        printf("req %d row %d: %-8s done@%d\n", r.id, r.row, kname[(int)r.kind], r.done);
        assert(r.done == expDone[r.id]);
        assert(r.kind == expKind[r.id]);
    }
    printf("05_dram_bank_fsm: all tests passed\n");
    return 0;
}
