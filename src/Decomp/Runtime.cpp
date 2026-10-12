#include "Decomp/Runtime.h"
#include "Decomp/Caller.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace psp {

uint8_t* g_mem = nullptr;
uint32_t g_trace[256];
uint32_t g_tracePos = 0;
thread_local uint32_t g_sp = 0;
thread_local Vfpu g_vfpu;

void Fatal(const char* fmt, ...) {
    va_list ap; va_start(ap, fmt);
    std::fprintf(stderr, "[psp] fatal: ");
    std::vfprintf(stderr, fmt, ap);
    std::fprintf(stderr, "\n");
    va_end(ap);
    std::abort();
}

void Unimplemented(uint32_t addr, const char* name) {
    static std::unordered_set<uint32_t> seen;
    if (seen.insert(addr).second) {
        std::fprintf(stderr, "[psp] unimplemented %08x %s (after", addr, name);
        for (uint32_t i = 4; i > 0; i--) std::fprintf(stderr, " %08x", g_trace[(g_tracePos - i) & 255]);
        std::fprintf(stderr, ")\n");
    }
}

// Allegrex VFPU constants (vcst), index = encoded constant number.
float VfpuConst(int i) {
    static const float k[32] = {
        0.f, 3.40282347e38f, 1.41421356f, 0.70710678f, 1.12837917f, 0.63661977f, 0.31830989f, 0.78539816f,
        1.57079633f, 3.14159265f, 2.71828183f, 1.44269504f, 0.43429448f, 0.69314718f, 2.30258509f, 6.28318531f,
        0.52359878f, 0.30103000f, 3.32192809f, 0.86602540f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    return k[i & 31];
}

// vcmp condition codes: FL EQ LT LE TR NE GE GT, EZ EN EI ES NZ NN NI NS
uint32_t VCmp(int op, const V16& a, const V16& b, int n) {
    uint32_t cc = 0, any = 0, all = 1;
    for (int i = 0; i < n; i++) {
        float x = a.f(i), y = b.f(i);
        bool r = false;
        switch (op & 15) {
        case 0: r = false; break;          case 1: r = x == y; break;
        case 2: r = x < y; break;          case 3: r = x <= y; break;
        case 4: r = true; break;           case 5: r = x != y; break;
        case 6: r = x >= y; break;         case 7: r = x > y; break;
        case 8: r = x == 0; break;         case 9: r = std::isnan(x); break;
        case 10: r = std::isinf(x); break; case 11: r = std::isnan(x) || std::isinf(x); break;
        case 12: r = x != 0; break;        case 13: r = !std::isnan(x); break;
        case 14: r = !std::isinf(x); break; case 15: r = !(std::isnan(x) || std::isinf(x)); break;
        }
        cc |= uint32_t(r) << i; any |= r; all &= r;
    }
    return cc | (any << 4) | (all << 5);
}

// String literals the decompiler inlined: interned in a guest pool at the top of memory.
uint32_t GStr(const char* s) {
    static std::unordered_map<std::string, uint32_t> pool;
    static uint32_t next = kMemSize - 0x100000;
    auto it = pool.find(s);
    if (it != pool.end()) return it->second;
    size_t n = std::strlen(s) + 1;
    if (next + n > kMemSize) Fatal("string pool full");
    std::memcpy(g_mem + next, s, n);
    pool.emplace(s, next);
    uint32_t a = next;
    next += uint32_t((n + 3) & ~size_t(3));
    return a;
}

void builtin_strncpy(uint32_t dst, const char* src, int n) { std::strncpy(reinterpret_cast<char*>(g_mem + dst), src, size_t(n)); }

} // namespace psp
