#pragma once
// Level sub-zone loading, ported from main.prx.
//
//   FUN_001f651c  CSubZone::Load        SBZ0: SZR0 fragment pool, BOX0 bounds, DPD0, OCT0 octree, INS0 instances
//   FUN_001f68f8  CSubZone::LoadInstances  INS0: INL4 model list, INN0 per-instance model index, INM1 matrices
//   FUN_001f6d08  CSubZone::LoadInstanceModel  sprintf("%s_%s.inst", subzoneName, modelName)
//   FUN_002182bc  CMeshHolder::SetMaterial     MFL1 bit decode (see MaterialFlags)
#include "Core/Geometry.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace med {

// MFL1 word as FUN_002182bc unpacks it into the mesh holder (field names are ours, bit sources are original).
struct MaterialFlags {
    bool bit1 = false;        // -> holder flag 0x4
    bool bit2 = false;        // -> holder flag 0x8
    uint8_t byte8 = 0;        // (mfl >> 3) & 0xFF, stored at holder+8 (0 opaque, 1 most terrain, 0x44/0x48 glass)
    uint8_t layer = 0;        // ((mfl >> 16) & 0x7F) - 1, stored at holder+9
    uint32_t mode = 0;        // (mfl >> 5) & 0x7C0, holder flags bits 6..10
    bool bit23 = false;       // -> 0x2000
    bool bit24 = false;       // -> 0x4000
    bool bit25 = false;       // -> 0x8000
    static MaterialFlags Decode(uint32_t mfl);
};

// INL4 entry
struct InstanceModel {
    std::string name;         // e.g. "000048"; asset is "<subzone>_<name>.inst"
    float lodDistance = 0;    // compared with a squared global distance (DAT_003dd718^2) before loading
    int8_t mode = 0;          // -1: no model, 0: always loaded, else: loaded by distance
};

struct Instance {
    uint16_t model = 0;       // INN0: index into InstanceModel list
    glm::mat4 world{1.0f};    // INM1 first matrix
    glm::mat4 extra{1.0f};    // INM1 second matrix (the game writes 1/row-scale into its w column on load)
};

struct SubZone {
    glm::vec4 bounds{0};      // BOX0
    MeshData mesh;            // static geometry: SZR0 pool through the OCT0 octree
    std::vector<InstanceModel> models;
    std::vector<Instance> instances;
    uint32_t nameHashPrefix = 0;   // hash state of the sub-zone base name (asset name without ".subz")

    // `assetHash` is the SUBZ TOC hash: the engine formats instance models with the sub-zone name, and the
    // hash of that name is recovered from it without knowing the string.
    bool Load(const std::vector<uint8_t>& data, uint32_t assetHash);
    uint32_t InstanceModelHash(const InstanceModel& m) const;
    std::string error;
};

} // namespace med
