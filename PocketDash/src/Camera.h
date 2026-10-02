#pragma once

#include "Math.h"

namespace pd {

// 2D follow camera. `position()` is the world coordinate of the screen's
// top-left corner (screen = world - position).
//
// The target can move freely inside a small dead-zone without the view
// scrolling, which keeps small movements calm on a small screen. Outside it,
// the camera eases towards the target with a look-ahead in the movement
// direction so the player sees what is coming.
class Camera {
public:
    Camera(float viewW, float viewH) : viewW_(viewW), viewH_(viewH) {}

    // World size the camera is clamped to. A world smaller than the view is
    // centred instead. `topMargin` lets the view scroll that far above the
    // world so the top rows are not hidden under the HUD.
    void setBounds(float worldW, float worldH, float topMargin = 0.0f);

    // Jumps straight to the target (level start, respawn).
    void snapTo(Vec2 target);

    // `lookAhead` is an offset added to the target (e.g. velocity * 0.25).
    void follow(Vec2 target, Vec2 lookAhead, float dt);

    // Screen shake. Multiple shakes keep the strongest.
    void shake(float strength, float duration);
    void setShakeEnabled(bool enabled) { shakeEnabled_ = enabled; }

    // Integer-rounded top-left (including shake): rounding keeps pixel art
    // from shimmering while scrolling.
    Vec2 position() const;
    // Exact, unshaken position (for tests and debug).
    Vec2 rawPosition() const { return pos_; }

    float viewWidth() const { return viewW_; }
    float viewHeight() const { return viewH_; }

    static constexpr float kDeadZoneW = 48.0f;
    static constexpr float kDeadZoneH = 32.0f;

private:
    Vec2 clamp(Vec2 p) const;

    float viewW_;
    float viewH_;
    float worldW_ = 0.0f;
    float worldH_ = 0.0f;
    float topMargin_ = 0.0f;
    Vec2 pos_;
    Vec2 focus_; // point the camera is centred on, before clamping

    bool shakeEnabled_ = true;
    float shakeStrength_ = 0.0f;
    float shakeTime_ = 0.0f;
    float shakeDuration_ = 0.0f;
    float shakeClock_ = 0.0f;
};

} // namespace pd
