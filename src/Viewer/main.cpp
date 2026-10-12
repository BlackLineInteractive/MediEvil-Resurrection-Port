// MediEvil: Resurrection level/model viewer - reads gamedata.bin directly.
//
//   medievil-viewer [--data gamedata.bin] [--names docs/names.tsv]
//                   [--group N] [--model name.mesh] [--shot out.png] [--yaw deg] [--pitch deg]
//
// --shot renders one frame with the camera framing the loaded geometry, writes a PNG and exits.
// Play mode: P (or the "Play as Dan" checkbox) drops Sir Daniel where the camera looks; WASD walk,
// Shift run, Shift+Ctrl sprint burst, Space jump, J or a short click attacks (3-hit combo), drag to orbit the camera. --play x,y,z [--play-move x,y --play-time s --play-jump t --play-attack every]
// does the same for --shot. --atbl-report <model> prints the motion of every ATBL animation. --model x.skel --anim <hash> previews an animation.
#include "Core/GameData.h"
#include "Core/Geometry.h"
#include "Core/World.h"
#include "Core/Entity.h"
#include "Core/Land.h"
#include "Core/Collision.h"
#include "Core/Player.h"
#include "Core/Skeleton.h"
#include "Viewer/Renderer.h"

#include <GL/glew.h>
#include <SDL.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl2.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

using namespace med;

namespace {

// Minimal PNG writer (stored deflate blocks) so the viewer needs no image library.
void WritePNG(const std::string& path, int w, int h, const std::vector<uint8_t>& rgbaBottomUp) {
    static uint32_t crcTab[256];
    if (!crcTab[1])
        for (uint32_t n = 0; n < 256; ++n) {
            uint32_t c = n;
            for (int k = 0; k < 8; ++k) c = c & 1 ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            crcTab[n] = c;
        }
    auto crc = [&](const uint8_t* p, size_t n, uint32_t c = 0xFFFFFFFF) {
        for (size_t i = 0; i < n; ++i) c = crcTab[(c ^ p[i]) & 255] ^ (c >> 8);
        return c;
    };
    std::vector<uint8_t> raw;
    for (int y = h - 1; y >= 0; --y) {
        raw.push_back(0);
        raw.insert(raw.end(), &rgbaBottomUp[size_t(y) * w * 4], &rgbaBottomUp[size_t(y) * w * 4] + size_t(w) * 4);
    }
    std::vector<uint8_t> z = {0x78, 0x01};
    uint32_t a = 1, b = 0;
    for (uint8_t v : raw) { a = (a + v) % 65521; b = (b + a) % 65521; }
    for (size_t off = 0; off < raw.size(); off += 65535) {
        size_t n = std::min<size_t>(65535, raw.size() - off);
        z.push_back(off + n == raw.size());
        z.push_back(uint8_t(n)); z.push_back(uint8_t(n >> 8));
        z.push_back(uint8_t(~n)); z.push_back(uint8_t(~n >> 8));
        z.insert(z.end(), raw.begin() + off, raw.begin() + off + n);
    }
    uint32_t adler = b << 16 | a;
    for (int i = 3; i >= 0; --i) z.push_back(uint8_t(adler >> (i * 8)));

    FILE* f = fopen(path.c_str(), "wb");
    if (!f) return;
    auto be32 = [&](uint32_t v) { uint8_t q[4] = {uint8_t(v >> 24), uint8_t(v >> 16), uint8_t(v >> 8), uint8_t(v)}; fwrite(q, 1, 4, f); };
    auto chunk = [&](const char* t, const std::vector<uint8_t>& d) {
        be32(uint32_t(d.size()));
        std::vector<uint8_t> td(t, t + 4);
        td.insert(td.end(), d.begin(), d.end());
        fwrite(td.data(), 1, td.size(), f);
        be32(~crc(td.data(), td.size()));
    };
    fwrite("\x89PNG\r\n\x1a\n", 1, 8, f);
    std::vector<uint8_t> ihdr = {uint8_t(w >> 24), uint8_t(w >> 16), uint8_t(w >> 8), uint8_t(w),
                                 uint8_t(h >> 24), uint8_t(h >> 16), uint8_t(h >> 8), uint8_t(h), 8, 6, 0, 0, 0};
    chunk("IHDR", ihdr);
    chunk("IDAT", z);
    chunk("IEND", {});
    fclose(f);
}

struct Camera {
    glm::vec3 pos{0, 10, 30};
    glm::vec3 pivot{0, 0, 0};
    float yaw = -90.0f, pitch = -20.0f, fov = 60.0f, speed = 20.0f;

