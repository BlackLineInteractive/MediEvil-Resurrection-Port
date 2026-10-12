#include "Viewer/OverlayLayer.h"
#include "Viewer/Renderer.h"
#include <imgui.h>
#include <SDL2/SDL.h>
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace med {

// ─────────────────────────────────────────────────────────────────────────────
// TouchLayout implementation
// ─────────────────────────────────────────────────────────────────────────────

TouchLayout::TouchLayout() {
    SetErgonomicPreset();
}

void TouchLayout::SetErgonomicPreset() {
    buttons.clear();
    // Right thumb action arc (2-thumb ergonomic layout)
    buttons.push_back({"Cross",    "X",      PSP_CTRL_CROSS,    0.84f, 0.82f, 36.0f, true,  0.0f, 0.0f});
    buttons.push_back({"Square",   "[ ]",    PSP_CTRL_SQUARE,   0.73f, 0.72f, 34.0f, true,  0.0f, 0.0f});
    buttons.push_back({"RTrigger", "R (Def)",PSP_CTRL_RTRIGGER, 0.84f, 0.64f, 28.0f, false, 72.0f, 40.0f});
    buttons.push_back({"Circle",   "O",      PSP_CTRL_CIRCLE,   0.93f, 0.72f, 32.0f, true,  0.0f, 0.0f});
    buttons.push_back({"Triangle", "/\\",    PSP_CTRL_TRIANGLE, 0.84f, 0.48f, 32.0f, true,  0.0f, 0.0f});

    // Left thumb movement & camera arc
    stickX = 0.15f;
    stickY = 0.75f;
    stickRadius = 65.0f;

    buttons.push_back({"LTrigger", "L (Cam)",PSP_CTRL_LTRIGGER, 0.15f, 0.52f, 28.0f, false, 72.0f, 40.0f});

    // D-Pad for inventory/weapon quick toggle (next to stick)
    buttons.push_back({"Up",       "^",      PSP_CTRL_UP,       0.30f, 0.65f, 22.0f, true,  0.0f, 0.0f});
    buttons.push_back({"Down",     "v",      PSP_CTRL_DOWN,     0.30f, 0.83f, 22.0f, true,  0.0f, 0.0f});
    buttons.push_back({"Left",     "<",      PSP_CTRL_LEFT,     0.24f, 0.74f, 22.0f, true,  0.0f, 0.0f});
    buttons.push_back({"Right",    ">",      PSP_CTRL_RIGHT,    0.36f, 0.74f, 22.0f, true,  0.0f, 0.0f});

    // System buttons at bottom center
    buttons.push_back({"Select",   "SELECT", PSP_CTRL_SELECT,   0.43f, 0.94f, 16.0f, false, 64.0f, 26.0f});
    buttons.push_back({"Start",    "START",  PSP_CTRL_START,    0.57f, 0.94f, 16.0f, false, 64.0f, 26.0f});
}

void TouchLayout::SetClassicPreset() {
    buttons.clear();
    // Shoulder buttons at top edges (PPSSPP-like)
    buttons.push_back({"LTrigger", "L",      PSP_CTRL_LTRIGGER, 0.08f, 0.10f, 28.0f, false, 80.0f, 44.0f});
    buttons.push_back({"RTrigger", "R",      PSP_CTRL_RTRIGGER, 0.92f, 0.10f, 28.0f, false, 80.0f, 44.0f});

    // Left side: D-pad in upper-mid, Analog stick at bottom
    buttons.push_back({"Up",       "^",      PSP_CTRL_UP,       0.12f, 0.40f, 26.0f, true,  0.0f, 0.0f});
    buttons.push_back({"Down",     "v",      PSP_CTRL_DOWN,     0.12f, 0.58f, 26.0f, true,  0.0f, 0.0f});
    buttons.push_back({"Left",     "<",      PSP_CTRL_LEFT,     0.06f, 0.49f, 26.0f, true,  0.0f, 0.0f});
    buttons.push_back({"Right",    ">",      PSP_CTRL_RIGHT,    0.18f, 0.49f, 26.0f, true,  0.0f, 0.0f});

    stickX = 0.12f;
    stickY = 0.78f;
    stickRadius = 65.0f;

    // Right side: Classic PlayStation diamond
    buttons.push_back({"Triangle", "/\\",    PSP_CTRL_TRIANGLE, 0.88f, 0.52f, 32.0f, true,  0.0f, 0.0f});
    buttons.push_back({"Square",   "[ ]",    PSP_CTRL_SQUARE,   0.80f, 0.65f, 32.0f, true,  0.0f, 0.0f});
    buttons.push_back({"Circle",   "O",      PSP_CTRL_CIRCLE,   0.96f, 0.65f, 32.0f, true,  0.0f, 0.0f});
    buttons.push_back({"Cross",    "X",      PSP_CTRL_CROSS,    0.88f, 0.78f, 34.0f, true,  0.0f, 0.0f});

    buttons.push_back({"Select",   "SELECT", PSP_CTRL_SELECT,   0.43f, 0.94f, 16.0f, false, 64.0f, 26.0f});
    buttons.push_back({"Start",    "START",  PSP_CTRL_START,    0.57f, 0.94f, 16.0f, false, 64.0f, 26.0f});
}

// ─────────────────────────────────────────────────────────────────────────────
// PerfMetrics implementation
// ─────────────────────────────────────────────────────────────────────────────

void PerfMetrics::BeginFrame() {
    tFrameStart = SDL_GetPerformanceCounter();
}

void PerfMetrics::MarkCpuDone() {
    tCpuDone = SDL_GetPerformanceCounter();
}

void PerfMetrics::MarkGpuDone() {
    tGpuDone = SDL_GetPerformanceCounter();
}

void PerfMetrics::EndFrame() {
    uint64_t freq = SDL_GetPerformanceFrequency();
    float curCpuMs = float(tCpuDone - tFrameStart) * 1000.0f / float(freq);
    float curGpuMs = float(tGpuDone - tCpuDone) * 1000.0f / float(freq);

    // Target frame time for 60 fps is 16.666 ms
    const float targetMs = 16.6667f;
    float curCpuPct = std::min(100.0f, (curCpuMs / targetMs) * 100.0f);
    float curGpuPct = std::min(100.0f, (curGpuMs / targetMs) * 100.0f);

    // Exponential moving average for smooth display
    cpuMs = cpuMs * 0.90f + curCpuMs * 0.10f;
    cpuPct = cpuPct * 0.90f + curCpuPct * 0.10f;
    gpuMs = gpuMs * 0.90f + curGpuMs * 0.10f;
    gpuPct = gpuPct * 0.90f + curGpuPct * 0.10f;

    frameCounter++;
    float totalDt = float(tGpuDone - tFrameStart) / float(freq);
    accumSeconds += totalDt;
    if (accumSeconds >= 0.5f) {
        fps = float(frameCounter) / accumSeconds;
        frameCounter = 0;
        accumSeconds = 0.0f;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// PostProcessor implementation (FBO + PPSSPP Shader Filters)
// ─────────────────────────────────────────────────────────────────────────────

static const char* kPostVS =
#if defined(MED_PLATFORM_VITA) || (defined(MED_GLES) && (!defined(__APPLE__) || TARGET_OS_IPHONE))
R"(#version 100
attribute vec2 aPos;
attribute vec2 aUV;
varying vec2 vUV;
void main() {
    vUV = aUV;
    gl_Position = vec4(aPos, 0.0, 1.0);
})";
#else
R"(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;
out vec2 vUV;
void main() {
    vUV = aUV;
    gl_Position = vec4(aPos, 0.0, 1.0);
})";
#endif

// Shader bodies (GLSL ES 1.0 / GL 3.3 compatible)
static const char* kFilterNoneFS =
#if defined(MED_PLATFORM_VITA) || (defined(MED_GLES) && (!defined(__APPLE__) || TARGET_OS_IPHONE))
R"(#version 100
precision mediump float;
varying vec2 vUV;
uniform sampler2D uTex;
void main() {
    gl_FragColor = texture2D(uTex, vUV);
})";
#else
R"(#version 330 core
in vec2 vUV;
uniform sampler2D uTex;
out vec4 fragColor;
void main() {
    fragColor = texture(uTex, vUV);
})";
#endif

