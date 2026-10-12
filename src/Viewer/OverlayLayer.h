#pragma once

#include "Viewer/GLPlatform.h"
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <cstdint>

namespace med {

class Renderer;

// PSP Hardware button bits (compatible with sceCtrl / PSP standard)
enum PspCtrlButtons : uint32_t {
    PSP_CTRL_SELECT   = 0x000001,
    PSP_CTRL_START    = 0x000008,
    PSP_CTRL_UP       = 0x000010,
    PSP_CTRL_RIGHT    = 0x000020,
    PSP_CTRL_DOWN     = 0x000040,
    PSP_CTRL_LEFT     = 0x000080,
    PSP_CTRL_LTRIGGER = 0x000100,
    PSP_CTRL_RTRIGGER = 0x000200,
    PSP_CTRL_TRIANGLE = 0x001000,
    PSP_CTRL_CIRCLE   = 0x002000,
    PSP_CTRL_CROSS    = 0x004000,
    PSP_CTRL_SQUARE   = 0x008000,
};

enum class ResolutionScale : int {
    Native = 0,
    PSP_1X = 1, // 480x272
    PSP_2X = 2, // 960x544 (PS Vita)
    PSP_3X = 3, // 1440x816
    PSP_4X = 4, // 1920x1088
};

enum class PostFilter : int {
    None = 0,
    FXAA,
    CRT,
    Scanlines,
    Sharpen,
    Vibrant,
    Cartoon,
    Count
};

struct PerfMetrics {
    float fps = 60.0f;
    float cpuMs = 0.0f;
    float cpuPct = 0.0f;
    float gpuMs = 0.0f;
    float gpuPct = 0.0f;

    uint64_t tFrameStart = 0;
    uint64_t tCpuDone = 0;
    uint64_t tGpuDone = 0;
    float accumSeconds = 0.0f;
    int frameCounter = 0;

    void BeginFrame();
    void MarkCpuDone();
    void MarkGpuDone();
    void EndFrame();
};

struct TouchButton {
    std::string name;
    std::string label;
    uint32_t bit = 0;
    float normX = 0.5f; // [0..1] normalized screen space
    float normY = 0.5f; // [0..1]
    float radius = 32.0f;
    bool isCircle = true;
    float width = 64.0f;
    float height = 36.0f;
    bool isPressed = false;
    bool isDragging = false;
};

struct TouchLayout {
    bool enabled = true;
    bool editMode = false;
    float opacity = 0.65f;
    float scale = 1.0f;

    // Analog stick
    float stickX = 0.15f;
    float stickY = 0.74f;
    float stickRadius = 65.0f;
    float currentOutX = 0.0f; // [-1..1]
    float currentOutY = 0.0f; // [-1..1]
    bool stickActive = false;
    bool stickDragging = false;

    std::vector<TouchButton> buttons;

    TouchLayout();
    void SetErgonomicPreset();
    void SetClassicPreset();
};

struct OverlayConfig {
    ResolutionScale resScale = ResolutionScale::Native;
    int anisotropy = 4;
    PostFilter filter = PostFilter::None;
    bool showPerfTile = true;
    int perfCorner = 1; // 0: TL, 1: TR, 2: BL, 3: BR
    TouchLayout touch;
};

class PostProcessor {
public:
    bool Init();
    void Shutdown();
    void Begin(ResolutionScale scale, int windowW, int windowH, int& outRenderW, int& outRenderH);
    void EndAndBlit(int windowW, int windowH, PostFilter filter, double timeSec);

    GLuint SceneTexture() const { return texColor_; }

private:
    void EnsureFbo(int w, int h);

    GLuint fbo_ = 0;
    GLuint texColor_ = 0;
    GLuint rboDepth_ = 0;
    int fboW_ = 0, fboH_ = 0;

    GLuint vaoQuad_ = 0;
    GLuint vboQuad_ = 0;

    GLuint progFilters_[size_t(PostFilter::Count)] = {0};
    GLint uTex_[size_t(PostFilter::Count)] = {-1};
    GLint uRes_[size_t(PostFilter::Count)] = {-1};
    GLint uTime_[size_t(PostFilter::Count)] = {-1};
};

class OverlayLayer {
public:
    bool Init();
    void Shutdown();

    void BeginFrame(int windowW, int windowH, int& outRenderW, int& outRenderH);
    void EndFrameAndBlit(int windowW, int windowH, double timeSec);

    void RenderPerfTile();
    void RenderTouchControls(uint32_t& outButtons, float& outStickX, float& outStickY);
    void RenderSettingsWindow(bool* pOpen, Renderer* renderer);

    OverlayConfig& Config() { return config_; }
    const OverlayConfig& Config() const { return config_; }
    PerfMetrics& Metrics() { return metrics_; }

private:
    OverlayConfig config_;
    PerfMetrics metrics_;
    PostProcessor post_;
};

} // namespace med
