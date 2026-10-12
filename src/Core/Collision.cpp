#include "Core/Collision.h"

#include "Core/Chunk.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace med {

bool LoadTCD(const std::vector<uint8_t>& data, std::vector<CollisionTri>& out) {
    if (data.size() < 0x30 || memcmp(data.data(), "TCD0", 4) != 0) return false;
    auto u16 = [&](size_t o) { uint16_t v; memcpy(&v, data.data() + o, 2); return v; };
    auto u32 = [&](size_t o) { uint32_t v; memcpy(&v, data.data() + o, 4); return v; };
    auto f32 = [&](size_t o) { float v; memcpy(&v, data.data() + o, 4); return v; };
    size_t triOff = u32(12), vertOff = u32(32);
    if (triOff > vertOff || vertOff > data.size()) return false;
    size_t nv = (data.size() - vertOff) / 12, nt = (vertOff - triOff) / 8;
    std::vector<glm::vec3> v(nv);
    for (size_t i = 0; i < nv; ++i) v[i] = {f32(vertOff + i * 12), f32(vertOff + i * 12 + 4), f32(vertOff + i * 12 + 8)};
    for (size_t i = 0; i < nt; ++i) {
        size_t o = triOff + i * 8;
        uint16_t a = u16(o), b = u16(o + 2), c = u16(o + 4);
        if (a >= nv || b >= nv || c >= nv) continue;
        CollisionTri t;
        t.a = v[a];
        t.b = v[b];
        t.c = v[c];
        t.flags = u16(o + 6);
        glm::vec3 n = glm::cross(t.b - t.a, t.c - t.a);
        float l = glm::length(n);
        if (l < 1e-8f) continue;
        t.n = n / l;
        out.push_back(t);
    }
    return true;
}

void CollisionWorld::Clear() {
    tris_.clear();
    cells_.clear();
    nx_ = nz_ = 0;
}

void CollisionWorld::Add(const std::vector<CollisionTri>& tris) { tris_.insert(tris_.end(), tris.begin(), tris.end()); }

void CollisionWorld::Build(float cellSize) {
    cell_ = cellSize;
    cells_.clear();
    if (tris_.empty()) return;
    glm::vec2 lo(1e30f), hi(-1e30f);
    for (const auto& t : tris_)
        for (const glm::vec3& p : {t.a, t.b, t.c}) {
            lo = glm::min(lo, glm::vec2(p.x, p.z));
            hi = glm::max(hi, glm::vec2(p.x, p.z));
        }
    origin_ = lo;
    nx_ = std::max(1, int(std::ceil((hi.x - lo.x) / cell_)) + 1);
    nz_ = std::max(1, int(std::ceil((hi.y - lo.y) / cell_)) + 1);
    cells_.assign(size_t(nx_) * nz_, {});
    for (uint32_t i = 0; i < tris_.size(); ++i) {
        const auto& t = tris_[i];
        glm::vec2 a = glm::min(glm::min(glm::vec2(t.a.x, t.a.z), glm::vec2(t.b.x, t.b.z)), glm::vec2(t.c.x, t.c.z));
        glm::vec2 b = glm::max(glm::max(glm::vec2(t.a.x, t.a.z), glm::vec2(t.b.x, t.b.z)), glm::vec2(t.c.x, t.c.z));
        ForCells(a, b, [&](std::vector<uint32_t>& c) { c.push_back(i); });
    }
    stamp_.assign(tris_.size(), 0);
}

template <class F> void CollisionWorld::ForCells(glm::vec2 lo, glm::vec2 hi, F f) const {
    if (cells_.empty()) return;
    int x0 = std::clamp(int(std::floor((lo.x - origin_.x) / cell_)), 0, nx_ - 1);
    int x1 = std::clamp(int(std::floor((hi.x - origin_.x) / cell_)), 0, nx_ - 1);
    int z0 = std::clamp(int(std::floor((lo.y - origin_.y) / cell_)), 0, nz_ - 1);
    int z1 = std::clamp(int(std::floor((hi.y - origin_.y) / cell_)), 0, nz_ - 1);
    for (int z = z0; z <= z1; ++z)
        for (int x = x0; x <= x1; ++x) f(const_cast<std::vector<uint32_t>&>(cells_[size_t(z) * nx_ + x]));
}

// Möller-Trumbore, both faces.
static bool RayTri(const glm::vec3& o, const glm::vec3& d, const CollisionTri& t, float& u) {
    glm::vec3 e1 = t.b - t.a, e2 = t.c - t.a, p = glm::cross(d, e2);
    float det = glm::dot(e1, p);
    if (std::fabs(det) < 1e-9f) return false;
    float inv = 1.0f / det;
    glm::vec3 s = o - t.a;
    float bu = glm::dot(s, p) * inv;
    if (bu < 0 || bu > 1) return false;
    glm::vec3 q = glm::cross(s, e1);
    float bv = glm::dot(d, q) * inv;
    if (bv < 0 || bu + bv > 1) return false;
    u = glm::dot(e2, q) * inv;
    return true;
}

