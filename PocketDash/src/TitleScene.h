#pragma once

#include "Scene.h"
#include "SdlPtr.h"

namespace pd {

// Phase 1 title screen: animated logo and "press A". Becomes the full main
// menu (Play / Level Select / Collection / High Scores / Settings / Quit) in
// Phase 6.
class TitleScene : public Scene {
public:
    explicit TitleScene(Game& game);

    const char* name() const override { return "title"; }
    void update(float dt) override;
    void render(SDL_Renderer* r) override;

private:
    void buildBackground(SDL_Renderer* r);

    TexturePtr background_;
    float time_ = 0.0f;
    float cloudScroll_ = 0.0f;
};

} // namespace pd
