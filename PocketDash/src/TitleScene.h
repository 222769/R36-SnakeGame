#pragma once

#include "Scene.h"

namespace pd {

// Title screen and main menu. Starts on "PRESS A" (attract); A opens the
// menu: Play, Level Select, Collection, High Scores, Settings, Quit.
// Other menus return here with the menu already open.
class TitleScene : public Scene {
public:
    explicit TitleScene(Game& game, bool startInMenu = false);

    const char* name() const override { return "title"; }
    void update(float dt) override;
    void render(SDL_Renderer* r) override;

private:
    void activate(int item);
    void renderLogo(SDL_Renderer* r, int y) const;
    void renderStats(SDL_Renderer* r) const;

    float time_ = 0.0f;
    bool menuOpen_ = false;
    float menuT_ = 0.0f; // 0..1 slide-in of the menu
    int index_ = 0;
};

} // namespace pd
