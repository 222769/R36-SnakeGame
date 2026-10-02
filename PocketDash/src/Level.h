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
    Ground,     // walkable grass / floor
    Wall,       // hedge block (solid)
    Tree,       // solid decoration
    Rock,       // solid decoration
    Water,      // blocks walking (Phase 3: hazard with respawn)
    Bridge,     // walkable planks over water
    Hazard,     // thorns: damages on touch (Phase 3)
    SecretWall, // looks like a hedge, but can be walked through
    Exit,       // touching it completes the level
};

// One ASCII map character per tile (also used by the Phase 5 level files):
//   .  ground      #  hedge wall   T  tree     o  rock
//   ~  water       =  bridge       ^  thorns   %  secret wall
//   E  exit        P  player spawn (ground)    c  coin (ground)
char tileToChar(Tile t);

// Result of a collision move (see moveAndCollide).
struct CollisionResult {
    bool hitX = false;
    bool hitY = false;
};

// A tile-based level: the tile grid plus entity spawn points.
class Level {
public:
    Level() = default;
    Level(int width, int height);

    // Builds a level from map rows (see legend above). Returns false and
    // fills `error` if the map is malformed (ragged rows, unknown
    // characters, missing spawn or exit). Metadata (id, name, world) is left
    // empty for the caller to fill in.
    static bool fromAscii(const std::vector<std::string>& rows, Level& out, std::string* error);

    int width() const { return width_; }
    int height() const { return height_; }
    float pixelWidth() const;
    float pixelHeight() const;

    bool inBounds(int tx, int ty) const { return tx >= 0 && ty >= 0 && tx < width_ && ty < height_; }
    Tile tileAt(int tx, int ty) const;
    Tile tileAtPixel(float px, float py) const;
    void setTile(int tx, int ty, Tile t);

    static bool isSolid(Tile t) { return t == Tile::Wall || t == Tile::Tree || t == Tile::Rock || t == Tile::Water; }
    // True if any tile overlapped by `box` is solid.
    bool overlapsSolid(const RectF& box) const;
    // True if any tile overlapped by `box` is of type `t`.
    bool overlapsTile(const RectF& box, Tile t) const;

    // Tile-space centre of a tile, in pixels.
    static Vec2 tileCenter(int tx, int ty);

    std::string id;
    std::string name;
    WorldId world = WorldId::SunnyMeadows;
    Vec2 spawn;
    Vec2 exit;
    std::vector<Vec2> coins;

private:
    int width_ = 0;
    int height_ = 0;
    std::vector<Tile> tiles_;
};

// Moves `box` by `delta`, stopping at solid tiles. Axes are resolved
// separately (X then Y) so the box slides along walls, and long moves are
// split into small sub-steps so fast dashes can never tunnel through a tile.
//
// With `cornerNudge` > 0, a box blocked by the very corner of a wall is
// pushed sideways by up to that many pixels so it slips around it. This
// makes doorways and gaps feel forgiving instead of sticky.
//
// Returns the distance actually moved.
Vec2 moveAndCollide(const Level& level, const RectF& box, Vec2 delta, float cornerNudge = 0.0f,
                    CollisionResult* result = nullptr);

} // namespace pd
