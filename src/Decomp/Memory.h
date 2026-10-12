#pragma once
#include <cstdint>

namespace psp {

// Guest address map: [0, ~0x440000) main.prx image + bss, heap above it, stack below kStackTop,
// interned strings in the last megabyte.
constexpr uint32_t kHeapBase = 0x00800000;
constexpr uint32_t kStackTop = 0x03E00000;

bool Boot(const char* prxPath);   // load main.prx into guest memory
int RunStaticCtors();
void InstallCrashHandler();   // prints the last guest calls on a host crash             // the game's C++ static constructors (.ctors)

} // namespace psp