static const char* kFilterFXAA_FS =
#if defined(MED_PLATFORM_VITA) || (defined(MED_GLES) && (!defined(__APPLE__) || TARGET_OS_IPHONE))
R"(#version 100
precision mediump float;
varying vec2 vUV;
uniform sampler2D uTex;
uniform vec2 uResolution;
void main() {
    vec2 rcpFrame = vec2(1.0) / uResolution;
    vec3 rgbNW = texture2D(uTex, vUV + vec2(-0.5, -0.5) * rcpFrame).xyz;
    vec3 rgbNE = texture2D(uTex, vUV + vec2( 0.5, -0.5) * rcpFrame).xyz;
    vec3 rgbSW = texture2D(uTex, vUV + vec2(-0.5,  0.5) * rcpFrame).xyz;
    vec3 rgbSE = texture2D(uTex, vUV + vec2( 0.5,  0.5) * rcpFrame).xyz;
    vec3 rgbM  = texture2D(uTex, vUV).xyz;
    vec3 luma = vec3(0.299, 0.587, 0.114);
    float lumaNW = dot(rgbNW, luma);
    float lumaNE = dot(rgbNE, luma);
    float lumaSW = dot(rgbSW, luma);
    float lumaSE = dot(rgbSE, luma);
    float lumaM  = dot(rgbM,  luma);
    float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
    float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));
    vec2 dir;
    dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
    dir.y =  ((lumaNW + lumaSW) - (lumaNE + lumaSE));
    float dirReduce = max((lumaNW + lumaNE + lumaSW + lumaSE) * (0.25 * 0.125), 0.0078125);
    float rcpDirMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);
    dir = min(vec2(8.0), max(vec2(-8.0), dir * rcpDirMin)) * rcpFrame;
    vec3 rgbA = 0.5 * (
        texture2D(uTex, vUV + dir * (1.0 / 3.0 - 0.5)).xyz +
        texture2D(uTex, vUV + dir * (2.0 / 3.0 - 0.5)).xyz);
    vec3 rgbB = rgbA * 0.5 + 0.25 * (
        texture2D(uTex, vUV + dir * -0.5).xyz +
        texture2D(uTex, vUV + dir *  0.5).xyz);
    float lumaB = dot(rgbB, luma);
    if (lumaB < lumaMin || lumaB > lumaMax) {
        gl_FragColor = vec4(rgbA, 1.0);
    } else {
        gl_FragColor = vec4(rgbB, 1.0);
    }
})";
#else
R"(#version 330 core
in vec2 vUV;
uniform sampler2D uTex;
uniform vec2 uResolution;
out vec4 fragColor;
void main() {
    vec2 rcpFrame = vec2(1.0) / uResolution;
    vec3 rgbNW = texture(uTex, vUV + vec2(-0.5, -0.5) * rcpFrame).xyz;
    vec3 rgbNE = texture(uTex, vUV + vec2( 0.5, -0.5) * rcpFrame).xyz;
    vec3 rgbSW = texture(uTex, vUV + vec2(-0.5,  0.5) * rcpFrame).xyz;
    vec3 rgbSE = texture(uTex, vUV + vec2( 0.5,  0.5) * rcpFrame).xyz;
    vec3 rgbM  = texture(uTex, vUV).xyz;
    vec3 luma = vec3(0.299, 0.587, 0.114);
    float lumaNW = dot(rgbNW, luma);
    float lumaNE = dot(rgbNE, luma);
    float lumaSW = dot(rgbSW, luma);
    float lumaSE = dot(rgbSE, luma);
    float lumaM  = dot(rgbM,  luma);
    float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
    float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));
    vec2 dir;
    dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
    dir.y =  ((lumaNW + lumaSW) - (lumaNE + lumaSE));
    float dirReduce = max((lumaNW + lumaNE + lumaSW + lumaSE) * (0.25 * 0.125), 0.0078125);
    float rcpDirMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);
    dir = min(vec2(8.0), max(vec2(-8.0), dir * rcpDirMin)) * rcpFrame;
    vec3 rgbA = 0.5 * (
        texture(uTex, vUV + dir * (1.0 / 3.0 - 0.5)).xyz +
        texture(uTex, vUV + dir * (2.0 / 3.0 - 0.5)).xyz);
    vec3 rgbB = rgbA * 0.5 + 0.25 * (
        texture(uTex, vUV + dir * -0.5).xyz +
        texture(uTex, vUV + dir *  0.5).xyz);
    float lumaB = dot(rgbB, luma);
    if (lumaB < lumaMin || lumaB > lumaMax) {
        fragColor = vec4(rgbA, 1.0);
    } else {
        fragColor = vec4(rgbB, 1.0);
    }
})";
#endif

