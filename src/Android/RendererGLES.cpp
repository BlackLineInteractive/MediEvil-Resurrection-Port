#include "Android/RendererGLES.h"
#include "Core/Texture.h"

#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cstdio>

namespace med::gles {

// ── GLSL ES 1.00 Vertex Shader (OpenGL ES 2.0 / VitaGL compatible) ──
static const char* kVS = R"(
precision highp float;

attribute vec3 aPos;
attribute vec3 aNrm;
attribute vec2 aUV;
attribute vec4 aCol;

uniform mat4 uViewProj;
uniform mat4 uView;
uniform mat4 uModel;
uniform int  uLightMode;          // 0: unlit (vertex colour x2), 1: GE lighting, world, 2: GE lighting, model
uniform vec3 uAmbient;
uniform vec3 uLightCol[2];
uniform vec3 uLightDir[2];
uniform int  uPointCount;
uniform vec3 uPointPos[4];
uniform vec3 uPointCol[4];
uniform float uPointAtt[4];

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
        // Models use 0x80 = 1.0 colours (no vertex colour -> 0x80 default), hence x2
        vec4 mat = (uLightMode == 2) ? min(aCol * 2.0, vec4(1.0)) : aCol;
        vec3 n = normalize(vNrm);
        vec3 c = uAmbient * mat.rgb;
        for (int i = 0; i < 2; ++i) {
            c += uLightCol[i] * mat.rgb * max(dot(n, uLightDir[i]), 0.0);
        }
        // GLES 1.00 requires constant loop limits with conditional breaks
        for (int i = 0; i < 4; ++i) {
            if (i >= uPointCount) break;
            vec3 d = uPointPos[i] - wp.xyz;
            float d2 = max(dot(d, d), 1e-6);
            c += uPointCol[i] * mat.rgb * max(dot(n, d * inversesqrt(d2)), 0.0) * min(1.0, 1.0 / (uPointAtt[i] * d2));
        }
        vCol = vec4(min(c, vec3(1.0)), mat.a);
    }
    gl_Position = uViewProj * wp;
}
)";

// ── GLSL ES 1.00 Fragment Shader (OpenGL ES 2.0 / VitaGL compatible) ──
static const char* kFS = R"(
precision mediump float;

varying vec2 vUV;
varying vec4 vCol;
varying vec3 vNrm;
varying float vDepth;

uniform sampler2D uTex;
uniform int uTextured;
uniform int uVColor;
uniform float uAlphaRef;
uniform int uFog;
uniform vec3 uFogColor;
uniform float uFogNear;
uniform float uFogFar;

void main() {
    vec4 t = (uTextured != 0) ? texture2D(uTex, vUV) : vec4(1.0);
    vec4 c = (uVColor != 0) ? vCol : vec4(1.0);
    if (uVColor == 0) {
        c.rgb *= 0.55 + 0.45 * abs(normalize(vNrm).y);
    }
    vec4 o = t * c;
    if (o.a < uAlphaRef) {
        discard;
    }
    if (uFog != 0 && uFogFar > uFogNear) {
        float f = clamp((vDepth - uFogNear) / (uFogFar - uFogNear), 0.0, 1.0);
        o.rgb = mix(o.rgb, uFogColor, f);
    }
    gl_FragColor = o;
}
)";

static GLuint CompileShader(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetShaderInfoLog(s, sizeof(log), nullptr, log);
        fprintf(stderr, "[GLES/VitaGL] shader compile failed:\n%s\n", log);
    }
    return s;
}

// In OpenGL ES 2.0 / VitaGL, internalFormat must equal format (GL_RGBA), not GL_RGBA8
static GLuint UploadImageGLES(const Image& img) {
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, img.width, img.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, img.rgba.data());
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    return tex;
}

GpuMeshGLES::~GpuMeshGLES() {
    if (ibo) glDeleteBuffers(1, &ibo);
    if (vbo) glDeleteBuffers(1, &vbo);
#if defined(GL_OES_vertex_array_object) && !defined(USE_VITAGL)
    if (vao) glDeleteVertexArraysOES(1, &vao);
#endif
}

