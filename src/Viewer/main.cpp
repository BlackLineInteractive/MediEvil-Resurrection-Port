// MediEvil: Resurrection level/model viewer — reads gamedata.bin directly.
//
//   medievil-viewer [--data gamedata.bin] [--names docs/names.tsv]
//                   [--group N] [--model name.mesh] [--shot out.png] [--yaw deg] [--pitch deg]
//
// --shot renders one frame with the camera framing the loaded geometry, writes a PNG and exits.
#include "Core/GameData.h"
#include "Core/Geometry.h"
#include "Viewer/Renderer.h"

#include <GL/glew.h>
#include <SDL.h>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl2.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
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
    float yaw = -90, pitch = -20, fov = 60, speed = 20;
    glm::vec3 Forward() const {
        float y = glm::radians(yaw), p = glm::radians(pitch);
        return {std::cos(p) * std::cos(y), std::sin(p), std::cos(p) * std::sin(y)};
    }
    void Frame(glm::vec3 bmin, glm::vec3 bmax) {
        glm::vec3 c = (bmin + bmax) * 0.5f;
        float r = std::max(glm::length(bmax - bmin) * 0.5f, 1.0f);
        pos = c - Forward() * (r / std::tan(glm::radians(fov) * 0.5f));
        speed = std::max(r * 0.5f, 2.0f);
    }
};

struct App {
    GameData gd;
    Renderer ren;
    std::vector<std::unique_ptr<GpuMesh>> meshes;
    int group = -1;
    std::string status;
    glm::vec3 bmin{0}, bmax{0};
    size_t tris = 0;

    void Clear() {
        meshes.clear();
        tris = 0;
        bmin = glm::vec3(1e30f);
        bmax = glm::vec3(-1e30f);
    }
    void Add(const MeshData& m, int g) {
        meshes.push_back(ren.Upload(m, gd, g));
        tris += m.indices.size() / 3;
        bmin = glm::min(bmin, m.bmin);
        bmax = glm::max(bmax, m.bmax);
    }
    // Every SUBZ of a group: the static level geometry.
    void LoadLevel(int g) {
        Clear();
        group = g;
        int ok = 0, bad = 0;
        for (const auto& a : gd.Groups()[g].assets) {
            if (a.kind != 1 || a.type != FourCC("SUBZ")) continue;
            MeshData m;
            if (LoadSubZone(gd.Read(a), m)) { Add(m, g); ++ok; }
            else ++bad;
        }
        status = "group " + std::to_string(g) + ": " + std::to_string(ok) + " subzones, " + std::to_string(bad) +
                 " failed, " + std::to_string(tris) + " tris, missing textures " + std::to_string(ren.MissingTextures());
    }
    bool LoadModelAsset(const Asset& a, int g) {
        Clear();
        group = g;
        MeshData m;
        if (!LoadModel(gd.Read(a), m)) { status = "model: " + m.error; return false; }
        Add(m, g);
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

    App app;
    std::string err;
    if (!app.gd.Open(dataPath, &err)) {
        fprintf(stderr, "%s: %s\n", dataPath.c_str(), err.c_str());
        return 1;
    }
    app.gd.LoadNames(namesPath);

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
        app.LoadLevel(startGroup);
        cam.Frame(app.bmin, app.bmax);
    }
    printf("%s\n", app.status.c_str());

    bool textured = true, vcolor = true, wire = false, running = true;
    char filter[64] = "";
    int listGroup = startGroup < 0 ? 0 : startGroup;
    Uint64 last = SDL_GetPerformanceCounter();

    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            ImGui_ImplSDL2_ProcessEvent(&e);
            if (e.type == SDL_QUIT) running = false;
            if (e.type == SDL_MOUSEMOTION && (e.motion.state & SDL_BUTTON_RMASK) && !ImGui::GetIO().WantCaptureMouse) {
                cam.yaw += e.motion.xrel * 0.2f;
                cam.pitch = std::clamp(cam.pitch - e.motion.yrel * 0.2f, -89.f, 89.f);
            }
            if (e.type == SDL_MOUSEWHEEL && !ImGui::GetIO().WantCaptureMouse)
                cam.speed = std::max(0.5f, cam.speed * (e.wheel.y > 0 ? 1.25f : 0.8f));
        }
        Uint64 now = SDL_GetPerformanceCounter();
        float dt = float(now - last) / SDL_GetPerformanceFrequency();
        last = now;
        if (!ImGui::GetIO().WantCaptureKeyboard) {
            const Uint8* k = SDL_GetKeyboardState(nullptr);
            glm::vec3 f = cam.Forward(), r = glm::normalize(glm::cross(f, glm::vec3(0, 1, 0)));
            float s = cam.speed * dt * (k[SDL_SCANCODE_LSHIFT] ? 4.f : 1.f);
            if (k[SDL_SCANCODE_W]) cam.pos += f * s;
            if (k[SDL_SCANCODE_S]) cam.pos -= f * s;
            if (k[SDL_SCANCODE_D]) cam.pos += r * s;
            if (k[SDL_SCANCODE_A]) cam.pos -= r * s;
            if (k[SDL_SCANCODE_E]) cam.pos.y += s;
            if (k[SDL_SCANCODE_Q]) cam.pos.y -= s;
        }

        int fbw, fbh;
        SDL_GL_GetDrawableSize(win, &fbw, &fbh);
        glViewport(0, 0, fbw, fbh);
        glClearColor(0.08f, 0.07f, 0.10f, 1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        float radius = glm::length(app.bmax - app.bmin) + 100.f;
        glm::mat4 proj = glm::perspective(glm::radians(cam.fov), float(fbw) / std::max(fbh, 1), 0.1f, radius * 4.f);
        glm::mat4 view = glm::lookAt(cam.pos, cam.pos + cam.Forward(), glm::vec3(0, 1, 0));
        app.ren.Draw(app.meshes, proj * view, textured, vcolor, wire);

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
        ImGui::Text("RMB look, WASD/QE move, Shift fast, wheel speed (%.1f)", cam.speed);
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
            app.LoadLevel(listGroup);
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
