#pragma once

#include "Math.h"

#include <SDL.h>

namespace pd {

class Game;

// Values a scene exposes to the debug overlay and the smoke test.
struct DebugInfo {
    const char* levelId = "-";
    bool hasPlayer = false;
    Vec2 playerPos;
    Vec2 playerVel;
    float playerZ = 0.0f;
    int enemyCount = 0;
    int coins = 0;
    int coinsTotal = 0;
    int hearts = 0;
    int maxHearts = 0;
    bool hasCamera = false;
    Vec2 camera;
    bool levelClear = false;
};

// A screen of the game (title, gameplay, menus...). The Game owns exactly
// one active scene and swaps it between update steps.
class Scene {
public:
    explicit Scene(Game& game) : game_(game) {}
    virtual ~Scene() = default;
    Scene(const Scene&) = delete;
    Scene& operator=(const Scene&) = delete;

    virtual const char* name() const = 0;
    virtual void update(float dt) = 0;
    virtual void render(SDL_Renderer* r) = 0;

    virtual void fillDebugInfo(DebugInfo& /*info*/) const {}
    // Draws collision boxes etc. on top of the scene when debug mode is on.
    virtual void renderDebug(SDL_Renderer* /*r*/) const {}

protected:
    Game& game_;
};

} // namespace pd
