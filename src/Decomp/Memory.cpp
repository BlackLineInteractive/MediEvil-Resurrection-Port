// Guest memory: main.prx loaded at address 0 (its Ghidra image base), stack below the string pool.
#include "Decomp/Memory.h"
#include "Decomp/Runtime.h"

#include <cstdio>
#include <cstdlib>
#include <csignal>
#include <sys/mman.h>
#include <unistd.h>
#include <vector>
#include "Decomp/Caller.h"

namespace psp {

static void OnCrash(int sig) {
    char buf[64];
    int n = snprintf(buf, sizeof buf, "\n[psp] signal %d, last guest calls (newest last):\n", sig);
    write(2, buf, size_t(n));
    for (uint32_t i = 32; i > 0; i--) {
        n = snprintf(buf, sizeof buf, "  %08x\n", g_trace[(g_tracePos - i) & 255]);
        write(2, buf, size_t(n));
    }
    _exit(128 + sig);
}

void InstallCrashHandler() {
    std::signal(SIGSEGV, OnCrash);
    std::signal(SIGBUS, OnCrash);
    std::signal(SIGILL, OnCrash);
    std::signal(SIGFPE, OnCrash);
    std::signal(SIGABRT, OnCrash);
}

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
    if (!g_mem) {
        // reserve the whole 32-bit space so a wild guest pointer reads zeros instead of crashing the host
        void* m = mmap(nullptr, size_t(1) << 32, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON | MAP_NORESERVE, -1, 0);
        if (m == MAP_FAILED) return false;
        g_mem = static_cast<uint8_t*>(m);
        InstallCrashHandler();
    }
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
