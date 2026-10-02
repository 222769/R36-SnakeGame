#include "Camera.h"

#include <cmath>

namespace pd {

void Camera::setBounds(float worldW, float worldH, float topMargin) {
    worldW_ = worldW;
    worldH_ = worldH;
    topMargin_ = topMargin;
    pos_ = clamp(pos_);
}

Vec2 Camera::clamp(Vec2 p) const {
    // Smaller than the screen: centre the world. Otherwise keep the view inside.
    p.x = worldW_ <= viewW_ ? (worldW_ - viewW_) * 0.5f : clampf(p.x, 0.0f, worldW_ - viewW_);
    const float h = worldH_ + topMargin_;
    p.y = h <= viewH_ ? (worldH_ - topMargin_ - viewH_) * 0.5f : clampf(p.y, -topMargin_, worldH_ - viewH_);
    return p;
}

void Camera::snapTo(Vec2 target) {
    focus_ = target;
    pos_ = clamp(target - Vec2{viewW_ * 0.5f, viewH_ * 0.5f});
}

void Camera::follow(Vec2 target, Vec2 lookAhead, float dt) {
    // Drag the focus point along when the target leaves the dead-zone.
    const Vec2 goal = target + lookAhead;
    const float halfW = kDeadZoneW * 0.5f;
    const float halfH = kDeadZoneH * 0.5f;
    if (goal.x < focus_.x - halfW) focus_.x = goal.x + halfW;
    if (goal.x > focus_.x + halfW) focus_.x = goal.x - halfW;
    if (goal.y < focus_.y - halfH) focus_.y = goal.y + halfH;
    if (goal.y > focus_.y + halfH) focus_.y = goal.y - halfH;

    // Frame-rate independent exponential ease towards the focus.
    const Vec2 desired = clamp(focus_ - Vec2{viewW_ * 0.5f, viewH_ * 0.5f});
    const float t = 1.0f - std::exp(-8.0f * dt);
    pos_ += (desired - pos_) * t;

    if (shakeTime_ > 0.0f) {
        shakeTime_ -= dt;
        shakeClock_ += dt;
    }
}

void Camera::shake(float strength, float duration) {
    if (!shakeEnabled_) return;
    if (shakeTime_ > 0.0f && strength < shakeStrength_ * (shakeTime_ / shakeDuration_)) return;
    shakeStrength_ = strength;
    shakeDuration_ = duration;
    shakeTime_ = duration;
}

Vec2 Camera::position() const {
    Vec2 p = pos_;
    if (shakeEnabled_ && shakeTime_ > 0.0f && shakeDuration_ > 0.0f) {
        // Decaying shake built from two out-of-phase sines: deterministic,
        // cheap and smoother than random jitter.
        const float amount = shakeStrength_ * (shakeTime_ / shakeDuration_);
        p.x += std::sin(shakeClock_ * 71.0f) * amount;
        p.y += std::sin(shakeClock_ * 53.0f + 1.3f) * amount;
    }
    return {std::round(p.x), std::round(p.y)};
}

} // namespace pd
