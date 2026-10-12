// Native PSP calls for the translated game. Arguments arrive positionally as the C prototype lists them
// (docs/names_functions/psp_prototypes.tsv); 64-bit arguments arrive truncated to their low word.
#include "Decomp/Platform.h"
#include "Decomp/Memory.h"
#include "Decomp/Runtime.h"
#include "decls.h"
#include "imports.h"

#include <chrono>
#include <cstdio>
#include <ctime>
#include <map>
#include <string>
#include <thread>

namespace psp {

PlatformConfig g_platform;

namespace {

uint32_t g_padButtons = 0;
uint8_t g_padX = 128, g_padY = 128;

using Clock = std::chrono::steady_clock;
const Clock::time_point g_start = Clock::now();
uint64_t Micros() { return uint64_t(std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - g_start).count()); }

std::string GuestString(uint32_t a) {
    std::string s;
    if (!a) return s;
    for (const char* p = reinterpret_cast<const char*>(g_mem + a); *p; p++) s += *p;
    return s;
}

#define TRACE(...) do { if (g_platform.trace) std::fprintf(stderr, "[psp] " __VA_ARGS__); } while (0)

// ── memory: kernel partitions as a bump allocator between the heap base and the stacks ──
constexpr uint32_t kHeapEnd = 0x03000000;
uint32_t g_heapLow = kHeapBase, g_heapHigh = kHeapEnd;
std::map<int, uint32_t> g_blocks;
int g_nextBlock = 1;

// ── files ──
std::map<int, FILE*> g_files;
int g_nextFd = 3;
std::map<int, int64_t> g_asyncResult;

std::string HostPath(const std::string& p) {
    auto colon = p.find(':');
    std::string dev = colon == std::string::npos ? "" : p.substr(0, colon);
    std::string rest = colon == std::string::npos ? p : p.substr(colon + 1);
    while (!rest.empty() && rest[0] == '/') rest.erase(0, 1);
    if (dev == "ms0") return g_platform.saveRoot + "/" + rest;
    return g_platform.discRoot + "/" + rest;   // disc0:, umd0:, umd1:, host0:, relative
}

int Open(const std::string& path, uint32_t flags) {
    const char* mode = (flags & 3) == 1 ? "rb" : (flags & 0x400) ? "w+b" : (flags & 0x200) ? "a+b" : "r+b";
    FILE* f = std::fopen(HostPath(path).c_str(), mode);
    TRACE("open %s -> %s\n", path.c_str(), f ? "ok" : "FAILED");
    if (!f) return int(0x80010002);   // SCE_ERROR_ERRNO_ENOENT
    g_files[g_nextFd] = f;
    return g_nextFd++;
}
FILE* Fd(uint32_t fd) { auto it = g_files.find(int(fd)); return it == g_files.end() ? nullptr : it->second; }

std::map<int, uint32_t> g_semas, g_flags;
int g_nextUid = 0x100;

} // namespace

void SetPad(uint32_t buttons, uint8_t lx, uint8_t ly) { g_padButtons = buttons; g_padX = lx; g_padY = ly; }

} // namespace psp

