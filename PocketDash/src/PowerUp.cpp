#include "PowerUp.h"

#include <limits>

namespace pd {

namespace {

const PowerUpInfo kInfo[kPowerUpCount] = {
    {"NONE", 0.0f, {255, 255, 255, 255}},
    {"SPEED SHOES", 8.0f, {255, 140, 40, 255}},
    {"SHIELD BUBBLE", 0.0f, {120, 200, 255, 255}},
    {"MAGNET", 10.0f, {230, 60, 70, 255}},
    {"SUPER DASH", 10.0f, {170, 110, 255, 255}},
    {"DOUBLE COINS", 12.0f, {255, 214, 64, 255}},
    {"TINY MODE", 10.0f, {120, 230, 160, 255}},
    {"GIANT MODE", 8.0f, {150, 90, 60, 255}},
    {"RAINBOW STAR", 6.0f, {255, 120, 200, 255}},
};

} // namespace

bool PowerUpState::isValid(PowerUpType type) {
    return type > PowerUpType::None && type < PowerUpType::Count;
}

const PowerUpInfo& powerUpInfo(PowerUpType type) {
    const int i = static_cast<int>(type);
    return kInfo[(i >= 0 && i < kPowerUpCount) ? i : 0];
}

PowerUpType PowerUpState::activateStored() {
    const PowerUpType type = stored_;
    if (type == PowerUpType::None) return type;
    stored_ = PowerUpType::None;
    activate(type);
    return type;
}

void PowerUpState::activate(PowerUpType type) {
    if (!isValid(type)) return;
    const float duration = powerUpInfo(type).duration;
    // Untimed power-ups (shield) stay until consumed.
    timers_[static_cast<int>(type)] = duration > 0.0f ? duration : std::numeric_limits<float>::infinity();
    // Tiny and Giant are mutually exclusive.
    if (type == PowerUpType::TinyMode) timers_[static_cast<int>(PowerUpType::GiantMode)] = 0.0f;
    if (type == PowerUpType::GiantMode) timers_[static_cast<int>(PowerUpType::TinyMode)] = 0.0f;
}

void PowerUpState::consume(PowerUpType type) {
    if (isValid(type)) timers_[static_cast<int>(type)] = 0.0f;
}

void PowerUpState::update(float dt) {
    for (float& t : timers_)
        if (t > 0.0f) t -= dt;
}

void PowerUpState::clear() {
    stored_ = PowerUpType::None;
    timers_.fill(0.0f);
}

} // namespace pd
