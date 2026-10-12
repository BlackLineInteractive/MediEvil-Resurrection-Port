#include "Viewer/Renderer.h"

#include "Core/Texture.h"

#include <glm/gtc/type_ptr.hpp>

#include <algorithm>

#ifndef GL_TEXTURE_MAX_ANISOTROPY_EXT
#define GL_TEXTURE_MAX_ANISOTROPY_EXT 0x84FE
#endif

static float s_currentAnisotropy = 4.0f;
#include <cctype>
#include <cstdio>
#include <string>

namespace med {

#if defined(MED_PLATFORM_VITA) || (defined(MED_GLES) && (!defined(__APPLE__) || TARGET_OS_IPHONE))
static const char* kVS = R"(#version 100
precision highp float;
precision mediump int;
attribute vec3 aPos;
attribute vec3 aNrm;
attribute vec2 aUV;
attribute vec4 aCol;
uniform mat4 uViewProj, uView, uModel;
uniform int uLightMode;          // 0: unlit (vertex colour x2), 1: GE lighting, world, 2: GE lighting, model
uniform vec3 uAmbient, uLightCol[2], uLightDir[2];
varying vec2 vUV;
varying vec4 vCol;
varying vec3 vNrm;
varying float vDepth;
void main() {
    vUV = aUV;
    vNrm = mat3(uModel[0].xyz, uModel[1].xyz, uModel[2].xyz) * aNrm;
    vec4 wp = uModel * vec4(aPos, 1.0);
    vDepth = -(uView * wp).z;
    if (uLightMode == 0) {
        vCol = vec4(min(aCol.rgb * 2.0, vec3(2.0)), min(aCol.a * 2.0, 1.0));
    } else {
        vec4 mat = uLightMode == 2 ? min(aCol * 2.0, vec4(1.0)) : aCol;
        vec3 n = normalize(vNrm);
        vec3 c = uAmbient * mat.rgb;
        for (int i = 0; i < 2; ++i) c += uLightCol[i] * mat.rgb * max(dot(n, uLightDir[i]), 0.0);
        vCol = vec4(min(c, vec3(1.0)), mat.a);
    }
    gl_Position = uViewProj * wp;
})";

static const char* kFS = R"(#version 100
precision mediump float;
varying vec2 vUV;
varying vec4 vCol;
varying vec3 vNrm;
varying float vDepth;
uniform sampler2D uTex;
uniform bool uTextured, uVColor;
uniform float uAlphaRef;
uniform bool uFog;
uniform vec3 uFogColor;
uniform float uFogNear, uFogFar;
void main() {
    vec4 t = uTextured ? texture2D(uTex, vUV) : vec4(1.0);
    vec4 c = uVColor ? vCol : vec4(1.0);
    if (!uVColor) c.rgb *= 0.55 + 0.45 * abs(normalize(vNrm).y);
    vec4 o = t * c;
    if (o.a < uAlphaRef) discard;
    if (uFog && uFogFar > uFogNear)
        o.rgb = mix(o.rgb, uFogColor, clamp((vDepth - uFogNear) / (uFogFar - uFogNear), 0.0, 1.0));
    gl_FragColor = o;
})";
#else
static const char* kVS = R"(#version 410 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNrm;
layout(location=2) in vec2 aUV;
layout(location=3) in vec4 aCol;
uniform mat4 uViewProj, uView, uModel;
uniform int uLightMode;          // 0: unlit (vertex colour x2), 1: GE lighting, world, 2: GE lighting, model
uniform vec3 uAmbient, uLightCol[2], uLightDir[2];
out vec2 vUV; out vec4 vCol; out vec3 vNrm; out float vDepth;
void main() {
    vUV = aUV; vNrm = mat3(uModel) * aNrm;
    vec4 wp = uModel * vec4(aPos, 1.0);
    vDepth = -(uView * wp).z;
    if (uLightMode == 0) {
        vCol = vec4(min(aCol.rgb * 2.0, vec3(2.0)), min(aCol.a * 2.0, 1.0));
    } else {
        // sceGuColorMaterial(ambient|diffuse): the vertex colour is the material (Land.h).
        // Models use 0x80 = 1.0 colours (no vertex colour -> 0x80 default), hence the x2 there.
        vec4 mat = uLightMode == 2 ? min(aCol * 2.0, vec4(1.0)) : aCol;
        vec3 n = normalize(vNrm);
        vec3 c = uAmbient * mat.rgb;
        for (int i = 0; i < 2; ++i) c += uLightCol[i] * mat.rgb * max(dot(n, uLightDir[i]), 0.0);
        vCol = vec4(min(c, vec3(1.0)), mat.a);
    }
    gl_Position = uViewProj * wp;
})";

