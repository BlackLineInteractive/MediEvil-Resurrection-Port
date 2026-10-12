#include "Core/Land.h"

#include "Core/Chunk.h"

#include <algorithm>
#include <cstring>

namespace med {

// FUN_001f3920 (LGT1 / older LGT0) + the static branch of FUN_00216a58.
// The payload is read from the next 16-byte boundary relative to the asset start. LGT1 layout (vec4s):
//   [0] object ambient   (zone +0xc0)
//   [1] fog colour 0..255 (+0x60, light set [0])
//   [2] fog near, far     (+0x70, light set [1])
//   [3] world ambient     (+0x80, light set [2])
//   [4] light 0 colour    (+0xd0)     [5] light 1 colour (+0xe0)
//   [6] light 0 direction (+0xf0)     [7] light 1 direction (+0x100)
//   u8 light 0 on, u8 light 1 on      (+0x130, +0x131)
// LGT0 has no leading object ambient; it is taken from vec4 [2] instead.
static void ParseLighting(const Chunk& c, bool v0, ZoneLighting& l) {
    size_t start = (c.OffsetInAsset() + 15) & ~size_t(15);
    size_t base = start - c.OffsetInAsset();
    auto vec = [&](int i) {
        size_t o = base + size_t(i) * 16;
        return glm::vec3(c.At<float>(o), c.At<float>(o + 4), c.At<float>(o + 8));
    };
    int k = 0;
    if (v0) {
        l.objectAmbient = vec(2);
    } else {
        l.objectAmbient = vec(0);
        k = 1;
    }
    l.fogColor = vec(k + 0);
    glm::vec3 fog = vec(k + 1);
    l.fogNear = fog.x;
    l.fogFar = fog.y;
    l.worldAmbient = vec(k + 2);
    for (int i = 0; i < 2; ++i) {
        l.lightColor[i] = glm::clamp(vec(k + 3 + i), glm::vec3(0), glm::vec3(1));
        l.lightDir[i] = vec(k + 5 + i);
        l.lightOn[i] = c.At<uint8_t>(base + size_t(k + 7) * 16 + i) != 0;
    }
}

bool LoadLand(const std::vector<uint8_t>& data, std::vector<Zone>& out) {
    bool found = false;
    ForEachChunk(data.data(), data.size(), [&](const Chunk& top) {
        if (!top.Is("LND0")) return;
        found = true;
        ForEachChunk(top, [&](const Chunk& zc) {
            if (!zc.Is("ZNE0")) return;
            Zone z;
            ForEachChunk(zc, [&](const Chunk& c) {
                if (c.Is("NAME")) z.name.assign(reinterpret_cast<const char*>(c.data), strnlen(reinterpret_cast<const char*>(c.data), c.size));
                else if (c.Is("LGT1")) ParseLighting(c, false, z.light);
                else if (c.Is("LGT0")) ParseLighting(c, true, z.light);
                else if (c.Is("ZDT0")) {
                    z.index = c.At<uint8_t>(7);
                    z.bmin = {c.At<float>(8), c.At<float>(12), c.At<float>(16)};
                    z.bmax = z.bmin + glm::vec3(c.At<float>(20), c.At<float>(24), c.At<float>(28));
                }
            });
            out.push_back(std::move(z));
        });
    });
    return found;
}

int FindZone(const std::vector<Zone>& zones, glm::vec3 bmin, glm::vec3 bmax) {
    int best = -1;
    float bestScore = -1e30f;
    glm::vec3 c = (bmin + bmax) * 0.5f;
    for (size_t i = 0; i < zones.size(); ++i) {
        glm::vec3 lo = glm::max(bmin, zones[i].bmin), hi = glm::min(bmax, zones[i].bmax);
        glm::vec3 e = glm::max(hi - lo, glm::vec3(0));
        float overlap = e.x * e.y * e.z;
        // no overlap (e.g. a point): prefer the nearest box
        glm::vec3 d = glm::max(glm::max(zones[i].bmin - c, c - zones[i].bmax), glm::vec3(0));
        float score = overlap > 0 ? overlap : -glm::dot(d, d);
        if (score > bestScore) { bestScore = score; best = int(i); }
    }
    return best;
}

} // namespace med