bool CollisionWorld::Ground(const glm::vec3& from, float maxDrop, float& y, glm::vec3* normal, float minNy) const {
    bool hit = false;
    float best = maxDrop;
    ++query_;
    ForCells({from.x, from.z}, {from.x, from.z}, [&](const std::vector<uint32_t>& cell) {
        for (uint32_t i : cell) {
            if (stamp_[i] == query_) continue;
            stamp_[i] = query_;
            const auto& t = tris_[i];
            if (std::fabs(t.n.y) < minNy) continue;
            float u;
            if (RayTri(from, {0, -1, 0}, t, u) && u >= -0.01f && u < best) {
                best = u;
                hit = true;
                if (normal) *normal = t.n.y < 0 ? -t.n : t.n;
            }
        }
    });
    if (hit) y = from.y - best;
    return hit;
}

bool CollisionWorld::Raycast(const glm::vec3& from, const glm::vec3& to, float& tOut, glm::vec3* normal) const {
    glm::vec3 d = to - from;
    bool hit = false;
    float best = 1.0f;
    ++query_;
    ForCells(glm::min(glm::vec2(from.x, from.z), glm::vec2(to.x, to.z)),
             glm::max(glm::vec2(from.x, from.z), glm::vec2(to.x, to.z)), [&](const std::vector<uint32_t>& cell) {
                 for (uint32_t i : cell) {
                     if (stamp_[i] == query_) continue;
                     stamp_[i] = query_;
                     float u;
                     if (RayTri(from, d, tris_[i], u) && u >= 0 && u < best) {
                         best = u;
                         hit = true;
                         if (normal) *normal = tris_[i].n;
                     }
                 }
             });
    if (hit) tOut = best;
    return hit;
}

static glm::vec3 ClosestOnTri(const glm::vec3& p, const glm::vec3& a, const glm::vec3& b, const glm::vec3& c) {
    // Ericson, Real-Time Collision Detection 5.1.5
    glm::vec3 ab = b - a, ac = c - a, ap = p - a;
    float d1 = glm::dot(ab, ap), d2 = glm::dot(ac, ap);
    if (d1 <= 0 && d2 <= 0) return a;
    glm::vec3 bp = p - b;
    float d3 = glm::dot(ab, bp), d4 = glm::dot(ac, bp);
    if (d3 >= 0 && d4 <= d3) return b;
    float vc = d1 * d4 - d3 * d2;
    if (vc <= 0 && d1 >= 0 && d3 <= 0) return a + ab * (d1 / (d1 - d3));
    glm::vec3 cp = p - c;
    float d5 = glm::dot(ab, cp), d6 = glm::dot(ac, cp);
    if (d6 >= 0 && d5 <= d6) return c;
    float vb = d5 * d2 - d1 * d6;
    if (vb <= 0 && d2 >= 0 && d6 <= 0) return a + ac * (d2 / (d2 - d6));
    float va = d3 * d6 - d5 * d4;
    if (va <= 0 && (d4 - d3) >= 0 && (d5 - d6) >= 0) return b + (c - b) * ((d4 - d3) / ((d4 - d3) + (d5 - d6)));
    float den = 1.0f / (va + vb + vc);
    return a + ab * (vb * den) + ac * (vc * den);
}

bool CollisionWorld::PushSphere(glm::vec3& centre, float radius, float maxNy) const {
    bool touched = false;
    for (int iter = 0; iter < 3; ++iter) {
        bool any = false;
        ++query_;
        ForCells({centre.x - radius, centre.z - radius}, {centre.x + radius, centre.z + radius},
                 [&](const std::vector<uint32_t>& cell) {
                     for (uint32_t i : cell) {
                         if (stamp_[i] == query_) continue;
                         stamp_[i] = query_;
                         const auto& t = tris_[i];
                         if (std::fabs(t.n.y) >= maxNy) continue;   // floors / ceilings handled elsewhere
                         glm::vec3 q = ClosestOnTri(centre, t.a, t.b, t.c);
                         glm::vec3 d = centre - q;
                         d.y = 0;   // walls push horizontally only
                         float l = glm::length(d);
                         if (l >= radius) continue;
                         glm::vec3 dir = l > 1e-5f ? d / l : glm::normalize(glm::vec3(t.n.x, 0, t.n.z) + 1e-6f);
                         centre += dir * (radius - l);
                         any = touched = true;
                     }
                 });
        if (!any) break;
    }
    return touched;
}

} // namespace med