static const char* kFilterCRT_FS =
#if defined(MED_PLATFORM_VITA) || (defined(MED_GLES) && (!defined(__APPLE__) || TARGET_OS_IPHONE))
R"(#version 100
precision mediump float;
varying vec2 vUV;
uniform sampler2D uTex;
uniform vec2 uResolution;
void main() {
    vec2 uv = vUV;
    vec2 cc = uv - 0.5;
    uv = 0.5 + cc * (1.0 + 0.06 * dot(cc, cc));
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) {
        gl_FragColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }
    vec3 col = texture2D(uTex, uv).rgb;
    float scanline = sin(uv.y * uResolution.y * 3.14159) * 0.18;
    col -= scanline;
    float vig = 16.0 * uv.x * uv.y * (1.0 - uv.x) * (1.0 - uv.y);
    col *= clamp(pow(vig, 0.15), 0.0, 1.0);
    gl_FragColor = vec4(col * vec3(0.96, 1.04, 0.98), 1.0);
})";
#else
R"(#version 330 core
in vec2 vUV;
uniform sampler2D uTex;
uniform vec2 uResolution;
out vec4 fragColor;
void main() {
    vec2 uv = vUV;
    vec2 cc = uv - 0.5;
    uv = 0.5 + cc * (1.0 + 0.06 * dot(cc, cc));
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) {
        fragColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }
    vec3 col = texture(uTex, uv).rgb;
    float scanline = sin(uv.y * uResolution.y * 3.14159) * 0.18;
    col -= scanline;
    float vig = 16.0 * uv.x * uv.y * (1.0 - uv.x) * (1.0 - uv.y);
    col *= clamp(pow(vig, 0.15), 0.0, 1.0);
    fragColor = vec4(col * vec3(0.96, 1.04, 0.98), 1.0);
})";
#endif

