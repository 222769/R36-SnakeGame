#pragma once

#include "Camera.h"
#include "Level.h"
#include "LevelSession.h"
#include "Scene.h"
#include "TileSet.h"

#include <memory>

namespace pd {

// Gameplay scene: presents a LevelSession (the simulation) with a camera,
// sound, HUD and menus. Phase 3 plays the built-in test meadow; Phase 5
// loads World 1 from files.
class PlayScene : public Scene {
public:
    explicit PlayScene(Game& game);

    const char* name() const override { return "play"; }
    void update(float dt) override;
    void render(SDL_Renderer* r) override;
    void fillDebugInfo(DebugInfo& info) const override;
    void renderDebug(SDL_Renderer* r) const override;

private:
    enum class Overlay { None, Paused, Info };

    void restart();
    void handleEvents(unsigned events);
    void updatePauseMenu();
    void updateCleared();
    void renderWorld(SDL_Renderer* r, Vec2 cam) const;
    void renderHud(SDL_Renderer* r) const;
    void renderBanner(SDL_Renderer* r) const;
    void renderKnockOut(SDL_Renderer* r, Vec2 cam) const;
    void renderPauseMenu(SDL_Renderer* r) const;
    void renderInfoPanel(SDL_Renderer* r) const;
    void renderClearPanel(SDL_Renderer* r) const;

    Level level_; // must outlive session_
    TileSet tiles_;
    Camera camera_;
    std::unique_ptr<LevelSession> session_;

    Overlay overlay_ = Overlay::None;
    int pauseIndex_ = 0;
    float animTime_ = 0.0f;  // drives tile, coin and HUD animation
    float hudHurt_ = 0.0f;   // hearts wobble after damage
    int debugEnemyIndex_ = 0; // L1 debug warp cycles through enemies
};

} // namespace pd