RendererGLES::~RendererGLES() {
    Shutdown();
}

void RendererGLES::Shutdown() {
    ClearTextureCache();
    if (white_) {
        glDeleteTextures(1, &white_);
        white_ = 0;
    }
    if (prog_) {
        glDeleteProgram(prog_);
        prog_ = 0;
    }
}

bool RendererGLES::Init() {
    prog_ = glCreateProgram();
    GLuint vs = CompileShader(GL_VERTEX_SHADER, kVS);
    GLuint fs = CompileShader(GL_FRAGMENT_SHADER, kFS);
    glAttachShader(prog_, vs);
    glAttachShader(prog_, fs);

    // Bind explicit attribute indices before linking for guaranteed GLES compatibility
    glBindAttribLocation(prog_, 0, "aPos");
    glBindAttribLocation(prog_, 1, "aNrm");
    glBindAttribLocation(prog_, 2, "aUV");
    glBindAttribLocation(prog_, 3, "aCol");

    glLinkProgram(prog_);

    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint ok = 0;
    glGetProgramiv(prog_, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetProgramInfoLog(prog_, sizeof(log), nullptr, log);
        fprintf(stderr, "[GLES/VitaGL] program link failed:\n%s\n", log);
        return false;
    }

    aPos_ = 0;
    aNrm_ = 1;
    aUV_ = 2;
    aCol_ = 3;

    uViewProj_   = glGetUniformLocation(prog_, "uViewProj");
    uModel_      = glGetUniformLocation(prog_, "uModel");
    uView_       = glGetUniformLocation(prog_, "uView");
    uLightMode_  = glGetUniformLocation(prog_, "uLightMode");
    uAmbient_    = glGetUniformLocation(prog_, "uAmbient");
    uLightCol_   = glGetUniformLocation(prog_, "uLightCol");
    uLightDir_   = glGetUniformLocation(prog_, "uLightDir");
    uFog_        = glGetUniformLocation(prog_, "uFog");
    uPointPos_   = glGetUniformLocation(prog_, "uPointPos");
    uPointCol_   = glGetUniformLocation(prog_, "uPointCol");
    uPointAtt_   = glGetUniformLocation(prog_, "uPointAtt");
    uPointCount_ = glGetUniformLocation(prog_, "uPointCount");
    uFogColor_   = glGetUniformLocation(prog_, "uFogColor");
    uFogNear_    = glGetUniformLocation(prog_, "uFogNear");
    uFogFar_     = glGetUniformLocation(prog_, "uFogFar");
    uTextured_   = glGetUniformLocation(prog_, "uTextured");
    uVColor_     = glGetUniformLocation(prog_, "uVColor");
    uTex_        = glGetUniformLocation(prog_, "uTex");
    uAlphaRef_   = glGetUniformLocation(prog_, "uAlphaRef");

    // 1x1 white texture for untextured rendering
    uint32_t px = 0xFFFFFFFF;
    glGenTextures(1, &white_);
    glBindTexture(GL_TEXTURE_2D, white_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &px);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    return true;
}

const GpuTextureGLES* RendererGLES::Texture(const TextureRef& ref, const GameData& gd, int preferGroup) {
    auto it = texCache_.find(ref.name);
    if (it != texCache_.end()) return it->second.get();

    std::unique_ptr<GpuTextureGLES> t;
    if (const Asset* a = gd.Find(ref.name, preferGroup)) {
        auto bytes = gd.Read(*a);
        if (a->type == FourCC("FLIP")) {
            Flipbook fb;
            if (DecodeFLIP(bytes.data(), bytes.size(), fb)) {
                t = std::make_unique<GpuTextureGLES>();
                t->fps = fb.fps;
                for (const auto& f : fb.frames) {
                    t->frames.push_back(UploadImageGLES(f));
                    t->translucent |= f.translucent;
                }
            }
        } else {
            Image img;
            if (DecodePSM(bytes.data(), bytes.size(), img)) {
                t = std::make_unique<GpuTextureGLES>();
                t->frames.push_back(UploadImageGLES(img));
                t->translucent = img.translucent;
            }
        }
    }

    if (!t) ++missing_;
    const GpuTextureGLES* p = t.get();
    texCache_[ref.name] = std::move(t);
    return p;
}