static const char* kFilterScanlines_FS =
#if defined(MED_PLATFORM_VITA) || (defined(MED_GLES) && (!defined(__APPLE__) || TARGET_OS_IPHONE))
R"(#version 100
precision mediump float;
varying vec2 vUV;
uniform sampler2D uTex;
uniform vec2 uResolution;
void main() {
    vec3 col = texture2D(uTex, vUV).rgb;
    float line = sin(vUV.y * uResolution.y * 3.14159265);
    float factor = 0.82 + 0.18 * line * line;
    gl_FragColor = vec4(col * factor, 1.0);
})";
#else
R"(#version 330 core
in vec2 vUV;
uniform sampler2D uTex;
uniform vec2 uResolution;
out vec4 fragColor;
void main() {
    vec3 col = texture(uTex, vUV).rgb;
    float line = sin(vUV.y * uResolution.y * 3.14159265);
    float factor = 0.82 + 0.18 * line * line;
    fragColor = vec4(col * factor, 1.0);
})";
#endif

static const char* kFilterSharpen_FS =
#if defined(MED_PLATFORM_VITA) || (defined(MED_GLES) && (!defined(__APPLE__) || TARGET_OS_IPHONE))
R"(#version 100
precision mediump float;
varying vec2 vUV;
uniform sampler2D uTex;
uniform vec2 uResolution;
void main() {
    vec2 step = vec2(1.0) / uResolution;
    vec3 c = texture2D(uTex, vUV).rgb;
    vec3 n = texture2D(uTex, vUV + vec2(0.0, -step.y)).rgb;
    vec3 s = texture2D(uTex, vUV + vec2(0.0,  step.y)).rgb;
    vec3 e = texture2D(uTex, vUV + vec2( step.x, 0.0)).rgb;
    vec3 w = texture2D(uTex, vUV + vec2(-step.x, 0.0)).rgb;
    vec3 sharp = c * 1.6 - (n + s + e + w) * 0.15;
    gl_FragColor = vec4(clamp(sharp, 0.0, 1.0), 1.0);
})";
#else
R"(#version 330 core
in vec2 vUV;
uniform sampler2D uTex;
uniform vec2 uResolution;
out vec4 fragColor;
void main() {
    vec2 step = vec2(1.0) / uResolution;
    vec3 c = texture(uTex, vUV).rgb;
    vec3 n = texture(uTex, vUV + vec2(0.0, -step.y)).rgb;
    vec3 s = texture(uTex, vUV + vec2(0.0,  step.y)).rgb;
    vec3 e = texture(uTex, vUV + vec2( step.x, 0.0)).rgb;
    vec3 w = texture(uTex, vUV + vec2(-step.x, 0.0)).rgb;
    vec3 sharp = c * 1.6 - (n + s + e + w) * 0.15;
    fragColor = vec4(clamp(sharp, 0.0, 1.0), 1.0);
})";
#endif

static const char* kFilterVibrant_FS =
#if defined(MED_PLATFORM_VITA) || (defined(MED_GLES) && (!defined(__APPLE__) || TARGET_OS_IPHONE))
R"(#version 100
precision mediump float;
varying vec2 vUV;
uniform sampler2D uTex;
void main() {
    vec3 c = texture2D(uTex, vUV).rgb;
    vec3 luma = vec3(0.299, 0.587, 0.114);
    float lum = dot(c, luma);
    vec3 sat = mix(vec3(lum), c, 1.35);
    gl_FragColor = vec4(clamp(pow(sat, vec3(0.96)), 0.0, 1.0), 1.0);
})";
#else
R"(#version 330 core
in vec2 vUV;
uniform sampler2D uTex;
out vec4 fragColor;
void main() {
    vec3 c = texture(uTex, vUV).rgb;
    vec3 luma = vec3(0.299, 0.587, 0.114);
    float lum = dot(c, luma);
    vec3 sat = mix(vec3(lum), c, 1.35);
    fragColor = vec4(clamp(pow(sat, vec3(0.96)), 0.0, 1.0), 1.0);
})";
#endif

static const char* kFilterCartoon_FS =
#if defined(MED_PLATFORM_VITA) || (defined(MED_GLES) && (!defined(__APPLE__) || TARGET_OS_IPHONE))
R"(#version 100
precision mediump float;
varying vec2 vUV;
uniform sampler2D uTex;
uniform vec2 uResolution;
void main() {
    vec3 c = texture2D(uTex, vUV).rgb;
    vec3 cel = floor(c * 6.0 + 0.5) / 6.0;
    vec2 step = vec2(1.0) / uResolution;
    vec3 n = texture2D(uTex, vUV + vec2(0.0, step.y)).rgb;
    vec3 e = texture2D(uTex, vUV + vec2(step.x, 0.0)).rgb;
    float diff = length(c - n) + length(c - e);
    if (diff > 0.3) cel *= 0.35;
    gl_FragColor = vec4(clamp(cel, 0.0, 1.0), 1.0);
})";
#else
R"(#version 330 core
in vec2 vUV;
uniform sampler2D uTex;
uniform vec2 uResolution;
out vec4 fragColor;
void main() {
    vec3 c = texture(uTex, vUV).rgb;
    vec3 cel = floor(c * 6.0 + 0.5) / 6.0;
    vec2 step = vec2(1.0) / uResolution;
    vec3 n = texture(uTex, vUV + vec2(0.0, step.y)).rgb;
    vec3 e = texture(uTex, vUV + vec2(step.x, 0.0)).rgb;
    float diff = length(c - n) + length(c - e);
    if (diff > 0.3) cel *= 0.35;
    fragColor = vec4(clamp(cel, 0.0, 1.0), 1.0);
})";
#endif

