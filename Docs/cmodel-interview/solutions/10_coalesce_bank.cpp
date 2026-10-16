// 10 GPU 访存：全局内存合并（coalescing）+ 共享内存 bank conflict
// 编译：g++ -std=c++17 -O2 -Wall -Wextra 10_coalesce_bank.cpp && ./a.out
#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <map>
#include <set>
#include <vector>

constexpr int kWarp = 32;
using Addrs = std::array<uint64_t, kWarp>;

struct CoalesceResult {
    int lines;    // 涉及的 128B 行数
    int sectors;  // 涉及的 32B sector 数（= 真正要搬的 32B 事务数）
};

// A. 一条 warp 级的 4B load：把活跃线程的地址按 128B 行分组，行内记 sector 掩码
CoalesceResult coalesce(const Addrs& a, uint32_t activeMask, int bytesPerThread = 4) {
    std::map<uint64_t, uint32_t> lineSectors;  // 行号 -> 4 位 sector 掩码
    for (int i = 0; i < kWarp; ++i) {
        if (!((activeMask >> i) & 1)) continue;
        // 一个线程的访问可能跨 sector（例如未对齐的 8B/16B），首尾字节都要算
        for (uint64_t b : {a[i], a[i] + bytesPerThread - 1}) lineSectors[b / 128] |= 1u << ((b % 128) / 32);
    }
    CoalesceResult r{(int)lineSectors.size(), 0};
    for (auto& kv : lineSectors) r.sectors += __builtin_popcount(kv.second);
    return r;
}

// B. 共享内存：32 个 bank、每 bank 4B 宽。同一 bank 的不同 4B 字要串行；同一个字是广播，不冲突。
// 返回需要的周期数 = 各 bank 上不同字数量的最大值
int bankConflictCycles(const Addrs& a, uint32_t activeMask) {
    std::array<std::set<uint64_t>, 32> wordsPerBank;
    for (int i = 0; i < kWarp; ++i) {
        if (!((activeMask >> i) & 1)) continue;
        uint64_t word = a[i] / 4;
        wordsPerBank[word % 32].insert(word);
    }
    size_t worst = 0;
    for (auto& s : wordsPerBank) worst = std::max(worst, s.size());
    return (int)worst;
}

static Addrs strided(uint64_t base, uint64_t stride) {
    Addrs a{};
    for (int i = 0; i < kWarp; ++i) a[i] = base + i * stride;
    return a;
}

int main() {
    const uint32_t ALL = 0xFFFFFFFFu, EVEN = 0x55555555u;
    struct Case { const char* name; Addrs a; uint32_t mask; int lines, sectors, bankCycles; };
    std::vector<Case> cases = {
        {"stride 4B, aligned",     strided(0, 4),   ALL,  1, 4, 1},
        {"stride 8B",              strided(0, 8),   ALL,  2, 8, 2},
        {"stride 128B",            strided(0, 128), ALL,  32, 32, 32},
        {"stride 4B, base 4",      strided(4, 4),   ALL,  2, 5, 1},
        {"all same address",       strided(64, 0),  ALL,  1, 1, 1},
        {"stride 4B, even lanes",  strided(0, 4),   EVEN, 1, 4, 1},
        {"stride 132B (padding)",  strided(0, 132), ALL,  32, 32, 1},
    };
    for (auto& c : cases) {
        CoalesceResult r = coalesce(c.a, c.mask);
        int bc = bankConflictCycles(c.a, c.mask);
        printf("%-24s lines=%2d sectors=%2d smem_cycles=%2d\n", c.name, r.lines, r.sectors, bc);
        assert(r.lines == c.lines && r.sectors == c.sectors && bc == c.bankCycles);
    }
    printf("10_coalesce_bank: all tests passed\n");
    return 0;
}
