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

// Blend only textures that are mostly semi-transparent (light beams: >80% of texels). Character atlases
// and foliage have soft edges (10-15% semi texels) and are cut-outs: drawn alpha-tested with depth
// writes, otherwise their own parts sort wrongly (arms over the body, sleeves showing through).
static void ClassifyAlpha(Image& img) {
    size_t semi = 0, n = img.rgba.size() / 4;
    for (size_t i = 3; i < img.rgba.size(); i += 4) semi += img.rgba[i] > 8 && img.rgba[i] < 247;
    img.translucent = n && semi * 2 > n;
}

static bool DecodePSMRaw(const uint8_t* d, size_t size, Image& out);

bool DecodePSM(const uint8_t* d, size_t size, Image& out) {
    if (!DecodePSMRaw(d, size, out)) return false;
    // byte 0x0C == 1 -> texture object flag 0x400 (FUN_002391d4) -> TFUNC TCC = RGBA (FUN_00239f38).
    // Otherwise the GE ignores the texture alpha and the fragment alpha is the vertex alpha only.
    if (d[0x0C] != 1)
        for (size_t i = 3; i < out.rgba.size(); i += 4) out.rgba[i] = 255;
    ClassifyAlpha(out);
    return true;
}

bool DecodeFLIP(const uint8_t* d, size_t size, Flipbook& out) {
    if (size < 16 || memcmp(d, "FLIP", 4) != 0) return false;
    uint32_t count, fps, stride;
    memcpy(&count, d + 4, 4);
    memcpy(&fps, d + 8, 4);
    memcpy(&stride, d + 12, 4);
    out.fps = fps ? float(fps) : 15.f;
    for (uint32_t i = 0; i < count && 16 + size_t(i + 1) * stride <= size; ++i) {
        Image img;
        if (!DecodePSM(d + 16 + size_t(i) * stride, stride, img)) break;
        out.frames.push_back(std::move(img));
    }
    return !out.frames.empty();
}

static bool DecodePSMRaw(const uint8_t* d, size_t size, Image& out) {
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