void RendererGLES::ClearTextureCache() {
    for (auto& [n, t] : texCache_) {
        if (t) {
            for (GLuint f : t->frames) {
                glDeleteTextures(1, &f);
            }
        }
    }
    texCache_.clear();
    missing_ = 0;
}

void RendererGLES::UpdateVertices(const GpuMeshGLES& mesh, const std::vector<Vertex>& vertices) {
    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, vertices.size() * sizeof(Vertex), vertices.data());
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

std::unique_ptr<GpuMeshGLES> RendererGLES::Upload(const MeshData& m, const GameData& gd, int preferGroup, bool dynamic) {
    auto g = std::make_unique<GpuMeshGLES>();

    glGenBuffers(1, &g->vbo);
    glBindBuffer(GL_ARRAY_BUFFER, g->vbo);
    glBufferData(GL_ARRAY_BUFFER, m.vertices.size() * sizeof(Vertex), m.vertices.data(),
                 dynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);

    glGenBuffers(1, &g->ibo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g->ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, m.indices.size() * sizeof(uint32_t), m.indices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    g->batches = m.batches;
    for (const auto& b : m.batches) {
        uint8_t a = 255;
        for (uint32_t i = 0; i < b.indexCount; ++i) {
            a = std::min<uint8_t>(a, uint8_t(m.vertices[m.indices[b.firstIndex + i]].color >> 24));
        }
        g->minAlpha.push_back(a);
    }

    for (const auto& t : m.textures) {
        g->textures.push_back(Texture(t, gd, preferGroup));
    }

    g->bmin = m.bmin;
    g->bmax = m.bmax;
    return g;
}

void RendererGLES::BindMeshBuffers(const GpuMeshGLES& mesh) {
    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.ibo);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, pos)));

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, normal)));

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, uv)));

    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, color)));
}

void RendererGLES::SetLighting(const DrawItemGLES& it, bool lighting, bool fog) {
    const ZoneLighting* l = it.light;
    glUniform1i(uLightMode_, lighting && l ? (it.object ? 2 : 1) : 0);
    glUniform1i(uFog_, (fog && l) ? 1 : 0);
    if (!l) return;

    glUniform3fv(uAmbient_, 1, glm::value_ptr(it.object ? l->objectAmbient : l->worldAmbient));

    glm::vec3 col[2], dir[2];
    for (int i = 0; i < 2; ++i) {
        col[i] = l->lightOn[i] ? l->lightColor[i] : glm::vec3(0.0f);
        dir[i] = l->lightDir[i];
    }
    glUniform3fv(uLightCol_, 2, glm::value_ptr(col[0]));

    int nDir = int(l->lightOn[0]) + int(l->lightOn[1]);
    int np = 0;
    glm::vec3 ppos[4], pcol[4];
    float patt[4] = {1.0f, 1.0f, 1.0f, 1.0f};

    if (it.points && !it.object) {
        for (const PointLight& p : *it.points) {
            if (np >= 4 - nDir) break;
            ppos[np] = p.pos;
            pcol[np] = p.color;
            float r2 = p.radius * p.radius;
            patt[np] = (r2 > 0.0f) ? (1.0f / (r2 * r2)) : 1.0f;
            ++np;
        }
    }

    glUniform1i(uPointCount_, np);
    if (np > 0) {
        glUniform3fv(uPointPos_, np, glm::value_ptr(ppos[0]));
        glUniform3fv(uPointCol_, np, glm::value_ptr(pcol[0]));
        glUniform1fv(uPointAtt_, np, patt);
    }
    glUniform3fv(uLightDir_, 2, glm::value_ptr(dir[0]));
    glUniform3fv(uFogColor_, 1, glm::value_ptr(l->fogColor / 255.0f));
    glUniform1f(uFogNear_, l->fogNear);
    glUniform1f(uFogFar_, l->fogFar);
}