static GLuint CompilePostShader(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char buf[512];
        glGetShaderInfoLog(s, sizeof(buf), nullptr, buf);
        fprintf(stderr, "Post-process shader compile error: %s\n", buf);
    }
    return s;
}

static GLuint LinkPostProgram(const char* vsSrc, const char* fsSrc) {
    GLuint p = glCreateProgram();
    GLuint vs = CompilePostShader(GL_VERTEX_SHADER, vsSrc);
    GLuint fs = CompilePostShader(GL_FRAGMENT_SHADER, fsSrc);
    glAttachShader(p, vs);
    glAttachShader(p, fs);
    glLinkProgram(p);
    glDeleteShader(vs);
    glDeleteShader(fs);
    return p;
}

bool PostProcessor::Init() {
    const char* fsSources[size_t(PostFilter::Count)] = {
        kFilterNoneFS,
        kFilterFXAA_FS,
        kFilterCRT_FS,
        kFilterScanlines_FS,
        kFilterSharpen_FS,
        kFilterVibrant_FS,
        kFilterCartoon_FS,
    };

    for (size_t i = 0; i < size_t(PostFilter::Count); ++i) {
        progFilters_[i] = LinkPostProgram(kPostVS, fsSources[i]);
        uTex_[i] = glGetUniformLocation(progFilters_[i], "uTex");
        uRes_[i] = glGetUniformLocation(progFilters_[i], "uResolution");
        uTime_[i] = glGetUniformLocation(progFilters_[i], "uTime");
    }

    // Fullscreen quad: 2 triangles
    static const float kQuadVerts[] = {
        // x,     y,     u,    v
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
        -1.0f,  1.0f,  0.0f, 1.0f,
        -1.0f,  1.0f,  0.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f,
    };

#if !defined(MED_PLATFORM_VITA) && (!defined(MED_GLES) || (defined(__APPLE__) && !TARGET_OS_IPHONE))
    glGenVertexArrays(1, &vaoQuad_);
    glBindVertexArray(vaoQuad_);
#endif
    glGenBuffers(1, &vboQuad_);
    glBindBuffer(GL_ARRAY_BUFFER, vboQuad_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(kQuadVerts), kQuadVerts, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

#if !defined(MED_PLATFORM_VITA) && (!defined(MED_GLES) || (defined(__APPLE__) && !TARGET_OS_IPHONE))
    glBindVertexArray(0);
#endif
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    return true;
}

void PostProcessor::Shutdown() {
    if (fbo_) glDeleteFramebuffers(1, &fbo_);
    if (texColor_) glDeleteTextures(1, &texColor_);
    if (rboDepth_) glDeleteRenderbuffers(1, &rboDepth_);
    if (vboQuad_) glDeleteBuffers(1, &vboQuad_);
#if !defined(MED_PLATFORM_VITA) && (!defined(MED_GLES) || (defined(__APPLE__) && !TARGET_OS_IPHONE))
    if (vaoQuad_) glDeleteVertexArrays(1, &vaoQuad_);
#endif
    for (size_t i = 0; i < size_t(PostFilter::Count); ++i) {
        if (progFilters_[i]) glDeleteProgram(progFilters_[i]);
    }
}

void PostProcessor::EnsureFbo(int w, int h) {
    if (fbo_ && fboW_ == w && fboH_ == h) return;
    fboW_ = w;
    fboH_ = h;

    if (!fbo_) glGenFramebuffers(1, &fbo_);
    if (!texColor_) glGenTextures(1, &texColor_);
    if (!rboDepth_) glGenRenderbuffers(1, &rboDepth_);

    glBindTexture(GL_TEXTURE_2D, texColor_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindRenderbuffer(GL_RENDERBUFFER, rboDepth_);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, w, h);

    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texColor_, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rboDepth_);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void PostProcessor::Begin(ResolutionScale scale, int windowW, int windowH, int& outRenderW, int& outRenderH) {
    switch (scale) {
        case ResolutionScale::PSP_1X: outRenderW = 480;  outRenderH = 272;  break;
        case ResolutionScale::PSP_2X: outRenderW = 960;  outRenderH = 544;  break;
        case ResolutionScale::PSP_3X: outRenderW = 1440; outRenderH = 816;  break;
        case ResolutionScale::PSP_4X: outRenderW = 1920; outRenderH = 1088; break;
        case ResolutionScale::Native:
        default:
            outRenderW = windowW; outRenderH = windowH;
            break;
    }
    EnsureFbo(outRenderW, outRenderH);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glViewport(0, 0, outRenderW, outRenderH);
}

void PostProcessor::EndAndBlit(int windowW, int windowH, PostFilter filter, double timeSec) {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, windowW, windowH);

    size_t fIdx = (size_t)filter;
    if (fIdx >= size_t(PostFilter::Count)) fIdx = 0;
    GLuint prog = progFilters_[fIdx];

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);

    glUseProgram(prog);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texColor_);
    if (uTex_[fIdx] >= 0) glUniform1i(uTex_[fIdx], 0);
    if (uRes_[fIdx] >= 0) glUniform2f(uRes_[fIdx], float(fboW_), float(fboH_));
    if (uTime_[fIdx] >= 0) glUniform1f(uTime_[fIdx], float(timeSec));

