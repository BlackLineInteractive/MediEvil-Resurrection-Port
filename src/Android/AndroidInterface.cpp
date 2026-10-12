#include "Android/AndroidInterface.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace med::android {

AndroidInterface::AndroidInterface() {
    Layout(screenW_, screenH_);
}

void AndroidInterface::Layout(float screenWidth, float screenHeight) {
    screenW_ = screenWidth;
    screenH_ = screenHeight;

    // Responsive stick sizing based on screen height
    stick_.radius = std::clamp(screenHeight * 0.16f, 65.0f, 130.0f);
    stick_.knobRadius = stick_.radius * 0.44f;
    stick_.basePos = {screenWidth * 0.15f, screenHeight * 0.74f};
    stick_.knobPos = stick_.basePos;
    stick_.value = {0.0f, 0.0f};

    InitButtons(screenWidth, screenHeight);
}

void AndroidInterface::InitButtons(float screenWidth, float screenHeight) {
    buttons_.clear();

    float btnR = std::clamp(screenHeight * 0.075f, 32.0f, 60.0f);
    float padSpacing = btnR * 1.55f;

    // Diamond cluster on the right side
    glm::vec2 clusterCenter = {screenWidth * 0.86f, screenHeight * 0.72f};

    // 1. Jump (South / Cross)
    TouchButton btnJump;
    btnJump.id = "jump";
    btnJump.label = "JUMP";
    btnJump.radius = btnR;
    btnJump.pos = {clusterCenter.x, clusterCenter.y + padSpacing};
    btnJump.baseColor = IM_COL32(70, 140, 240, 65);
    btnJump.activeColor = IM_COL32(70, 150, 255, 200);
    buttons_.push_back(btnJump);

    // 2. Attack (West / Square)
    TouchButton btnAtk;
    btnAtk.id = "attack";
    btnAtk.label = "ATK";
    btnAtk.radius = btnR;
    btnAtk.pos = {clusterCenter.x - padSpacing, clusterCenter.y};
    btnAtk.baseColor = IM_COL32(230, 80, 80, 65);
    btnAtk.activeColor = IM_COL32(255, 90, 90, 200);
    buttons_.push_back(btnAtk);

    // 3. Action / Interact (East / Circle)
    TouchButton btnAct;
    btnAct.id = "action";
    btnAct.label = "ACT";
    btnAct.radius = btnR;
    btnAct.pos = {clusterCenter.x + padSpacing, clusterCenter.y};
    btnAct.baseColor = IM_COL32(230, 180, 40, 65);
    btnAct.activeColor = IM_COL32(255, 200, 50, 200);
    buttons_.push_back(btnAct);

    // 4. Sprint / Run (North / Triangle)
    TouchButton btnRun;
    btnRun.id = "sprint";
    btnRun.label = "RUN";
    btnRun.radius = btnR;
    btnRun.pos = {clusterCenter.x, clusterCenter.y - padSpacing};
    btnRun.baseColor = IM_COL32(80, 210, 110, 65);
    btnRun.activeColor = IM_COL32(90, 240, 130, 200);
    buttons_.push_back(btnRun);

    // Top control pills: Play/Dan mode toggle, Menu, Reset
    float topBtnR = std::clamp(screenHeight * 0.052f, 24.0f, 40.0f);

    TouchButton btnDan;
    btnDan.id = "dan_toggle";
    btnDan.label = "DAN";
    btnDan.radius = topBtnR;
    btnDan.pos = {screenWidth * 0.07f, screenHeight * 0.08f};
    btnDan.baseColor = IM_COL32(200, 140, 60, 60);
    btnDan.activeColor = IM_COL32(255, 170, 70, 210);
    buttons_.push_back(btnDan);

    TouchButton btnCam;
    btnCam.id = "cam_reset";
    btnCam.label = "CAM";
    btnCam.radius = topBtnR;
    btnCam.pos = {screenWidth * 0.16f, screenHeight * 0.08f};
    btnCam.baseColor = IM_COL32(100, 120, 150, 60);
    btnCam.activeColor = IM_COL32(130, 160, 210, 210);
    buttons_.push_back(btnCam);

    TouchButton btnMenu;
    btnMenu.id = "menu";
    btnMenu.label = "MENU";
    btnMenu.radius = topBtnR;
    btnMenu.pos = {screenWidth * 0.93f, screenHeight * 0.08f};
    btnMenu.baseColor = IM_COL32(150, 150, 150, 60);
    btnMenu.activeColor = IM_COL32(240, 240, 240, 210);
    buttons_.push_back(btnMenu);
}

