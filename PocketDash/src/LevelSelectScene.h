#pragma once

#include "Math.h"
#include "Scene.h"
#include "SdlPtr.h"

#include <string>
#include <vector>

namespace pd {

// World 1 map: the eight levels as stops along a path over a painted
// island. Shows lock state, stars found and Golden Stars, plus the selected
// level's records. A plays, X shows its high scores, B goes back.
class LevelSelectScene : public Scene {
public:
    // `selectId` picks the level highlighted first ("" = where to continue).
    explicit LevelSelectScene(Game& game, const std::string& selectId = {});

    const char* name() const override { return "levels"; }
    void update(float dt) override;
    void render(SDL_Renderer* r) override;

private:
    static constexpr int kMapX = 20;
    static constexpr int kMapY = 74;
    static constexpr int kMapW = 600;
    static constexpr int kMapH = 300;

    void buildMap(SDL_Renderer* r);
    void renderNode(SDL_Renderer* r, int i) const;
    void renderInfo(SDL_Renderer* r) const;
    Vec2 nodePos(int i) const; // screen position

    TexturePtr map_;
    std::vector<std::string> ids_;
    int index_ = 0;
    Vec2 heroPos_;
    bool heroFacingLeft_ = false;
    float time_ = 0.0f;
    float lockedFlash_ = 0.0f; // "locked" message after pressing A on a locked level
};

} // namespace pd