static const char* kFS = R"(#version 410 core
in vec2 vUV; in vec4 vCol; in vec3 vNrm; in float vDepth;
uniform sampler2D uTex; uniform bool uTextured, uVColor; uniform float uAlphaRef;
uniform bool uFog; uniform vec3 uFogColor; uniform float uFogNear, uFogFar;
out vec4 frag;
void main() {
    vec4 t = uTextured ? texture(uTex, vUV) : vec4(1.0);
    vec4 c = uVColor ? vCol : vec4(1.0);
    if (!uVColor) c.rgb *= 0.55 + 0.45 * abs(normalize(vNrm).y);
    vec4 o = t * c;
    if (o.a < uAlphaRef) discard;
    if (uFog && uFogFar > uFogNear)
        o.rgb = mix(o.rgb, uFogColor, clamp((vDepth - uFogNear) / (uFogFar - uFogNear), 0.0, 1.0));
    frag = o;
})";
#endif

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
    if (s_currentAnisotropy > 1.0f) {
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, s_currentAnisotropy);
    }
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
    uView_ = glGetUniformLocation(prog_, "uView");
    uLightMode_ = glGetUniformLocation(prog_, "uLightMode");
    uAmbient_ = glGetUniformLocation(prog_, "uAmbient");
    uLightCol_ = glGetUniformLocation(prog_, "uLightCol");
    uLightDir_ = glGetUniformLocation(prog_, "uLightDir");
    uFog_ = glGetUniformLocation(prog_, "uFog");
    uFogColor_ = glGetUniformLocation(prog_, "uFogColor");
    uFogNear_ = glGetUniformLocation(prog_, "uFogNear");
    uFogFar_ = glGetUniformLocation(prog_, "uFogFar");
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

    if (t) {
        // Tag additive textures based on name (god rays, light cones, flares, fire)
        std::string lower = ref.name;
        for (char& c : lower) c = (char)tolower((unsigned char)c);
        if (lower.find("ray") != std::string::npos || lower.find("beam") != std::string::npos ||
            lower.find("god") != std::string::npos || lower.find("shaft") != std::string::npos ||
            lower.find("sun") != std::string::npos || lower.find("glow") != std::string::npos ||
            lower.find("flame") != std::string::npos || lower.find("fire") != std::string::npos) {
            t->translucent = true;
            t->additive = true;
        }
    } else {
        ++missing_;
    }

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

void Renderer::SetLighting(const DrawItem& it, bool lighting, bool fog) {
    const ZoneLighting* l = it.light;
    glUniform1i(uLightMode_, lighting && l ? (it.object ? 2 : 1) : 0);
    glUniform1i(uFog_, fog && l);
    if (!l) return;
    glUniform3fv(uAmbient_, 1, glm::value_ptr(it.object ? l->objectAmbient : l->worldAmbient));
    glm::vec3 col[2], dir[2];
    for (int i = 0; i < 2; ++i) {
        col[i] = l->lightOn[i] ? l->lightColor[i] : glm::vec3(0);
        dir[i] = l->lightDir[i];
    }
    glUniform3fv(uLightCol_, 2, glm::value_ptr(col[0]));
    glUniform3fv(uLightDir_, 2, glm::value_ptr(dir[0]));
    glUniform3fv(uFogColor_, 1, glm::value_ptr(l->fogColor / 255.0f));
    glUniform1f(uFogNear_, l->fogNear);
    glUniform1f(uFogFar_, l->fogFar);
}

