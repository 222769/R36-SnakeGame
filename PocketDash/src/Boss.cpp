#include "Boss.h"

#include <algorithm>
#include <cmath>

namespace pd {

namespace {

// Phase 1 is gentle; each phase is a little quicker and adds a twist.
const Boss::PhaseRules kPhases[3] = {
    // idle telegraph leap dazed ringSpeed rings leaps walk
    {1.5f, 0.95f, 1.0f, 2.6f, 150.0f, 1, 1, 26.0f},
    {1.2f, 0.80f, 0.9f, 2.2f, 170.0f, 2, 1, 34.0f},
    {0.9f, 0.70f, 0.8f, 2.0f, 190.0f, 1, 2, 42.0f},
};
constexpr float kHurtTime = 0.7f;
constexpr float kLandedTime = 0.35f;
constexpr float kSecondLeapTelegraph = 0.4f;
constexpr float kSecondRingDelay = 0.45f;
constexpr float kKnockbackDistance = 56.0f;

} // namespace

const Boss::PhaseRules& Boss::rules(int phase) { return kPhases[std::clamp(phase, 1, 3) - 1]; }

const char* bossStateName(Boss::State s) {
    switch (s) {
    case Boss::State::Sleeping: return "sleeping";
    case Boss::State::Intro: return "intro";
    case Boss::State::Idle: return "idle";
    case Boss::State::Telegraph: return "telegraph";
    case Boss::State::Leap: return "leap";
    case Boss::State::Landed: return "landed";
    case Boss::State::Dazed: return "dazed";
    case Boss::State::Hurt: return "hurt";
    case Boss::State::Defeated: return "defeated";
    }
    return "?";
}

void Boss::reset(Vec2 home, RectF arena) {
    home_ = home;
    arena_ = arena;
    health_ = kMaxHealth;
    introShown_ = false;
    leapsDone_ = 0;
    sleep();
}

void Boss::sleep() {
    pos_ = home_;
    z_ = 0.0f;
    target_ = home_;
    knockback_ = {};
    leapsLeft_ = 0;
    landed_ = false;
    for (Ring& r : rings_) r = Ring{};
    if (health_ <= 0) {
        enter(State::Defeated);
        return;
    }
    enter(State::Sleeping);
}

void Boss::wake(bool longIntro) {
    if (state_ != State::Sleeping) return;
    enter(State::Intro);
    stateLength_ = longIntro && !introShown_ ? kIntroTime : kShortIntroTime;
    introShown_ = true;
}

int Boss::phase() const { return std::clamp(1 + hitsTaken() / 2, 1, 3); }

bool Boss::hurtsOnContact() const {
    return z_ < 12.0f && (state_ == State::Idle || state_ == State::Telegraph || state_ == State::Landed ||
                          state_ == State::Intro);
}

float Boss::leapProgress() const {
    if (state_ != State::Leap || stateLength_ <= 0.0f) return 0.0f;
    return std::clamp(stateTime_ / stateLength_, 0.0f, 1.0f);
}

void Boss::enter(State s) {
    state_ = s;
    stateTime_ = 0.0f;
    const PhaseRules& p = rules(phase());
    switch (s) {
    case State::Idle: stateLength_ = p.idleTime; break;
    case State::Telegraph: stateLength_ = leapsLeft_ < p.leapsPerAttack ? kSecondLeapTelegraph : p.telegraphTime; break;
    case State::Leap: stateLength_ = p.leapTime; break;
    case State::Landed: stateLength_ = kLandedTime; break;
    case State::Dazed: stateLength_ = p.dazedTime; break;
    case State::Hurt: stateLength_ = kHurtTime; break;
    default: stateLength_ = 0.0f; break;
    }
}

Vec2 Boss::clampToArena(Vec2 p) const {
    const float m = kBodyRadius + 4.0f;
    return {std::clamp(p.x, arena_.x + m, arena_.x + arena_.w - m),
            std::clamp(p.y, arena_.y + m, arena_.y + arena_.h - 6.0f)};
}

void Boss::spawnRings() {
    const PhaseRules& p = rules(phase());
    int spawned = 0;
    for (Ring& r : rings_) {
        if (r.active) continue;
        r = Ring{};
        r.active = true;
        r.center = pos_;
        r.speed = p.ringSpeed;
        r.delay = static_cast<float>(spawned) * kSecondRingDelay;
        if (++spawned >= p.ringsPerLanding) break;
    }
}

void Boss::update(float dt, Vec2 playerPos) {
    landed_ = false;
    stateTime_ += dt;

    // Rings keep spreading whatever the boss does.
    for (Ring& r : rings_) {
        if (!r.active) continue;
        if (r.delay > 0.0f) {
            r.delay -= dt;
            continue;
        }
        r.radius += r.speed * dt;
        if (r.radius > kRingMaxRadius) r.active = false;
    }

    const PhaseRules& p = rules(phase());
    switch (state_) {
    case State::Sleeping:
    case State::Defeated: break;
    case State::Intro:
        if (stateTime_ >= stateLength_) enter(State::Idle);
        break;
    case State::Idle: {
        // Shuffle slowly towards the player.
        const Vec2 d = playerPos - pos_;
        if (d.lengthSq() > 40.0f * 40.0f) pos_ = clampToArena(pos_ + d.normalized() * (p.walkSpeed * dt));
        if (stateTime_ >= stateLength_) {
            leapsLeft_ = p.leapsPerAttack;
            enter(State::Telegraph);
        }
        break;
    }
    case State::Telegraph:
        if (stateTime_ >= stateLength_) {
            leapStart_ = pos_;
            target_ = clampToArena(playerPos); // where the player stands now
            --leapsLeft_;
            enter(State::Leap);
        }
        break;
    case State::Leap: {
        const float t = leapProgress();
        pos_ = leapStart_ + (target_ - leapStart_) * t;
        z_ = 4.0f * kLeapHeight * t * (1.0f - t);
        if (t >= 1.0f) {
            pos_ = target_;
            z_ = 0.0f;
            landed_ = true;
            ++leapsDone_;
            spawnRings();
            enter(State::Landed);
        }
        break;
    }
    case State::Landed:
        if (stateTime_ >= stateLength_) enter(leapsLeft_ > 0 ? State::Telegraph : State::Dazed);
        break;
    case State::Dazed:
        if (stateTime_ >= stateLength_) enter(State::Idle);
        break;
    case State::Hurt:
        // Slide back from the blow, easing out.
        pos_ = clampToArena(pos_ + knockback_ * (dt * std::max(0.0f, 1.0f - stateTime_ / kHurtTime) * 2.0f));
        if (stateTime_ >= stateLength_) enter(health_ <= 0 ? State::Defeated : State::Idle);
        break;
    }
}

void Boss::debugDefeat() {
    if (!fighting()) return;
    health_ = 0;
    knockback_ = {};
    z_ = 0.0f;
    for (Ring& r : rings_) r.active = false;
    enter(State::Hurt);
}

bool Boss::hit(Vec2 from) {
    if (!vulnerable()) return false;
    --health_;
    Vec2 away = pos_ - from;
    if (away.lengthSq() < 1.0f) away = {0.0f, -1.0f};
    knockback_ = away.normalized() * (kKnockbackDistance / kHurtTime);
    for (Ring& r : rings_) r.active = false; // no surprise hits right after a success
    enter(State::Hurt);
    return true;
}

} // namespace pd
