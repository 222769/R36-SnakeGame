#pragma once

#include "EntityTypes.h"
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
    Water,      // walkable but you fall in: costs a heart, back to the last safe spot
                // (hop over a narrow stream, or skim across while dashing)
    Bridge,     // walkable planks over water
    Hazard,     // thorns: hurt on touch unless hopping over them
    SecretWall, // looks like a hedge, but can be walked through
    Exit,       // touching it completes the level
    Crate,      // solid; smashed by a dash or by Giant Mode
    Boulder,    // solid; only Giant Mode can smash it
    TinyGap,    // a hole in the hedge: solid unless the player is in Tiny Mode
};

// Collision exceptions, e.g. Tiny Mode slipping through tiny gaps.
enum CollisionPass : unsigned {
    kPassNone = 0,
    kPassTinyGaps = 1u << 0,
};

// One ASCII map character per tile (also used by the Phase 5 level files).
// Tiles:
//   .  ground      #  hedge wall   T  tree     o  rock
//   ~  water       =  bridge       ^  thorns   %  secret wall
//   x  crate       X  boulder      :  tiny gap E  exit
// Entities (placed on ground):
//   P  player spawn    c  coin          h  heart pickup
//   s  slime           b  beetle (patrols left/right)
//   m  mushroom        B  beetle (patrols up/down)
//   C  checkpoint (all difficulties)
//   k  checkpoint (Relaxed + Normal)    r  checkpoint (Relaxed only)
//   *  star (3 per level)  g  gem
//   1-8  power-up: 1 speed shoes, 2 shield, 3 magnet, 4 super dash,
//        5 double coins, 6 tiny, 7 giant, 8 rainbow star
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

    static bool isSolid(Tile t, unsigned pass = kPassNone) {
        if (t == Tile::TinyGap) return (pass & kPassTinyGaps) == 0;
        return t == Tile::Wall || t == Tile::Tree || t == Tile::Rock || t == Tile::Crate || t == Tile::Boulder;
    }
    // Can this tile be smashed? Crates by a dash or a giant, boulders only by a giant.
    static bool isBreakable(Tile t, bool giant) { return t == Tile::Crate || (giant && t == Tile::Boulder); }
    // Tiles that are walkable but dangerous (enemies avoid them too).
    static bool isDanger(Tile t) { return t == Tile::Water || t == Tile::Hazard; }
    // True if any tile overlapped by `box` is solid (given collision exceptions).
    bool overlapsSolid(const RectF& box, unsigned pass = kPassNone) const;
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
    std::vector<Vec2> hearts;
    std::vector<EnemySpawn> enemies;
    std::vector<CheckpointSpawn> checkpoints;
    std::vector<Vec2> stars;
    std::vector<Vec2> gems;
    std::vector<PowerUpSpawn> powerUps;

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
                    CollisionResult* result = nullptr, unsigned pass = kPassNone);

} // namespace pd
