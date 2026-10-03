#pragma once

#include "Scene.h"

namespace pd {

// What you have found: gems per level, star and Golden Star totals, and the
// hero outfits that gems unlock (gems are never spent). A wears an outfit.
class CollectionScene : public Scene {
public:
    explicit CollectionScene(Game& game);

    const char* name() const override { return "collection"; }
    void update(float dt) override;
    void render(SDL_Renderer* r) override;

private:
    void renderTotals(SDL_Renderer* r) const;
    void renderGems(SDL_Renderer* r) const;
    void renderOutfits(SDL_Renderer* r) const;

    int index_ = 0;
    float time_ = 0.0f;
    float deniedTime_ = 0.0f;
};

} // namespace pd