    glm::vec3 Forward() const {
        float y = glm::radians(yaw), p = glm::radians(pitch);
        return {std::cos(p) * std::cos(y), std::sin(p), std::cos(p) * std::sin(y)};
    }
    glm::vec3 Right() const {
        glm::vec3 r = glm::cross(Forward(), glm::vec3(0, 1, 0));
        float len = glm::length(r);
        return len > 0.0001f ? r / len : glm::vec3(1, 0, 0);
    }
    glm::vec3 Up() const {
        return glm::normalize(glm::cross(Right(), Forward()));
    }
    void Frame(glm::vec3 bmin, glm::vec3 bmax) {
        pivot = (bmin + bmax) * 0.5f;
        float r = std::max(glm::length(bmax - bmin) * 0.5f, 1.0f);
        pos = pivot - Forward() * (r / std::tan(glm::radians(fov) * 0.5f));
        speed = std::max(r * 0.5f, 2.0f);
    }
    void Orbit(float dYaw, float dPitch) {
        yaw += dYaw;
        pitch = std::clamp(pitch + dPitch, -89.0f, 89.0f);
        float dist = glm::length(pos - pivot);
        if (dist < 0.1f) dist = 1.0f;
        pos = pivot - Forward() * dist;
    }
    void Pan(float dx, float dy) {
        float factor = speed * 0.05f;
        glm::vec3 delta = -Right() * (dx * factor) + Up() * (dy * factor);
        pos += delta;
        pivot += delta;
    }
    void Dolly(float delta) {
        float step = delta * (speed * 0.15f);
        pos += Forward() * step;
    }
};

// An entity with a skeleton: its own copy of the skinned mesh, posed on the CPU every frame.
struct AnimatedModel {
    const GpuMesh* mesh = nullptr;
    std::shared_ptr<const MeshData> bind;
    std::shared_ptr<const Skeleton> skel;
    std::shared_ptr<const Animation> anim;   // null: bind pose
    float phase = 0;
    std::vector<Vertex> posed;
    std::string base;   // model base name (diagnostics)
    // combat state (viewer-side until the brains are ported)
    glm::vec3 pos{0};
    int hp = 3;
    std::shared_ptr<const Animation> hitAnim, deathAnim;   // <anims>_trip / <anims>_death
    int mode = 0;        // 0 idle, 1 hit reaction, 2 dead
    float modeTime = 0;
};

void Skin(const MeshData& bind, const std::vector<glm::mat4>& palette, std::vector<Vertex>& out) {
    out = bind.vertices;
    for (size_t i = 0; i < out.size() && i < bind.skin.size(); ++i) {
        const SkinWeights& s = bind.skin[i];
        glm::vec4 p(0), n(0);
        for (int k = 0; k < 4; ++k) {
            if (s.weight[k] <= 0 || s.joint[k] >= palette.size()) continue;
            p += s.weight[k] * (palette[s.joint[k]] * glm::vec4(bind.vertices[i].pos, 1));
            n += s.weight[k] * (palette[s.joint[k]] * glm::vec4(bind.vertices[i].normal, 0));
        }
        out[i].pos = glm::vec3(p);
        out[i].normal = glm::vec3(n);
    }
}

// Drop the batches of holders that `keep` rejects (costume variants, props).
template <class Pred> void KeepHolders(MeshData& m, Pred keep) {
    std::vector<Batch> out;
    for (const Batch& b : m.batches)
        if (b.holder < 0 || b.holder >= int(m.holders.size()) || keep(m.holders[b.holder].name)) out.push_back(b);
    m.batches = std::move(out);
}

// dan.skel holds Dan's default body (dan_*) plus costume variants and props (dad_*, captainshat,
// wooden_leg, spade, TorsoShape1) that the game switches on per level / cutscene.
bool DanHolder(const std::string& n) { return n.rfind("dan_", 0) == 0; }

struct App {
    GameData gd;
    Renderer ren;
    std::vector<std::unique_ptr<GpuMesh>> meshes;
    std::unordered_map<uint32_t, const GpuMesh*> instanceMeshes;   // INST asset hash -> uploaded mesh
    std::unordered_map<std::string, const GpuMesh*> entityMeshes;  // corporeal RES base name -> MESH
    std::unordered_map<const GpuMesh*, size_t> meshTris;
    TemplateDB templates;
    std::vector<Entity> entities;
    std::vector<AnimatedModel> animated;
    bool animate = true;
    // per level caches for skinned models
    struct SkinnedSource { std::shared_ptr<const MeshData> mesh; std::shared_ptr<const Skeleton> skel; };
    std::unordered_map<std::string, SkinnedSource> skinned;
    std::unordered_map<std::string, std::shared_ptr<const Animation>> anims;
    std::vector<DrawItem> items;
    std::vector<Zone> zones;   // LAND zones of the loaded level (lighting, fog)
    std::vector<std::unique_ptr<std::vector<PointLight>>> pointLights;   // LIT0 per sub-zone
    CollisionWorld collision;  // TCD of every sub-zone
    // Sir Daniel (resident assets dan.skel / dan.atbl)
    Player player;
    bool playing = false;
    std::shared_ptr<const MeshData> danBind;
    std::shared_ptr<const Skeleton> danSkel;
    std::vector<std::shared_ptr<const Animation>> danAnimRefs;   // keeps player.anims alive
    const GpuMesh* danMesh = nullptr;
    std::vector<Vertex> danPosed;
    size_t danItem = SIZE_MAX;
    const GpuMesh* swordMesh = nullptr;   // all_sword_short.mesh, carried on joint weapon_r
    size_t swordItem = SIZE_MAX;
    int weaponJoint = -1;
    int group = -1;
    bool verbose = false;
    std::string status;
    glm::vec3 bmin{0}, bmax{0};
    size_t tris = 0;

