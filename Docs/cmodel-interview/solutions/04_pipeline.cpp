// 04 五级流水线：stall（load-use）、flush（分支误预测）、反压（MEM 忙）
// 编译：g++ -std=c++17 -O2 -Wall -Wextra 04_pipeline.cpp && ./a.out
#include <array>
#include <cassert>
#include <cstdio>
#include <optional>
#include <set>
#include <vector>

enum class Kind { ALU, LOAD, BRANCH };

struct Inst {
    int id;           // 程序里的下标，打印用
    Kind kind;
    int dst;          // -1 表示不写寄存器
    int src1, src2;   // -1 表示不读
    bool mispredict;  // 只对 BRANCH 有意义：在 EX 解析时发现预测错
    int target;       // 误预测后的正确取指地址
};

enum Stage { IF, ID, EX, MEM, WB, NSTAGE };

class Pipeline {
public:
    Pipeline(std::vector<Inst> prog, std::set<int> memBusy)
        : prog_(std::move(prog)), memBusy_(std::move(memBusy)), commitCycle_(prog_.size(), -1) {}

    // 运行到流水线排空，返回每条指令的提交（离开 WB）周期，被冲掉的为 -1
    std::vector<int> run(bool verbose) {
        if (verbose) printf("cycle |  IF  ID  EX MEM  WB | event\n");
        for (int t = 0; t < 100; ++t) {
            if (pc_ >= (int)prog_.size() && empty()) break;
            tick(t, verbose);
        }
        return commitCycle_;
    }

private:
    using Slot = std::optional<Inst>;

    bool empty() const {
        for (auto& s : s_) if (s) return false;
        return true;
    }

    static bool reads(const Inst& i, int r) { return r >= 0 && (i.src1 == r || i.src2 == r); }

    // 两阶段：先只读 cur 计算所有握手信号，再一次性写出 next。
    // 握手信号从下游往上游推：ready_in[i] = 本级空 || 本级这拍会走掉。
    void tick(int t, bool verbose) {
        const auto& cur = s_;
        std::array<bool, NSTAGE> fire{};  // fire[i]：第 i 级的指令本拍流向 i+1（WB 表示提交）
        const char* event = "";

        fire[WB] = cur[WB].has_value();                       // WB 总能提交
        bool readyWB = true;                                  // WB 每拍都会腾空
        bool memBusy = cur[MEM] && cur[MEM]->kind == Kind::LOAD && memBusy_.count(t);
        fire[MEM] = cur[MEM] && !memBusy && readyWB;
        bool readyMEM = !cur[MEM] || fire[MEM];
        fire[EX] = cur[EX] && readyMEM;
        bool readyEX = !cur[EX] || fire[EX];
        // load-use：ID 中指令依赖"本拍开始时"在 EX 的 load，必须停一拍（EX 插入气泡）
        bool hazard = cur[ID] && cur[EX] && cur[EX]->kind == Kind::LOAD && reads(*cur[ID], cur[EX]->dst);
        fire[ID] = cur[ID] && !hazard && readyEX;
        bool readyID = !cur[ID] || fire[ID];
        fire[IF] = cur[IF] && readyID;
        bool readyIF = !cur[IF] || fire[IF];

        // 分支在离开 EX 时解析；误预测则冲掉比它年轻的 IF、ID，并重定向 pc
        bool flush = fire[EX] && cur[EX]->kind == Kind::BRANCH && cur[EX]->mispredict;

        if (memBusy) event = "MEM busy -> backpressure";
        else if (hazard) event = "load-use stall";
        if (flush) event = "mispredict -> flush IF/ID";

        std::array<Slot, NSTAGE> nxt{};
        if (fire[WB]) commitCycle_[cur[WB]->id] = t;
        nxt[WB] = fire[MEM] ? cur[MEM] : Slot{};
        nxt[MEM] = fire[EX] ? cur[EX] : (fire[MEM] ? Slot{} : cur[MEM]);
        nxt[EX] = (fire[ID] && !flush) ? cur[ID] : (fire[EX] ? Slot{} : cur[EX]);
        if (flush) {
            nxt[ID] = Slot{};
            pc_ = cur[EX]->target;
        } else {
            nxt[ID] = fire[IF] ? cur[IF] : (fire[ID] ? Slot{} : cur[ID]);
        }
        // 取指：IF 能接收（或刚被冲掉）且还有指令，就取 pc 处的指令
        if ((flush || readyIF) && pc_ < (int)prog_.size()) {
            nxt[IF] = prog_[pc_++];
        } else {
            nxt[IF] = flush ? Slot{} : (fire[IF] ? Slot{} : cur[IF]);
        }

        // 打印"本拍开始时"各级的占用，和本拍发生的事件（第 0 拍还没有指令，只做取指）
        if (verbose) {
            printf("%5d |", t);
            for (int i = 0; i < NSTAGE; ++i) {
                if (cur[i]) printf(" %3d", cur[i]->id); else printf("   -");
            }
            printf(" | %s\n", event);
        }
        s_ = nxt;
    }

    std::vector<Inst> prog_;
    std::set<int> memBusy_;
    std::vector<int> commitCycle_;
    std::array<Slot, NSTAGE> s_{};
    int pc_ = 0;
};

int main() {
    //  id kind           dst s1 s2 mis tgt
    std::vector<Inst> prog = {
        {0, Kind::ALU, 1, 0, 0, false, 0},     // r1 = r0 + r0
        {1, Kind::LOAD, 2, 1, -1, false, 0},   // r2 = [r1]
        {2, Kind::ALU, 3, 2, 1, false, 0},     // r3 = r2 + r1   ← 依赖上一条 load
        {3, Kind::BRANCH, -1, 3, -1, true, 6}, // beq r3, ...    ← 实际跳到 6，预测成不跳
        {4, Kind::ALU, 4, 1, 1, false, 0},     // 错误路径
        {5, Kind::ALU, 5, 1, 1, false, 0},     // 错误路径
        {6, Kind::LOAD, 6, 3, -1, false, 0},   // r6 = [r3]      ← MEM 在第 11、12 拍忙
        {7, Kind::ALU, 7, 1, 1, false, 0},     // r7 = r1 + r1
    };
    Pipeline p(prog, {11, 12});
    std::vector<int> c = p.run(true);
    const std::vector<int> expect = {5, 6, 8, 9, -1, -1, 14, 15};
    for (size_t i = 0; i < c.size(); ++i) printf("inst %zu commit@%d\n", i, c[i]);
    assert(c == expect);
    printf("04_pipeline: all tests passed\n");
    return 0;
}
