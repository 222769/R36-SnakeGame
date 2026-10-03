#pragma once

#include "Math.h"
#include "SdlPtr.h"
#include "World.h"

#include <SDL.h>

#include <cstdint>
#include <vector>

namespace pd {

class Level;

// Draws a Level's tile grid.
//
// All tile art is generated once per world from its WorldTheme colours, at
// 16x16 and packed into a single atlas texture, then drawn at 2x. The art
// choice for every map cell (grass variant, hedge top or front face, shore
// water, bridge direction, ...) is precomputed in prepare(), so rendering a
// frame is just one texture copy per visible tile with no neighbour lookups.
class TileSet {
public:
    // Atlas slots. Animated tiles have their frames in consecutive slots.
    enum Art : uint8_t {
        kGrassA0, kGrassA1, kGrassA2, kGrassA3, // checker colour A, 4 variants
        kGrassB0, kGrassB1, kGrassB2, kGrassB3, // checker colour B
        kHedgeTop, kHedgeFront,
        kSecretTop, kSecretFront,
        kTree, kRock,
        kWater0, kWater1,
        kShore0, kShore1,                       // water with a grass bank above
        kBridgeV, kBridgeH,                     // walk north-south / east-west
        kThorns,
        kExit0, kExit1,
        kCrate, kBoulder,
        kTinyGap,                               // hedge with a little mouse-hole
        kArtCount
    };

    static constexpr int kArtSize = 16; // atlas pixels per tile (drawn at 2x)

    bool build(SDL_Renderer* renderer, const WorldTheme& theme);
    void prepare(const Level& level);
    void render(SDL_Renderer* r, Vec2 camera, float time) const;

    SDL_Texture* atlas() const { return atlas_.get(); }
    Art artAt(int tx, int ty) const;

private:
    TexturePtr atlas_;
    std::vector<Art> art_;
    int width_ = 0;
    int height_ = 0;
    SDL_Color border_{0, 0, 0, 255}; // fill outside the map
};

} // namespace pd
