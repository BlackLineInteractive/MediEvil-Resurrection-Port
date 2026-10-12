#include "Core/Texture.h"

#include <cstring>

namespace med {

// PSP GE swizzle: the image is stored as 16-byte x 8-row blocks, blocks in row-major order.
static std::vector<uint8_t> Unswizzle(const uint8_t* src, int widthBytes, int height) {
    std::vector<uint8_t> out(size_t(widthBytes) * height);
    int blocksX = widthBytes / 16;
    for (int by = 0; by < height; by += 8)
        for (int bx = 0; bx < blocksX; ++bx)
            for (int r = 0; r < 8 && by + r < height; ++r, src += 16)
                memcpy(&out[size_t(by + r) * widthBytes + bx * 16], src, 16);
    return out;
}

bool DecodePSM(const uint8_t* d, size_t size, Image& out) {
    if (size < 16 || memcmp(d, "PSM0", 4) != 0) return false;
    int fmt = d[6], flags = d[7];
    int w = d[8] | d[9] << 8, h = d[10] | d[11] << 8;
    bool swizzled = flags & 0x80;
    out.width = w;
    out.height = h;
    out.rgba.assign(size_t(w) * h * 4, 0);

    if (fmt == 4 || fmt == 5) {
        int ncol = fmt == 4 ? 16 : 256, bpp = fmt == 4 ? 4 : 8;
        size_t palOff = 0x20, pxOff = palOff + ncol * 4;
        int wb = w * bpp / 8;
        if (pxOff + size_t(wb) * h > size) return false;
        std::vector<uint8_t> px(d + pxOff, d + pxOff + size_t(wb) * h);
        if (swizzled && wb >= 16) px = Unswizzle(px.data(), wb, h);
        const uint8_t* pal = d + palOff;
        uint8_t* o = out.rgba.data();
        for (size_t i = 0; i < px.size(); ++i) {
            if (fmt == 4) {
                memcpy(o, pal + (px[i] & 15) * 4, 4); o += 4;
                memcpy(o, pal + (px[i] >> 4) * 4, 4); o += 4;
            } else {
                memcpy(o, pal + px[i] * 4, 4); o += 4;
            }
        }
        // PSP alpha is 0..0x80 = opaque in many Sony titles; keep raw values, the renderer alpha-tests low values.
        return true;
    }
    if (fmt == 1) {
        int wb = w * 2;
        if (16 + size_t(wb) * h > size) return false;
        std::vector<uint8_t> px(d + 16, d + 16 + size_t(wb) * h);
        if (swizzled) px = Unswizzle(px.data(), wb, h);
        uint8_t* o = out.rgba.data();
        for (size_t i = 0; i + 1 < px.size(); i += 2) {
            uint16_t v = uint16_t(px[i] | px[i + 1] << 8);
            *o++ = uint8_t((v & 31) * 255 / 31);
            *o++ = uint8_t((v >> 5 & 31) * 255 / 31);
            *o++ = uint8_t((v >> 10 & 31) * 255 / 31);
            *o++ = v & 0x8000 ? 255 : 0;
        }
        return true;
    }
    return false;
}

} // namespace med
