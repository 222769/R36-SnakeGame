#pragma once

#include <SDL.h>

#include <array>

namespace pd {

enum class PowerUpType {
    None,
    SpeedShoes,   // move faster
    ShieldBubble, // blocks one hit
    Magnet,       // attracts coins
    SuperDash,    // longer dash
    DoubleCoins,  // coins worth 2x
    TinyMode,     // fit through small gaps
    GiantMode,    // smash obstacles
    RainbowStar,  // invincibility
    Count
};

constexpr int kPowerUpCount = static_cast<int>(PowerUpType::Count);

struct PowerUpInfo {
    const char* name;
    float duration;   // seconds; 0 = lasts until used (shield)
    SDL_Color color;  // placeholder icon colour
};

const PowerUpInfo& powerUpInfo(PowerUpType type);

// Tracks which timed power-ups are active. The player holds one stored
// power-up (activated with X); several can be active at once.
class PowerUpState {
public:
    void store(PowerUpType type) { stored_ = type; }
    PowerUpType stored() const { return stored_; }

    // Activates the stored power-up. Returns the type activated (or None).
    PowerUpType activateStored();
    void activate(PowerUpType type);
    void consume(PowerUpType type); // e.g. shield absorbed a hit
    // Keeps an active power-up running for at least `seconds` more.
    void extend(PowerUpType type, float seconds);

    void update(float dt);
    void clear();

    bool active(PowerUpType type) const { return isValid(type) && timers_[static_cast<int>(type)] > 0.0f; }
    float timeLeft(PowerUpType type) const { return isValid(type) ? timers_[static_cast<int>(type)] : 0.0f; }

    // True for real power-ups (not None/Count); guards values read from saves.
    static bool isValid(PowerUpType type);

private:
    PowerUpType stored_ = PowerUpType::None;
    std::array<float, kPowerUpCount> timers_{};
};

} // namespace pd
