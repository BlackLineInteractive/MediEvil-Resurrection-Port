#pragma once
// Level collision (TCD asset "<subzone>.tcd", one per sub-zone, world space).
//
//   TCD0 header (0x28): u32 ?, u32 triOffset, vec3 centre, float halfSize, u32 vertOffset, u32 0
//   then an octree of u16 node / triangle-list entries (0x8000 = child node) up to triOffset,
//   triangles: {u16 a, b, c, u16 flags} from triOffset (flags high nibble: surface type),
//   vertices:  float3 from vertOffset to the end (vertOffset may be preceded by padding).
// Checked on all 228 TCD assets (indices always < vertex count). The octree is not used here: the
// viewer builds its own XZ grid over the triangles.
#include <glm/glm.hpp>

#include <cstdint>
#include <vector>

namespace med {

struct CollisionTri {
    glm::vec3 a, b, c, n;
    uint16_t flags = 0;
};

bool LoadTCD(const std::vector<uint8_t>& data, std::vector<CollisionTri>& out);

class CollisionWorld {
public:
    void Clear();
    void Add(const std::vector<CollisionTri>& tris);
    void Build(float cellSize = 4.0f);
    bool Empty() const { return tris_.empty(); }
    size_t Size() const { return tris_.size(); }

    // Highest walkable surface (normal.y >= minNy) below `from` within `maxDrop`.
    bool Ground(const glm::vec3& from, float maxDrop, float& y, glm::vec3* normal = nullptr,
                float minNy = 0.5f) const;
    // First hit along a segment; t in [0,1].
    bool Raycast(const glm::vec3& from, const glm::vec3& to, float& t, glm::vec3* normal = nullptr) const;
    // Push a sphere out of steep triangles (walls). Returns true when it touched something.
    bool PushSphere(glm::vec3& centre, float radius, float maxNy = 0.5f) const;

private:
    template <class F> void ForCells(glm::vec2 lo, glm::vec2 hi, F f) const;
    std::vector<CollisionTri> tris_;
    std::vector<std::vector<uint32_t>> cells_;
    glm::vec2 origin_{0};
    float cell_ = 4;
    int nx_ = 0, nz_ = 0;
    mutable std::vector<uint32_t> stamp_;
    mutable uint32_t query_ = 0;
};

} // namespace med
