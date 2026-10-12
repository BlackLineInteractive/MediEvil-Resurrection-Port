#pragma once
#include "Core/GameData.h"
#include "Core/Geometry.h"

#include <GL/glew.h>
#include <glm/glm.hpp>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace med {

struct GpuTexture {
    std::vector<GLuint> frames;    // one for PSM, several for a FLIP flipbook
    float fps = 0;
    bool translucent = false;
    GLuint At(double time) const {
        if (frames.empty()) return 0;
        if (frames.size() == 1 || fps <= 0) return frames[0];
        return frames[size_t(time * fps) % frames.size()];
    }
};

struct GpuMesh {
    GLuint vao = 0, vbo = 0, ibo = 0;
    std::vector<Batch> batches;
    std::vector<const GpuTexture*> textures;   // per MeshData::textures entry, nullptr = missing
    glm::vec3 bmin{0}, bmax{0};
    ~GpuMesh();
};

// One placed copy of a mesh (static level geometry uses identity).
struct DrawItem {
    const GpuMesh* mesh = nullptr;
    glm::mat4 model{1.0f};
};

class Renderer {
public:
    bool Init();
    std::unique_ptr<GpuMesh> Upload(const MeshData& m, const GameData& gd, int preferGroup, bool dynamic = false);
    // Replace the vertex data of a mesh uploaded with dynamic = true (same vertex count).
    void UpdateVertices(const GpuMesh& mesh, const std::vector<Vertex>& vertices);
    void Draw(const std::vector<DrawItem>& items, const glm::mat4& viewProj, double time, bool textured,
              bool vertexColor, bool wireframe);
    void ClearTextureCache();
    int MissingTextures() const { return missing_; }

private:
    const GpuTexture* Texture(const TextureRef& ref, const GameData& gd, int preferGroup);
    void DrawPass(const std::vector<DrawItem>& items, double time, bool textured, bool translucentPass);
    GLuint prog_ = 0;
    GLint uViewProj_ = -1, uModel_ = -1, uTextured_ = -1, uVColor_ = -1, uTex_ = -1, uAlphaRef_ = -1;
    GLuint white_ = 0;
    std::unordered_map<std::string, std::unique_ptr<GpuTexture>> texCache_;
    int missing_ = 0;
};

} // namespace med
