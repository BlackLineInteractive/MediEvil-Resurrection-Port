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
#include <algorithm>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>

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

// Files on the disc get a made-up sector number in their stat (SceIoStat.st_private[0]); the game then opens
// them raw as disc0:/sce_lbn0x<lbn>_size0x<size>, which maps back to the file.
std::map<uint32_t, std::string> g_lbnFiles;
uint32_t g_nextLbn = 0x1000;
uint32_t LbnFor(const std::string& host) {
    for (auto& [lbn, p] : g_lbnFiles) if (p == host) return lbn;
    g_lbnFiles[g_nextLbn] = host;
    return (g_nextLbn += 0x100000) - 0x100000;
}

int Open(const std::string& path, uint32_t flags) {
    unsigned lbn = 0, size = 0;
    if (std::sscanf(path.c_str(), "disc0:/sce_lbn0x%x_size0x%x", &lbn, &size) == 2 ||
        std::sscanf(path.c_str(), "umd0:/sce_lbn0x%x_size0x%x", &lbn, &size) == 2) {
        auto it = g_lbnFiles.find(lbn);
        if (it == g_lbnFiles.end()) { TRACE("open %s: unknown sector\n", path.c_str()); return int(0x80010002); }
        FILE* f = std::fopen(it->second.c_str(), "rb");
        TRACE("open %s -> %s\n", path.c_str(), it->second.c_str());
        if (!f) return int(0x80010002);
        g_files[g_nextFd] = f;
        return g_nextFd++;
    }
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
    long pos = std::ftell(f);
    uint32_t n = uint32_t(std::fread(g_mem + uint32_t(a1), 1, uint32_t(a2), f));
    TRACE("read fd %u @%ld %u -> %08x = %u\n", uint32_t(a0), pos, uint32_t(a2), uint32_t(a1), n);
    return n;
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
    // PSP EABI aligns 64-bit arguments to even register pairs ($a2:$a3).
    // a0: fd, a1: _pad, a2: offset_lo, a3: offset_hi, a4: whence ($t0)
    int64_t offset = (int64_t(int32_t(uint32_t(a3))) << 32) | uint32_t(a2);
    int whence = int(uint32_t(a4));
    std::fseek(f, long(offset), whence);
    TRACE("lseek fd %u %lld whence %d -> %ld\n", uint32_t(a0), (long long)offset, whence, std::ftell(f));
    return uint64_t(std::ftell(f));
}
PSPCALL(sceIoLseekAsync) {
    UNUSED;
    g_asyncResult[int(uint32_t(a0))] = int64_t(PSP_sceIoLseek(a0, a1, a2, a3, a4, 0, 0, 0));
    return 0;
}
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
    *GP<uint32_t>(uint32_t(a1) + 0x40) = LbnFor(HostPath(GuestString(a0)));   // st_private[0]: sector
    TRACE("getstat %s size %lld\n", GuestString(a0).c_str(), (long long)size);
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
    if (uint32_t(a0)) { GP<uint32_t> tv{uint32_t(a0)}; tv[0] = uint32_t(us / 1000000); tv[1] = uint32_t(us % 1000000); }
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

// ── threads: the game's own thread bodies run as host threads, one at a time (a baton), switching only
// in blocking calls - the single-core PSP's behaviour. Lower priority number runs first. ──
} // namespace psp::game

