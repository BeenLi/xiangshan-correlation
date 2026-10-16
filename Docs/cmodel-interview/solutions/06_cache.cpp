// 06 组相联 cache：LRU、写回 + 写分配；B 部分为 GPU 风格的 sector cache
// 编译：g++ -std=c++17 -O2 -Wall -Wextra 06_cache.cpp && ./a.out
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <vector>

static bool isPow2(uint64_t x) { return x && !(x & (x - 1)); }
static int log2u(uint64_t x) { return 63 - __builtin_clzll(x); }

// ---------------- A. 写回、写分配、真 LRU ----------------
struct AccessResult {
    bool hit;
    bool writeback;      // 是否踢出了脏行
    uint64_t wbAddr;     // 被写回行的首地址
};

class Cache {
public:
    Cache(uint64_t sizeBytes, int assoc, int lineBytes) : assoc_(assoc) {
        assert(isPow2(sizeBytes) && isPow2(lineBytes) && sizeBytes % (assoc * lineBytes) == 0);
        sets_ = sizeBytes / (assoc * lineBytes);
        assert(isPow2(sets_));
        offBits_ = log2u(lineBytes);
        idxBits_ = log2u(sets_);
        lines_.resize(sets_ * assoc_);
    }

    AccessResult access(uint64_t addr, bool isWrite) {
        ++clock_;
        uint64_t set = (addr >> offBits_) & (sets_ - 1);
        uint64_t tag = addr >> (offBits_ + idxBits_);
        Line* way = &lines_[set * assoc_];

        for (int w = 0; w < assoc_; ++w) {
            if (way[w].valid && way[w].tag == tag) {
                way[w].lastUse = clock_;
                way[w].dirty |= isWrite;
                ++hits;
                return {true, false, 0};
            }
        }
        ++misses;
        // 选牺牲者：优先无效行，否则 lastUse 最小的（最久未用）
        int victim = 0;
        for (int w = 0; w < assoc_; ++w) {
            if (!way[w].valid) { victim = w; break; }
            if (way[w].lastUse < way[victim].lastUse) victim = w;
        }
        AccessResult r{false, false, 0};
        Line& v = way[victim];
        if (v.valid && v.dirty) {
            r.writeback = true;
            r.wbAddr = ((v.tag << idxBits_) | set) << offBits_;  // 用 tag 和 set 拼回地址
            ++writebacks;
        }
        v = Line{true, isWrite, tag, clock_};  // 写分配：写缺失也先把行取进来，再标脏
        return r;
    }

    uint64_t hits = 0, misses = 0, writebacks = 0;

private:
    struct Line { bool valid = false; bool dirty = false; uint64_t tag = 0; uint64_t lastUse = 0; };
    int assoc_;
    uint64_t sets_;
    int offBits_, idxBits_;
    uint64_t clock_ = 0;
    std::vector<Line> lines_;
};

// ---------------- B. Sector cache（只读即可）：tag 按 128B 行分配，数据按 32B sector 取 ----------------
class SectorCache {
public:
    SectorCache(int sets, int assoc, int lineBytes, int sectorBytes)
        : sets_(sets), assoc_(assoc), lineBytes_(lineBytes), sectorBytes_(sectorBytes), lines_(sets * assoc) {
        assert(lineBytes / sectorBytes <= 32);
    }

    void read(uint64_t addr) {
        ++clock_;
        uint64_t lineAddr = addr / lineBytes_;
        uint64_t set = lineAddr % sets_;
        uint64_t tag = lineAddr / sets_;
        uint32_t secBit = 1u << ((addr % lineBytes_) / sectorBytes_);
        Line* way = &lines_[set * assoc_];
        for (int w = 0; w < assoc_; ++w) {
            if (way[w].valid && way[w].tag == tag) {
                way[w].lastUse = clock_;
                if (way[w].sectorMask & secBit) { ++hit; return; }
                ++sectorMiss;  // tag 命中但该 sector 不在：只取 32B，不换行
                way[w].sectorMask |= secBit;
                bytesFetched += sectorBytes_;
                return;
            }
        }
        ++lineMiss;
        int victim = 0;
        for (int w = 0; w < assoc_; ++w) {
            if (!way[w].valid) { victim = w; break; }
            if (way[w].lastUse < way[victim].lastUse) victim = w;
        }
        way[victim] = Line{true, tag, secBit, clock_};
        bytesFetched += sectorBytes_;
    }

    uint64_t hit = 0, sectorMiss = 0, lineMiss = 0, bytesFetched = 0;

private:
    struct Line { bool valid = false; uint64_t tag = 0; uint32_t sectorMask = 0; uint64_t lastUse = 0; };
    int sets_, assoc_, lineBytes_, sectorBytes_;
    uint64_t clock_ = 0;
    std::vector<Line> lines_;
};

int main() {
    // A：128B、2 路、16B 行 → 4 组。地址 [3:0] 偏移，[5:4] 组号，其余 tag
    Cache c(128, 2, 16);
    struct { uint64_t addr; bool w; bool hit; bool wb; uint64_t wbAddr; } seq[] = {
        {0x000, false, false, false, 0},
        {0x040, true, false, false, 0},
        {0x004, false, true, false, 0},
        {0x080, false, false, true, 0x040},  // 组 0 满，踢掉 LRU 的 0x040（脏）
        {0x000, true, true, false, 0},
        {0x0C0, false, false, false, 0},     // 踢掉 0x080（干净）
        {0x010, false, false, false, 0},     // 组 1
        {0x040, false, false, true, 0x000},  // 踢掉 0x000（第 5 步写脏）
    };
    for (auto& s : seq) {
        AccessResult r = c.access(s.addr, s.w);
        printf("%c 0x%03llx -> %s%s", s.w ? 'W' : 'R', (unsigned long long)s.addr, r.hit ? "hit " : "miss",
               r.writeback ? " writeback" : "");
        if (r.writeback) printf(" 0x%03llx", (unsigned long long)r.wbAddr);
        printf("\n");
        assert(r.hit == s.hit && r.writeback == s.wb);
        if (r.writeback) assert(r.wbAddr == s.wbAddr);
    }
    assert(c.hits == 2 && c.misses == 6 && c.writebacks == 2);

    // B：2 组 2 路，128B 行，32B sector
    SectorCache sc(2, 2, 128, 32);
    for (uint64_t a : {0x000, 0x020, 0x010, 0x100, 0x200, 0x030, 0x0A0}) sc.read(a);
    printf("sector cache: hit=%llu sectorMiss=%llu lineMiss=%llu bytes=%llu\n", (unsigned long long)sc.hit,
           (unsigned long long)sc.sectorMiss, (unsigned long long)sc.lineMiss, (unsigned long long)sc.bytesFetched);
    assert(sc.hit == 1 && sc.sectorMiss == 1 && sc.lineMiss == 5 && sc.bytesFetched == 192);
    printf("06_cache: all tests passed\n");
    return 0;
}