TouchButton* AndroidInterface::FindButtonAt(const glm::vec2& pt) {
    for (auto& btn : buttons_) {
        float d2 = glm::dot(pt - btn.pos, pt - btn.pos);
        if (d2 <= (btn.radius * 1.25f) * (btn.radius * 1.25f)) {
            return &btn;
        }
    }
    return nullptr;
}

TouchButton* AndroidInterface::GetButton(const std::string& id) {
    for (auto& btn : buttons_) {
        if (btn.id == id) return &btn;
    }
    return nullptr;
}

const TouchButton* AndroidInterface::GetButton(const std::string& id) const {
    for (const auto& btn : buttons_) {
        if (btn.id == id) return &btn;
    }
    return nullptr;
}

bool AndroidInterface::IsMenuTriggered() const {
    const TouchButton* b = GetButton("menu");
    return b && b->triggered;
}

bool AndroidInterface::IsPlayToggleTriggered() const {
    const TouchButton* b = GetButton("dan_toggle");
    return b && b->triggered;
}

bool AndroidInterface::IsResetViewTriggered() const {
    const TouchButton* b = GetButton("cam_reset");
    return b && b->triggered;
}

void AndroidInterface::Update(float dt) {
    (void)dt;
    // Reset one-frame triggers
    for (auto& btn : buttons_) {
        btn.triggered = false;
    }
    camera_.delta = {0.0f, 0.0f};
    camera_.zoomDelta = 0.0f;
}

bool AndroidInterface::ProcessEvent(const SDL_Event& event, float screenWidth, float screenHeight) {
    if (!visible_) return false;

    // 1. Touch finger down
    if (event.type == SDL_FINGERDOWN) {
        glm::vec2 pt = {event.tfinger.x * screenWidth, event.tfinger.y * screenHeight};
        SDL_FingerID fid = event.tfinger.fingerId;

        // Check buttons first
        TouchButton* hitBtn = FindButtonAt(pt);
        if (hitBtn) {
            hitBtn->pressed = true;
            hitBtn->triggered = true;
            hitBtn->fingerId = fid;
            return true;
        }

        // Left region: thumbstick
        if (pt.x < screenWidth * 0.45f && !stick_.active) {
            stick_.active = true;
            stick_.fingerId = fid;
            // Floating stick: if touched close enough to default base, stick stays put; otherwise recenter
            if (glm::distance(pt, stick_.basePos) > stick_.radius * 1.3f) {
                stick_.basePos = pt;
            }
            stick_.knobPos = pt;
            stick_.value = {0.0f, 0.0f};
            return true;
        }

        // Right region: camera look swipe
        if (pt.x >= screenWidth * 0.45f && !camera_.active) {
            camera_.active = true;
            camera_.fingerId = fid;
            camera_.lastPos = pt;
            return true;
        }
    }
    // 2. Touch finger motion
    else if (event.type == SDL_FINGERMOTION) {
        glm::vec2 pt = {event.tfinger.x * screenWidth, event.tfinger.y * screenHeight};
        SDL_FingerID fid = event.tfinger.fingerId;

        // Thumbstick motion
        if (stick_.active && stick_.fingerId == fid) {
            glm::vec2 diff = pt - stick_.basePos;
            float dist = glm::length(diff);
            if (dist > stick_.radius) {
                diff = (diff / dist) * stick_.radius;
            }
            stick_.knobPos = stick_.basePos + diff;
            // y is inverted for game forward: up = +1, down = -1
            stick_.value = {diff.x / stick_.radius, -diff.y / stick_.radius};
            return true;
        }

        // Camera swipe motion
        if (camera_.active && camera_.fingerId == fid) {
            glm::vec2 diff = pt - camera_.lastPos;
            camera_.delta.x += diff.x * camera_.sensitivity;
            camera_.delta.y += diff.y * camera_.sensitivity;
            camera_.lastPos = pt;
            return true;
        }
    }
    // 3. Touch finger up
    else if (event.type == SDL_FINGERUP) {
        SDL_FingerID fid = event.tfinger.fingerId;

        for (auto& btn : buttons_) {
            if (btn.fingerId == fid) {
                btn.pressed = false;
                btn.fingerId = -1;
            }
        }

        if (stick_.active && stick_.fingerId == fid) {
            stick_.active = false;
            stick_.fingerId = -1;
            stick_.knobPos = stick_.basePos;
            stick_.value = {0.0f, 0.0f};
        }

        if (camera_.active && camera_.fingerId == fid) {
            camera_.active = false;
            camera_.fingerId = -1;
        }
    }

    return false;
}

