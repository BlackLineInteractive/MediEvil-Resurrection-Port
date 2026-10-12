#pragma once
// Sir Daniel's movement: a simple character controller over the level collision (not a port of the
// game's DanBrain yet). Animation states come from dan.atbl:
//   state 4 = idle (variant 0; 200/201 are long "bored" idles), 1 = walk, 0 = run (variant 2: sprint),
//   2/3 and 44..47 = strafes / diagonals, 28 = attacks per weapon type, 64 / 35 / 38 = other sets.
// The model faces +Z; animations carry no root motion.
#include "Core/Collision.h"

#include <glm/glm.hpp>

namespace med {

struct PlayerInput {
    glm::vec2 move{0};      // x = right, y = forward, length <= 1, relative to camYaw
    float camYaw = 0;       // radians, camera heading (0 = looking along +Z)
    bool walk = false;
    bool jump = false;
};

enum class PlayerAnim { Idle, Walk, Run, Air };

struct Player {
    glm::vec3 pos{0};
    glm::vec3 vel{0};
    float yaw = 0;          // model heading, radians (0 = facing +Z)
    bool onGround = false;
    float animTime = 0;
    PlayerAnim anim = PlayerAnim::Idle;

    // tuning (metres, seconds)
    float walkSpeed = 2.2f, runSpeed = 5.5f, gravity = 22.0f, jumpSpeed = 7.5f;
    float radius = 0.4f, stepHeight = 0.45f, turnRate = 12.0f;

    void Spawn(const CollisionWorld& world, glm::vec3 at);
    void Update(const CollisionWorld& world, const PlayerInput& in, float dt);
};

} // namespace med
