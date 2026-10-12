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

struct GpuMesh {
    GLuint vao = 0, vbo = 0, ibo = 0;
    std::vector<Batch> batches;
    std::vector<GLuint> textures;   // per MeshData::textures entry, 0 = missing
    glm::mat4 model{1.0f};
    glm::vec3 bmin{0}, bmax{0};
    ~GpuMesh();
};

class Renderer {
public:
    bool Init();
    std::unique_ptr<GpuMesh> Upload(const MeshData& m, const GameData& gd, int preferGroup);
    void Draw(const std::vector<std::unique_ptr<GpuMesh>>& meshes, const glm::mat4& viewProj, bool textured,
              bool vertexColor, bool wireframe);
    void ClearTextureCache();
    size_t TextureCount() const { return texCache_.size(); }
    int MissingTextures() const { return missing_; }

private:
    GLuint Texture(const std::string& name, const GameData& gd, int preferGroup);
    GLuint prog_ = 0;
    GLint uViewProj_ = -1, uModel_ = -1, uTextured_ = -1, uVColor_ = -1, uTex_ = -1;
    GLuint white_ = 0;
    std::unordered_map<std::string, GLuint> texCache_;
    int missing_ = 0;
};

} // namespace med
