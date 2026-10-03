#pragma once

#include "PowerUp.h"
#include "SdlPtr.h"

#include <SDL.h>

#include <string>

namespace pd {

// Smooth, shaded artwork painted at startup with Canvas (anti-aliased shapes,
// lit spheres) so the game runs without any image files. Every sheet is
// drawn 1:1 on screen.
//
// Any sheet can be replaced by dropping a PNG with the same layout into
// assets/sprites/ (e.g. assets/sprites/player.png) — no code changes needed.
class Sprites {
public:
    // Player sheet layout: columns = animation frames, rows = facing.
    static constexpr int kPlayerFrameW = 32;
    static constexpr int kPlayerFrameH = 40;
    static constexpr int kPlayerFrames = 2;
    enum PlayerRow { kRowFront = 0, kRowBack = 1, kRowSide = 2, kPlayerRows = 3 };

    static constexpr int kHeartW = 28;
    static constexpr int kHeartH = 24;

    // Big hero portrait for the title screen: 2 frames side by side.
    static constexpr int kHeroLargeW = 80;
    static constexpr int kHeroLargeH = 100;
    // Outfit previews (collection screen): one 64x80 frame per outfit.
    static constexpr int kOutfitPreviewW = 64;
    static constexpr int kOutfitPreviewH = 80;

    // Coin sheet: 3 frames of 24x24 in a row (full, turning, edge-on).
    static constexpr int kCoinSize = 24;
    static constexpr int kCoinFrames = 3;

    // Enemy sheet: 32x32 frames, 2 columns (animation) x 4 rows (type).
    static constexpr int kEnemyFrame = 32;
    enum EnemyRow { kRowSlime = 0, kRowBeetle = 1, kRowMushroom = 2, kRowFriend = 3, kEnemyRows = 4 };
    // Props sheet (checkpoint()): 32x32 frames: flag not reached, flag reached, sign.
    static constexpr int kPropFlag = 0;
    static constexpr int kPropFlagReached = 1;
    static constexpr int kPropSign = 2;

    // Item sheet: 24x24 frames in a row: star, empty star, gem, then one
    // icon per power-up (see itemFrame).
    static constexpr int kItemSize = 24;
    static constexpr int kItemStar = 0;
    static constexpr int kItemStarEmpty = 1;
    static constexpr int kItemGem = 2;
    static constexpr int kItemKey = 3 + kPowerUpCount - 1;
    static constexpr int kItemFrames = kItemKey + 1;
    static constexpr int itemFrame(PowerUpType t) { return 2 + static_cast<int>(t); }

    bool create(SDL_Renderer* renderer, const std::string& spriteDir);
    // Repaints the hero (game sheet unless replaced by a PNG, and the title
    // hero) in an outfit from Progress.h.
    bool setOutfit(SDL_Renderer* renderer, int outfitIndex);

    SDL_Texture* player() const { return player_.get(); }
    SDL_Texture* heroLarge() const { return heroLarge_.get(); }
    SDL_Texture* outfitPreviews() const { return outfitPreviews_.get(); }
    SDL_Texture* heartFull() const { return heartFull_.get(); }
    SDL_Texture* heartEmpty() const { return heartEmpty_.get(); }
    SDL_Texture* coin() const { return coin_.get(); }
    SDL_Texture* enemies() const { return enemies_.get(); }
    SDL_Texture* checkpoint() const { return checkpoint_.get(); }
    SDL_Texture* items() const { return items_.get(); }

    // Paints every built-in sprite and checks the result is sane (used by
    // the unit tests).
    static bool validateBuiltinArt(std::string* error);

private:
    TexturePtr player_;
    TexturePtr heroLarge_;
    TexturePtr outfitPreviews_;
    bool playerOverridden_ = false;
    TexturePtr heartFull_;
    TexturePtr heartEmpty_;
    TexturePtr coin_;
    TexturePtr enemies_;
    TexturePtr checkpoint_;
    TexturePtr items_;
};

} // namespace pd
