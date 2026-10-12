#include "Core/Skeleton.h"

#include "Core/Chunk.h"
#include "Core/GameData.h"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
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

float Animation::Length() const { return frames > 1 ? frameTime * float(frames - 1) : 0.0f; }

void SampleLocal(const Skeleton& skel, const Animation* anim, float time, LocalPose& out) {
    size_t n = skel.Size();
    out.rot = skel.bindRot;
    out.pos = skel.bindPos;
    out.scl.assign(n, glm::vec3(1));
    if (!anim || !anim->frames) return;
    float len = anim->Length();
    float t = len > 0 ? (anim->Loops() ? std::fmod(std::fmod(time, len) + len, len) : std::clamp(time, 0.0f, len)) : 0;
    float f = anim->frameTime > 0 ? t / anim->frameTime : 0;
    size_t f0 = std::min<size_t>(size_t(f), anim->frames - 1), f1 = std::min<size_t>(f0 + 1, anim->frames - 1);
    float a = f - float(f0);
    auto lerpTrack = [&](const auto& joints, const auto& keys, auto apply) {
        size_t k = joints.size();
        for (size_t i = 0; i < k; ++i) {
            int j = skel.Find(joints[i]);
            if (j >= 0) apply(size_t(j), keys[f0 * k + i], keys[f1 * k + i]);
        }
    };
    // ROTA keys are relative to the bind rotation, TRAN keys are offsets from the bind position
    // (pelvis key ~(0,-0.07,0) vs bind height 1.16); absolute keys lay characters on the ground
    lerpTrack(anim->rotJoints, anim->rot, [&](size_t j, glm::quat x, glm::quat y) { out.rot[j] = out.rot[j] * glm::slerp(x, y, a); });
    lerpTrack(anim->tranJoints, anim->tran, [&](size_t j, glm::vec3 x, glm::vec3 y) { out.pos[j] += glm::mix(x, y, a); });
    lerpTrack(anim->scalJoints, anim->scal, [&](size_t j, glm::vec3 x, glm::vec3 y) { out.scl[j] = glm::mix(x, y, a); });
}

void BlendPose(LocalPose& a, const LocalPose& b, float w) {
    if (w <= 0) return;
    for (size_t j = 0; j < a.rot.size() && j < b.rot.size(); ++j) {
        a.rot[j] = glm::slerp(a.rot[j], b.rot[j], w);
        a.pos[j] = glm::mix(a.pos[j], b.pos[j], w);
        a.scl[j] = glm::mix(a.scl[j], b.scl[j], w);
    }
}

void PoseToPalette(const Skeleton& skel, const LocalPose& pose, std::vector<glm::mat4>& palette,
                   std::vector<glm::mat4>* worldOut) {
    size_t n = skel.Size();
    std::vector<glm::mat4> world(n);
    palette.resize(n);
    for (size_t j = 0; j < n; ++j) {
        glm::mat4 local = Compose(pose.pos[j], pose.rot[j], pose.scl[j]);
        world[j] = skel.parent[j] < 0 ? local : world[skel.parent[j]] * local;
        palette[j] = world[j] * skel.invBind[j];
    }
    if (worldOut) *worldOut = std::move(world);
}

void SamplePose(const Skeleton& skel, const Animation* anim, float time, std::vector<glm::mat4>& palette) {
    LocalPose pose;
    SampleLocal(skel, anim, time, pose);
    PoseToPalette(skel, pose, palette);
}

// Animator: current layer (+ optional second animation blended on the same phase) and the pose it
// replaced, faded out over `fade` seconds.
void Animator::Play(const Animation* a, const Animation* b, float weightB, float rate, float fade, bool restart) {
    if (!restart) {
        // same primary: only the blend partner / weight / rate change, the cycle keeps running
        if (cur_.a == a) {
            cur_.b = b;
            cur_.weightB = weightB;
            cur_.rate = rate;
            return;
        }
        // the blend partner became the primary (e.g. walk -> run): keep the shared phase
        if (a && cur_.b == a && cur_.a && a->Loops()) {
            float la = cur_.a->Length(), lb = a->Length();
            float phase = la > 0 ? std::fmod(cur_.time / la, 1.0f) : 0;
            cur_ = {a, b, weightB, rate, phase * lb};
            return;
        }
    }
    if (skel_ && cur_.a) {
        Evaluate(prevPose_);
        fadeLen_ = fade;
        fadeT_ = fade;
    }
    cur_ = {a, b, weightB, rate, 0.0f};
}

void Animator::Update(float dt) {
    cur_.time += dt * cur_.rate;
    if (fadeT_ > 0) fadeT_ = std::max(0.0f, fadeT_ - dt);
}

bool Animator::Finished() const { return cur_.a && !cur_.a->Loops() && cur_.time >= cur_.a->Length(); }

float Animator::Progress() const {
    float len = cur_.a ? cur_.a->Length() : 0;
    return len > 0 ? cur_.time / len : 1.0f;
}

void Animator::Evaluate(LocalPose& out) const {
    if (!skel_) return;
    SampleLocal(*skel_, cur_.a, cur_.time, out);
    if (cur_.b && cur_.weightB > 0) {
        // same normalised phase in both cycles
        LocalPose pb;
        float la = cur_.a ? cur_.a->Length() : 0, lb = cur_.b->Length();
        float phase = la > 0 ? cur_.time / la : 0;
        SampleLocal(*skel_, cur_.b, phase * lb, pb);
        BlendPose(out, pb, cur_.weightB);
    }
    if (fadeT_ > 0 && fadeLen_ > 0 && !prevPose_.rot.empty()) {
        LocalPose cur = out;
        out = prevPose_;
        BlendPose(out, cur, 1.0f - fadeT_ / fadeLen_);
    }
}

} // namespace med
