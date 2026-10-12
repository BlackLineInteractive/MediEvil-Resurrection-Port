#pragma once
// Native implementations of the PSP calls the game's platform layer makes (src/Decomp/Platform.cpp).
// Only thin host mappings live here (files, memory, time, pad); heavy subsystems (GE graphics, threads,
// audio, video) are replaced at the level of the game's own functions instead of being emulated.
#include <cstdint>
#include <string>

namespace psp {

struct PlatformConfig {
    std::string discRoot = "extracted";   // disc0:/ and umd0:/
    std::string saveRoot = "save";        // ms0:/
    bool trace = false;                   // log every platform call
};
extern PlatformConfig g_platform;

// Pad state the next sceCtrlReadBuffer* returns: PSP button bits, analog axes 0..255 (128 = centre).
void SetPad(uint32_t buttons, uint8_t lx, uint8_t ly);

} // namespace psp
