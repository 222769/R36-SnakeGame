#pragma once

#include "Player.h"
#include "Scene.h"
#include "SdlPtr.h"

namespace pd {

// Gameplay scene. In Phase 1 this is a single-screen sandbox meadow used to
// tune movement; Phase 2 replaces the fixed arena with tile maps and a camera.
class PlayScene : public Scene {
public:
    explicit PlayScene(Game& game);

    const char* name() const override { return "play"; }
    void update(float dt) override;
    void render(SDL_Renderer* r) override;
    void fillDebugInfo(DebugInfo& info) const override;
    void renderDebug(SDL_Renderer* r) const override;

private:
    void buildBackground(SDL_Renderer* r);
    void updatePauseMenu();
    void renderHud(SDL_Renderer* r) const;
    void renderPauseMenu(SDL_Renderer* r) const;
    void renderInfoPanel(SDL_Renderer* r) const;
    void restart();

    Player player_;
    RectF arena_;
    TexturePtr background_;

    bool paused_ = false;
    int pauseIndex_ = 0;
    bool infoOpen_ = false;
    int hearts_ = 3;
    int maxHearts_ = 3;
    float time_ = 0.0f;
};

} // namespace pd