namespace psp {
namespace {

struct GThread {
    int id = 0;
    std::string name;
    uint32_t entry = 0, prio = 0x20, stackTop = 0;
    std::function<bool()> ready;          // the condition a blocked thread waits for
    uint64_t wakeAt = 0;                  // delay / vblank deadline (micros), 0 = none
    bool started = false, done = false;
    int wakeups = 0;
    std::condition_variable cv;
};

std::mutex g_sched;
std::map<int, std::unique_ptr<GThread>> g_threads;
int g_current = 1;
thread_local int t_self = 1;
struct SubIntr { uint32_t handler, arg; bool enabled; };
std::map<int, SubIntr> g_vblankHandlers;
uint64_t g_nextVblank = 0;
constexpr uint64_t kVblankUs = 16683;

thread_local bool t_inInterrupt = false;

void RunVblankHandlers(std::unique_lock<std::mutex>& lk) {
    uint64_t now = Micros();
    if (now < g_nextVblank) return;
    g_nextVblank = now + kVblankUs;
    auto handlers = g_vblankHandlers;
    lk.unlock();   // the running thread still holds the baton; handlers may call platform functions
    t_inInterrupt = true;
    for (auto& [no, h] : handlers)
        if (h.enabled && h.handler)
            if (Thunk t = FindThunk(h.handler)) { CallRegs r; r.i[0] = uint32_t(no); r.i[1] = h.arg; r.ni = 2; t(r); }
    t_inInterrupt = false;
    lk.lock();
}

// Called with the lock held by the running thread: hand the baton to the best runnable thread, wait for it back.
void Reschedule(std::unique_lock<std::mutex>& lk) {
    GThread* self = g_threads[t_self].get();
    for (;;) {
        RunVblankHandlers(lk);
        uint64_t now = Micros();
        GThread* best = nullptr;
        uint64_t earliest = UINT64_MAX;
        for (auto& [id, t] : g_threads) {
            if (!t->started || t->done) continue;
            bool timeUp = t->wakeAt && now >= t->wakeAt;
            bool ok = (!t->ready || t->ready()) && (!t->wakeAt || timeUp);
            if (t->ready && t->wakeAt && timeUp) ok = true;   // wait with timeout expired
            if (ok && (!best || t->prio < best->prio)) best = t.get();
            if (!ok && t->wakeAt) earliest = std::min(earliest, t->wakeAt);
        }
        if (best) {
            best->ready = nullptr;
            best->wakeAt = 0;
            g_current = best->id;
            if (best != self) {
                best->cv.notify_one();
                if (!self->done) self->cv.wait(lk, [&] { return g_current == self->id; });
            }
            return;
        }
        // nobody can run: sleep until the next deadline or vblank
        uint64_t until = std::min(earliest, g_nextVblank);
        if (until == UINT64_MAX) Fatal("all guest threads blocked forever");
        lk.unlock();
        if (until > now) std::this_thread::sleep_for(std::chrono::microseconds(until - now));
        lk.lock();
    }
}

void Block(std::function<bool()> cond, uint64_t wakeAt = 0) {
    if (t_inInterrupt) return;   // interrupt handlers never block
    std::unique_lock<std::mutex> lk(g_sched);
    GThread* self = g_threads[t_self].get();
    if (cond && cond() && !wakeAt) { if (self) self->ready = nullptr; return; }
    self->ready = std::move(cond);
    self->wakeAt = wakeAt;
    Reschedule(lk);
}

void ThreadMain(GThread* t, uint32_t argLen, uint32_t argp) {
    t_self = t->id;
    {
        std::unique_lock<std::mutex> lk(g_sched);
        t->cv.wait(lk, [&] { return g_current == t->id; });
    }
    g_sp = t->stackTop;
    if (Thunk th = FindThunk(t->entry)) {
        CallRegs r; r.i[0] = argLen; r.i[1] = argp; r.ni = 2;
        th(r);
    } else Unimplemented(t->entry, t->name.c_str());
    std::unique_lock<std::mutex> lk(g_sched);
    t->done = true;
    Reschedule(lk);
}

void EnsureMainThread() {
    if (g_threads.count(1)) return;
    auto t = std::make_unique<GThread>();
    t->id = 1; t->name = "main"; t->prio = 0x20; t->started = true;
    g_threads[1] = std::move(t);
}

} // namespace

void GuestYield() { EnsureMainThread(); Block(nullptr, Micros() + 1); }

} // namespace psp

