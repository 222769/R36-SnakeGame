#pragma once

#include "SdlPtr.h"

#include <SDL.h>

#include <string>

namespace pd {

// Placeholder pixel art, generated at startup from palette strings so the
// game runs without any image files.
//
// Any sheet can be replaced by dropping a PNG with the same layout into
// assets/sprites/ (e.g. assets/sprites/player.png) — no code changes needed.
class Sprites {
public:
    // Player sheet layout: columns = animation frames, rows = facing.
    static constexpr int kPlayerFrameW = 16;
    static constexpr int kPlayerFrameH = 16;
    static constexpr int kPlayerFrames = 2;
    enum PlayerRow { kRowFront = 0, kRowBack = 1, kRowSide = 2, kPlayerRows = 3 };

    static constexpr int kHeartW = 9;
    static constexpr int kHeartH = 8;

    // Coin sheet: 3 frames of 12x12 in a row (full, turning, edge-on).
    static constexpr int kCoinSize = 12;
    static constexpr int kCoinFrames = 3;

    bool create(SDL_Renderer* renderer, const std::string& spriteDir);

    SDL_Texture* player() const { return player_.get(); }
    SDL_Texture* heartFull() const { return heartFull_.get(); }
    SDL_Texture* heartEmpty() const { return heartEmpty_.get(); }
    SDL_Texture* coin() const { return coin_.get(); }

    // Checks every built-in sprite for consistent row widths and known
    // palette characters (used by the unit tests).
    static bool validateBuiltinArt(std::string* error);

private:
    TexturePtr player_;
    TexturePtr heartFull_;
    TexturePtr heartEmpty_;
    TexturePtr coin_;
};

} // namespace pd