#if !defined(MED_PLATFORM_VITA) && (!defined(MED_GLES) || (defined(__APPLE__) && !TARGET_OS_IPHONE))
    glBindVertexArray(vaoQuad_);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
#else
    glBindBuffer(GL_ARRAY_BUFFER, vboQuad_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
#endif

    glEnable(GL_DEPTH_TEST);
}

// ─────────────────────────────────────────────────────────────────────────────
// OverlayLayer implementation
// ─────────────────────────────────────────────────────────────────────────────

bool OverlayLayer::Init() {
    return post_.Init();
}

void OverlayLayer::Shutdown() {
    post_.Shutdown();
}

void OverlayLayer::BeginFrame(int windowW, int windowH, int& outRenderW, int& outRenderH) {
    metrics_.BeginFrame();
    post_.Begin(config_.resScale, windowW, windowH, outRenderW, outRenderH);
}

void OverlayLayer::EndFrameAndBlit(int windowW, int windowH, double timeSec) {
    metrics_.MarkCpuDone();
    post_.EndAndBlit(windowW, windowH, config_.filter, timeSec);
    metrics_.MarkGpuDone();
    metrics_.EndFrame();
}

void OverlayLayer::RenderPerfTile() {
    if (!config_.showPerfTile) return;

    ImGuiIO& io = ImGui::GetIO();
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
                             ImGuiWindowFlags_AlwaysAutoResize |
                             ImGuiWindowFlags_NoSavedSettings |
                             ImGuiWindowFlags_NoFocusOnAppearing |
                             ImGuiWindowFlags_NoNav |
                             ImGuiWindowFlags_NoMove;

    const float kMargin = 14.0f;
    ImVec2 targetPos;
    switch (config_.perfCorner) {
        case 0: // Top-Left
            targetPos = ImVec2(kMargin, kMargin);
            break;
        case 1: // Top-Right
        default:
            targetPos = ImVec2(io.DisplaySize.x - 170.0f - kMargin, kMargin);
            break;
        case 2: // Bottom-Left
            targetPos = ImVec2(kMargin, io.DisplaySize.y - 75.0f - kMargin);
            break;
        case 3: // Bottom-Right
            targetPos = ImVec2(io.DisplaySize.x - 170.0f - kMargin, io.DisplaySize.y - 75.0f - kMargin);
            break;
    }

    ImGui::SetNextWindowPos(targetPos, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.82f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 7.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.06f, 0.08f, 0.12f, 0.88f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.20f, 0.65f, 0.82f, 0.65f));

    if (ImGui::Begin("##PerfTile", nullptr, flags)) {
        ImVec4 fpsColor = (metrics_.fps >= 55.0f) ? ImVec4(0.3f, 0.95f, 0.4f, 1.0f) :
                          (metrics_.fps >= 30.0f) ? ImVec4(0.95f, 0.85f, 0.3f, 1.0f) :
                                                    ImVec4(0.95f, 0.3f, 0.3f, 1.0f);

        ImGui::TextColored(fpsColor, "FPS: %.1f", metrics_.fps);
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "CPU: %.0f%% / %.1fms", metrics_.cpuPct, metrics_.cpuMs);
        ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), "GPU: %.0f%% / %.1fms", metrics_.gpuPct, metrics_.gpuMs);
    }
    ImGui::End();

    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);
}

