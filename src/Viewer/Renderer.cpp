#include "Viewer/Renderer.h"

#include "Core/Texture.h"

#include <glm/gtc/type_ptr.hpp>

#include <cstdio>

namespace med {

static const char* kVS = R"(#version 410 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNrm;
layout(location=2) in vec2 aUV;
layout(location=3) in vec4 aCol;
uniform mat4 uViewProj, uModel;
out vec2 vUV; out vec4 vCol; out vec3 vNrm;
void main() {
    vUV = aUV; vCol = aCol; vNrm = mat3(uModel) * aNrm;
    gl_Position = uViewProj * uModel * vec4(aPos, 1.0);
})";

// PSP vertex colours use 0x80 as 1.0 (baked lighting), hence the x2.
static const char* kFS = R"(#version 410 core
in vec2 vUV; in vec4 vCol; in vec3 vNrm;
uniform sampler2D uTex; uniform bool uTextured, uVColor; uniform float uAlphaRef;
out vec4 frag;
void main() {
    vec4 t = uTextured ? texture(uTex, vUV) : vec4(1.0);
    vec4 c = uVColor ? vec4(min(vCol.rgb * 2.0, vec3(2.0)), min(vCol.a * 2.0, 1.0)) : vec4(1.0);
    if (!uVColor) c.rgb *= 0.55 + 0.45 * abs(normalize(vNrm).y);
    vec4 o = t * c;
    if (o.a < uAlphaRef) discard;
    frag = o;
})";

static GLuint Compile(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetShaderInfoLog(s, sizeof log, nullptr, log);
        fprintf(stderr, "shader: %s\n", log);
    }
    return s;
}

static GLuint UploadImage(const Image& img) {
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, img.width, img.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, img.rgba.data());
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    return tex;
}

GpuMesh::~GpuMesh() {
    if (ibo) glDeleteBuffers(1, &ibo);
    if (vbo) glDeleteBuffers(1, &vbo);
    if (vao) glDeleteVertexArrays(1, &vao);
}

bool Renderer::Init() {
    prog_ = glCreateProgram();
    glAttachShader(prog_, Compile(GL_VERTEX_SHADER, kVS));
    glAttachShader(prog_, Compile(GL_FRAGMENT_SHADER, kFS));
    glLinkProgram(prog_);
    GLint ok;
    glGetProgramiv(prog_, GL_LINK_STATUS, &ok);
    if (!ok) return false;
    uViewProj_ = glGetUniformLocation(prog_, "uViewProj");
    uModel_ = glGetUniformLocation(prog_, "uModel");
    uTextured_ = glGetUniformLocation(prog_, "uTextured");
    uVColor_ = glGetUniformLocation(prog_, "uVColor");
    uTex_ = glGetUniformLocation(prog_, "uTex");
    uAlphaRef_ = glGetUniformLocation(prog_, "uAlphaRef");
    uint32_t px = 0xFFFFFFFF;
    glGenTextures(1, &white_);
    glBindTexture(GL_TEXTURE_2D, white_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &px);
    return true;
}

const GpuTexture* Renderer::Texture(const TextureRef& ref, const GameData& gd, int preferGroup) {
    auto it = texCache_.find(ref.name);
    if (it != texCache_.end()) return it->second.get();
    std::unique_ptr<GpuTexture> t;
    if (const Asset* a = gd.Find(ref.name, preferGroup)) {
        auto bytes = gd.Read(*a);
        if (a->type == FourCC("FLIP")) {
            Flipbook fb;
            if (DecodeFLIP(bytes.data(), bytes.size(), fb)) {
                t = std::make_unique<GpuTexture>();
                t->fps = fb.fps;
                for (const auto& f : fb.frames) {
                    t->frames.push_back(UploadImage(f));
                    t->translucent |= f.translucent;
                }
            }
        } else {
            Image img;
            if (DecodePSM(bytes.data(), bytes.size(), img)) {
                t = std::make_unique<GpuTexture>();
                t->frames.push_back(UploadImage(img));
                t->translucent = img.translucent;
            }
        }
    }
    if (!t) ++missing_;
    const GpuTexture* p = t.get();
    texCache_[ref.name] = std::move(t);
    return p;
}