    void Clear() {
        items.clear();
        zones.clear();
        pointLights.clear();
        collision.Clear();
        playing = false;
        danMesh = nullptr;
        danItem = SIZE_MAX;
        swordMesh = nullptr;
        swordItem = SIZE_MAX;
        instanceMeshes.clear();
        entityMeshes.clear();
        meshTris.clear();
        entities.clear();
        animated.clear();
        skinned.clear();
        anims.clear();
        meshes.clear();
        tris = 0;
        bmin = glm::vec3(1e30f);
        bmax = glm::vec3(-1e30f);
    }
    const GpuMesh* Upload(const MeshData& m, int g) {
        meshes.push_back(ren.Upload(m, gd, g));
        meshTris[meshes.back().get()] = m.indices.size() / 3;
        return meshes.back().get();
    }
    // corporeal RES "x.group" -> the model inside the group: "x.mesh" for static objects,
    // "x.skel" (SKE0 skeleton + skinned MOD0, drawn in bind pose for now) for animated ones; cached per level
    const GpuMesh* EntityMesh(const std::string& base, int g) {
        auto it = entityMeshes.find(base);
        if (it != entityMeshes.end()) return it->second;
        const GpuMesh* gm = nullptr;
        for (const char* ext : {".mesh", ".skel"}) {
            const Asset* a = gd.Find(base + ext, g);
            if (!a) continue;
            MeshData md;
            if (LoadModel(gd.Read(*a), md)) { gm = Upload(md, g); break; }
        }
        return entityMeshes[base] = gm;
    }
    // SKEL asset of a model base name: skeleton + skinned mesh (null when the model is rigid)
    const SkinnedSource* Skinned(const std::string& base, int g) {
        auto it = skinned.find(base);
        if (it != skinned.end()) return it->second.mesh ? &it->second : nullptr;
        SkinnedSource src;
        if (!gd.Find(base + ".mesh", g))
            if (const Asset* a = gd.Find(base + ".skel", g)) {
                auto bytes = gd.Read(*a);
                auto md = std::make_shared<MeshData>();
                auto sk = std::make_shared<Skeleton>();
                if (LoadModel(bytes, *md) && !md->skin.empty() && LoadSkeleton(bytes, *sk)) {
                    src.mesh = md;
                    src.skel = sk;
                }
            }
        auto& slot = skinned[base] = src;
        return slot.mesh ? &slot : nullptr;
    }
    std::shared_ptr<const Animation> LoadAnim(uint32_t hash, int g) {
        const Asset* a = gd.Find(hash, g);
        if (!a || a->type != FourCC("ANIM")) return nullptr;
        auto an = std::make_shared<Animation>();
        if (!LoadAnimation(gd.Read(*a), *an)) return nullptr;
        return an;
    }
    // Idle animation for a model: <base>_wait/_idle/_walk..., else the <base>.atbl entry for state 85 (wait)
    // (ATBL: 8-byte header, then 56-byte records starting with the animation name hash).
    std::shared_ptr<const Animation> IdleAnim(const std::string& base, int g) {
        auto it = anims.find(base);
        if (it != anims.end()) return it->second;
        std::shared_ptr<const Animation> found;
        for (const char* sfx : {"_wait", "_idle", "_idle1", "_wait1", "_stand", "_walk", "_hover", "_float", ""})
            if ((found = LoadAnim(NameHash(base + sfx + ".anim"), g))) break;
        if (!found)
            if (const Asset* t = gd.Find(base + ".atbl", g)) {
                // records: {u32 animHash, u32 state, ...}; state 85 (0x55) is the wait/idle state
                // (zombie_guard_wait.anim), so prefer it, then fall back to any loadable entry
                auto tb = gd.Read(*t);
                for (int pass = 0; pass < 2 && !found; ++pass)
                    for (size_t o = 8; o + 8 <= tb.size() && !found; o += 56) {
                        uint32_t h, state;
                        memcpy(&h, tb.data() + o, 4);
                        memcpy(&state, tb.data() + o + 4, 4);
                        if (h && (pass == 1 || state == 85)) found = LoadAnim(h, g);
                    }
            }
        return anims[base] = found;
    }
    void LoadEntitiesOfGroup(int g, int& placed, int& noModel, int& missing, int& hidden) {
        for (const auto& a : gd.Groups()[g].assets) {
            if (a.kind != 1 || a.type != FourCC("ENTS")) continue;
            size_t first = entities.size();
            LoadEntities(gd.Read(a), &templates, entities);
            for (size_t i = first; i < entities.size(); ++i) {
                const Entity& e = entities[i];
                // HIB=1: hibernating until another entity wakes it (e.g. the gold under a grave)
                if (e.Get("HIB").value_or("0") == "1") { ++hidden; continue; }
                std::string base = e.ResourceBaseName();
                if (base.empty()) { ++noModel; continue; }
                glm::mat4 world = glm::translate(glm::mat4(1.0f), e.Position()) * glm::mat4_cast(e.Rotation());
                const ZoneLighting* light = ZoneLight(e.Position(), e.Position());
                if (const SkinnedSource* src = Skinned(base, g)) {
                    AnimatedModel am;
                    am.bind = src->mesh;
                    am.skel = src->skel;
                    am.anim = IdleAnim(e.AnimBaseName(), g);
                    am.base = base;
                    am.pos = e.Position();
                    std::string ab = e.AnimBaseName();
                    am.hitAnim = LoadAnim(NameHash(ab + "_trip.anim"), g);
                    am.deathAnim = LoadAnim(NameHash(ab + "_death.anim"), g);
                    if (!am.deathAnim) am.deathAnim = LoadAnim(NameHash(ab + "_dead.anim"), g);
                    meshes.push_back(ren.Upload(*am.bind, gd, g, true));
                    am.mesh = meshes.back().get();
                    meshTris[am.mesh] = am.bind->indices.size() / 3;
                    Place(am.mesh, world, meshTris[am.mesh], light, true);
                    animated.push_back(std::move(am));
                    ++placed;
                    continue;
                }
                const GpuMesh* m = EntityMesh(base, g);
                if (!m) { ++missing; continue; }
                Place(m, world, meshTris[m], light, true);
                ++placed;
            }
        }
    }
    const ZoneLighting* ZoneLight(glm::vec3 lo, glm::vec3 hi) const {
        int z = FindZone(zones, lo, hi);
        return z < 0 ? nullptr : &zones[z].light;
    }
    void Place(const GpuMesh* mesh, const glm::mat4& model, size_t triCount, const ZoneLighting* light = nullptr,
               bool object = false, const std::vector<PointLight>* points = nullptr) {
        items.push_back({mesh, model, light, object, points});
        tris += triCount;
        for (int i = 0; i < 8; ++i) {
            glm::vec3 c(i & 1 ? mesh->bmax.x : mesh->bmin.x, i & 2 ? mesh->bmax.y : mesh->bmin.y,
                        i & 4 ? mesh->bmax.z : mesh->bmin.z);
            glm::vec3 w = glm::vec3(model * glm::vec4(c, 1));
            bmin = glm::min(bmin, w);
            bmax = glm::max(bmax, w);
        }
    }
    // Every SUBZ of a group (static geometry + placed instances), loaded with the ported CSubZone code.
    void LoadLevel(int g, bool withInstances, bool withEntities) {
        Clear();
        group = g;
        int ok = 0, bad = 0, placed = 0, missing = 0;
        std::unordered_map<uint32_t, size_t> instTris;
        for (const auto& a : gd.Groups()[g].assets)
            if (a.kind == 1 && a.type == FourCC("LAND")) LoadLand(gd.Read(a), zones);
        if (verbose)
            for (const auto& z : zones)
                printf("zone %s: (%.1f %.1f %.1f) - (%.1f %.1f %.1f) ambient %.2f %.2f %.2f, fog %.0f..%.0f\n", z.name.c_str(),
                       z.bmin.x, z.bmin.y, z.bmin.z, z.bmax.x, z.bmax.y, z.bmax.z, z.light.worldAmbient.x,
                       z.light.worldAmbient.y, z.light.worldAmbient.z, z.light.fogNear, z.light.fogFar);
        for (const auto& a : gd.Groups()[g].assets) {
            if (a.kind != 1 || a.type != FourCC("SUBZ")) continue;
            SubZone sz;
            if (!sz.Load(gd.Read(a), a.hash)) { ++bad; continue; }
            ++ok;
            if (verbose)
                printf("subzone %s: bounds (%.1f %.1f %.1f) - (%.1f %.1f %.1f), %zu instances\n", gd.NameOf(a.hash).c_str(),
                       sz.mesh.bmin.x, sz.mesh.bmin.y, sz.mesh.bmin.z, sz.mesh.bmax.x, sz.mesh.bmax.y, sz.mesh.bmax.z,
                       sz.instances.size());
            const ZoneLighting* light = ZoneLight(sz.mesh.bmin, sz.mesh.bmax);
            // "<subzone>.lit": point lights of this sub-zone
            const std::vector<PointLight>* points = nullptr;
            if (const Asset* la = gd.Find(NameHash(".lit", sz.nameHashPrefix), g)) {
                auto pl = std::make_unique<std::vector<PointLight>>();
                if (LoadLights(gd.Read(*la), *pl) && !pl->empty()) {
                    points = pl.get();
                    if (verbose) printf("  %zu point lights\n", pl->size());
                    pointLights.push_back(std::move(pl));
                }
            }
            Place(Upload(sz.mesh, g), glm::mat4(1.0f), sz.mesh.indices.size() / 3, light, false, points);
            // "<subzone>.tcd": collision triangles
            if (const Asset* ta = gd.Find(NameHash(".tcd", sz.nameHashPrefix), g)) {
                std::vector<CollisionTri> tris;
                if (LoadTCD(gd.Read(*ta), tris)) collision.Add(tris);
            }
            if (!withInstances) continue;
            for (const auto& inst : sz.instances) {
                if (inst.model >= sz.models.size() || sz.models[inst.model].mode == -1) continue;
                uint32_t h = sz.InstanceModelHash(sz.models[inst.model]);
                auto it = instanceMeshes.find(h);
                if (it == instanceMeshes.end()) {
                    const GpuMesh* gm = nullptr;
                    if (const Asset* ia = gd.Find(h, g)) {
                        MeshData md;
                        if (LoadInstanceMesh(gd.Read(*ia), md)) {
                            gm = Upload(md, g);
                            instTris[h] = md.indices.size() / 3;
                        }
                    }
                    it = instanceMeshes.emplace(h, gm).first;
                }
                if (!it->second) { ++missing; continue; }
                Place(it->second, inst.world, instTris[h], light, false, points);
                ++placed;
            }
        }
        collision.Build();
        int ePlaced = 0, eNoModel = 0, eMissing = 0, eHidden = 0;
        if (withEntities) LoadEntitiesOfGroup(g, ePlaced, eNoModel, eMissing, eHidden);
        status = "group " + std::to_string(g) + ": " + std::to_string(ok) + " subzones (" + std::to_string(bad) +
                 " failed), " + std::to_string(placed) + " instances (" + std::to_string(missing) + " missing), " +
                 std::to_string(entities.size()) + " entities (" + std::to_string(ePlaced) + " drawn, " +
                 std::to_string(eHidden) + " hibernating, " + std::to_string(eNoModel) + " no model, " + std::to_string(eMissing) + " mesh not found), " +
                 std::to_string(tris) + " tris, missing textures " + std::to_string(ren.MissingTextures()) +
                 ", collision " + std::to_string(collision.Size()) + " tris";
    }
    // ATBL record {u32 animHash, u32 state, float ?, float variant, ...} (56 bytes after an 8-byte header)
    // `type`: third float of the record (weapon type for attacks), -1 = any
    std::shared_ptr<const Animation> AtblAnim(uint32_t atblHash, uint32_t state, float variant, int g, float type = -1) {
        const Asset* t = gd.Find(atblHash, g);
        if (!t) return nullptr;
        auto tb = gd.Read(*t);
        for (size_t o = 8; o + 56 <= tb.size(); o += 56) {
            uint32_t h, st;
            float v, ty;
            memcpy(&h, tb.data() + o, 4);
            memcpy(&st, tb.data() + o + 4, 4);
            memcpy(&v, tb.data() + o + 12, 4);
            memcpy(&ty, tb.data() + o + 16, 4);
            if (st == state && v == variant && (type < 0 || ty == type)) return LoadAnim(h, g);
        }
        return nullptr;
    }
    int GroupOf(uint32_t hash) const {
        for (const auto& gr : gd.Groups())
            if (gr.byHash.count(hash)) return gr.index;
        return 0;
    }
    bool LoadDan() {
        if (danMesh) return true;
        uint32_t skelHash = NameHash("dan.skel");
        const Asset* a = gd.Find(skelHash);
        if (!a) return false;
        int g = GroupOf(skelHash);
        auto bytes = gd.Read(*a);
        auto md = std::make_shared<MeshData>();
        auto sk = std::make_shared<Skeleton>();
        if (!LoadModel(bytes, *md) || !LoadSkeleton(bytes, *sk)) return false;
        KeepHolders(*md, DanHolder);
        danBind = md;
        danSkel = sk;
        uint32_t atbl = NameHash("dan.atbl");
        danAnimRefs.clear();
        // states / variants: see Core/Player.h
        auto take = [&](uint32_t state, float var, float type = -1) {
            auto a = AtblAnim(atbl, state, var, g, type);
            danAnimRefs.push_back(a);
            return a.get();
        };
        PlayerAnims& pa = player.anims;
        pa.idle = take(4, 0);
        pa.walk = take(0, 0);
        pa.run = take(0, 1);
        pa.sprint = take(0, 2);
        pa.jumpStart = take(64, 0);
        pa.air = take(64, 1);
        pa.land = take(64, 2);
        for (int i = 0; i < 3; ++i) pa.attack[i] = take(28, float(i), 2);
        player.animator.SetSkeleton(danSkel.get());
        meshes.push_back(ren.Upload(*danBind, gd, g, true));
        danMesh = meshes.back().get();
        meshTris[danMesh] = danBind->indices.size() / 3;
        weaponJoint = danSkel->Find(NameHash("weapon_r"));
        if (const Asset* w = gd.Find(NameHash("all_sword_short.mesh"))) {
            MeshData wm;
            if (LoadModel(gd.Read(*w), wm)) {
                swordMesh = Upload(wm, GroupOf(w->hash));
            }
        }
        return true;
    }
    bool StartPlay(glm::vec3 at) {
        if (!LoadDan()) { status = "dan.skel not found"; return false; }
        player.Spawn(collision, at);
        if (danItem == SIZE_MAX) {
            Place(danMesh, glm::mat4(1.0f), meshTris[danMesh], nullptr, true);
            danItem = items.size() - 1;
            if (swordMesh && weaponJoint >= 0) {
                Place(swordMesh, glm::mat4(1.0f), meshTris[swordMesh], nullptr, true);
                swordItem = items.size() - 1;
            }
        }
        playing = true;
        return true;
    }
    void UpdateDan() {
        if (danItem == SIZE_MAX) return;
        std::vector<glm::mat4> pal, world;
        LocalPose pose;
        player.animator.Evaluate(pose);
        PoseToPalette(*danSkel, pose, pal, &world);
        Skin(*danBind, pal, danPosed);
        ren.UpdateVertices(*danMesh, danPosed);
        items[danItem].model = glm::translate(glm::mat4(1.0f), player.pos) *
                               glm::rotate(glm::mat4(1.0f), player.yaw, glm::vec3(0, 1, 0));
        items[danItem].light = ZoneLight(player.pos, player.pos);
        if (swordItem != SIZE_MAX) {
            // joint world = palette * bind world (palette = world * inverse bind)
            items[swordItem].model = items[danItem].model * world[weaponJoint];
            items[swordItem].light = items[danItem].light;
        }
    }