static uint8_t BlendMode(uint32_t mfl1) {
    return uint8_t(mfl1 >> 3);
}

static bool SetBlendGLES(uint8_t mode) {
    GLenum eq = GL_FUNC_ADD;
    GLenum src = GL_SRC_ALPHA;
    GLenum dst = GL_ONE_MINUS_SRC_ALPHA;

    switch (mode) {
    case 0x44: break; // SRC_ALPHA, 1-SRC_ALPHA
    case 0x01: break;
    case 0x48: dst = GL_ONE; break; // additive
    case 0x58: src = GL_ONE; dst = GL_ONE; break;
    case 0x54: src = GL_ONE; dst = GL_ZERO; break;
    case 0x42: eq = GL_FUNC_REVERSE_SUBTRACT; src = GL_ONE; dst = GL_SRC_ALPHA; break;
    case 0x52: eq = GL_FUNC_REVERSE_SUBTRACT; src = GL_ONE; dst = GL_ONE; break;
    default: return false;
    }

    glBlendEquation(eq);
    glBlendFunc(src, dst);
    return true;
}

void RendererGLES::DrawPass(const std::vector<DrawItemGLES>& items, double time, bool textured,
                            bool translucentPass, bool lighting, bool fog) {
    for (const auto& it : items) {
        const GpuMeshGLES* m = it.mesh;
        if (!m) continue;

        glUniformMatrix4fv(uModel_, 1, GL_FALSE, glm::value_ptr(it.model));
        SetLighting(it, lighting, fog);
        BindMeshBuffers(*m);

        for (const auto& b : m->batches) {
            const GpuTextureGLES* t = (b.texture >= 0 && b.texture < int(m->textures.size()))
                                          ? m->textures[b.texture]
                                          : nullptr;

            uint8_t mode = BlendMode(b.material);
            size_t bi = size_t(&b - m->batches.data());
            bool vertexAlpha = bi < m->minAlpha.size() && m->minAlpha[bi] < (it.object ? 0x78 : 0xF0);
            bool alphaMode = (mode == 0x44 || mode == 0x01);
            bool translucent = (mode != 0) && !(alphaMode && !vertexAlpha && !(textured && t && t->translucent));

            if (translucent != translucentPass) continue;

            if (translucent && !SetBlendGLES(mode)) {
                SetBlendGLES(0x44);
            }

            GLuint tex = (textured && t) ? t->At(time) : 0;
            glUniform1i(uTextured_, tex != 0 ? 1 : 0);
            glBindTexture(GL_TEXTURE_2D, tex ? tex : white_);

            glDrawElements(GL_TRIANGLES, GLsizei(b.indexCount), GL_UNSIGNED_INT,
                           reinterpret_cast<void*>(size_t(b.firstIndex) * sizeof(uint32_t)));
        }
    }
}

void RendererGLES::Draw(const std::vector<DrawItemGLES>& items, const glm::mat4& view, const glm::mat4& proj,
                        double time, bool textured, bool vertexColor, bool wireframe, bool lighting, bool fog) {
    (void)wireframe; // GLES 2.0 / VitaGL does not support glPolygonMode(GL_LINE)

    glUseProgram(prog_);
    glm::mat4 viewProj = proj * view;
    glUniformMatrix4fv(uViewProj_, 1, GL_FALSE, glm::value_ptr(viewProj));
    glUniformMatrix4fv(uView_, 1, GL_FALSE, glm::value_ptr(view));
    glUniform1i(uTex_, 0);
    glUniform1i(uVColor_, vertexColor ? 1 : 0);

    glActiveTexture(GL_TEXTURE0);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    // Pass 1: Opaque / Cutout
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    glUniform1f(uAlphaRef_, 0.25f);
    DrawPass(items, time, textured, false, lighting, fog);

    // Pass 2: Translucent sorted pass
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glUniform1f(uAlphaRef_, 0.01f);
    DrawPass(items, time, textured, true, lighting, fog);

    // Cleanup state
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

} // namespace med::gles
