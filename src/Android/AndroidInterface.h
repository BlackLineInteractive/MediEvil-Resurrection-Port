#pragma once

#include "Core/Player.h"

#if __has_include(<SDL2/SDL.h>)
    #include <SDL2/SDL.h>
#else
    #include <SDL.h>
#endif
#include <glm/glm.hpp>
#include <imgui.h>

#include <string>
#include <vector>

namespace med::android {

// ── Virtual Touch Button ──
struct TouchButton {
    std::string id;
    std::string label;
    glm::vec2 pos{0.0f};      // Screen coordinates (pixels)
    float radius = 40.0f;     // Button radius
    bool pressed = false;
    bool triggered = false;   // True on the frame it was pressed down
    SDL_FingerID fingerId = -1;
    ImU32 baseColor = IM_COL32(255, 255, 255, 45);
    ImU32 activeColor = IM_COL32(230, 160, 40, 180);
};

// ── Virtual Touch Analog Stick ──
struct TouchStick {
    glm::vec2 basePos{0.0f};  // Center of the stick base
    glm::vec2 knobPos{0.0f};  // Position of the movable knob
    float radius = 70.0f;     // Outer boundary radius
    float knobRadius = 32.0f; // Inner knob radius
    bool active = false;
    SDL_FingerID fingerId = -1;
    glm::vec2 value{0.0f};    // Normalized output [-1..1] (x: right, y: forward)
};

// ── Virtual Touch Camera Look Area ──
struct TouchCamera {
    bool active = false;
    SDL_FingerID fingerId = -1;
    glm::vec2 lastPos{0.0f};
    glm::vec2 delta{0.0f};    // dYaw, dPitch accumulated this frame
    float sensitivity = 0.25f;

    // Pinch-to-zoom tracking
    bool pinching = false;
    SDL_FingerID pinchFingerId = -1;
    float lastPinchDist = 0.0f;
    float zoomDelta = 0.0f;
};

// ── Android Touch Interface & Virtual Controller ──
class AndroidInterface {
public:
    AndroidInterface();

    // Call on screen resize or orientation change
    void Layout(float screenWidth, float screenHeight);

    // Feed SDL events (supports both SDL_FINGER* and desktop mouse simulation)
    bool ProcessEvent(const SDL_Event& event, float screenWidth, float screenHeight);

    // Call once at the start of each frame before reading inputs
    void Update(float dt);

    // Retrieve PlayerInput populated from virtual touch controls
    PlayerInput GetPlayerInput(float cameraYaw) const;

    // Camera look deltas from swipe gestures (degrees)
    glm::vec2 GetCameraDelta() const { return camera_.delta; }
    float GetZoomDelta() const { return camera_.zoomDelta; }

    // System button triggers
    bool IsMenuTriggered() const;
    bool IsPlayToggleTriggered() const;
    bool IsResetViewTriggered() const;

    // Render the on-screen touch HUD using Dear ImGui
    void Render(ImDrawList* drawList, float screenWidth, float screenHeight);

    // Visibility toggles
    void SetVisible(bool visible) { visible_ = visible; }
    bool IsVisible() const { return visible_; }

    // Platform helpers for Android storage & asset paths
    static std::string GetAndroidStoragePath();
    static std::string ResolveGameDataPath(const std::string& fallbackPath = "extracted/PSP_GAME/USRDIR/gamedata.bin");

private:
    void InitButtons(float screenWidth, float screenHeight);
    TouchButton* FindButtonAt(const glm::vec2& pt);
    TouchButton* GetButton(const std::string& id);
    const TouchButton* GetButton(const std::string& id) const;

    bool visible_ = true;
    float screenW_ = 1280.0f;
    float screenH_ = 720.0f;

    TouchStick stick_;
    TouchCamera camera_;
    std::vector<TouchButton> buttons_;

    bool sprintToggle_ = false;
};

} // namespace med::android