namespace psp::game {

PSPCALL(sceKernelCreateThread) {
    UNUSED;
    EnsureMainThread();
    auto t = std::make_unique<GThread>();
    t->id = g_nextUid++;
    t->name = GuestString(a0);
    t->entry = a1;
    t->prio = a2;
    uint32_t stack = std::max<uint32_t>(uint32_t(a3), 0x4000);
    g_heapHigh = (g_heapHigh - stack) & ~15u;
    t->stackTop = g_heapHigh + stack - 64;
    TRACE("thread '%s' entry %08x prio %u stack %x\n", t->name.c_str(), t->entry, t->prio, stack);
    int id = t->id;
    g_threads[id] = std::move(t);
    return uint32_t(id);
}
PSPCALL(sceKernelStartThread) {
    UNUSED;
    EnsureMainThread();
    GThread* t = g_threads.count(int(uint32_t(a0))) ? g_threads[int(uint32_t(a0))].get() : nullptr;
    if (!t) return uint32_t(0x80020198);
    // arguments are copied onto the new thread's stack
    uint32_t argp = 0;
    if (uint32_t(a1) && uint32_t(a2)) {
        t->stackTop = (t->stackTop - uint32_t(a1)) & ~15u;
        std::memcpy(g_mem + t->stackTop, g_mem + uint32_t(a2), uint32_t(a1));
        argp = t->stackTop;
        t->stackTop -= 64;
    }
    t->started = true;
    std::thread(ThreadMain, t, uint32_t(a1), argp).detach();
    GuestYield();   // let a higher priority thread start right away
    return 0;
}
PSPCALL(sceKernelExitThread) { UNUSED; std::unique_lock<std::mutex> lk(g_sched); g_threads[t_self]->done = true; Reschedule(lk); for (;;) std::this_thread::sleep_for(std::chrono::hours(1)); }
PSPCALL(sceKernelExitDeleteThread) { return PSP_sceKernelExitThread(a0, a1, a2, a3, a4, a5, a6, a7); }
PSPCALL(sceKernelChangeThreadPriority) {
    UNUSED;
    int id = uint32_t(a0) ? int(uint32_t(a0)) : t_self;
    EnsureMainThread();
    if (g_threads.count(id)) g_threads[id]->prio = a1;
    return 0;
}
PSPCALL(sceKernelGetThreadId) { UNUSED; return uint32_t(t_self); }
PSPCALL(sceKernelDelayThread) { UNUSED; EnsureMainThread(); Block(nullptr, Micros() + std::max<uint32_t>(uint32_t(a0), 1)); return 0; }
PSPCALL(sceKernelDelayThreadCB) { return PSP_sceKernelDelayThread(a0, a1, a2, a3, a4, a5, a6, a7); }
PSPCALL(sceKernelSleepThread) {
    UNUSED; EnsureMainThread();
    GThread* self = g_threads[t_self].get();
    Block([self] { if (self->wakeups > 0) { self->wakeups--; return true; } return false; });
    return 0;
}
PSPCALL(sceKernelWakeupThread) { UNUSED; if (g_threads.count(int(uint32_t(a0)))) g_threads[int(uint32_t(a0))]->wakeups++; return 0; }
PSPCALL(sceKernelWaitThreadEnd) {
    UNUSED; EnsureMainThread();
    int id = int(uint32_t(a0));
    Block([id] { return !g_threads.count(id) || g_threads[id]->done; });
    return 0;
}
PSPCALL(sceKernelSuspendDispatchThread) { UNUSED; return 1; }
PSPCALL(sceKernelResumeDispatchThread) { UNUSED; return 0; }

// ── semaphores and event flags ──
PSPCALL(sceKernelCreateSema) { UNUSED; int id = g_nextUid++; g_semas[id] = a2; return uint32_t(id); }
PSPCALL(sceKernelDeleteSema) { UNUSED; return 0; }
PSPCALL(sceKernelSignalSema) { UNUSED; g_semas[int(uint32_t(a0))] += uint32_t(a1); GuestYield(); return 0; }
PSPCALL(sceKernelWaitSema) {
    UNUSED; EnsureMainThread();
    int id = int(uint32_t(a0)); uint32_t n = std::max<uint32_t>(uint32_t(a1), 1);
    Block([id, n] { uint32_t& v = g_semas[id]; if (v >= n) { v -= n; return true; } return false; });
    return 0;
}
PSPCALL(sceKernelPollSema) { UNUSED; uint32_t& v = g_semas[int(uint32_t(a0))]; if (v < uint32_t(a1)) return uint32_t(0x800201a9); v -= uint32_t(a1); return 0; }
PSPCALL(sceKernelCreateEventFlag) { UNUSED; int id = g_nextUid++; g_flags[id] = a2; return uint32_t(id); }
PSPCALL(sceKernelDeleteEventFlag) { UNUSED; return 0; }
PSPCALL(sceKernelSetEventFlag) { UNUSED; g_flags[int(uint32_t(a0))] |= uint32_t(a1); return 0; }
PSPCALL(sceKernelClearEventFlag) { UNUSED; g_flags[int(uint32_t(a0))] &= uint32_t(a1); return 0; }
static bool FlagTest(int id, uint32_t bits, uint32_t mode, uint32_t out) {
    uint32_t& v = g_flags[id];
    bool ok = (mode & 1) ? (v & bits) != 0 : (v & bits) == bits;
    if (ok) {
        if (out) *GP<uint32_t>(out) = v;
        if (mode & 0x20) v &= ~bits;
        if (mode & 0x10) v = 0;
    }
    return ok;
}
PSPCALL(sceKernelWaitEventFlag) {
    UNUSED; EnsureMainThread();
    int id = int(uint32_t(a0)); uint32_t bits = a1, mode = a2, out = a3;
    Block([=] { return FlagTest(id, bits, mode, out); });
    return 0;
}
PSPCALL(sceKernelPollEventFlag) { UNUSED; return FlagTest(int(uint32_t(a0)), a1, a2, a3) ? 0 : uint32_t(0x800201af); }

// ── interrupts: the vblank sub-interrupt handlers run at 60 Hz from the scheduler ──
PSPCALL(sceKernelRegisterSubIntrHandler) {
    UNUSED;
    if (uint32_t(a0) == 30) g_vblankHandlers[int(uint32_t(a1))] = {a2, a3, false};   // PSP_VBLANK_INT
    return 0;
}
PSPCALL(sceKernelEnableSubIntr) { UNUSED; if (uint32_t(a0) == 30) g_vblankHandlers[int(uint32_t(a1))].enabled = true; return 0; }
PSPCALL(sceKernelReleaseSubIntrHandler) { UNUSED; if (uint32_t(a0) == 30) g_vblankHandlers.erase(int(uint32_t(a1))); return 0; }

// ── display: no PSP framebuffer; frames are drawn by the port's renderer ──
PSPCALL(sceDisplaySetMode) { UNUSED; return 0; }
PSPCALL(sceDisplaySetFrameBuf) { UNUSED; return 0; }
PSPCALL(sceDisplayWaitVblankStart) {
    UNUSED; EnsureMainThread();
    uint64_t next = g_nextVblank ? g_nextVblank : Micros() + kVblankUs;
    Block(nullptr, next);
    return 0;
}

} // namespace psp::game
