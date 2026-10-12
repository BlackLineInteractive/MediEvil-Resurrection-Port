#include "Viewer/Renderer.h"

#include "Core/Texture.h"

#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cstdio>

namespace med {

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
    for (const auto& b : m.batches) {
        uint8_t a = 255;
        for (uint32_t i = 0; i < b.indexCount; ++i) a = std::min<uint8_t>(a, uint8_t(m.vertices[m.indices[b.firstIndex + i]].color >> 24));
        g->minAlpha.push_back(a);
    }
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

// Mesh holder blend mode: MFL1 >> 3 & 0xFF (holder+8, FUN_002182bc), applied per holder by FUN_00212adc
// through FUN_001ee028 -> sceGuBlendFunc. 0 = blending off. Destination alpha (stencil on the PSP) is
// taken as 1.
static uint8_t BlendMode(uint32_t mfl1) { return uint8_t(mfl1 >> 3); }

static bool SetBlend(uint8_t mode) {
    GLenum eq = GL_FUNC_ADD, src = GL_SRC_ALPHA, dst = GL_ONE_MINUS_SRC_ALPHA;
    switch (mode) {
    case 0x44: break;                                                         // SRC_ALPHA, 1-SRC_ALPHA
    // 0x01 is (1-SRC_ALPHA, SRC_ALPHA) in FUN_001ee028, but it is set on ordinary opaque geometry
    // (mausoleum arches, vertex alpha 255) that would vanish with it; drawn as normal alpha until the
    // alpha source for these holders is understood.
    case 0x01: break;
    case 0x48: dst = GL_ONE; break;                                           // additive
    case 0x58: src = GL_ONE; dst = GL_ONE; break;                             // DST_ALPHA, FIX white
    case 0x54: src = GL_ONE; dst = GL_ZERO; break;                            // DST_ALPHA, 1-DST_ALPHA
    case 0x42: eq = GL_FUNC_REVERSE_SUBTRACT; src = GL_ONE; dst = GL_SRC_ALPHA; break;
    case 0x52: eq = GL_FUNC_REVERSE_SUBTRACT; src = GL_ONE; dst = GL_ONE; break;
    default: return false;
    }
    glBlendEquation(eq);
    glBlendFunc(src, dst);
    return true;
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
            // Blended holders go to the sorted-last pass without depth writes, except alpha blending
            // (0x44 / 0x01) of opaque vertices with cut-out textures (character atlases, arches), which is
            // drawn alpha-tested with depth writes. Models use 0x80 = 1.0 vertex alpha.
            uint8_t mode = BlendMode(b.material);
            size_t bi = size_t(&b - m->batches.data());
            bool vertexAlpha = bi < m->minAlpha.size() && m->minAlpha[bi] < (it.object ? 0x78 : 0xF0);
            bool alphaMode = mode == 0x44 || mode == 0x01;
            bool translucent = mode != 0 && !(alphaMode && !vertexAlpha && !(textured && t && t->translucent));
            if (translucent != translucentPass) continue;
            if (translucent && !SetBlend(mode)) SetBlend(0x44);
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
    glPolygonMode(GL_FRONT_AND_BACK, wireframe ? GL_LINE : GL_FILL);

    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    glUniform1f(uAlphaRef_, 0.25f);
    DrawPass(items, time, textured, false, lighting, fog);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glUniform1f(uAlphaRef_, 0.01f);
    DrawPass(items, time, textured, true, lighting, fog);

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glBindVertexArray(0);
}

} // namespace med
