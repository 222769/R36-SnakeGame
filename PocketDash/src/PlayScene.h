#pragma once

#include "Camera.h"
#include "Collectibles.h"
#include "Effects.h"
#include "Level.h"
#include "Player.h"
#include "Scene.h"
#include "TileSet.h"

namespace pd {

// Gameplay scene: one level with its tile map, camera, coins and exit.
// Phase 2 plays the built-in test meadow; Phase 5 loads World 1 from files.
class PlayScene : public Scene {
public:
    explicit PlayScene(Game& game);

    const char* name() const override { return "play"; }
    void update(float dt) override;
    void render(SDL_Renderer* r) override;
    void fillDebugInfo(DebugInfo& info) const override;
    void renderDebug(SDL_Renderer* r) const override;

private:
    enum class State { Playing, Paused, Info, Clear };

    void restart();
    void updatePlaying(float dt);
    void updatePauseMenu();
    void updateClear(float dt);
    void debugWarpToExit();
    void renderHud(SDL_Renderer* r) const;
    void renderBanner(SDL_Renderer* r) const;
    void renderPauseMenu(SDL_Renderer* r) const;
    void renderInfoPanel(SDL_Renderer* r) const;
    void renderClearPanel(SDL_Renderer* r) const;

    Level level_;
    TileSet tiles_;
    Camera camera_;
    CoinField coins_;
    Effects effects_;
    Player player_;

    State state_ = State::Playing;
    int pauseIndex_ = 0;
    int hearts_ = 3;
    int maxHearts_ = 3;
    float levelTime_ = 0.0f;  // gameplay clock (stops when paused / cleared)
    float animTime_ = 0.0f;   // drives tile and coin animation
    float stateTime_ = 0.0f;  // time spent in the current state
};

} // namespace pd
