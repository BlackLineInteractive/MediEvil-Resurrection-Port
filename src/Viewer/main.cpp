// MediEvil: Resurrection level/model viewer - reads gamedata.bin directly.
//
//   medievil-viewer [--data gamedata.bin] [--names docs/names.tsv]
//                   [--group N] [--model name.mesh] [--shot out.png] [--yaw deg] [--pitch deg]
//
// --shot renders one frame with the camera framing the loaded geometry, writes a PNG and exits.
#include "Core/GameData.h"
#include "Core/Geometry.h"
#include "Core/World.h"
#include "Core/Entity.h"
#include "Core/Land.h"
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
    int group = -1;
    bool verbose = false;
    std::string status;
    glm::vec3 bmin{0}, bmax{0};
    size_t tris = 0;

    void Clear() {
        items.clear();
        zones.clear();
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
        if (const char* f = getenv("MED_MATDBG"))
            for (const auto& b : m.batches)
                if (b.texture >= 0 && (!*f || m.textures[b.texture].name.find(f) != std::string::npos))
                    printf("mat %-36s mfl1 %08x\n", m.textures[b.texture].name.c_str(), b.material);
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
                    if (verbose) {
                        std::vector<glm::mat4> pal;
                        SamplePose(*am.skel, nullptr, 0, pal);
                        float dev = 0;
                        for (const auto& m : pal)
                            for (int c = 0; c < 4; ++c) dev = std::max(dev, glm::length(m[c] - glm::mat4(1.0f)[c]));
                        size_t maxJ = 0;
                        for (const auto& w : am.bind->skin) for (int k = 0; k < 4; ++k) if (w.weight[k] > 0) maxJ = std::max<size_t>(maxJ, w.joint[k]);
                        // vertices far from every joint they are weighted to: wrong joint mapping
                        std::vector<glm::vec3> jp(am.skel->Size());
                        for (size_t j = 0; j < jp.size(); ++j) jp[j] = glm::vec3(glm::inverse(am.skel->invBind[j])[3]);
                        int far = 0, multi = 0;
                        for (size_t v = 0; v < am.bind->skin.size(); ++v) {
                            const auto& w = am.bind->skin[v];
                            float best = 1e9f;
                            int n = 0;
                            for (int k = 0; k < 4; ++k) if (w.weight[k] > 0) { ++n; best = std::min(best, glm::length(am.bind->vertices[v].pos - jp[w.joint[k]])); }
                            far += best > 0.6f;
                            multi += n > 1;
                        }
                        printf("  skin verts %zu far %d multi %d\n", am.bind->skin.size(), far, multi);
                        {
                            const auto& V = am.bind->vertices; const auto& I = am.bind->indices;
                            double lsum = 0; int agree = 0, tot = 0;
                            for (const auto& v : V) lsum += glm::length(v.normal);
                            for (size_t t = 0; t + 2 < I.size(); t += 3) {
                                glm::vec3 fn = glm::cross(V[I[t+1]].pos - V[I[t]].pos, V[I[t+2]].pos - V[I[t]].pos);
                                if (glm::length(fn) < 1e-8f) continue;
                                glm::vec3 vn = V[I[t]].normal + V[I[t+1]].normal + V[I[t+2]].normal;
                                ++tot; agree += glm::dot(fn, vn) > 0;
                            }
                            printf("  normal len avg %.3f, face agree %d/%d\n", lsum / V.size(), agree, tot);
                        }
                        printf("skinned %s at %.1f,%.1f,%.1f joints %zu bind dev %.4f max joint ref %zu\n", base.c_str(), e.Position().x,
                               e.Position().y, e.Position().z, am.skel->Size(), dev, maxJ);
                    }
                    am.phase = float(i % 7) * 0.37f;   // desynchronise identical characters
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
               bool object = false) {
        items.push_back({mesh, model, light, object});
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
            Place(Upload(sz.mesh, g), glm::mat4(1.0f), sz.mesh.indices.size() / 3, light);
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
                Place(it->second, inst.world, instTris[h], light);
                ++placed;
            }
        }
        int ePlaced = 0, eNoModel = 0, eMissing = 0, eHidden = 0;
        if (withEntities) LoadEntitiesOfGroup(g, ePlaced, eNoModel, eMissing, eHidden);
        status = "group " + std::to_string(g) + ": " + std::to_string(ok) + " subzones (" + std::to_string(bad) +
                 " failed), " + std::to_string(placed) + " instances (" + std::to_string(missing) + " missing), " +
                 std::to_string(entities.size()) + " entities (" + std::to_string(ePlaced) + " drawn, " +
                 std::to_string(eHidden) + " hibernating, " + std::to_string(eNoModel) + " no model, " + std::to_string(eMissing) + " mesh not found), " +
                 std::to_string(tris) + " tris, missing textures " + std::to_string(ren.MissingTextures());
    }
    bool LoadModelAsset(const Asset& a, int g) {
        Clear();
        group = g;
        MeshData m;
        if (!LoadModel(gd.Read(a), m)) { status = "model: " + m.error; return false; }
        Place(Upload(m, g), glm::mat4(1.0f), m.indices.size() / 3);
        status = gd.NameOf(a.hash) + ": " + std::to_string(m.fragments) + " fragments, " + std::to_string(tris) + " tris";
        return true;
    }
};

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
        app.LoadModelAsset(*a, g);
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
    printf("%s\n", app.status.c_str());

    bool textured = Arg(argc, argv, "--textures", "1") != "0", vcolor = true, wire = Arg(argc, argv, "--wire", "0") != "0",
         running = true;
    bool lighting = Arg(argc, argv, "--lighting", "1") != "0", fog = Arg(argc, argv, "--fog", "1") != "0";
    char filter[64] = "";
    int listGroup = startGroup < 0 ? 0 : startGroup;
    Uint64 last = SDL_GetPerformanceCounter();

    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            ImGui_ImplSDL2_ProcessEvent(&e);
            if (e.type == SDL_QUIT) running = false;

            bool uiMouse = ImGui::GetIO().WantCaptureMouse;
            bool uiKey = ImGui::GetIO().WantCaptureKeyboard;

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
                if (e.key.keysym.sym == SDLK_f) {
                    cam.Frame(app.bmin, app.bmax);
                }
            }
        }
        Uint64 now = SDL_GetPerformanceCounter();
        float dt = float(now - last) / SDL_GetPerformanceFrequency();
        last = now;
        if (!ImGui::GetIO().WantCaptureKeyboard) {
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
        std::vector<glm::mat4> palette;
        for (auto& am : app.animated) {
            SamplePose(*am.skel, app.animate ? am.anim.get() : nullptr, animTime + am.phase, palette);
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
