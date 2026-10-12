#pragma once
// PSM0 texture decode (base mip level only).
#include <cstdint>
#include <vector>

namespace med {

struct Image {
    int width = 0, height = 0;
    std::vector<uint8_t> rgba;   // width*height*4, top row first
    bool translucent = false;    // has alpha values other than ~0 / ~255
};

// FLIP flipbook: 'FLIP', u32 frameCount, u32 fps, u32 frameStride, then frameCount PSM0 images.
struct Flipbook {
    std::vector<Image> frames;
    float fps = 15;
};
bool DecodeFLIP(const uint8_t* data, size_t size, Flipbook& out);

// Header: 'PSM0', u16 mipFlag, u8 GE pixel format (1 = 5551, 4 = CLUT4, 5 = CLUT8), u8 flags (0x80 = swizzled),
// u16 w, u16 h, u32 ?. CLUT formats add a 16-byte palette header and an RGBA8888 palette at 0x20.
bool DecodePSM(const uint8_t* data, size_t size, Image& out);

} // namespace med
