#include "Core/Player.h"

#include <algorithm>
#include <cmath>

namespace med {

void Player::Spawn(const CollisionWorld& world, glm::vec3 at) {
    float y;
    if (world.Ground(at + glm::vec3(0, 2, 0), 200.0f, y)) at.y = y;
    pos = at;
    vel = glm::vec3(0);
    onGround = true;
    anim = PlayerAnim::Idle;
    animTime = 0;
}

void Player::Update(const CollisionWorld& world, const PlayerInput& in, float dt) {
    dt = std::min(dt, 0.05f);
    // camera-relative wish direction (camera yaw 0 looks along +Z, right = -X)
    glm::vec3 fwd(std::sin(in.camYaw), 0, std::cos(in.camYaw));
    glm::vec3 right(-fwd.z, 0, fwd.x);
    glm::vec3 wish = fwd * in.move.y + right * in.move.x;
    float amount = std::min(glm::length(wish), 1.0f);
    if (amount > 1e-3f) wish /= glm::length(wish);
    float speed = (in.walk ? walkSpeed : runSpeed) * amount;

    // horizontal velocity: quick acceleration on the ground, little control in the air
    glm::vec3 target = wish * speed;
    float accel = onGround ? 30.0f : 6.0f;
    glm::vec3 h(vel.x, 0, vel.z);
    glm::vec3 dv = target - h;
    float maxDv = accel * dt;
    if (glm::length(dv) > maxDv) dv = glm::normalize(dv) * maxDv;
    vel.x += dv.x;
    vel.z += dv.z;

    if (amount > 1e-3f) {
        float want = std::atan2(wish.x, wish.z);
        float d = std::remainder(want - yaw, 2.0f * float(M_PI));
        yaw += std::clamp(d, -turnRate * dt, turnRate * dt);
    }

    if (onGround && in.jump) {
        vel.y = jumpSpeed;
        onGround = false;
    }
    vel.y -= gravity * dt;

    // move, then resolve walls with two spheres (knees and chest) and snap to the ground
    glm::vec3 p = pos + vel * dt;
    for (float hgt : {stepHeight + radius, 1.3f}) {
        glm::vec3 c = p + glm::vec3(0, hgt, 0);
        if (world.PushSphere(c, radius)) {
            p.x = c.x;
            p.z = c.z;
        }
    }
    float gy;
    float probe = stepHeight + (onGround ? stepHeight : 0.0f) + std::max(0.0f, -vel.y * dt);
    if (vel.y <= 0 && world.Ground(p + glm::vec3(0, stepHeight, 0), probe, gy)) {
        p.y = gy;
        vel.y = 0;
        onGround = true;
    } else {
        onGround = false;
    }
    // walls and steps changed the move: keep the velocity that actually happened (stops the run cycle
    // against a wall)
    if (dt > 0) {
        vel.x = (p.x - pos.x) / dt;
        vel.z = (p.z - pos.z) / dt;
    }
    pos = p;

    float hs = glm::length(glm::vec2(vel.x, vel.z));
    PlayerAnim next = !onGround ? PlayerAnim::Air
                      : hs < 0.3f ? PlayerAnim::Idle
                      : hs < (walkSpeed + runSpeed) * 0.5f ? PlayerAnim::Walk
                                                            : PlayerAnim::Run;
    if (next != anim) {
        anim = next;
        animTime = 0;
    }
    animTime += dt;
}

} // namespace med
