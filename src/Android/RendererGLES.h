#pragma once

#include "Core/GameData.h"
#include "Core/Geometry.h"
#include "Core/Land.h"

// ── Platform-specific OpenGL ES / VitaGL headers ──
#if defined(VITA) || defined(__vita__) || defined(USE_VITAGL)
    #include <vitaGL.h>
#elif defined(__ANDROID__) || defined(USE_GLES2)
    #include <GLES2/gl2.h>
    #include <GLES2/gl2ext.h>
#else
    // Desktop host / testing (GLEW or system OpenGL)
    #include <GL/glew.h>
#endif

#include <glm/glm.hpp>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace med::gles {

struct GpuTextureGLES {
    std::vector<GLuint> frames;    // one for PSM, several for a FLIP flipbook
    float fps = 0;
    bool translucent = false;

    GLuint At(double time) const {
        if (frames.empty()) return 0;
        if (frames.size() == 1 || fps <= 0) return frames[0];
        return frames[size_t(time * fps) % frames.size()];
    }
};

struct GpuMeshGLES {
    GLuint vbo = 0;
    GLuint ibo = 0;
    GLuint vao = 0;  // Optional VAO (supported on VitaGL / GLES2 OES_vertex_array_object)
    std::vector<Batch> batches;
    std::vector<uint8_t> minAlpha;              // per batch: lowest vertex alpha
    std::vector<const GpuTextureGLES*> textures; // per MeshData::textures entry, nullptr = missing
    glm::vec3 bmin{0}, bmax{0};

    ~GpuMeshGLES();
};

// One placed copy of a mesh (static level geometry uses identity).
struct DrawItemGLES {
    const GpuMeshGLES* mesh = nullptr;
    glm::mat4 model{1.0f};
    const ZoneLighting* light = nullptr;   // zone lighting; null: unlit
    bool object = false;                   // model (object ambient) vs sub-zone geometry
    const std::vector<PointLight>* points = nullptr; // sub-zone LIT0 point lights
};

class RendererGLES {
public:
    RendererGLES() = default;
    ~RendererGLES();

    bool Init();
    void Shutdown();

    std::unique_ptr<GpuMeshGLES> Upload(const MeshData& m, const GameData& gd, int preferGroup, bool dynamic = false);
    void UpdateVertices(const GpuMeshGLES& mesh, const std::vector<Vertex>& vertices);

    void Draw(const std::vector<DrawItemGLES>& items, const glm::mat4& view, const glm::mat4& proj, double time,
              bool textured, bool vertexColor, bool wireframe, bool lighting, bool fog);

    void ClearTextureCache();
    int MissingTextures() const { return missing_; }

private:
    const GpuTextureGLES* Texture(const TextureRef& ref, const GameData& gd, int preferGroup);
    void DrawPass(const std::vector<DrawItemGLES>& items, double time, bool textured, bool translucentPass,
                  bool lighting, bool fog);
    void SetLighting(const DrawItemGLES& it, bool lighting, bool fog);
    void BindMeshBuffers(const GpuMeshGLES& mesh);

    GLuint prog_ = 0;
    GLint aPos_ = -1;
    GLint aNrm_ = -1;
    GLint aUV_ = -1;
    GLint aCol_ = -1;

    GLint uViewProj_ = -1;
    GLint uView_ = -1;
    GLint uModel_ = -1;
    GLint uLightMode_ = -1;
    GLint uAmbient_ = -1;
    GLint uLightCol_ = -1;
    GLint uLightDir_ = -1;
    GLint uPointPos_ = -1;
    GLint uPointCol_ = -1;
    GLint uPointAtt_ = -1;
    GLint uPointCount_ = -1;
    GLint uFog_ = -1;
    GLint uFogColor_ = -1;
    GLint uFogNear_ = -1;
    GLint uFogFar_ = -1;
    GLint uTextured_ = -1;
    GLint uVColor_ = -1;
    GLint uTex_ = -1;
    GLint uAlphaRef_ = -1;

    GLuint white_ = 0;
    bool hasVao_ = false;

    std::unordered_map<std::string, std::unique_ptr<GpuTextureGLES>> texCache_;
    int missing_ = 0;
};

} // namespace med::gles
