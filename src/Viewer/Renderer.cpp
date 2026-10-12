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
uniform sampler2D uTex; uniform bool uTextured, uVColor;
out vec4 frag;
void main() {
    vec4 t = uTextured ? texture(uTex, vUV) : vec4(1.0);
    vec4 c = uVColor ? vec4(min(vCol.rgb * 2.0, vec3(2.0)), 1.0) : vec4(1.0);
    if (!uVColor) c.rgb *= 0.55 + 0.45 * abs(normalize(vNrm).y);
    vec4 o = t * c;
    if (o.a < 0.25) discard;
    frag = vec4(o.rgb, 1.0);
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
    uint32_t px = 0xFFFFFFFF;
    glGenTextures(1, &white_);
    glBindTexture(GL_TEXTURE_2D, white_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &px);
    return true;
}

GLuint Renderer::Texture(const std::string& name, const GameData& gd, int preferGroup) {
    auto it = texCache_.find(name);
    if (it != texCache_.end()) return it->second;
    GLuint tex = 0;
    if (const Asset* a = gd.Find(name, preferGroup)) {
        auto bytes = gd.Read(*a);
        Image img;
        if (DecodePSM(bytes.data(), bytes.size(), img)) {
            glGenTextures(1, &tex);
            glBindTexture(GL_TEXTURE_2D, tex);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, img.width, img.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, img.rgba.data());
            glGenerateMipmap(GL_TEXTURE_2D);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        }
    }
    if (!tex) ++missing_;
    texCache_[name] = tex;
    return tex;
}

void Renderer::ClearTextureCache() {
    for (auto& [n, t] : texCache_)
        if (t) glDeleteTextures(1, &t);
    texCache_.clear();
    missing_ = 0;
}

std::unique_ptr<GpuMesh> Renderer::Upload(const MeshData& m, const GameData& gd, int preferGroup) {
    auto g = std::make_unique<GpuMesh>();
    glGenVertexArrays(1, &g->vao);
    glBindVertexArray(g->vao);
    glGenBuffers(1, &g->vbo);
    glBindBuffer(GL_ARRAY_BUFFER, g->vbo);
    glBufferData(GL_ARRAY_BUFFER, m.vertices.size() * sizeof(Vertex), m.vertices.data(), GL_STATIC_DRAW);
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
    for (const auto& n : m.textures) g->textures.push_back(Texture(n, gd, preferGroup));
    g->bmin = m.bmin;
    g->bmax = m.bmax;
    return g;
}

void Renderer::Draw(const std::vector<std::unique_ptr<GpuMesh>>& meshes, const glm::mat4& viewProj, bool textured,
                    bool vertexColor, bool wireframe) {
    glUseProgram(prog_);
    glUniformMatrix4fv(uViewProj_, 1, GL_FALSE, glm::value_ptr(viewProj));
    glUniform1i(uTex_, 0);
    glUniform1i(uVColor_, vertexColor);
    glActiveTexture(GL_TEXTURE0);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glPolygonMode(GL_FRONT_AND_BACK, wireframe ? GL_LINE : GL_FILL);
    for (const auto& m : meshes) {
        glUniformMatrix4fv(uModel_, 1, GL_FALSE, glm::value_ptr(m->model));
        glBindVertexArray(m->vao);
        for (const auto& b : m->batches) {
            GLuint t = b.texture >= 0 && b.texture < int(m->textures.size()) ? m->textures[b.texture] : 0;
            glUniform1i(uTextured_, textured && t);
            glBindTexture(GL_TEXTURE_2D, t ? t : white_);
            glDrawElements(GL_TRIANGLES, GLsizei(b.indexCount), GL_UNSIGNED_INT,
                           reinterpret_cast<void*>(size_t(b.firstIndex) * 4));
        }
    }
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glBindVertexArray(0);
}

} // namespace med
