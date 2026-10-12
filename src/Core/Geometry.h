#pragma once
// Geometry decode for SUBZ (level sub-zones), INST (placed instance meshes) and MESH (MOD0 models).
//
// All of them are built from FRG0 fragments:
//   VFT0/VFT1  u32 GE vertex type, u32 stride [, u32 flags]
//   TXF0       float uScale, vScale, uOffset, vOffset   (optional)
//   FVT1       filler to 16-byte alignment (relative to the asset start), then vertices
//   FSU0       u16 triangle-strip lengths, 0-terminated
// grouped by FMH0 "mesh holders":
//   MFL1       material flags (decoded by MaterialFlags, port of FUN_002182bc)
//   FRR0       u16 fragment refs, 0xFFFF-terminated: n = SZR0 pool fragment n,
//              0x8000|n = n-th FRG0 inside this FMH0
//   TXI1       per ref {u8 index into TEX0 (0xFF = none), u8 flags}; as many entries as refs
//   FRG0...    local fragments
#include <glm/glm.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace med {

struct Chunk;

struct Vertex {
    glm::vec3 pos{0};
    glm::vec3 normal{0, 1, 0};
    glm::vec2 uv{0};
    uint32_t color = 0xFFFFFFFF;   // RGBA8888 (0x80 = 1.0 on PSP)
};

struct Batch {
    uint32_t firstIndex = 0, indexCount = 0;
    int texture = -1;              // index into MeshData::textures, -1 = untextured
    uint32_t material = 0;         // raw MFL1 word of the owning FMH0
};

struct TextureRef {
    std::string name;              // "T:" prefix stripped
    uint8_t kind = 3;              // byte before the name: 0x03 = PSM, 0x23 ('#') = FLIP flipbook
};

struct MeshData {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<Batch> batches;
    std::vector<TextureRef> textures;
    glm::vec3 bmin{1e30f}, bmax{-1e30f};
    int fragments = 0;
    std::string error;
};

struct Fragment {
    std::vector<Vertex> verts;
    std::vector<uint16_t> strips;
};

bool ParseFragment(const Chunk& frg, Fragment& out);
void AppendFragment(MeshData& m, const Fragment& f, int texture, uint32_t material);
void ParseTextureList(const Chunk& tex0, std::vector<TextureRef>& out);

// FMH0 holder: resolves its fragment refs against `pool` (may be empty for INST/MESH) and appends them.
// `poolUsed` (optional, sized like pool) skips pool fragments already referenced by another octree leaf.
void AppendMeshHolder(MeshData& m, const Chunk& fmh0, const std::vector<Fragment>& pool,
                      std::vector<uint8_t>* poolUsed = nullptr);
// Walk an OCT0 octree (or any container) appending every FMH0 found.
void AppendOctree(MeshData& m, const Chunk& c, const std::vector<Fragment>& pool,
                  std::vector<uint8_t>* poolUsed = nullptr);

bool LoadModel(const std::vector<uint8_t>& data, MeshData& out);      // MESH (MOD0)
bool LoadInstanceMesh(const std::vector<uint8_t>& data, MeshData& out); // INST (BOX0, DPD0, OCT0, TEX0)

} // namespace med
