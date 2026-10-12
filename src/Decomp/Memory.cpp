// Guest memory: main.prx loaded at address 0 (its Ghidra image base), stack below the string pool.
#include "Decomp/Memory.h"
#include "Decomp/Runtime.h"

#include <cstdio>
#include <cstdlib>
#include <vector>

namespace psp {

bool Boot(const char* prxPath) {
    FILE* f = std::fopen(prxPath, "rb");
    if (!f) return false;
    std::vector<uint8_t> d;
    std::fseek(f, 0, SEEK_END);
    d.resize(size_t(std::ftell(f)));
    std::fseek(f, 0, SEEK_SET);
    size_t got = std::fread(d.data(), 1, d.size(), f);
    std::fclose(f);
    if (got != d.size() || d.size() < 0x34) return false;
    if (!g_mem) g_mem = static_cast<uint8_t*>(std::calloc(kMemSize, 1));
    auto u32 = [&](size_t o) { uint32_t v; std::memcpy(&v, &d[o], 4); return v; };
    uint32_t phoff = u32(0x1c);
    uint16_t phnum; std::memcpy(&phnum, &d[0x2c], 2);
    for (int i = 0; i < phnum; i++) {
        size_t p = phoff + size_t(i) * 32;
        uint32_t type = u32(p), off = u32(p + 4), vaddr = u32(p + 8), filesz = u32(p + 16), memsz = u32(p + 20);
        if (type != 1) continue;
        if (vaddr + memsz > kMemSize || off + filesz > d.size()) return false;
        std::memcpy(g_mem + vaddr, &d[off], filesz);   // bss stays zero
    }
    // PRX relocations add the load address; loading at 0 makes them no-ops.
    g_sp = kStackTop;
    return true;
}

// GCC .ctors: a list of function addresses, run last to first; 0 and -1 are sentinels.
int RunStaticCtors() {
    constexpr uint32_t kCtors = 0x3527a8, kCtorsSize = 0xa08;
    int n = 0;
    for (int32_t o = int32_t(kCtorsSize) - 4; o >= 0; o -= 4) {
        uint32_t f; std::memcpy(&f, g_mem + kCtors + uint32_t(o), 4);
        if (f == 0 || f == 0xffffffffu) continue;
        Thunk t = FindThunk(f);
        if (!t) { std::fprintf(stderr, "[psp] ctor %08x has no translation\n", f); continue; }
        t(CallRegs{});
        n++;
    }
    return n;
}

} // namespace psp
