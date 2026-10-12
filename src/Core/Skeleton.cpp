#include "Core/Skeleton.h"

#include "Core/Chunk.h"
#include "Core/GameData.h"

#include <glm/gtc/matrix_transform.hpp>

#include <cmath>
#include <cstring>

namespace med {

static glm::mat4 Compose(const glm::vec3& t, const glm::quat& q, const glm::vec3& s = glm::vec3(1)) {
    return glm::scale(glm::translate(glm::mat4(1.0f), t) * glm::mat4_cast(q), s);
}

int Skeleton::Find(uint32_t hash) const {
    for (size_t i = 0; i < nameHash.size(); ++i)
        if (nameHash[i] == hash) return int(i);
    return -1;
}

bool LoadSkeleton(const std::vector<uint8_t>& data, Skeleton& out) {
    bool ok = false;
    ForEachChunk(data.data(), data.size(), [&](const Chunk& top) {
        if (!top.Is("SKE0")) return;
        ForEachChunk(top, [&](const Chunk& c) {
            if (c.Is("SKL0")) {
                uint16_t n = c.At<uint16_t>(0);
                out.parent.assign(n, -1);
                out.bindRot.resize(n);
                out.bindPos.resize(n);
                for (uint16_t j = 0; j < n; ++j) {
                    size_t o = 4 + size_t(j) * 36;
                    out.bindRot[j] = glm::quat(c.At<float>(o + 12), c.At<float>(o), c.At<float>(o + 4), c.At<float>(o + 8));
                    out.bindPos[j] = {c.At<float>(o + 16), c.At<float>(o + 20), c.At<float>(o + 24)};
                }
                size_t h = 4 + size_t(n) * 36;
                for (uint16_t j = 0; j < n && h < c.size; ++j) {
                    while (h < c.size && c.data[h] != 0xFF) {
                        if (c.data[h] < n) out.parent[c.data[h]] = j;
                        ++h;
                    }
                    ++h;
                }
                ok = n > 0;
            } else if (c.Is("JNA0")) {
                size_t i = 0;
                while (i < c.size && out.names.size() < out.parent.size()) {
                    const char* s = reinterpret_cast<const char*>(c.data + i);
                    size_t len = strnlen(s, c.size - i);
                    out.names.emplace_back(s, len);
                    i += len + 1;
                }
            } else if (c.Is("BMA0")) {
                size_t n = c.size / 28;
                out.invBind.resize(n);
                for (size_t j = 0; j < n; ++j) {
                    size_t o = j * 28;
                    glm::quat q(c.At<float>(o + 12), c.At<float>(o), c.At<float>(o + 4), c.At<float>(o + 8));
                    out.invBind[j] = Compose({c.At<float>(o + 16), c.At<float>(o + 20), c.At<float>(o + 24)}, q);
                }
            }
        });
    });
    if (!ok) return false;
    out.names.resize(out.parent.size());
    out.nameHash.clear();
    for (const auto& n : out.names) out.nameHash.push_back(NameHash(n));
    if (out.invBind.size() != out.parent.size()) {
        // derive from the bind pose when BMA0 is missing
        std::vector<glm::mat4> world(out.parent.size());
        out.invBind.resize(out.parent.size());
        for (size_t j = 0; j < world.size(); ++j) {
            glm::mat4 local = Compose(out.bindPos[j], out.bindRot[j]);
            world[j] = out.parent[j] < 0 ? local : world[out.parent[j]] * local;
            out.invBind[j] = glm::inverse(world[j]);
        }
    }
    return true;
}

bool LoadAnimation(const std::vector<uint8_t>& data, Animation& out) {
    if (data.size() < 16 || (memcmp(data.data(), "ACT4", 4) != 0)) { out.error = "not ACT4"; return false; }
    uint32_t chunkSize;
    memcpy(&chunkSize, data.data() + 4, 4);
    const uint8_t* d = data.data();
    size_t end = std::min<size_t>(data.size(), 8 + size_t(chunkSize));
    auto u32 = [&](size_t o) { uint32_t v = 0; if (o + 4 <= end) memcpy(&v, d + o, 4); return v; };
    auto f32 = [&](size_t o) { float v = 0; if (o + 4 <= end) memcpy(&v, d + o, 4); return v; };

    size_t p = 8;
    out.flags = u32(p);
    if (out.flags & 0x40) { out.error = "compressed rotation stream (flag 0x40) not supported"; return false; }
    p += 12;
    // track lists (FUN_0019e56c)
    while (p + 8 <= end) {
        std::vector<uint32_t>* list = nullptr;
        if (!memcmp(d + p, "ROTA", 4)) list = &out.rotJoints;
        else if (!memcmp(d + p, "TRAN", 4)) list = &out.tranJoints;
        else if (!memcmp(d + p, "SCAL", 4)) list = &out.scalJoints;
        else break;
        uint32_t n = u32(p + 4);
        for (uint32_t i = 0; i < n; ++i) list->push_back(u32(p + 8 + i * 4));
        p += 8 + size_t(n) * 4;
    }
    out.duration = f32(p + 4);
    out.frameTime = f32(p + 8);
    out.frames = u32(p + 12);
    p += 16;
    if (out.frameTime == 0.0f) { out.error = "per-frame time table not supported"; return false; }

    size_t nr = out.rotJoints.size(), nt = out.tranJoints.size(), ns = out.scalJoints.size(), F = out.frames;
    if (nr) {
        p = (p + 7) & ~size_t(7);
        if (p + F * nr * 8 > end) { out.error = "truncated rotations"; return false; }
        out.rot.resize(F * nr);
        for (size_t i = 0; i < F * nr; ++i) {
            int16_t q[4];
            memcpy(q, d + p + i * 8, 8);
            out.rot[i] = glm::normalize(glm::quat(q[3] / 32767.f, q[0] / 32767.f, q[1] / 32767.f, q[2] / 32767.f));
        }
        p += F * nr * 8;
    }
    if (nt) {
        bool packed = out.flags & 0x100;
        if (!packed) p = (p + 15) & ~size_t(15);
        size_t stride = packed ? 12 : 16;
        if (p + F * nt * stride > end) { out.error = "truncated translations"; return false; }
        out.tran.resize(F * nt);
        for (size_t i = 0; i < F * nt; ++i)
            out.tran[i] = {f32(p + i * stride), f32(p + i * stride + 4), f32(p + i * stride + 8)};
        p += F * nt * stride;
    }
    if (ns) {
        p = (p + 15) & ~size_t(15);
        if (p + F * ns * 16 > end) { out.error = "truncated scales"; return false; }
        out.scal.resize(F * ns);
        for (size_t i = 0; i < F * ns; ++i) out.scal[i] = {f32(p + i * 16), f32(p + i * 16 + 4), f32(p + i * 16 + 8)};
    }
    return F > 0;
}

void SamplePose(const Skeleton& skel, const Animation* anim, float time, std::vector<glm::mat4>& palette) {
    size_t n = skel.Size();
    std::vector<glm::quat> rot(skel.bindRot);
    std::vector<glm::vec3> pos(skel.bindPos), scl(n, glm::vec3(1));
    if (anim && anim->frames) {
        float len = anim->frameTime * float(anim->frames - 1);
        float t = len > 0 ? (anim->Loops() ? std::fmod(time, len) : std::min(time, len)) : 0;
        float f = t / anim->frameTime;
        size_t f0 = std::min<size_t>(size_t(f), anim->frames - 1), f1 = std::min<size_t>(f0 + 1, anim->frames - 1);
        float a = f - float(f0);
        auto lerpTrack = [&](const auto& joints, const auto& keys, auto& dst, auto mix) {
            size_t k = joints.size();
            for (size_t i = 0; i < k; ++i) {
                int j = skel.Find(joints[i]);
                if (j >= 0) dst[j] = mix(keys[f0 * k + i], keys[f1 * k + i], a);
            }
        };
        // ROTA keys are relative to the bind rotation, like TRAN keys are relative to the bind position
        std::vector<glm::quat> rel(n, glm::quat(1, 0, 0, 0));
        lerpTrack(anim->rotJoints, anim->rot, rel, [](glm::quat x, glm::quat y, float s) { return glm::slerp(x, y, s); });
        for (size_t j = 0; j < n; ++j) rot[j] = rot[j] * rel[j];
        // TRAN keys are offsets from the bind position (pelvis key ~(0,-0.07,0) vs bind height 1.16)
        std::vector<glm::vec3> offs(n, glm::vec3(0));
        lerpTrack(anim->tranJoints, anim->tran, offs, [](glm::vec3 x, glm::vec3 y, float s) { return glm::mix(x, y, s); });
        for (size_t j = 0; j < n; ++j) pos[j] += offs[j];
        lerpTrack(anim->scalJoints, anim->scal, scl, [](glm::vec3 x, glm::vec3 y, float s) { return glm::mix(x, y, s); });
    }
    std::vector<glm::mat4> world(n);
    palette.resize(n);
    for (size_t j = 0; j < n; ++j) {
        glm::mat4 local = Compose(pos[j], rot[j], scl[j]);
        world[j] = skel.parent[j] < 0 ? local : world[skel.parent[j]] * local;
        palette[j] = world[j] * skel.invBind[j];
    }
}

} // namespace med