    // A swing that connects hits every living animated entity near the blade: trip, or die on the 3rd hit.
    void ResolvePlayerHit() {
        if (!player.hitFrame) return;
        glm::vec3 c = player.HitCentre();
        for (auto& am : animated) {
            if (am.mode == 2) continue;
            glm::vec3 d = am.pos + glm::vec3(0, 1.0f, 0) - c;
            if (glm::length(d) > player.HitRadius() + 0.5f) continue;
            if (--am.hp <= 0) am.mode = 2;
            else if (am.hitAnim) am.mode = 1;
            am.modeTime = 0;
        }
    }
    // Pose of an animated entity for this frame (idle loop, hit reaction, death held on its last frame).
    const Animation* EntityAnim(AnimatedModel& am, float idleTime, float& t, float dt) {
        am.modeTime += dt;
        if (am.mode == 1 && am.hitAnim && am.modeTime < am.hitAnim->Length()) { t = am.modeTime; return am.hitAnim.get(); }
        if (am.mode == 1) am.mode = 0;
        if (am.mode == 2 && am.deathAnim) {
            t = std::min(am.modeTime, std::max(am.deathAnim->Length() - 1e-3f, 0.0f));
            return am.deathAnim.get();
        }
        t = idleTime + am.phase;
        return animate ? am.anim.get() : nullptr;
    }

