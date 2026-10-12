#include "Core/World.h"

#include "Core/Chunk.h"
#include "Core/GameData.h"

#include <glm/gtc/type_ptr.hpp>

#include <cstring>

namespace med {

MaterialFlags MaterialFlags::Decode(uint32_t v) {
    MaterialFlags f;
    f.bit1 = v & 2;
    f.bit2 = v & 4;
    f.mode = (v >> 5 & 0x1C0) | (v >> 5 & 0x600);
    f.byte8 = uint8_t(v >> 3);
    f.layer = uint8_t(((v >> 16) & 0x7F) - 1);
    f.bit23 = v & 0x800000;
    f.bit24 = v & 0x1000000;
    f.bit25 = v & 0x2000000;
    return f;
}

// FUN_001f68f8. The original allocates runtime state per model/instance alongside; only the data is kept.
static void LoadInstances(SubZone& sz, const Chunk& ins0) {
    ForEachChunk(ins0, [&](const Chunk& c) {
        if (c.Is("INL4")) {
            // u32 n; float lodDistance[n]; u16 mode[n] (low byte, signed); then a NUL-terminated
            // name for every entry whose mode != -1
            uint32_t n = c.At<uint32_t>(0);
            const uint8_t* names = c.data + 4 + n * 6;
            const uint8_t* end = c.data + c.size;
            sz.models.resize(n);
            for (uint32_t i = 0; i < n; ++i) {
                InstanceModel& m = sz.models[i];
                m.lodDistance = c.At<float>(4 + i * 4);
                m.mode = int8_t(c.At<uint8_t>(4 + n * 4 + i * 2));
                if (m.mode == -1) continue;
                const uint8_t* e = static_cast<const uint8_t*>(memchr(names, 0, size_t(end - names)));
                if (!e) break;
                m.name.assign(reinterpret_cast<const char*>(names), size_t(e - names));
                names = e + 1;
            }
        } else if (c.Is("INN0")) {
            sz.instances.resize(c.size / 2);
            for (uint32_t i = 0; i < c.size / 2; ++i) sz.instances[i].model = c.At<uint16_t>(i * 2);
        } else if (c.Is("INM1")) {
            // two 4x4 float matrices per instance after filler to 16-byte alignment relative to the
            // asset start (the game aligns the pointer: (p + 15) & ~15)
            size_t pad = (16 - c.OffsetInAsset() % 16) % 16;
            size_t count = c.size > pad ? (c.size - pad) / 128 : 0;
            const uint8_t* base = c.data + pad;
            if (sz.instances.size() < count) sz.instances.resize(count);
            for (size_t i = 0; i < count; ++i) {
                float m[32];
                memcpy(m, base + i * 128, sizeof m);
                sz.instances[i].world = glm::make_mat4(m);
                sz.instances[i].extra = glm::make_mat4(m + 16);
            }
        }
    });
}

bool SubZone::Load(const std::vector<uint8_t>& data, uint32_t assetHash) {
    nameHashPrefix = NameHashStripSuffix(assetHash, ".subz");
    std::vector<Fragment> pool;
    bool found = false;
    ForEachChunk(data.data(), data.size(), [&](const Chunk& top) {
        if (top.Is("TEX0")) { ParseTextureList(top, mesh.textures); return; }
        if (!top.Is("SBZ0")) return;
        found = true;
        // FUN_001f651c: SZR0 pool first, then BOX0, DPD0 and the OCT0 tree; INS0 last.
        ForEachChunk(top, [&](const Chunk& c) {
            if (c.Is("SZR0"))
                ForEachChunk(c, [&](const Chunk& f) {
                    if (!f.Is("FRG0")) return;
                    pool.emplace_back();
                    ParseFragment(f, pool.back());
                });
            else if (c.Is("BOX0"))
                bounds = {c.At<float>(0), c.At<float>(4), c.At<float>(8), c.At<float>(12)};
        });
        std::vector<uint8_t> used(pool.size(), 0);
        ForEachChunk(top, [&](const Chunk& c) {
            if (c.Is("OCT0")) AppendOctree(mesh, c, pool, &used);
            else if (c.Is("INS0")) LoadInstances(*this, c);
        });
    });
    if (!found) { error = "no SBZ0"; return false; }
    return true;
}

// FUN_001f6d08: "%s_%s.inst" with the sub-zone name, hashed by LoadAssetByName.
uint32_t SubZone::InstanceModelHash(const InstanceModel& m) const {
    return NameHash("_" + m.name + ".inst", nameHashPrefix);
}

} // namespace med