void OverlayLayer::RenderTouchControls(uint32_t& outButtons, float& outStickX, float& outStickY) {
    if (!config_.touch.enabled) return;

    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* draw = ImGui::GetForegroundDrawList();
    float dispW = io.DisplaySize.x;
    float dispH = io.DisplaySize.y;

    float op = config_.touch.opacity;
    float sc = config_.touch.scale;
    bool isEdit = config_.touch.editMode;

    ImVec2 mousePos = io.MousePos;
    bool mouseDown = io.MouseDown[0];

    // ── Analog Stick ──────────────────────────────────────────────────────────
    ImVec2 stickCenter(config_.touch.stickX * dispW, config_.touch.stickY * dispH);
    float baseRadius = config_.touch.stickRadius * sc;
    float deadZone = baseRadius * 0.20f;

    if (isEdit) {
        // Drag stick position in edit mode
        float distToStick = std::hypot(mousePos.x - stickCenter.x, mousePos.y - stickCenter.y);
        if (io.MouseClicked[0] && distToStick < baseRadius) {
            config_.touch.stickDragging = true;
        }
        if (!mouseDown) {
            config_.touch.stickDragging = false;
        }
        if (config_.touch.stickDragging) {
            config_.touch.stickX = std::clamp(mousePos.x / dispW, 0.05f, 0.95f);
            config_.touch.stickY = std::clamp(mousePos.y / dispH, 0.05f, 0.95f);
            stickCenter = ImVec2(config_.touch.stickX * dispW, config_.touch.stickY * dispH);
        }
        draw->AddCircle(stickCenter, baseRadius + 4.0f, IM_COL32(255, 170, 40, 220), 32, 2.0f);
    } else {
        // Normal gameplay touch on stick
        float distToStick = std::hypot(mousePos.x - stickCenter.x, mousePos.y - stickCenter.y);
        if (io.MouseClicked[0] && distToStick < baseRadius * 1.3f) {
            config_.touch.stickActive = true;
        }
        if (!mouseDown) {
            config_.touch.stickActive = false;
        }
        if (config_.touch.stickActive) {
            float dx = mousePos.x - stickCenter.x;
            float dy = mousePos.y - stickCenter.y;
            float dist = std::hypot(dx, dy);
            if (dist > deadZone) {
                float normDist = std::min(1.0f, dist / baseRadius);
                config_.touch.currentOutX = (dx / dist) * normDist;
                config_.touch.currentOutY = (dy / dist) * normDist;
            } else {
                config_.touch.currentOutX = 0.0f;
                config_.touch.currentOutY = 0.0f;
            }
        } else {
            config_.touch.currentOutX = 0.0f;
            config_.touch.currentOutY = 0.0f;
        }
    }

    outStickX = config_.touch.currentOutX;
    outStickY = config_.touch.currentOutY;

    // Draw stick base ring
    draw->AddCircleFilled(stickCenter, baseRadius, IM_COL32(20, 24, 32, int(op * 160.0f)), 36);
    draw->AddCircle(stickCenter, baseRadius, IM_COL32(80, 160, 200, int(op * 220.0f)), 36, 2.0f);
    draw->AddCircle(stickCenter, deadZone, IM_COL32(100, 120, 140, int(op * 100.0f)), 24, 1.0f);

    // Draw stick thumb knob
    ImVec2 knobPos = ImVec2(stickCenter.x + config_.touch.currentOutX * (baseRadius * 0.65f),
                            stickCenter.y + config_.touch.currentOutY * (baseRadius * 0.65f));
    float knobRadius = baseRadius * 0.40f;
    draw->AddCircleFilled(knobPos, knobRadius, IM_COL32(40, 50, 68, int(op * 240.0f)), 24);
    draw->AddCircle(knobPos, knobRadius, IM_COL32(120, 220, 255, int(op * 255.0f)), 24, 2.5f);

    // ── Buttons ───────────────────────────────────────────────────────────────
    for (auto& btn : config_.touch.buttons) {
        ImVec2 btnPos(btn.normX * dispW, btn.normY * dispH);
        float r = btn.radius * sc;
        float w = btn.width * sc;
        float h = btn.height * sc;

        bool hit = false;
        if (btn.isCircle) {
            float dist = std::hypot(mousePos.x - btnPos.x, mousePos.y - btnPos.y);
            hit = (dist <= r);
        } else {
            hit = (std::abs(mousePos.x - btnPos.x) <= w * 0.5f && std::abs(mousePos.y - btnPos.y) <= h * 0.5f);
        }

        if (isEdit) {
            if (io.MouseClicked[0] && hit) {
                btn.isDragging = true;
            }
            if (!mouseDown) {
                btn.isDragging = false;
            }
            if (btn.isDragging) {
                btn.normX = std::clamp(mousePos.x / dispW, 0.04f, 0.96f);
                btn.normY = std::clamp(mousePos.y / dispH, 0.04f, 0.96f);
                btnPos = ImVec2(btn.normX * dispW, btn.normY * dispH);
            }
            // Edit indicator
            if (btn.isCircle) {
                draw->AddCircle(btnPos, r + 3.0f, IM_COL32(255, 180, 50, 220), 24, 1.5f);
            } else {
                draw->AddRect(ImVec2(btnPos.x - w*0.5f - 3.0f, btnPos.y - h*0.5f - 3.0f),
                              ImVec2(btnPos.x + w*0.5f + 3.0f, btnPos.y + h*0.5f + 3.0f),
                              IM_COL32(255, 180, 50, 220), 6.0f, 0, 1.5f);
            }
        } else {
            btn.isPressed = (mouseDown && hit);
            if (btn.isPressed) {
                outButtons |= btn.bit;
            }
        }

        // Visual colours
        ImU32 fillCol = btn.isPressed ? IM_COL32(30, 150, 210, int(op * 240.0f)) :
                                        IM_COL32(22, 26, 34, int(op * 175.0f));
        ImU32 borderCol = btn.isPressed ? IM_COL32(100, 230, 255, int(op * 255.0f)) :
                                          IM_COL32(110, 130, 160, int(op * 220.0f));

        if (btn.isCircle) {
            draw->AddCircleFilled(btnPos, r, fillCol, 28);
            draw->AddCircle(btnPos, r, borderCol, 28, 2.0f);
        } else {
            draw->AddRectFilled(ImVec2(btnPos.x - w*0.5f, btnPos.y - h*0.5f),
                                ImVec2(btnPos.x + w*0.5f, btnPos.y + h*0.5f),
                                fillCol, 8.0f);
            draw->AddRect(ImVec2(btnPos.x - w*0.5f, btnPos.y - h*0.5f),
                          ImVec2(btnPos.x + w*0.5f, btnPos.y + h*0.5f),
                          borderCol, 8.0f, 0, 2.0f);
        }

        // Label text
        ImVec2 txtSz = ImGui::CalcTextSize(btn.label.c_str());
        ImVec2 txtPos(btnPos.x - txtSz.x * 0.5f, btnPos.y - txtSz.y * 0.5f);
        ImU32 txtCol = btn.isPressed ? IM_COL32(255, 255, 255, 255) :
                                       IM_COL32(200, 225, 245, int(op * 250.0f));
        draw->AddText(txtPos, txtCol, btn.label.c_str());
    }

    if (isEdit) {
        // Helpful hint banner during layout editing
        ImVec2 hintPos(dispW * 0.5f, 20.0f);
        const char* hint = "[EDIT MODE] Drag buttons or stick to adjust layout";
        ImVec2 sz = ImGui::CalcTextSize(hint);
        draw->AddRectFilled(ImVec2(hintPos.x - sz.x*0.5f - 12.0f, hintPos.y - 4.0f),
                            ImVec2(hintPos.x + sz.x*0.5f + 12.0f, hintPos.y + sz.y + 6.0f),
                            IM_COL32(20, 25, 35, 220), 6.0f);
        draw->AddText(ImVec2(hintPos.x - sz.x*0.5f, hintPos.y), IM_COL32(255, 190, 50, 255), hint);
    }
}

