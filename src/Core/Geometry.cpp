#include "Core/Geometry.h"

#include "Core/Chunk.h"

#include <algorithm>
#include <cstring>

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

// TXI1: one {u8 texture, u8 0xFF} per fragment reference, terminated by FF FF.
std::vector<int> ParseTxi(const Chunk& c) {
    std::vector<int> t;
    for (uint32_t o = 0; o + 2 <= c.size; o += 2) {
        if (c.data[o] == 0xFF && c.data[o + 1] == 0xFF) break;
        t.push_back(c.data[o]);
    }
    return t;
}

} // namespace

bool ParseFragment(const Chunk& frg, Fragment& out) {
    uint32_t vtype = 0, stride = 0;
    float txf[4] = {1, 1, 0, 0};
    Chunk fvt;
    bool haveFvt = false;
    ForEachChunk(frg, [&](const Chunk& c) {
        if (c.Is("VFT0") || c.Is("VFT1")) { vtype = c.At<uint32_t>(0); stride = c.At<uint32_t>(4); }
        else if (c.Is("TXF0")) for (int i = 0; i < 4; ++i) txf[i] = c.At<float>(i * 4);
        else if (c.Is("FVT1")) { fvt = c; haveFvt = true; }
        else if (c.Is("FSU0"))
            for (uint32_t o = 0; o + 2 <= c.size; o += 2) {
                uint16_t n = c.At<uint16_t>(o);
                if (!n) break;
                out.strips.push_back(n);
            }
    });
    if (!haveFvt || !vtype) return false;
    VertexLayout L(vtype);
    if (!stride) stride = L.stride;
    size_t count = 0;
    for (auto n : out.strips) count += n;
    // FVT1 starts with 'PAD' filler up to 16-byte alignment; the vertices are the tail.
    if (count * stride > fvt.size) return false;
    const uint8_t* base = fvt.data + (fvt.size - count * stride);

    out.verts.resize(count);
    for (size_t i = 0; i < count; ++i) {
        const uint8_t* v = base + i * stride;
        Vertex& o = out.verts[i];
        if (L.texOff >= 0) {
            int e = L.tex == 3 ? 4 : L.tex;
            float u = ReadComp(v + L.texOff, L.tex, false, 128.f, 32768.f);
            float w = ReadComp(v + L.texOff + e, L.tex, false, 128.f, 32768.f);
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

void AppendFragment(MeshData& m, const Fragment& f, int texture, uint32_t material) {
    uint32_t base = uint32_t(m.vertices.size());
    Batch b;
    b.firstIndex = uint32_t(m.indices.size());
    b.texture = texture;
    b.material = material;
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
        Batch& prev = m.batches.empty() ? b : m.batches.back();
        if (!m.batches.empty() && prev.texture == texture && prev.material == material &&
            prev.firstIndex + prev.indexCount == b.firstIndex)
            prev.indexCount += b.indexCount;
        else
            m.batches.push_back(b);
    }
    ++m.fragments;
}

void ParseTextureList(const Chunk& tex0, std::vector<TextureRef>& out) {
    // entries: u8 kind (0x03 PSM, 0x23 FLIP), "T:name", NUL
    size_t i = 0;
    while (i < tex0.size) {
        if (tex0.data[i] == 0) { ++i; continue; }
        TextureRef t;
        t.kind = tex0.data[i++];
        size_t start = i;
        while (i < tex0.size && tex0.data[i]) ++i;
        t.name.assign(reinterpret_cast<const char*>(tex0.data + start), i - start);
        if (t.name.size() > 2 && t.name[1] == ':') t.name = t.name.substr(2);
        if (!t.name.empty()) out.push_back(t);
    }
}

void AppendMeshHolder(MeshData& m, const Chunk& fmh0, const std::vector<Fragment>& pool,
                      std::vector<uint8_t>* poolUsed) {
    uint32_t material = 0;
    std::vector<uint16_t> refs;
    std::vector<int> txi;
    std::vector<Fragment> local;
    ForEachChunk(fmh0, [&](const Chunk& c) {
        if (c.Is("MFL1")) material = c.At<uint32_t>(0);
        else if (c.Is("FRR0"))
            for (uint32_t o = 0; o + 2 <= c.size; o += 2) {
                uint16_t v = c.At<uint16_t>(o);
                if (v == 0xFFFF) break;
                refs.push_back(v);
            }
        else if (c.Is("TXI1")) txi = ParseTxi(c);
        else if (c.Is("FRG0")) { local.emplace_back(); ParseFragment(c, local.back()); }
    });
    // MOD0 holders have no FRR0: their local fragments are used in order.
    if (refs.empty())
        for (size_t i = 0; i < local.size(); ++i) refs.push_back(uint16_t(0x8000 | i));
    for (size_t i = 0; i < refs.size(); ++i) {
        const Fragment* f = nullptr;
        if (refs[i] & 0x8000) {
            size_t n = refs[i] & 0x7FFF;
            if (n < local.size()) f = &local[n];
        } else if (refs[i] < pool.size()) {
            if (poolUsed && (*poolUsed)[refs[i]]) continue;
            if (poolUsed) (*poolUsed)[refs[i]] = 1;
            f = &pool[refs[i]];
        }
        if (f) AppendFragment(m, *f, i < txi.size() ? txi[i] : -1, material);
    }
}

void AppendOctree(MeshData& m, const Chunk& c, const std::vector<Fragment>& pool,
                  std::vector<uint8_t>* poolUsed) {
    ForEachChunk(c, [&](const Chunk& ch) {
        if (ch.Is("FMH0")) AppendMeshHolder(m, ch, pool, poolUsed);
        else if (ch.container) AppendOctree(m, ch, pool, poolUsed);
    });
}

bool LoadModel(const std::vector<uint8_t>& data, MeshData& out) {
    ForEachChunk(data.data(), data.size(), [&](const Chunk& top) {
        if (top.Is("TEX0")) ParseTextureList(top, out.textures);
        else if (top.Is("MOD0")) AppendOctree(out, top, {});
    });
    if (!out.fragments) { out.error = "no MOD0 fragments"; return false; }
    return true;
}

bool LoadInstanceMesh(const std::vector<uint8_t>& data, MeshData& out) {
    ForEachChunk(data.data(), data.size(), [&](const Chunk& top) {
        if (top.Is("TEX0")) ParseTextureList(top, out.textures);
        else if (top.Is("OCT0")) AppendOctree(out, top, {});
    });
    if (!out.fragments) { out.error = "no INST fragments"; return false; }
    return true;
}

} // namespace med
