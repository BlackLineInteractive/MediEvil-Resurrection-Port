#pragma once
// Geometry decode for SUBZ (level sub-zones) and MESH (MOD0 models).
//
// Both are built from FRG0 fragments:
//   VFT0/VFT1  u32 GE vertex type, u32 stride [, u32 flags]
//   TXF0       float uScale, vScale, uOffset, vOffset   (optional)
//   FVT1       'PAD' fill to 16-byte alignment, then vertices
//   FSU0       u16 triangle-strip lengths, 0-terminated
// Texture per fragment comes from TXI1 (u8 index into TEX0, u8 0xFF) in the owning FMH0;
// in SUBZ the FMH0 leaves of the OCT0 octree reference fragments of the SZR0 pool through FRR0.
#include <glm/glm.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace med {

struct Vertex {
    glm::vec3 pos{0};
    glm::vec3 normal{0, 1, 0};
    glm::vec2 uv{0};
    uint32_t color = 0xFFFFFFFF;   // RGBA8888 (0x80 = 1.0 on PSP)
};

struct Batch {
    uint32_t firstIndex = 0, indexCount = 0;
    int texture = -1;              // index into MeshData::textures, -1 = untextured
};

struct MeshData {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<Batch> batches;
    std::vector<std::string> textures;   // names from TEX0, "T:" prefix stripped
    glm::vec3 bmin{1e30f}, bmax{-1e30f};
    int fragments = 0;
    std::string error;
};

bool LoadSubZone(const std::vector<uint8_t>& data, MeshData& out);
bool LoadModel(const std::vector<uint8_t>& data, MeshData& out);

} // namespace med