void OverlayLayer::RenderSettingsWindow(bool* pOpen, Renderer* renderer) {
    if (!pOpen || !*pOpen) return;

    ImGui::SetNextWindowSize(ImVec2(440, 540), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("System & Overlay Settings", pOpen)) {
        if (ImGui::CollapsingHeader("Display & Post-Processing (PPSSPP Style)", ImGuiTreeNodeFlags_DefaultOpen)) {
            // Resolution Scale
            const char* resScales[] = {
                "Native (Window)",
                "1x PSP (480 x 272)",
                "2x PSP (960 x 544 - PS Vita)",
                "3x PSP (1440 x 816)",
                "4x PSP (1920 x 1088)"
            };
            int curRes = (int)config_.resScale;
            if (ImGui::Combo("Resolution Scale", &curRes, resScales, IM_ARRAYSIZE(resScales))) {
                config_.resScale = (ResolutionScale)curRes;
            }

            // Anisotropic filtering
            const char* anisoLevels[] = { "Off (1x)", "2x", "4x", "8x", "16x" };
            int anisoValues[] = { 1, 2, 4, 8, 16 };
            int curAnisoIdx = 2; // 4x default
            for (int i = 0; i < 5; ++i) {
                if (anisoValues[i] == config_.anisotropy) { curAnisoIdx = i; break; }
            }
            if (ImGui::Combo("Anisotropic Filtering", &curAnisoIdx, anisoLevels, IM_ARRAYSIZE(anisoLevels))) {
                config_.anisotropy = anisoValues[curAnisoIdx];
                if (renderer) renderer->SetAnisotropy(config_.anisotropy);
            }

            // Post-processing filter
            const char* filters[] = {
                "Off (None)",
                "FXAA (Anti-Aliasing)",
                "CRT (Retro Tube)",
                "Scanlines",
                "Sharpen (Adaptive)",
                "Vibrant Colors",
                "Cartoon (Cel-Shaded)"
            };
            int curFilter = (int)config_.filter;
            if (ImGui::Combo("Post Filter", &curFilter, filters, IM_ARRAYSIZE(filters))) {
                config_.filter = (PostFilter)curFilter;
            }
        }

        if (ImGui::CollapsingHeader("Touch Controls & Layout", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Checkbox("Enable On-Screen Touch Controls", &config_.touch.enabled);
            ImGui::Checkbox("Edit Layout (Drag buttons on screen)", &config_.touch.editMode);

            ImGui::SliderFloat("Opacity", &config_.touch.opacity, 0.10f, 1.0f, "%.2f");
            ImGui::SliderFloat("Scale / Size", &config_.touch.scale, 0.50f, 1.60f, "%.2f");

            ImGui::Text("Presets:");
            if (ImGui::Button("Ergonomic 2-Thumb")) {
                config_.touch.SetErgonomicPreset();
            }
            ImGui::SameLine();
            if (ImGui::Button("Classic PSP (PPSSPP)")) {
                config_.touch.SetClassicPreset();
            }
            ImGui::SameLine();
            if (ImGui::Button("Reset")) {
                config_.touch.SetErgonomicPreset();
            }
        }

        if (ImGui::CollapsingHeader("Performance HUD Tile", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Checkbox("Show Performance Tile", &config_.showPerfTile);
            const char* corners[] = { "Top-Left", "Top-Right", "Bottom-Left", "Bottom-Right" };
            ImGui::Combo("Tile Position", &config_.perfCorner, corners, IM_ARRAYSIZE(corners));
        }
    }
    ImGui::End();
}

} // namespace med
