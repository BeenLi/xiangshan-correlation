// 08 gem5 风格的离散事件队列：schedule / deschedule / reschedule，(时间, 优先级, 插入序) 全序
// 编译：g++ -std=c++17 -O2 -Wall -Wextra 08_event_queue.cpp && ./a.out
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <set>
#include <string>
#include <vector>

using Tick = uint64_t;

class Event {
public:
    // 和 gem5 一样：数值越小越先执行
    static constexpr int kDefaultPri = 0;
    Event(std::string name, std::function<void()> fn, int pri = kDefaultPri)
        : name_(std::move(name)), fn_(std::move(fn)), pri_(pri) {}
    bool scheduled() const { return scheduled_; }
    Tick when() const { return when_; }
    const std::string& name() const { return name_; }

private:
    friend class EventQueue;
    std::string name_;
    std::function<void()> fn_;
    int pri_;
    bool scheduled_ = false;
    Tick when_ = 0;
    uint64_t seq_ = 0;
};

class EventQueue {
public:
    Tick curTick() const { return cur_; }

    void schedule(Event* e, Tick when) {
        assert(!e->scheduled_ && when >= cur_);  // 不能调度到过去
        e->when_ = when;
        e->seq_ = nextSeq_++;
        e->scheduled_ = true;
        q_.insert(e);
    }
    void deschedule(Event* e) {
        assert(e->scheduled_);
        q_.erase(e);  // 比较器用 (when, pri, seq) 唯一定位，O(log n)
        e->scheduled_ = false;
    }
    void reschedule(Event* e, Tick when) {
        if (e->scheduled_) deschedule(e);
        schedule(e, when);
    }

    // 执行所有 when <= until 的事件；事件回调里可以继续 schedule 新事件
    void serviceUntil(Tick until) {
        while (!q_.empty()) {
            Event* e = *q_.begin();
            if (e->when_ > until) break;
            q_.erase(q_.begin());
            e->scheduled_ = false;  // 先清标志再回调，回调里才能重新 schedule 自己
            cur_ = e->when_;
            e->fn_();
        }
        cur_ = until;
    }
    bool empty() const { return q_.empty(); }

private:
    struct Cmp {
        bool operator()(const Event* a, const Event* b) const {
            if (a->when_ != b->when_) return a->when_ < b->when_;
            if (a->pri_ != b->pri_) return a->pri_ < b->pri_;
            return a->seq_ < b->seq_;  // 同时同优先级按插入顺序，保证结果确定
        }
    };
    std::set<Event*, Cmp> q_;
    Tick cur_ = 0;
    uint64_t nextSeq_ = 0;
};

// 用"自己调度自己"实现的时钟对象，类似 gem5 的 ClockedObject::tickEvent
class ClockedCounter {
public:
    ClockedCounter(EventQueue& eq, const char* name, Tick period, std::vector<std::string>& log)
        : eq_(eq), period_(period), log_(log), ev_(name, [this] { tick(); }) {}
    void start(Tick at) { eq_.schedule(&ev_, at); }
    void stop() { if (ev_.scheduled()) eq_.deschedule(&ev_); }
    int ticks = 0;

private:
    void tick() {
        ++ticks;
        log_.push_back(ev_.name() + "@" + std::to_string(eq_.curTick()));
        eq_.schedule(&ev_, eq_.curTick() + period_);
    }
    EventQueue& eq_;
    Tick period_;
    std::vector<std::string>& log_;
    Event ev_;
};

int main() {
    // (a) 排序规则
    {
        EventQueue eq;
        std::vector<std::string> log;
        auto mk = [&](const char* n, int pri) { return Event(n, [&log, n] { log.push_back(n); }, pri); };
        Event e1 = mk("E1", 0), e2 = mk("E2", 0), e3 = mk("E3", -1), e4 = mk("E4", 0), e5 = mk("E5", 0);
        eq.schedule(&e1, 10);
        eq.schedule(&e2, 5);
        eq.schedule(&e3, 10);  // 同一时刻、更高优先级
        eq.schedule(&e4, 10);  // 同一时刻、同优先级、后插入
        eq.schedule(&e5, 7);
        eq.deschedule(&e4);
        eq.reschedule(&e5, 12);
        eq.serviceUntil(100);
        const std::vector<std::string> expect = {"E2", "E3", "E1", "E5"};
        for (auto& s : log) printf("%s ", s.c_str());
        printf("\n");
        assert(log == expect);
    }
    // (b) 两个不同周期的时钟对象 + 中途停掉一个
    {
        EventQueue eq;
        std::vector<std::string> log;
        ClockedCounter a(eq, "A", 3, log), b(eq, "B", 5, log);
        a.start(0);
        b.start(0);
        eq.serviceUntil(10);
        b.stop();  // 在第 10 拍执行完之后停掉 B
        eq.serviceUntil(20);
        for (auto& s : log) printf("%s ", s.c_str());
        printf("\nA ticks=%d B ticks=%d\n", a.ticks, b.ticks);
        // A: 0,3,6,9,12,15,18 共 7 次；B: 0,5,10 共 3 次（15 被取消）
        assert(a.ticks == 7 && b.ticks == 3);
        const std::vector<std::string> head = {"A@0", "B@0", "A@3", "B@5", "A@6", "A@9", "B@10"};
        assert(std::vector<std::string>(log.begin(), log.begin() + 7) == head);
    }
    printf("08_event_queue: all tests passed\n");
    return 0;
}