    // `anim`: ANIM hash to play on a skinned model (0: bind pose)
    bool LoadModelAsset(const Asset& a, int g, uint32_t anim = 0) {
        Clear();
        group = g;
        auto bytes = gd.Read(a);
        MeshData m;
        if (!LoadModel(bytes, m)) { status = "model: " + m.error; return false; }
        if (a.hash == NameHash("dan.skel")) KeepHolders(m, DanHolder);
        auto sk = std::make_shared<Skeleton>();
        if (!m.skin.empty() && LoadSkeleton(bytes, *sk)) {
            AnimatedModel am;
            am.bind = std::make_shared<MeshData>(m);
            am.skel = sk;
            am.anim = anim ? LoadAnim(anim, g) : nullptr;
            meshes.push_back(ren.Upload(*am.bind, gd, g, true));
            am.mesh = meshes.back().get();
            Place(am.mesh, glm::mat4(1.0f), m.indices.size() / 3);
            animated.push_back(std::move(am));
        } else {
            Place(Upload(m, g), glm::mat4(1.0f), m.indices.size() / 3);
        }
        status = gd.NameOf(a.hash) + ": " + std::to_string(m.fragments) + " fragments, " + std::to_string(tris) + " tris";
        return true;
    }
};

// Third-person camera behind Dan (DAN_TRACKING plugin: VIEWLEN=4.25, Y_OFFSET=2.5), orbiting with the
// mouse and pulled in when level collision is in the way.
void FollowCamera(Camera& cam, const App& app) {
    cam.pitch = std::clamp(cam.pitch, -70.0f, 35.0f);
    glm::vec3 target = app.player.pos + glm::vec3(0, 1.7f, 0);
    glm::vec3 want = target - cam.Forward() * 4.25f + glm::vec3(0, 0.8f, 0);
    float t;
    if (app.collision.Raycast(target, want, t)) want = target + (want - target) * std::max(t - 0.08f, 0.05f);
    cam.pos = want;
    cam.pivot = target;
}

PlayerInput ReadPlayerInput(const Camera& cam, const Uint8* k) {
    PlayerInput in;
    in.move.y = float(k[SDL_SCANCODE_W] || k[SDL_SCANCODE_UP]) - float(k[SDL_SCANCODE_S] || k[SDL_SCANCODE_DOWN]);
    in.move.x = float(k[SDL_SCANCODE_D] || k[SDL_SCANCODE_RIGHT]) - float(k[SDL_SCANCODE_A] || k[SDL_SCANCODE_LEFT]);
    if (glm::length(in.move) > 1) in.move = glm::normalize(in.move);
    glm::vec3 f = cam.Forward();
    in.camYaw = std::atan2(f.x, f.z);
    in.run = k[SDL_SCANCODE_LSHIFT] || k[SDL_SCANCODE_RSHIFT];
    in.sprint = k[SDL_SCANCODE_LCTRL] || k[SDL_SCANCODE_RCTRL] || k[SDL_SCANCODE_LGUI];
    in.jump = k[SDL_SCANCODE_SPACE];
    return in;
}

std::string Arg(int argc, char** argv, const char* key, const char* def = "") {
    for (int i = 1; i + 1 < argc; ++i)
        if (!strcmp(argv[i], key)) return argv[i + 1];
    return def;
}

} // namespace