namespace psp::game {
using A = U4;
#define PSPCALL(name) uint64_t PSP_##name(A a0, A a1, A a2, A a3, A a4, A a5, A a6, A a7)
#define UNUSED (void)a0; (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6; (void)a7

// ── memory ──
PSPCALL(sceKernelAllocPartitionMemory) {
    UNUSED;
    uint32_t size = (uint32_t(a3) + 255) & ~255u;
    uint32_t addr;
    if (uint32_t(a2) == 1) { g_heapHigh -= size; addr = g_heapHigh; }
    else if (uint32_t(a2) == 2) addr = uint32_t(a4);
    else { addr = g_heapLow; g_heapLow += size; }
    if (g_heapLow > g_heapHigh) Fatal("guest heap exhausted (%u bytes for %s)", size, GuestString(a1).c_str());
    int id = g_nextBlock++;
    g_blocks[id] = addr;
    TRACE("alloc %s %u -> %08x\n", GuestString(a1).c_str(), size, addr);
    return uint32_t(id);
}
PSPCALL(sceKernelGetBlockHeadAddr) { UNUSED; auto it = g_blocks.find(int(uint32_t(a0))); return it == g_blocks.end() ? 0 : it->second; }
PSPCALL(sceKernelFreePartitionMemory) { UNUSED; g_blocks.erase(int(uint32_t(a0))); return 0; }
PSPCALL(sceKernelMaxFreeMemSize) { UNUSED; return g_heapHigh - g_heapLow; }
PSPCALL(sceKernelTotalFreeMemSize) { UNUSED; return g_heapHigh - g_heapLow; }

// ── files ──
PSPCALL(sceIoOpen) { UNUSED; return uint32_t(Open(GuestString(a0), a1)); }
PSPCALL(sceIoOpenAsync) { UNUSED; int fd = Open(GuestString(a0), a1); if (fd > 0) g_asyncResult[fd] = fd; return uint32_t(fd); }
PSPCALL(sceIoClose) { UNUSED; if (FILE* f = Fd(a0)) { std::fclose(f); g_files.erase(int(uint32_t(a0))); return 0; } return uint32_t(-1); }
PSPCALL(sceIoCloseAsync) { UNUSED; uint64_t r = PSP_sceIoClose(a0, 0, 0, 0, 0, 0, 0, 0); g_asyncResult[int(uint32_t(a0))] = 0; return r; }
PSPCALL(sceIoRead) {
    UNUSED;
    FILE* f = Fd(a0);
    if (!f) return uint32_t(-1);
    return uint32_t(std::fread(g_mem + uint32_t(a1), 1, uint32_t(a2), f));
}
PSPCALL(sceIoReadAsync) { UNUSED; g_asyncResult[int(uint32_t(a0))] = int32_t(PSP_sceIoRead(a0, a1, a2, 0, 0, 0, 0, 0)); return 0; }
PSPCALL(sceIoWrite) {
    UNUSED;
    uint32_t fd = a0;
    if (fd == 1 || fd == 2) { std::fwrite(g_mem + uint32_t(a1), 1, uint32_t(a2), stderr); return a2; }
    FILE* f = Fd(fd);
    return f ? uint32_t(std::fwrite(g_mem + uint32_t(a1), 1, uint32_t(a2), f)) : uint32_t(-1);
}
PSPCALL(sceIoLseek) {
    UNUSED;
    FILE* f = Fd(a0);
    if (!f) return uint64_t(-1);
    // EABI: fd in a0, the 64-bit offset in the next register (positional), whence after it
    std::fseek(f, long(int32_t(uint32_t(a1))), int(uint32_t(a2)));
    return uint64_t(std::ftell(f));
}
PSPCALL(sceIoLseekAsync) { UNUSED; g_asyncResult[int(uint32_t(a0))] = int64_t(PSP_sceIoLseek(a0, a1, a2, 0, 0, 0, 0, 0)); return 0; }
PSPCALL(sceIoWaitAsync) {
    UNUSED;
    int64_t r = g_asyncResult[int(uint32_t(a0))];
    if (uint32_t(a1)) std::memcpy(g_mem + uint32_t(a1), &r, 8);
    return 0;
}
PSPCALL(sceIoPollAsync) { return PSP_sceIoWaitAsync(a0, a1, a2, a3, a4, a5, a6, a7); }
PSPCALL(sceIoGetstat) {
    UNUSED;
    FILE* f = std::fopen(HostPath(GuestString(a0)).c_str(), "rb");
    if (!f) return uint32_t(0x80010002);
    std::fseek(f, 0, SEEK_END);
    int64_t size = std::ftell(f);
    std::fclose(f);
    // SceIoStat: int mode, uint attr, SceOff size (offset 8), times...
    std::memset(g_mem + uint32_t(a1), 0, 0x58);
    *GP<uint32_t>(uint32_t(a1)) = 0x21ff;   // FIO_S_IFREG | rwx
    std::memcpy(g_mem + uint32_t(a1) + 8, &size, 8);
    return 0;
}
PSPCALL(sceIoChangeAsyncPriority) { UNUSED; return 0; }
PSPCALL(sceIoDopen) { UNUSED; TRACE("dopen %s (not supported)\n", GuestString(a0).c_str()); return uint32_t(0x80010002); }
PSPCALL(sceIoDread) { UNUSED; return 0; }
PSPCALL(sceIoDclose) { UNUSED; return 0; }

// ── time ──
PSPCALL(sceKernelGetSystemTimeWide) { UNUSED; return Micros(); }
PSPCALL(sceKernelGetSystemTime) { UNUSED; uint64_t t = Micros(); std::memcpy(g_mem + uint32_t(a0), &t, 8); return 0; }
PSPCALL(sceKernelLibcClock) { UNUSED; return uint32_t(Micros()); }
PSPCALL(sceKernelLibcTime) { UNUSED; uint32_t t = uint32_t(std::time(nullptr)); if (uint32_t(a0)) *GP<uint32_t>(uint32_t(a0)) = t; return t; }
PSPCALL(sceKernelLibcGettimeofday) {
    UNUSED;
    uint64_t us = uint64_t(std::time(nullptr)) * 1000000 + Micros() % 1000000;
    if (uint32_t(a0)) { GP<uint32_t> tv(uint32_t(a0)); tv[0] = uint32_t(us / 1000000); tv[1] = uint32_t(us % 1000000); }
    return 0;
}
PSPCALL(sceRtcGetCurrentTick) { UNUSED; uint64_t t = 62135596800000000ull + uint64_t(std::time(nullptr)) * 1000000; std::memcpy(g_mem + uint32_t(a0), &t, 8); return 0; }
PSPCALL(sceKernelPowerTick) { UNUSED; return 0; }

// ── pad ──
PSPCALL(sceCtrlSetSamplingCycle) { UNUSED; return 0; }
PSPCALL(sceCtrlSetSamplingMode) { UNUSED; return 0; }
PSPCALL(sceCtrlReadBufferPositive) {
    UNUSED;
    // SceCtrlData: u32 TimeStamp, u32 Buttons, u8 Lx, u8 Ly, u8 Rsrv[6]
    uint32_t p = a0;
    for (uint32_t i = 0; i < uint32_t(a1); i++, p += 16) {
        std::memset(g_mem + p, 0, 16);
        *GP<uint32_t>(p) = uint32_t(Micros());
        *GP<uint32_t>(p + 4) = g_padButtons;
        g_mem[p + 8] = g_padX;
        g_mem[p + 9] = g_padY;
    }
    return a1;
}

// ── disc / power / misc kernel ──
PSPCALL(sceUmdCheckMedium) { UNUSED; return 1; }
PSPCALL(sceUmdActivate) { UNUSED; return 0; }
PSPCALL(sceUmdDeactivate) { UNUSED; return 0; }
PSPCALL(sceUmdWaitDriveStat) { UNUSED; return 0; }
PSPCALL(sceUmdGetDriveStat) { UNUSED; return 0x32; }   // PRESENT | INITED | READABLE
PSPCALL(scePowerRegisterCallback) { UNUSED; return 0; }
PSPCALL(sceKernelRegisterExitCallback) { UNUSED; return 0; }
PSPCALL(sceKernelCreateCallback) { UNUSED; return uint32_t(g_nextUid++); }
PSPCALL(sceKernelCpuSuspendIntr) { UNUSED; return 0; }
PSPCALL(sceKernelCpuResumeIntr) { UNUSED; return 0; }
PSPCALL(sceKernelIsCpuIntrEnable) { UNUSED; return 1; }
PSPCALL(sceKernelSuspendDispatchThread) { UNUSED; return 1; }
PSPCALL(sceKernelResumeDispatchThread) { UNUSED; return 0; }
PSPCALL(sceKernelStdout) { UNUSED; return 1; }
PSPCALL(sceKernelStderr) { UNUSED; return 2; }
PSPCALL(sceKernelStdin) { UNUSED; return 0; }
PSPCALL(sceUtilityGetSystemParamInt) {
    UNUSED;
    // 8 = language (1 English), 9 = confirm button (1 = cross), others 0
    uint32_t v = uint32_t(a0) == 8 ? 1 : uint32_t(a0) == 9 ? 1 : 0;
    if (uint32_t(a1)) *GP<uint32_t>(uint32_t(a1)) = v;
    return 0;
}

// ── synchronisation: one guest thread runs, so waits never block ──
PSPCALL(sceKernelCreateSema) { UNUSED; int id = g_nextUid++; g_semas[id] = a2; return uint32_t(id); }
PSPCALL(sceKernelDeleteSema) { UNUSED; return 0; }
PSPCALL(sceKernelSignalSema) { UNUSED; g_semas[int(uint32_t(a0))] += uint32_t(a1); return 0; }
PSPCALL(sceKernelWaitSema) { UNUSED; uint32_t& v = g_semas[int(uint32_t(a0))]; if (v >= uint32_t(a1)) v -= uint32_t(a1); return 0; }
PSPCALL(sceKernelPollSema) { UNUSED; uint32_t& v = g_semas[int(uint32_t(a0))]; if (v < uint32_t(a1)) return uint32_t(0x800201a9); v -= uint32_t(a1); return 0; }
PSPCALL(sceKernelCreateEventFlag) { UNUSED; int id = g_nextUid++; g_flags[id] = a2; return uint32_t(id); }
PSPCALL(sceKernelDeleteEventFlag) { UNUSED; return 0; }
PSPCALL(sceKernelSetEventFlag) { UNUSED; g_flags[int(uint32_t(a0))] |= uint32_t(a1); return 0; }
PSPCALL(sceKernelClearEventFlag) { UNUSED; g_flags[int(uint32_t(a0))] &= uint32_t(a1); return 0; }
static uint64_t FlagWait(A id, A bits, A mode, A out) {
    uint32_t& v = g_flags[int(uint32_t(id))];
    if (uint32_t(out)) *GP<uint32_t>(uint32_t(out)) = v;
    bool ok = (uint32_t(mode) & 1) ? (v & uint32_t(bits)) != 0 : (v & uint32_t(bits)) == uint32_t(bits);
    if (ok && (uint32_t(mode) & 0x20)) v &= ~uint32_t(bits);
    if (ok && (uint32_t(mode) & 0x10)) v = 0;
    return ok ? 0 : uint32_t(0x800201af);
}
PSPCALL(sceKernelWaitEventFlag) { UNUSED; return FlagWait(a0, a1, a2, a3); }
PSPCALL(sceKernelPollEventFlag) { UNUSED; return FlagWait(a0, a1, a2, a3); }

// ── threads: not run yet; the game's thread bodies get replaced or called where needed ──
PSPCALL(sceKernelCreateThread) {
    UNUSED;
    int id = g_nextUid++;
    std::fprintf(stderr, "[psp] thread '%s' entry %08x (not started)\n", GuestString(a0).c_str(), uint32_t(a1));
    return uint32_t(id);
}
PSPCALL(sceKernelStartThread) { UNUSED; return 0; }
PSPCALL(sceKernelChangeThreadPriority) { UNUSED; return 0; }
PSPCALL(sceKernelGetThreadId) { UNUSED; return 1; }
PSPCALL(sceKernelDelayThread) { UNUSED; return 0; }
PSPCALL(sceKernelDelayThreadCB) { UNUSED; return 0; }

} // namespace psp::game
