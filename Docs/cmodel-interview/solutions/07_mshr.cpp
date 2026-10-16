// 07 非阻塞 cache 的 MSHR：缺失合并、MSHR/target 满时反压
// 编译：g++ -std=c++17 -O2 -Wall -Wextra 07_mshr.cpp && ./a.out
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <unordered_set>
#include <vector>

struct Resp { int id; int cycle; };

class NonBlockingCache {
public:
    NonBlockingCache(int numMshr, int maxTargets, int hitLat, int memLat)
        : mshr_(numMshr), maxTargets_(maxTargets), hitLat_(hitLat), memLat_(memLat) {}

    // 每拍最先调用：处理到期的回填，释放 MSHR，给所有合并的请求回响应
    void processFills(int t) {
        for (auto& m : mshr_) {
            if (m.valid && m.fillAt == t) {
                present_.insert(m.line);
                for (int id : m.targets) resps.push_back({id, t});
                m = Mshr{};
            }
        }
    }

    // 尝试接收一个请求；返回 false 表示本拍拒收（上游下一拍重试）
    bool request(int id, uint64_t line, int t) {
        if (present_.count(line)) {  // 命中
            resps.push_back({id, t + hitLat_});
            return true;
        }
        for (auto& m : mshr_) {      // 次级缺失：同一行已经在路上，合并
            if (m.valid && m.line == line) {
                if ((int)m.targets.size() >= maxTargets_) return false;
                m.targets.push_back(id);
                return true;
            }
        }
        for (auto& m : mshr_) {      // 初级缺失：分配新 MSHR，向下发请求
            if (!m.valid) {
                m = Mshr{true, line, t + memLat_, {id}};
                return true;
            }
        }
        return false;                // MSHR 全满
    }

    std::vector<Resp> resps;

private:
    struct Mshr { bool valid = false; uint64_t line = 0; int fillAt = 0; std::vector<int> targets; };
    std::vector<Mshr> mshr_;
    int maxTargets_, hitLat_, memLat_;
    std::unordered_set<uint64_t> present_;  // 只关注 MSHR，tag 阵列简化为无限容量
};

int main() {
    // 2 个 MSHR，每个最多 2 个 target，命中延迟 1，访存延迟 10
    NonBlockingCache c(2, 2, 1, 10);
    // 上游按序发请求，每拍最多一个，被拒就下一拍重发同一个
    const uint64_t A = 0, B = 1, C = 2, D = 3;
    std::vector<uint64_t> lines = {A, A, A, B, C, D, B};
    std::vector<int> acceptAt(lines.size(), -1);
    size_t head = 0;
    for (int t = 0; t < 100 && (head < lines.size() || c.resps.size() < lines.size()); ++t) {
        c.processFills(t);
        if (head < lines.size() && c.request((int)head, lines[head], t)) acceptAt[head++] = t;
    }
    std::vector<int> respAt(lines.size(), -1);
    for (auto& r : c.resps) respAt[r.id] = r.cycle;
    const std::vector<int> expAccept = {0, 1, 10, 11, 12, 21, 22};
    const std::vector<int> expResp = {10, 10, 11, 21, 22, 31, 23};
    for (size_t i = 0; i < lines.size(); ++i)
        printf("req %zu line %llu accepted@%d resp@%d\n", i, (unsigned long long)lines[i], acceptAt[i], respAt[i]);
    assert(acceptAt == expAccept);
    assert(respAt == expResp);
    printf("07_mshr: all tests passed\n");
    return 0;
}