PlayerInput AndroidInterface::GetPlayerInput(float cameraYaw) const {
    PlayerInput in;
    in.move = stick_.value;
    if (glm::length(in.move) > 1.0f) {
        in.move = glm::normalize(in.move);
    }

    in.camYaw = cameraYaw;

    const TouchButton* btnJump = GetButton("jump");
    in.jump = btnJump && btnJump->pressed;

    const TouchButton* btnAtk = GetButton("attack");
    in.attack = btnAtk && btnAtk->triggered;

    const TouchButton* btnRun = GetButton("sprint");
    in.run = btnRun && btnRun->pressed;
    in.sprint = in.run && (glm::length(in.move) > 0.85f);

    return in;
}

void AndroidInterface::Render(ImDrawList* drawList, float screenWidth, float screenHeight) {
    if (!visible_ || !drawList) return;

    // Check if layout update needed
    if (std::abs(screenWidth - screenW_) > 1.0f || std::abs(screenHeight - screenH_) > 1.0f) {
        Layout(screenWidth, screenHeight);
    }

    // ── 1. Render Analog Stick ──
    ImVec2 baseCenter(stick_.basePos.x, stick_.basePos.y);
    ImVec2 knobCenter(stick_.knobPos.x, stick_.knobPos.y);

    // Outer base circle
    drawList->AddCircleFilled(baseCenter, stick_.radius, IM_COL32(20, 25, 35, 75), 32);
    drawList->AddCircle(baseCenter, stick_.radius, IM_COL32(180, 190, 210, 80), 32, 2.5f);

    // Directional axis guidelines
    float crossLen = stick_.radius * 0.45f;
    drawList->AddLine(ImVec2(baseCenter.x - crossLen, baseCenter.y),
                      ImVec2(baseCenter.x + crossLen, baseCenter.y), IM_COL32(255, 255, 255, 40), 1.5f);
    drawList->AddLine(ImVec2(baseCenter.x, baseCenter.y - crossLen),
                      ImVec2(baseCenter.x, baseCenter.y + crossLen), IM_COL32(255, 255, 255, 40), 1.5f);

    // Knob with glow when active
    ImU32 knobFill = stick_.active ? IM_COL32(255, 175, 45, 190) : IM_COL32(220, 230, 245, 120);
    ImU32 knobBorder = stick_.active ? IM_COL32(255, 230, 150, 240) : IM_COL32(255, 255, 255, 180);

    drawList->AddCircleFilled(knobCenter, stick_.knobRadius, knobFill, 24);
    drawList->AddCircle(knobCenter, stick_.knobRadius, knobBorder, 24, 2.0f);

    // ── 2. Render Action Buttons ──
    for (const auto& btn : buttons_) {
        ImVec2 p(btn.pos.x, btn.pos.y);
        ImU32 fill = btn.pressed ? btn.activeColor : btn.baseColor;
        ImU32 border = btn.pressed ? IM_COL32(255, 255, 255, 230) : IM_COL32(200, 210, 230, 110);

        // Glass background
        drawList->AddCircleFilled(p, btn.radius, fill, 28);
        drawList->AddCircle(p, btn.radius, border, 28, btn.pressed ? 3.0f : 2.0f);

        // Centered label text
        ImVec2 txtSz = ImGui::CalcTextSize(btn.label.c_str());
        ImVec2 txtPos(p.x - txtSz.x * 0.5f, p.y - txtSz.y * 0.5f);
        drawList->AddText(txtPos, btn.pressed ? IM_COL32(255, 255, 255, 255) : IM_COL32(230, 235, 245, 210),
                          btn.label.c_str());
    }
}

std::string AndroidInterface::GetAndroidStoragePath() {
#if defined(__ANDROID__)
    const char* ext = SDL_AndroidGetExternalStoragePath();
    if (ext && ext[0]) return std::string(ext);
#endif
    return "";
}

std::string AndroidInterface::ResolveGameDataPath(const std::string& fallbackPath) {
#if defined(__ANDROID__)
    // 1. Check external app storage (e.g. /sdcard/Android/data/org.medievil/files/gamedata.bin)
    std::string ext = GetAndroidStoragePath();
    if (!ext.empty()) {
        std::string p = ext + "/gamedata.bin";
        FILE* f = fopen(p.c_str(), "rb");
        if (f) { fclose(f); return p; }
    }
    // 2. Check public root /sdcard/MediEvil/gamedata.bin
    const char* publicPaths[] = {
        "/sdcard/MediEvil/gamedata.bin",
        "/storage/emulated/0/MediEvil/gamedata.bin",
        "/sdcard/PSP_GAME/USRDIR/gamedata.bin"
    };
    for (const char* pp : publicPaths) {
        FILE* f = fopen(pp, "rb");
        if (f) { fclose(f); return std::string(pp); }
    }
#endif
    return fallbackPath;
}

} // namespace med::android