static bool BatchIsTranslucent(const Batch& b, const GpuTexture* t, bool isObject) {
    if (isObject) return false;
    if (t && t->translucent) return true;
    if (b.hasVertexAlpha) return true;
    uint8_t byte8 = uint8_t(b.material >> 3);
    if (byte8 == 0x44 || byte8 == 0x48) return true;
    return false;
}

static bool BatchIsAdditive(const Batch& b, const GpuTexture* t) {
    if (t && t->additive) return true;
    uint8_t byte8 = uint8_t(b.material >> 3);
    if (byte8 == 0x44) return true;
    return false;
}

void Renderer::DrawPass(const std::vector<DrawItem>& items, double time, bool textured, bool translucentPass,
                        bool lighting, bool fog) {
    for (const auto& it : items) {
        const GpuMesh* m = it.mesh;
        glUniformMatrix4fv(uModel_, 1, GL_FALSE, glm::value_ptr(it.model));
        SetLighting(it, lighting, fog);
        glBindVertexArray(m->vao);
        for (const auto& b : m->batches) {
            const GpuTexture* t = b.texture >= 0 && b.texture < int(m->textures.size()) ? m->textures[b.texture] : nullptr;
            bool translucent = BatchIsTranslucent(b, t, it.object);
            if (translucent != translucentPass) continue;

            if (translucentPass) {
                if (BatchIsAdditive(b, t)) {
                    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
                } else {
                    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                }
            }

            GLuint tex = textured && t ? t->At(time) : 0;
            glUniform1i(uTextured_, tex != 0);
            glBindTexture(GL_TEXTURE_2D, tex ? tex : white_);
            glDrawElements(GL_TRIANGLES, GLsizei(b.indexCount), GL_UNSIGNED_INT,
                           reinterpret_cast<void*>(size_t(b.firstIndex) * 4));
        }
    }
}

void Renderer::Draw(const std::vector<DrawItem>& items, const glm::mat4& view, const glm::mat4& proj, double time,
                    bool textured, bool vertexColor, bool wireframe, bool lighting, bool fog) {
    glUseProgram(prog_);
    glm::mat4 viewProj = proj * view;
    glUniformMatrix4fv(uViewProj_, 1, GL_FALSE, glm::value_ptr(viewProj));
    glUniformMatrix4fv(uView_, 1, GL_FALSE, glm::value_ptr(view));
    glUniform1i(uTex_, 0);
    glUniform1i(uVColor_, vertexColor);
    glActiveTexture(GL_TEXTURE0);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
#if !defined(MED_GLES) && !defined(MED_PLATFORM_VITA)
    glPolygonMode(GL_FRONT_AND_BACK, wireframe ? GL_LINE : GL_FILL);
#endif

    // Pass 1: Opaque
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    glUniform1f(uAlphaRef_, 0.25f);
    DrawPass(items, time, textured, false, lighting, fog);

    // Pass 2: Translucent (god rays, windows, water, particles)
    glEnable(GL_BLEND);
    glDepthMask(GL_FALSE);
    glUniform1f(uAlphaRef_, 0.01f);
    DrawPass(items, time, textured, true, lighting, fog);

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
#if !defined(MED_GLES) && !defined(MED_PLATFORM_VITA)
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
#endif
    glBindVertexArray(0);
}

void Renderer::SetAnisotropy(int level) {
    anisotropy_ = level;
    s_currentAnisotropy = float(level);
    for (auto& [name, tex] : texCache_) {
        if (!tex) continue;
        for (GLuint f : tex->frames) {
            if (f) {
                glBindTexture(GL_TEXTURE_2D, f);
                glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, s_currentAnisotropy);
            }
        }
    }
    glBindTexture(GL_TEXTURE_2D, 0);
}

} // namespace med
