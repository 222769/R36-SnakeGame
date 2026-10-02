#pragma once

#include "Math.h"
#include "World.h"

#include <cstdint>
#include <string>
#include <vector>

namespace pd {

// Tile types for the ground layer of a level. Entities (coins, enemies,
// stars, ...) live in separate lists, not in the tile grid.
enum class Tile : uint8_t {
    Empty,      // walkable ground
    Wall,       // solid
    Water,      // hurts / blocks unless on a platform
    Hazard,     // spikes / thorns: damages on touch
    SecretWall, // looks solid, can be walked through
    Exit,
};

// A tile-based level. Phase 1 only defines the data model; rendering and
// collision arrive in Phase 2 and file loading in Phase 5.
//
// Planned on-disk format (assets/levels/1-1.lvl), plain text:
//
//   id=1-1
//   name=WELCOME MEADOW
//   world=0
//   objective=reach_exit
//   time_limit=0
//   [map]
//   ####################
//   #P....c.c.c......E#
//   #....~~~~.........#
//   ####################
//   [entities]
//   enemy=slime,10,4
//
// One character per tile; letters like P (spawn), E (exit), c (coin) and
// * (star) place entities on the grid.
class Level {
public:
    Level() = default;
    Level(int width, int height);

    int width() const { return width_; }
    int height() const { return height_; }
    float pixelWidth() const;
    float pixelHeight() const;

    bool inBounds(int tx, int ty) const { return tx >= 0 && ty >= 0 && tx < width_ && ty < height_; }
    Tile tileAt(int tx, int ty) const;
    void setTile(int tx, int ty, Tile t);
    static bool isSolid(Tile t) { return t == Tile::Wall; }

    std::string id;
    std::string name;
    WorldId world = WorldId::SunnyMeadows;
    Vec2 spawn;
    Vec2 exit;

private:
    int width_ = 0;
    int height_ = 0;
    std::vector<Tile> tiles_;
};

} // namespace pd
