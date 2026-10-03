#pragma once

#include "Camera.h"
#include "Level.h"
#include "LevelSession.h"
#include "Scene.h"
#include "TileSet.h"

#include <memory>
#include <string>

namespace pd {

// Gameplay scene: presents a LevelSession (the simulation) with a camera,
// sound, HUD and menus. Levels load from assets/levels/<id>.lvl; the id
// "test" (or a broken/missing file) plays the built-in test meadow.
class PlayScene : public Scene {
public:
    PlayScene(Game& game, std::string levelId);

    const char* name() const override { return "play"; }
    void update(float dt) override;
    void render(SDL_Renderer* r) override;
    void fillDebugInfo(DebugInfo& info) const override;
    void renderDebug(SDL_Renderer* r) const override;

private:
    enum class Overlay { None, Paused, Info, Sign };

    void restart();
    void handleEvents(unsigned events);
    void updatePauseMenu();
    void updateCleared();
    void renderWorld(SDL_Renderer* r, Vec2 cam) const;
    void renderHud(SDL_Renderer* r) const;
    void renderCollectionHud(SDL_Renderer* r) const;
    void renderPowerHud(SDL_Renderer* r) const;
    void renderBanner(SDL_Renderer* r) const;
    void renderKnockOut(SDL_Renderer* r, Vec2 cam) const;
    void renderPauseMenu(SDL_Renderer* r) const;
    void renderInfoPanel(SDL_Renderer* r) const;
    void renderClearPanel(SDL_Renderer* r) const;
    void renderTimeUpPanel(SDL_Renderer* r) const;
    void renderObjectiveHud(SDL_Renderer* r) const;
    void renderPrompt(SDL_Renderer* r, Vec2 cam) const;
    void renderSignDialog(SDL_Renderer* r) const;
    void renderToast(SDL_Renderer* r) const;
    void showToast(const char* text);
    // Banner/info subtitle for the objective, e.g. "RESCUE 3 FRIENDS!".
    void objectiveText(char* out, size_t size) const;
    void goToNextLevel();

    std::string levelId_;
    Level level_; // must outlive session_
    TileSet tiles_;
    Camera camera_;
    std::unique_ptr<LevelSession> session_;

    Overlay overlay_ = Overlay::None;
    int pauseIndex_ = 0;
    float animTime_ = 0.0f;  // drives tile, coin and HUD animation
    float hudHurt_ = 0.0f;   // hearts wobble after damage
    int debugEnemyIndex_ = 0; // L1 debug warp cycles through enemies
    char toast_[64] = {};
    float toastTime_ = 0.0f;  // seconds left on screen
    int signIndex_ = -1;      // sign being read
};

} // namespace pd
