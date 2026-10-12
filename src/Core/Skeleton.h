#pragma once
// Skeletons (SKEL / SKE0) and keyframe animations (ANIM / ACT4).
//
// SKE0 { SKL0, JNA0, BMA0 }, followed by a skinned MOD0:
//   SKL0  u16 n, u16 n; n x {quat x,y,z,w; vec3 pos; u8[8] (0x73 0x80.. - not decoded)}  local bind pose
//         then for each joint its child list, 0xFF-terminated
//   JNA0  n NUL-terminated joint names
//   BMA0  n x {quat; vec3}  inverse bind transforms (== inverse of the SKL0 world bind pose)
// Conventions (checked against BMA0 to 3e-7): local = T(pos) * R(quat), world = parent * local.
//
// ACT4 (main.prx FUN_0019e300 / FUN_0019f09c, keyframe variant without flag 0x40):
//   u32 flags; u32 ?; float ?; then track lists 'ROTA' / 'TRAN' / 'SCAL': u32 n, u32 jointNameHash[n]
//   u32 ?, float duration, float frameTime, u32 frameCount
//   rotations    [frame][rotTrack]   4 x s16 quaternion / 32767, start aligned to 8
//   translations [frame][tranTrack]  3 x float (flag 0x100) or 4 x float aligned to 16; offsets from bind pos
//   scales       [frame][scalTrack]  4 x float, aligned to 16
//   flags: 0x10 = no loop, 0x40 = compressed rotation stream (FUN_001a1a38, not supported here)
// Track joints are identified by HashName129(joint name).
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace med {

struct Skeleton {
    std::vector<std::string> names;
    std::vector<int> parent;
    std::vector<glm::quat> bindRot;
    std::vector<glm::vec3> bindPos;
    std::vector<glm::mat4> invBind;
    std::vector<uint32_t> nameHash;
    int Find(uint32_t hash) const;
    size_t Size() const { return names.size(); }
};

bool LoadSkeleton(const std::vector<uint8_t>& skelAsset, Skeleton& out);

struct Animation {
    uint32_t flags = 0;
    float duration = 0, frameTime = 0;
    uint32_t frames = 0;
    std::vector<uint32_t> rotJoints, tranJoints, scalJoints;   // joint name hashes
    std::vector<glm::quat> rot;    // frames * rotJoints
    std::vector<glm::vec3> tran;   // frames * tranJoints
    std::vector<glm::vec3> scal;   // frames * scalJoints
    bool Loops() const { return !(flags & 0x10); }
    float Length() const;
    std::string error;
};

bool LoadAnimation(const std::vector<uint8_t>& animAsset, Animation& out);

// Joint-local pose (bind pose with the animation applied).
struct LocalPose {
    std::vector<glm::quat> rot;
    std::vector<glm::vec3> pos, scl;
};

// `time` wraps for looping animations and clamps otherwise; bind pose when anim is null.
void SampleLocal(const Skeleton& skel, const Animation* anim, float time, LocalPose& out);
void BlendPose(LocalPose& a, const LocalPose& b, float w);   // a = mix(a, b, w)
void PoseToPalette(const Skeleton& skel, const LocalPose& pose, std::vector<glm::mat4>& palette,
                   std::vector<glm::mat4>* world = nullptr);

// Skinning palette (world * invBind per joint) for `anim` at `time` seconds; bind pose when anim is null.
void SamplePose(const Skeleton& skel, const Animation* anim, float time, std::vector<glm::mat4>& palette);

// Plays one animation (optionally blended with a second one on the same phase, e.g. walk/run) and
// crossfades from the previous pose when the animation changes.
class Animator {
public:
    void SetSkeleton(const Skeleton* s) { skel_ = s; }
    void Play(const Animation* a, const Animation* b = nullptr, float weightB = 0, float rate = 1, float fade = 0.15f,
              bool restart = false);
    void Update(float dt);
    void Evaluate(LocalPose& out) const;
    const Animation* Current() const { return cur_.a; }
    float Time() const { return cur_.time; }
    float Progress() const;   // time / length (0..1+ for one-shots)
    bool Finished() const;    // a one-shot reached its end
private:
    struct Layer {
        const Animation* a = nullptr;
        const Animation* b = nullptr;
        float weightB = 0, rate = 1, time = 0;
    };
    const Skeleton* skel_ = nullptr;
    Layer cur_;
    LocalPose prevPose_;
    float fadeT_ = 0, fadeLen_ = 0;
};

} // namespace med
