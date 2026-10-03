#pragma once

#include "Scene.h"

#include <string>
#include <vector>

namespace pd {

// Top-5 table for one level; Left/Right flips through the levels. B returns
// to the level select map or the main menu, whichever opened it.
class HighScoresScene : public Scene {
public:
    HighScoresScene(Game& game, const std::string& levelId, bool fromLevelSelect);

    const char* name() const override { return "scores"; }
    void update(float dt) override;
    void render(SDL_Renderer* r) override;

private:
    std::vector<std::string> ids_;
    int index_ = 0;
    bool fromLevelSelect_ = false;
    float time_ = 0.0f;
};

} // namespace pd
