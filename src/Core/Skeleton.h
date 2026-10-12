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
//   translations [frame][tranTrack]  3 x float (flag 0x100) or 4 x float aligned to 16
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
    std::string error;
};

bool LoadAnimation(const std::vector<uint8_t>& animAsset, Animation& out);

// Skinning palette (world * invBind per joint) for `anim` at `time` seconds; bind pose when anim is null.
void SamplePose(const Skeleton& skel, const Animation* anim, float time, std::vector<glm::mat4>& palette);

} // namespace med
