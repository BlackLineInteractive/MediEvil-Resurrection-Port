#include "Core/Player.h"

#include <algorithm>
#include <cmath>

namespace med {

glm::vec3 Player::Facing() const { return {std::sin(yaw), 0, std::cos(yaw)}; }
glm::vec3 Player::HitCentre() const { return pos + Facing() * 1.0f + glm::vec3(0, 1.0f, 0); }

void Player::Spawn(const CollisionWorld& world, glm::vec3 at) {
    float y;
    if (world.Ground(at + glm::vec3(0, 2, 0), 200.0f, y)) at.y = y;
    pos = at;
    vel = glm::vec3(0);
    onGround = true;
    action = PlayerAction::Move;
    animator.Play(anims.idle, nullptr, 0, 1, 0);
}

// Idle / walk / run / sprint: neighbouring cycles share a phase and are blended by speed. The dominant
// cycle sets the step frequency and the rate follows the ground speed, so the feet stay planted.
void Player::Locomotion(float speed) {
    if (speed < 0.25f) {
        animator.Play(anims.idle, nullptr, 0, 1, 0.2f);
        return;
    }
    const Animation* lo = anims.walk;
    const Animation* hi = anims.run;
    float loV = walkSpeed, hiV = runSpeed, loN = anims.walkNatural, hiN = anims.runNatural;
    if (speed > runSpeed && anims.sprint) {
        lo = anims.run;
        hi = anims.sprint;
        loV = runSpeed;
        hiV = sprintSpeed;
        loN = anims.runNatural;
        hiN = anims.sprintNatural;
    }
    float w = std::clamp((speed - loV) / std::max(hiV - loV, 0.01f), 0.0f, 1.0f);
    if (w < 0.5f) animator.Play(lo, hi, w, std::clamp(speed / loN, 0.5f, 1.6f), 0.2f);
    else animator.Play(hi, lo, 1.0f - w, std::clamp(speed / hiN, 0.5f, 1.6f), 0.2f);
}

void Player::Update(const CollisionWorld& world, const PlayerInput& in, float dt) {
    dt = std::min(dt, 0.05f);
    hitFrame = false;

    // camera-relative wish direction (camera yaw 0 looks along +Z, right = -X)
    glm::vec3 fwd(std::sin(in.camYaw), 0, std::cos(in.camYaw));
    glm::vec3 right(-fwd.z, 0, fwd.x);
    glm::vec3 wish = fwd * in.move.y + right * in.move.x;
    float amount = std::min(glm::length(wish), 1.0f);
    if (amount > 1e-3f) wish /= glm::length(wish);
    // walk by default, run with Shift, short sprint burst with Shift + Ctrl (limited, then recharges)
    bool sprinting = in.run && in.sprint && amount > 0.1f && sprintLeft_ > 0 && onGround;
    if (sprinting) sprintLeft_ = std::max(0.0f, sprintLeft_ - dt);
    else if (!(in.run && in.sprint)) sprintLeft_ = std::min(sprintMax, sprintLeft_ + dt * sprintMax / sprintRecharge);
    float speed = (sprinting ? sprintSpeed : in.run ? runSpeed : walkSpeed) * amount;

    // actions that start from the ground
    bool jumpPressed = in.jump && !prevJump_;
    prevJump_ = in.jump;
    if (onGround && (action == PlayerAction::Move || action == PlayerAction::Land)) {
        if (jumpPressed) {
            action = PlayerAction::JumpStart;
            vel.y = jumpSpeed;
            onGround = false;
            animator.Play(anims.jumpStart, nullptr, 0, 1, 0.08f, true);
        } else if (in.attack && anims.attack[0]) {
            action = PlayerAction::Attack;
            combo = 0;
            comboQueued = false;
            hitDone_ = false;
            animator.Play(anims.attack[0], nullptr, 0, 1.15f, 0.08f, true);
        }
    } else if (action == PlayerAction::Attack && in.attack && animator.Progress() > 0.3f) {
        comboQueued = true;
    }

    // attacking roots Dan (turning stays possible early in the swing)
    if (action == PlayerAction::Attack) speed *= 0.15f;

    glm::vec3 target = wish * speed;
    float accel = onGround ? 30.0f : 6.0f;
    glm::vec3 h(vel.x, 0, vel.z);
    glm::vec3 dv = target - h;
    float maxDv = accel * dt;
    if (glm::length(dv) > maxDv) dv = glm::normalize(dv) * maxDv;
    vel.x += dv.x;
    vel.z += dv.z;

    if (amount > 1e-3f && (action != PlayerAction::Attack || animator.Progress() < 0.25f)) {
        float want = std::atan2(wish.x, wish.z);
        float d = std::remainder(want - yaw, 2.0f * float(M_PI));
        yaw += std::clamp(d, -turnRate * dt, turnRate * dt);
    }
    vel.y -= gravity * dt;

    // move, resolve walls with two spheres (knees and chest), snap to the ground
    glm::vec3 p = pos + vel * dt;
    for (float hgt : {stepHeight + radius, 1.3f}) {
        glm::vec3 c = p + glm::vec3(0, hgt, 0);
        if (world.PushSphere(c, radius)) {
            p.x = c.x;
            p.z = c.z;
        }
    }
    bool wasGround = onGround;
    float gy;
    float probe = stepHeight + (onGround ? stepHeight : 0.0f) + std::max(0.0f, -vel.y * dt);
    if (vel.y <= 0 && world.Ground(p + glm::vec3(0, stepHeight, 0), probe, gy)) {
        p.y = gy;
        vel.y = 0;
        onGround = true;
    } else {
        onGround = false;
    }
    // keep the velocity that actually happened (stops the run cycle against a wall)
    if (dt > 0) {
        vel.x = (p.x - pos.x) / dt;
        vel.z = (p.z - pos.z) / dt;
    }
    pos = p;
    float hs = glm::length(glm::vec2(vel.x, vel.z));
    // smoothed ground speed for the animation (the measured one jitters on slopes and walls)
    animSpeed_ += (hs - animSpeed_) * std::min(1.0f, dt * 10.0f);

    // action state machine
    switch (action) {
    case PlayerAction::JumpStart:
        if (animator.Finished() || animator.Progress() > 0.9f) {
            action = PlayerAction::Air;
            animator.Play(anims.air, nullptr, 0, 1, 0.1f, true);
        }
        if (onGround && vel.y <= 0 && animator.Progress() > 0.5f) {
            action = PlayerAction::Land;
            animator.Play(anims.land, nullptr, 0, 1.3f, 0.08f, true);
        }
        break;
    case PlayerAction::Air:
        if (onGround) {
            action = PlayerAction::Land;
            animator.Play(anims.land, nullptr, 0, 1.3f, 0.08f, true);
        }
        break;
    case PlayerAction::Land:
        if (!onGround) {
            action = PlayerAction::Air;
            animator.Play(anims.air, nullptr, 0, 1, 0.15f, true);
        } else if (animator.Finished() || (amount > 0.1f && animator.Progress() > 0.5f)) {
            action = PlayerAction::Move;
            Locomotion(animSpeed_);
        }
        break;
    case PlayerAction::Attack:
        if (!hitDone_ && animator.Progress() > 0.35f) {
            hitFrame = true;
            hitDone_ = true;
        }
        if (animator.Finished()) {
            if (comboQueued && combo < 2 && anims.attack[combo + 1]) {
                ++combo;
                comboQueued = false;
                hitDone_ = false;
                animator.Play(anims.attack[combo], nullptr, 0, 1.15f, 0.06f, true);
            } else {
                action = PlayerAction::Move;
                Locomotion(animSpeed_);
            }
        }
        break;
    case PlayerAction::Move:
        if (!onGround && wasGround) {
            // walked off a ledge: fall only if the drop is real (not a small step down)
            float below;
            if (!world.Ground(pos + glm::vec3(0, 0.1f, 0), 1.0f, below)) {
                action = PlayerAction::Air;
                animator.Play(anims.air, nullptr, 0, 1, 0.15f, true);
                break;
            }
        }
        Locomotion(animSpeed_);
        break;
    }
    animator.Update(dt);
}

} // namespace med