int main(int argc, char** argv) {
    std::string dataPath = Arg(argc, argv, "--data", "extracted/PSP_GAME/USRDIR/gamedata.bin");
    std::string namesPath = Arg(argc, argv, "--names", "docs/names.tsv");
    std::string shot = Arg(argc, argv, "--shot");
    std::string modelName = Arg(argc, argv, "--model");
    int startGroup = std::stoi(Arg(argc, argv, "--group", "-1"));
    bool withInstances = Arg(argc, argv, "--instances", "1") != "0";
    bool withEntities = Arg(argc, argv, "--entities", "1") != "0";
    std::string templatesPath = Arg(argc, argv, "--templates", "docs/entity_templates.txt");

    App app;
    std::string err;
    if (!app.gd.Open(dataPath, &err)) {
        fprintf(stderr, "%s: %s\n", dataPath.c_str(), err.c_str());
        return 1;
    }
    app.gd.LoadNames(namesPath);
    if (!app.templates.Load(templatesPath) && !app.templates.Load("assets/entity_templates.txt"))
        fprintf(stderr, "no entity templates at %s (run tools/entity_templates.py)\n", templatesPath.c_str());
    app.verbose = !shot.empty();

    // --atbl-report <model>: per ATBL record, the motion the animation implies (no window).
    // Movement comes from the planted (lower) ankle: its velocity in model space is minus the body velocity.
    std::string report = Arg(argc, argv, "--atbl-report");
    if (!report.empty()) {
        const Asset* sa = app.gd.Find(report + ".skel");
        const Asset* ta = app.gd.Find(report + ".atbl");
        if (!sa || !ta) { fprintf(stderr, "no %s.skel / .atbl\n", report.c_str()); return 1; }
        Skeleton sk;
        LoadSkeleton(app.gd.Read(*sa), sk);
        int al = sk.Find(NameHash("ankle_l")), ar = sk.Find(NameHash("ankle_r")), pel = sk.Find(NameHash("pelvis"));
        auto tb = app.gd.Read(*ta);
        for (size_t o = 8; o + 56 <= tb.size(); o += 56) {
            uint32_t h, st;
            float f[12];
            memcpy(&h, tb.data() + o, 4);
            memcpy(&st, tb.data() + o + 4, 4);
            memcpy(f, tb.data() + o + 8, 48);
            const Asset* aa = app.gd.Find(h);
            Animation an;
            if (!aa || !LoadAnimation(app.gd.Read(*aa), an)) { printf("%3u %8.0f %08x  (no anim)\n", st, f[1], h); continue; }
            float len = an.frameTime * float(an.frames - 1);
            const int N = 40;
            glm::vec3 vsum(0);
            float plo = 1e9f, phi = -1e9f, footLo = 1e9f;
            int contacts = 0;
            std::vector<glm::mat4> pal;
            glm::vec3 prevL, prevR;
            for (int i = 0; i <= N; ++i) {
                float t = len * float(i) / N;
                SamplePose(sk, &an, std::min(t, len - 1e-4f), pal);
                auto jp = [&](int j) { return glm::vec3((pal[j] * glm::inverse(sk.invBind[j]))[3]); };
                glm::vec3 L = jp(al), R = jp(ar), P = jp(pel);
                plo = std::min(plo, P.y);
                phi = std::max(phi, P.y);
                footLo = std::min(footLo, std::min(L.y, R.y));
                if (i > 0) {
                    glm::vec3 d = (L.y < R.y ? L - prevL : R - prevR) / (len / N);
                    vsum += glm::vec3(d.x, 0, d.z);
                    ++contacts;
                }
                prevL = L;
                prevR = R;
            }
            glm::vec3 v = contacts ? -vsum / float(contacts) : glm::vec3(0);
            // facing: chest -> eye direction in the first frame
            SamplePose(sk, &an, 0, pal);
            int eye = sk.Find(NameHash("eye")), chest = sk.Find(NameHash("chest"));
            glm::vec3 face(0);
            if (eye >= 0 && chest >= 0)
                face = glm::vec3((pal[eye] * glm::inverse(sk.invBind[eye]))[3] - (pal[chest] * glm::inverse(sk.invBind[chest]))[3]);
            printf("%3u %8.0f %5.0f %08x  %5.2fs %s  move x %5.2f z %5.2f (%.2f m/s)  pelvis y %.2f..%.2f  foot min %.2f  face x %5.2f z %5.2f\n",
                   st, f[1], f[2], h, len, an.Loops() ? "loop" : "once", v.x, v.z, glm::length(v), plo, phi, footLo, face.x, face.z);
        }
        return 0;
    }

    if (SDL_Init(SDL_INIT_VIDEO) != 0) { fprintf(stderr, "SDL: %s\n", SDL_GetError()); return 1; }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    Uint32 flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;
    if (!shot.empty()) flags |= SDL_WINDOW_HIDDEN;
    SDL_Window* win = SDL_CreateWindow("MediEvil Resurrection Viewer", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                       1280, 800, flags);
    SDL_GLContext ctx = SDL_GL_CreateContext(win);
    SDL_GL_SetSwapInterval(1);
    glewExperimental = GL_TRUE;
    glewInit();
    if (!app.ren.Init()) { fprintf(stderr, "renderer init failed\n"); return 1; }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplSDL2_InitForOpenGL(win, ctx);
    ImGui_ImplOpenGL3_Init("#version 410 core");

    Camera cam;
    if (!shot.empty()) {
        cam.yaw = std::stof(Arg(argc, argv, "--yaw", "-90"));
        cam.pitch = std::stof(Arg(argc, argv, "--pitch", "-35"));
    }
    if (!modelName.empty()) {
        const Asset* a = app.gd.Find(modelName);
        if (!a) { fprintf(stderr, "model %s not found\n", modelName.c_str()); return 1; }
        int g = 0;
        for (const auto& gr : app.gd.Groups())
            if (gr.byHash.count(a->hash)) { g = gr.index; break; }
        app.LoadModelAsset(*a, g, uint32_t(std::stoul(Arg(argc, argv, "--anim", "0"), nullptr, 16)));
        cam.Frame(app.bmin, app.bmax);
    } else if (startGroup >= 0) {
        app.LoadLevel(startGroup, withInstances, withEntities);
        cam.Frame(app.bmin, app.bmax);
    }
    // --at x,y,z [--dist d]: aim the shot camera at a point instead of framing everything
    std::string at = Arg(argc, argv, "--at");
    if (!at.empty()) {
        glm::vec3 p;
        if (sscanf(at.c_str(), "%f,%f,%f", &p.x, &p.y, &p.z) == 3) {
            float dist = std::stof(Arg(argc, argv, "--dist", "40"));
            cam.pos = p - cam.Forward() * dist;
            cam.speed = std::max(dist * 0.5f, 2.0f);
        }
    }
    // --play x,y,z [--play-move x,y --play-time s]: spawn Dan there and simulate input before the shot
    std::string playAt = Arg(argc, argv, "--play");
    if (!playAt.empty()) {
        glm::vec3 p;
        if (sscanf(playAt.c_str(), "%f,%f,%f", &p.x, &p.y, &p.z) == 3 && app.StartPlay(p)) {
            glm::vec2 mv(0);
            sscanf(Arg(argc, argv, "--play-move", "0,0").c_str(), "%f,%f", &mv.x, &mv.y);
            float simT = std::stof(Arg(argc, argv, "--play-time", "0"));
            float attackEvery = std::stof(Arg(argc, argv, "--play-attack", "0"));   // seconds between presses
            float jumpAt = std::stof(Arg(argc, argv, "--play-jump", "-1"));
            float nextAttack = 0;
            for (float t = 0; t < simT; t += 1.0f / 60) {
                PlayerInput in;
                in.move = mv;
                std::string gait = Arg(argc, argv, "--play-gait", "walk");
                in.run = gait != "walk";
                in.sprint = gait == "sprint";
                if (attackEvery > 0 && t >= nextAttack) { in.attack = true; nextAttack = t + attackEvery; }
                in.jump = jumpAt >= 0 && t >= jumpAt && t < jumpAt + 0.1f;
                glm::vec3 f = cam.Forward();
                in.camYaw = std::atan2(f.x, f.z);
                app.player.Update(app.collision, in, 1.0f / 60);
                app.ResolvePlayerHit();
                for (auto& am : app.animated) { float tt; app.EntityAnim(am, t, tt, 1.0f / 60); }
            }
            FollowCamera(cam, app);
            printf("dan at %.2f %.2f %.2f, anim time %.2f, ground %d, anim %d\n", app.player.pos.x, app.player.pos.y, app.player.pos.z, app.player.animator.Time(),
                   app.player.onGround, int(app.player.action));
        }
    }
    printf("%s\n", app.status.c_str());

    bool textured = Arg(argc, argv, "--textures", "1") != "0", vcolor = true, wire = Arg(argc, argv, "--wire", "0") != "0",
         running = true;
    bool lighting = Arg(argc, argv, "--lighting", "1") != "0", fog = Arg(argc, argv, "--fog", "1") != "0";
    char filter[64] = "";
    int listGroup = startGroup < 0 ? 0 : startGroup;
    Uint64 last = SDL_GetPerformanceCounter();

    Uint32 lmbDownAt = 0;
    float lmbTravel = 0;
    while (running) {
        bool attackPressed = false;
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            ImGui_ImplSDL2_ProcessEvent(&e);
            if (e.type == SDL_QUIT) running = false;

            bool uiMouse = ImGui::GetIO().WantCaptureMouse;
            bool uiKey = ImGui::GetIO().WantCaptureKeyboard;

            // play mode: J or a short left click (no drag) attacks; dragging still orbits the camera
            if (app.playing && e.type == SDL_KEYDOWN && !uiKey && !e.key.repeat && e.key.keysym.sym == SDLK_j)
                attackPressed = true;
            if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT && !uiMouse) {
                lmbDownAt = SDL_GetTicks();
                lmbTravel = 0;
            }
            if (e.type == SDL_MOUSEMOTION) lmbTravel += std::fabs(float(e.motion.xrel)) + std::fabs(float(e.motion.yrel));
            if (app.playing && e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_LEFT &&
                SDL_GetTicks() - lmbDownAt < 250 && lmbTravel < 8)
                attackPressed = true;
            if (e.type == SDL_MOUSEBUTTONDOWN && !uiMouse) {
                // 1-finger click on trackpad (LMB) or RMB or MMB anywhere in viewport: look around
                if (e.button.button == SDL_BUTTON_LEFT || e.button.button == SDL_BUTTON_RIGHT || e.button.button == SDL_BUTTON_MIDDLE) {
                    SDL_SetRelativeMouseMode(SDL_TRUE);
                }
            } else if (e.type == SDL_MOUSEBUTTONUP) {
                if (SDL_GetRelativeMouseMode()) {
                    Uint32 mstate = SDL_GetMouseState(nullptr, nullptr);
                    if (!(mstate & (SDL_BUTTON_LMASK | SDL_BUTTON_RMASK | SDL_BUTTON_MMASK))) {
                        SDL_SetRelativeMouseMode(SDL_FALSE);
                    }
                }
            } else if (e.type == SDL_MOUSEMOTION) {
                if (SDL_GetRelativeMouseMode()) {
                    bool shift = (SDL_GetModState() & KMOD_SHIFT) != 0;
                    if (shift) {
                        cam.Pan(float(e.motion.xrel), float(e.motion.yrel));
                    } else {
                        // 1-finger drag on trackpad (LMB) or RMB: smooth camera rotation
                        cam.yaw += e.motion.xrel * 0.2f;
                        cam.pitch = std::clamp(cam.pitch - e.motion.yrel * 0.2f, -89.0f, 89.0f);
                        cam.pivot = cam.pos + cam.Forward() * 20.0f;
                    }
                }
            } else if (e.type == SDL_MOUSEWHEEL && !uiMouse) {
                // Trackpad 2-finger scroll or mouse wheel: smooth zoom
                cam.Dolly(float(e.wheel.y) * 1.5f);
            } else if (e.type == SDL_KEYDOWN && !uiKey) {
                if (e.key.keysym.sym == SDLK_f && !app.playing) {
                    cam.Frame(app.bmin, app.bmax);
                }
                if (e.key.keysym.sym == SDLK_p) {
                    if (app.playing) {
                        app.playing = false;
                    } else {
                        // spawn Dan where the camera looks
                        glm::vec3 from = cam.pos, to = cam.pos + cam.Forward() * 300.0f, at = cam.pos + cam.Forward() * 8.0f;
                        float t;
                        if (app.collision.Raycast(from, to, t)) at = from + (to - from) * t;
                        if (app.danItem != SIZE_MAX) at = app.player.pos;   // resume where he stood
                        app.StartPlay(at);
                    }
                }
            }
        }
        Uint64 now = SDL_GetPerformanceCounter();
        float dt = float(now - last) / SDL_GetPerformanceFrequency();
        last = now;
        if (app.playing) {
            PlayerInput in;
            if (!ImGui::GetIO().WantCaptureKeyboard) in = ReadPlayerInput(cam, SDL_GetKeyboardState(nullptr));
            else in.camYaw = std::atan2(cam.Forward().x, cam.Forward().z);
            in.attack = attackPressed;
            app.player.Update(app.collision, in, dt);
            app.ResolvePlayerHit();
            FollowCamera(cam, app);
        } else if (!ImGui::GetIO().WantCaptureKeyboard) {
            const Uint8* k = SDL_GetKeyboardState(nullptr);
            glm::vec3 f = cam.Forward(), r = cam.Right();
            float mult = 1.0f;
            if (k[SDL_SCANCODE_LSHIFT] || k[SDL_SCANCODE_RSHIFT]) mult = 4.0f;
            if (k[SDL_SCANCODE_LCTRL] || k[SDL_SCANCODE_RCTRL]) mult = 0.25f;
            float s = cam.speed * dt * mult;
            if (k[SDL_SCANCODE_W]) cam.pos += f * s;
            if (k[SDL_SCANCODE_S]) cam.pos -= f * s;
            if (k[SDL_SCANCODE_D]) cam.pos += r * s;
            if (k[SDL_SCANCODE_A]) cam.pos -= r * s;
            if (k[SDL_SCANCODE_E] || k[SDL_SCANCODE_SPACE]) cam.pos.y += s;
            if (k[SDL_SCANCODE_Q] || k[SDL_SCANCODE_C]) cam.pos.y -= s;
            cam.pivot = cam.pos + cam.Forward() * 20.0f;
        }

        int fbw, fbh;
        SDL_GL_GetDrawableSize(win, &fbw, &fbh);
        glViewport(0, 0, fbw, fbh);
        glClearColor(0.08f, 0.07f, 0.10f, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        float radius = glm::length(app.bmax - app.bmin) + 100.f;
        glm::mat4 proj = glm::perspective(glm::radians(cam.fov), float(fbw) / std::max(fbh, 1), 0.1f, radius * 4.f);
        glm::mat4 view = glm::lookAt(cam.pos, cam.pos + cam.Forward(), glm::vec3(0, 1, 0));
        float animTime = SDL_GetTicks() / 1000.0f;
        if (!shot.empty()) animTime = std::stof(Arg(argc, argv, "--time", "0.5"));
        app.UpdateDan();
        std::vector<glm::mat4> palette;
        for (auto& am : app.animated) {
            float t;
            const Animation* an = app.EntityAnim(am, animTime, t, dt);
            SamplePose(*am.skel, an, t, palette);
            Skin(*am.bind, palette, am.posed);
            app.ren.UpdateVertices(*am.mesh, am.posed);
            if (app.verbose) {
                glm::vec3 lo(1e30f), hi(-1e30f);
                bool nan = false;
                for (const auto& v : am.posed) {
                    nan |= !std::isfinite(v.pos.x + v.pos.y + v.pos.z);
                    lo = glm::min(lo, v.pos);
                    hi = glm::max(hi, v.pos);
                }
                printf("animated %s: joints %zu, anim %s flags %#x frames %u rot/tran/scal %zu/%zu/%zu, nan %d, "
                       "posed y %.2f..%.2f\n", am.base.c_str(), am.skel->Size(), am.anim ? "yes" : "no", am.anim ? am.anim->flags : 0,
                       am.anim ? am.anim->frames : 0, am.anim ? am.anim->rotJoints.size() : 0,
                       am.anim ? am.anim->tranJoints.size() : 0, am.anim ? am.anim->scalJoints.size() : 0, nan,
                       lo.y, hi.y);
            }
        }
        app.ren.Draw(app.items, view, proj, animTime, textured, vcolor, wire, lighting, fog);

        if (!shot.empty()) {
            std::vector<uint8_t> px(size_t(fbw) * fbh * 4);
            glPixelStorei(GL_PACK_ALIGNMENT, 1);
            glReadPixels(0, 0, fbw, fbh, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
            WritePNG(shot, fbw, fbh, px);
            printf("wrote %s (%dx%d)\n", shot.c_str(), fbw, fbh);
            break;
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();
        ImGui::SetNextWindowSize(ImVec2(380, 600), ImGuiCond_FirstUseEver);
        ImGui::Begin("gamedata.bin");
        ImGui::TextWrapped("%s", app.status.c_str());
        ImGui::Checkbox("Textures", &textured); ImGui::SameLine();
        ImGui::Checkbox("Vertex colour", &vcolor); ImGui::SameLine();
        ImGui::Checkbox("Wire", &wire);
        if (app.collision.Size()) {
            bool play = app.playing;
            if (ImGui::Checkbox("Play as Dan (P)", &play)) {
                if (play) app.StartPlay(app.danItem != SIZE_MAX ? app.player.pos : cam.pos + cam.Forward() * 8.0f);
                else app.playing = false;
            }
            if (app.playing) {
                ImGui::SameLine();
                ImGui::Text("WASD walk, +Shift run, +Shift+Ctrl sprint, Space jump, J / click attack, drag to look | %.1f %.1f %.1f%s", app.player.pos.x,
                            app.player.pos.y, app.player.pos.z, app.player.onGround ? "" : " (air)");
            }
        }
        ImGui::Checkbox("Lighting (LGT1)", &lighting); ImGui::SameLine();
        ImGui::Checkbox("Fog", &fog);
        ImGui::Checkbox("Instances (INS0)", &withInstances); ImGui::SameLine();
        ImGui::Checkbox("Entities (ENTS)", &withEntities); ImGui::SameLine();
        ImGui::Checkbox("Animate", &app.animate);
        ImGui::Text("1 finger trackpad drag = Look | WASD+QE = Move | Scroll = Zoom | F = Frame (spd %.1f)", cam.speed);
        if (ImGui::Button("Frame")) cam.Frame(app.bmin, app.bmax);
        ImGui::Separator();
        const auto& groups = app.gd.Groups();
        auto label = [&](const Group& g) { return std::to_string(g.index) + " " + g.name.substr(0, g.name.find(".group")); };
        if (ImGui::BeginCombo("Group", label(groups[listGroup]).c_str())) {
            for (const auto& g : groups)
                if (ImGui::Selectable(label(g).c_str(), g.index == listGroup)) listGroup = g.index;
            ImGui::EndCombo();
        }
        if (ImGui::Button("Load level geometry (all SUBZ)")) {
            app.LoadLevel(listGroup, withInstances, withEntities);
            cam.Frame(app.bmin, app.bmax);
        }
        ImGui::InputText("Filter models", filter, sizeof filter);
        ImGui::BeginChild("models");
        for (const auto& a : groups[listGroup].assets) {
            if (a.kind != 1 || a.type != FourCC("MESH")) continue;
            std::string n = app.gd.NameOf(a.hash);
            if (filter[0] && n.find(filter) == std::string::npos) continue;
            if (ImGui::Selectable(n.c_str())) {
                app.LoadModelAsset(a, listGroup);
                cam.Frame(app.bmin, app.bmax);
            }
        }
        ImGui::EndChild();
        ImGui::End();
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(win);
    }

    app.meshes.clear();
    app.ren.ClearTextureCache();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    SDL_GL_DeleteContext(ctx);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