void Renderer::ClearTextureCache() {
    for (auto& [n, t] : texCache_)
        if (t)
            for (GLuint f : t->frames) glDeleteTextures(1, &f);
    texCache_.clear();
    missing_ = 0;
}

void Renderer::UpdateVertices(const GpuMesh& mesh, const std::vector<Vertex>& vertices) {
    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, vertices.size() * sizeof(Vertex), vertices.data());
}

std::unique_ptr<GpuMesh> Renderer::Upload(const MeshData& m, const GameData& gd, int preferGroup, bool dynamic) {
    auto g = std::make_unique<GpuMesh>();
    glGenVertexArrays(1, &g->vao);
    glBindVertexArray(g->vao);
    glGenBuffers(1, &g->vbo);
    glBindBuffer(GL_ARRAY_BUFFER, g->vbo);
    glBufferData(GL_ARRAY_BUFFER, m.vertices.size() * sizeof(Vertex), m.vertices.data(),
                 dynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
    glGenBuffers(1, &g->ibo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g->ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, m.indices.size() * 4, m.indices.data(), GL_STATIC_DRAW);
    auto attr = [](GLuint i, GLint n, GLenum t, GLboolean norm, size_t off) {
        glEnableVertexAttribArray(i);
        glVertexAttribPointer(i, n, t, norm, sizeof(Vertex), reinterpret_cast<void*>(off));
    };
    attr(0, 3, GL_FLOAT, GL_FALSE, offsetof(Vertex, pos));
    attr(1, 3, GL_FLOAT, GL_FALSE, offsetof(Vertex, normal));
    attr(2, 2, GL_FLOAT, GL_FALSE, offsetof(Vertex, uv));
    attr(3, 4, GL_UNSIGNED_BYTE, GL_TRUE, offsetof(Vertex, color));
    glBindVertexArray(0);
    g->batches = m.batches;
    for (const auto& t : m.textures) g->textures.push_back(Texture(t, gd, preferGroup));
    g->bmin = m.bmin;
    g->bmax = m.bmax;
    return g;
}

void Renderer::DrawPass(const std::vector<DrawItem>& items, double time, bool textured, bool translucentPass) {
    for (const auto& it : items) {
        const GpuMesh* m = it.mesh;
        glUniformMatrix4fv(uModel_, 1, GL_FALSE, glm::value_ptr(it.model));
        glBindVertexArray(m->vao);
        for (const auto& b : m->batches) {
            const GpuTexture* t = b.texture >= 0 && b.texture < int(m->textures.size()) ? m->textures[b.texture] : nullptr;
            // Translucency comes from the texture alpha for now: the GE blend state the game sets per
            // material (MFL1 -> holder flags) is not mapped yet.
            bool translucent = textured && t && t->translucent;
            if (translucent != translucentPass) continue;
            GLuint tex = textured && t ? t->At(time) : 0;
            glUniform1i(uTextured_, tex != 0);
            glBindTexture(GL_TEXTURE_2D, tex ? tex : white_);
            glDrawElements(GL_TRIANGLES, GLsizei(b.indexCount), GL_UNSIGNED_INT,
                           reinterpret_cast<void*>(size_t(b.firstIndex) * 4));
        }
    }
}

void Renderer::Draw(const std::vector<DrawItem>& items, const glm::mat4& viewProj, double time, bool textured,
                    bool vertexColor, bool wireframe) {
    glUseProgram(prog_);
    glUniformMatrix4fv(uViewProj_, 1, GL_FALSE, glm::value_ptr(viewProj));
    glUniform1i(uTex_, 0);
    glUniform1i(uVColor_, vertexColor);
    glActiveTexture(GL_TEXTURE0);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glPolygonMode(GL_FRONT_AND_BACK, wireframe ? GL_LINE : GL_FILL);

    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    glUniform1f(uAlphaRef_, 0.25f);
    DrawPass(items, time, textured, false);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glUniform1f(uAlphaRef_, 0.01f);
    DrawPass(items, time, textured, true);

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glBindVertexArray(0);
}

} // namespace med
