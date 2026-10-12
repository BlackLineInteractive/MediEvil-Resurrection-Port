#pragma once
// Sir Daniel's movement and actions: a character controller over the level collision (not a port of
// the game's DanBrain yet) driving an Animator with the dan.atbl animations.
//
// dan.atbl states, identified from the motion they imply (planted-foot velocity, --atbl-report):
//   4 idle (var 0; 200/201 long idles)      0 forward: var 0 walk 2.06 m/s, 1 bouncy jog 2.39 m/s, 2 run 3.26 m/s
//   1 backward    2 / 3 strafe +X / -X      44 / 45 forward diagonals, 46 / 47 backward diagonals
//   64 jump: var 0 take-off (0.2 s), 1 airborne, 2 landing (pelvis drops to 0.83)
//   28 attacks, 3rd float = weapon type; type 2 (one-handed sword): var 0, 1, 2 = three-hit combo
//   38 hit reactions, 97 crouch, 35 climbing / special moves
// The model faces +Z and the animations have no root motion, so playback rate follows the speed.
#include "Core/Collision.h"
#include "Core/Skeleton.h"

#include <glm/glm.hpp>

namespace med {

struct PlayerInput {
    glm::vec2 move{0};      // x = right, y = forward, length <= 1, relative to camYaw
    float camYaw = 0;       // radians, camera heading (0 = looking along +Z)
    bool run = false;       // Shift
    bool sprint = false;    // Ctrl (with Shift): short burst
    bool jump = false;      // held
    bool attack = false;    // pressed this frame
};

struct PlayerAnims {
    const Animation* idle = nullptr;
    const Animation* walk = nullptr;        // natural speed walkNatural
    const Animation* run = nullptr;         // natural speed runNatural
    const Animation* sprint = nullptr;      // natural speed sprintNatural
    const Animation* jumpStart = nullptr;
    const Animation* air = nullptr;
    const Animation* land = nullptr;
    const Animation* attack[3] = {};
    float walkNatural = 2.06f, runNatural = 2.39f, sprintNatural = 3.26f;   // dan.atbl state 0 var 0 / 1 / 2
};

enum class PlayerAction { Move, JumpStart, Air, Land, Attack };

struct Player {
    glm::vec3 pos{0};
    glm::vec3 vel{0};
    float yaw = 0;          // model heading, radians (0 = facing +Z)
    bool onGround = false;
    PlayerAction action = PlayerAction::Move;
    int combo = 0;          // attack index while action == Attack
    bool comboQueued = false;
    bool hitFrame = false;  // true on the frame a swing connects (see HitSphere)

    Animator animator;
    PlayerAnims anims;

    // tuning (metres, seconds)
    float walkSpeed = 2.0f, runSpeed = 2.6f, sprintSpeed = 4.3f, gravity = 22.0f, jumpSpeed = 7.5f;
    float sprintMax = 1.5f, sprintRecharge = 2.0f;   // burst length, time to refill it
    float radius = 0.4f, stepHeight = 0.45f, turnRate = 12.0f;

    void Spawn(const CollisionWorld& world, glm::vec3 at);
    void Update(const CollisionWorld& world, const PlayerInput& in, float dt);
    // Where the current swing hits: centre and radius in world space.
    glm::vec3 HitCentre() const;
    float HitRadius() const { return 1.1f; }
    glm::vec3 Facing() const;

private:
    void Locomotion(float speed);
    bool prevJump_ = false;
    float sprintLeft_ = 1.5f;
    float animSpeed_ = 0;
    bool hitDone_ = false;
};

} // namespace med
