#include "Core/Geometry.h"

#include "Core/Chunk.h"

#include <algorithm>
#include <cstring>
#include <map>

namespace med {
namespace {

// PSP GE vertex type (sceGuDrawArray vtype) layout: weights, texture, color, normal, position,
// each component aligned to its element size, stride aligned to the largest element.
struct VertexLayout {
    int tex = 0, col = 0, nrm = 0, pos = 0, wgt = 0, wgtCount = 0;
    int texOff = -1, colOff = -1, nrmOff = -1, posOff = -1;
    int stride = 0;

    explicit VertexLayout(uint32_t vt) {
        tex = vt & 3;
        col = vt >> 2 & 7;
        nrm = vt >> 5 & 3;
        pos = vt >> 7 & 3;
        wgt = vt >> 9 & 3;
        wgtCount = wgt ? int(vt >> 14 & 7) + 1 : 0;
        static const int sz[4] = {0, 1, 2, 4};
        int off = 0, maxAlign = 1;
        auto place = [&](int elem, int count) {
            off = (off + elem - 1) / elem * elem;
            int at = off;
            off += elem * count;
            maxAlign = std::max(maxAlign, elem);
            return at;
        };
        if (wgt) place(sz[wgt], wgtCount);
        if (tex) texOff = place(sz[tex], 2);
        if (col) colOff = col == 7 ? place(4, 1) : place(2, 1);
        if (nrm) nrmOff = place(sz[nrm], 3);
        if (pos) posOff = place(sz[pos], 3);
        stride = (off + maxAlign - 1) / maxAlign * maxAlign;
    }
};

float ReadComp(const uint8_t* p, int type, bool isSigned, float norm8, float norm16) {
    switch (type) {
        case 1: return isSigned ? int8_t(*p) / norm8 : *p / norm8;
        case 2: {
            uint16_t v;
            memcpy(&v, p, 2);
            return isSigned ? int16_t(v) / norm16 : v / norm16;
        }
        case 3: {
            float f;
            memcpy(&f, p, 4);
            return f;
        }
    }
    return 0;
}

uint32_t ReadColor(const uint8_t* p, int col) {
    if (col == 7) {
        uint32_t c;
        memcpy(&c, p, 4);
        return c;
    }
    uint16_t v;
    memcpy(&v, p, 2);
    uint32_t r, g, b, a;
    if (col == 4) { r = (v & 31) * 255 / 31; g = (v >> 5 & 63) * 255 / 63; b = (v >> 11) * 255 / 31; a = 255; }
    else if (col == 5) { r = (v & 31) * 255 / 31; g = (v >> 5 & 31) * 255 / 31; b = (v >> 10 & 31) * 255 / 31; a = v >> 15 ? 255 : 0; }
    else { r = (v & 15) * 17; g = (v >> 4 & 15) * 17; b = (v >> 8 & 15) * 17; a = (v >> 12) * 17; }
    return r | g << 8 | b << 16 | a << 24;
}

struct Fragment {
    std::vector<Vertex> verts;
    std::vector<uint16_t> strips;
};

bool ParseFragment(const Chunk& frg, Fragment& out) {
    uint32_t vtype = 0, stride = 0;
    float txf[4] = {1, 1, 0, 0};
    const Chunk* fvt = nullptr;
    Chunk fvtCopy;
    ForEachChunk(frg, [&](const Chunk& c) {
        if (c.Is("VFT0") || c.Is("VFT1")) { vtype = c.At<uint32_t>(0); stride = c.At<uint32_t>(4); }
        else if (c.Is("TXF0")) for (int i = 0; i < 4; ++i) txf[i] = c.At<float>(i * 4);
        else if (c.Is("FVT1")) { fvtCopy = c; fvt = &fvtCopy; }
        else if (c.Is("FSU0"))
            for (uint32_t o = 0; o + 2 <= c.size; o += 2) {
                uint16_t n = c.At<uint16_t>(o);
                if (!n) break;
                out.strips.push_back(n);
            }
    });
    if (!fvt || !vtype) return false;
    VertexLayout L(vtype);
    if (!stride) stride = L.stride;
    size_t count = 0;
    for (auto n : out.strips) count += n;
    // FVT1 starts with 'PAD' filler up to 16-byte alignment; the vertices are the tail.
    if (count * stride > fvt->size) return false;
    const uint8_t* base = fvt->data + (fvt->size - count * stride);

    out.verts.resize(count);
    for (size_t i = 0; i < count; ++i) {
        const uint8_t* v = base + i * stride;
        Vertex& o = out.verts[i];
        if (L.texOff >= 0) {
            float u = ReadComp(v + L.texOff, L.tex, false, 128.f, 32768.f);
            float w = ReadComp(v + L.texOff + (L.tex == 3 ? 4 : L.tex), L.tex, false, 128.f, 32768.f);
            o.uv = {u * txf[0] + txf[2], w * txf[1] + txf[3]};
        }
        if (L.colOff >= 0) o.color = ReadColor(v + L.colOff, L.col);
        if (L.nrmOff >= 0) {
            int e = L.nrm == 3 ? 4 : L.nrm;
            o.normal = {ReadComp(v + L.nrmOff, L.nrm, true, 127.f, 32767.f),
                        ReadComp(v + L.nrmOff + e, L.nrm, true, 127.f, 32767.f),
                        ReadComp(v + L.nrmOff + 2 * e, L.nrm, true, 127.f, 32767.f)};
        }
        if (L.posOff >= 0) {
            int e = L.pos == 3 ? 4 : L.pos;
            o.pos = {ReadComp(v + L.posOff, L.pos, true, 128.f, 32768.f),
                     ReadComp(v + L.posOff + e, L.pos, true, 128.f, 32768.f),
                     ReadComp(v + L.posOff + 2 * e, L.pos, true, 128.f, 32768.f)};
        }
    }
    return true;
}

void Append(MeshData& m, const Fragment& f, int texture) {
    uint32_t base = uint32_t(m.vertices.size());
    Batch b;
    b.firstIndex = uint32_t(m.indices.size());
    b.texture = texture;
    m.vertices.insert(m.vertices.end(), f.verts.begin(), f.verts.end());
    uint32_t s = 0;
    for (uint16_t n : f.strips) {
        for (uint32_t i = 0; i + 2 < n; ++i) {
            uint32_t a = base + s + i, c1 = a + 1, c2 = a + 2;
            if (i & 1) std::swap(c1, c2);
            m.indices.insert(m.indices.end(), {a, c1, c2});
        }
        s += n;
    }
    for (const auto& v : f.verts) {
        m.bmin = glm::min(m.bmin, v.pos);
        m.bmax = glm::max(m.bmax, v.pos);
    }
    b.indexCount = uint32_t(m.indices.size()) - b.firstIndex;
    if (b.indexCount) {
        // merge with the previous batch when the texture matches and it is contiguous
        if (!m.batches.empty() && m.batches.back().texture == texture &&
            m.batches.back().firstIndex + m.batches.back().indexCount == b.firstIndex)
            m.batches.back().indexCount += b.indexCount;
        else
            m.batches.push_back(b);
    }
    ++m.fragments;
}

void ParseTextureList(const Chunk& tex0, std::vector<std::string>& out) {
    // entries: u8 3, "T:name.psm", NUL
    size_t i = 0;
    while (i < tex0.size) {
        if (tex0.data[i] == 0) { ++i; continue; }
        if (tex0.data[i] < 32) ++i;
        size_t start = i;
        while (i < tex0.size && tex0.data[i]) ++i;
        std::string s(reinterpret_cast<const char*>(tex0.data + start), i - start);
        if (s.size() > 2 && s[1] == ':') s = s.substr(2);
        if (!s.empty()) out.push_back(s);
    }
}

// TXI1: one {u8 texture, u8 0xFF} per fragment reference, terminated by FF FF.
std::vector<int> ParseTxi(const Chunk& c) {
    std::vector<int> t;
    for (uint32_t o = 0; o + 2 <= c.size; o += 2) {
        if (c.data[o] == 0xFF && c.data[o + 1] == 0xFF) break;
        t.push_back(c.data[o]);
    }
    return t;
}

void WalkOctree(const Chunk& c, std::map<int, int>& fragTex) {
    ForEachChunk(c, [&](const Chunk& ch) {
        if (ch.Is("FMH0")) {
            std::vector<uint16_t> frr;
            std::vector<int> txi;
            ForEachChunk(ch, [&](const Chunk& l) {
                if (l.Is("FRR0"))
                    for (uint32_t o = 0; o + 2 <= l.size; o += 2) {
                        uint16_t v = l.At<uint16_t>(o);
                        if (v == 0xFFFF) break;
                        frr.push_back(v);
                    }
                else if (l.Is("TXI1")) txi = ParseTxi(l);
            });
            for (size_t i = 0; i < frr.size(); ++i)
                if (!(frr[i] & 0x8000))   // high bit: not a pool fragment (meaning still unknown)
                    fragTex.emplace(frr[i], i < txi.size() ? txi[i] : -1);
        } else if (ch.container) {
            WalkOctree(ch, fragTex);
        }
    });
}

} // namespace

bool LoadSubZone(const std::vector<uint8_t>& data, MeshData& out) {
    std::vector<Fragment> pool;
    std::map<int, int> fragTex;
    ForEachChunk(data.data(), data.size(), [&](const Chunk& top) {
        if (top.Is("TEX0")) ParseTextureList(top, out.textures);
        if (!top.Is("SBZ0")) return;
        ForEachChunk(top, [&](const Chunk& c) {
            if (c.Is("SZR0"))
                ForEachChunk(c, [&](const Chunk& f) {
                    if (!f.Is("FRG0")) return;
                    pool.emplace_back();
                    ParseFragment(f, pool.back());
                });
            else if (c.Is("OCT0"))
                WalkOctree(c, fragTex);
        });
    });
    if (pool.empty()) { out.error = "no SZR0 fragments"; return false; }
    for (size_t i = 0; i < pool.size(); ++i) {
        auto it = fragTex.find(int(i));
        Append(out, pool[i], it == fragTex.end() ? -1 : it->second);
    }
    return true;
}

bool LoadModel(const std::vector<uint8_t>& data, MeshData& out) {
    ForEachChunk(data.data(), data.size(), [&](const Chunk& top) {
        if (top.Is("TEX0")) ParseTextureList(top, out.textures);
        if (!top.Is("MOD0")) return;
        ForEachChunk(top, [&](const Chunk& fmh) {
            if (!fmh.Is("FMH0")) return;
            std::vector<Fragment> frags;
            std::vector<int> txi;
            ForEachChunk(fmh, [&](const Chunk& c) {
                if (c.Is("FRG0")) { frags.emplace_back(); ParseFragment(c, frags.back()); }
                else if (c.Is("TXI1")) txi = ParseTxi(c);
            });
            for (size_t i = 0; i < frags.size(); ++i) Append(out, frags[i], i < txi.size() ? txi[i] : -1);
        });
    });
    if (!out.fragments) { out.error = "no MOD0 fragments"; return false; }
    return true;
}

} // namespace med
