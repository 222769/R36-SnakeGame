#include "Level.h"

#include "Constants.h"

#include <algorithm>
#include <cmath>

namespace pd {

namespace {

constexpr float kTile = static_cast<float>(kTileSize);
constexpr float kEps = 0.001f;
// Longest distance moved per collision sub-step. Must stay well below the
// smallest hitbox dimension so nothing can skip over a tile.
constexpr float kMaxSubStep = 4.0f;

int toTile(float px) { return static_cast<int>(std::floor(px / kTile)); }

bool charToTile(char c, Tile& out) {
    switch (c) {
    case '.': case 'P': case 'c': out = Tile::Ground; return true;
    case '#': out = Tile::Wall; return true;
    case 'T': out = Tile::Tree; return true;
    case 'o': out = Tile::Rock; return true;
    case '~': out = Tile::Water; return true;
    case '=': out = Tile::Bridge; return true;
    case '^': out = Tile::Hazard; return true;
    case '%': out = Tile::SecretWall; return true;
    case 'E': out = Tile::Exit; return true;
    default: return false;
    }
}

// Moves `box` along one axis by `d`, stopping flush against solid tiles.
// Returns the distance moved; sets `blocked` if a wall was hit.
float moveAxis(const Level& level, RectF& box, float d, bool horizontal, bool& blocked) {
    if (d == 0.0f) return 0.0f;
    RectF moved = box;
    (horizontal ? moved.x : moved.y) += d;
    if (!level.overlapsSolid(moved)) {
        box = moved;
        return d;
    }
    blocked = true;

    // Snap flush against the tile we ran into.
    if (horizontal) {
        moved.x = d > 0 ? static_cast<float>(toTile(moved.right() - kEps)) * kTile - box.w
                        : static_cast<float>(toTile(moved.left()) + 1) * kTile;
    } else {
        moved.y = d > 0 ? static_cast<float>(toTile(moved.bottom() - kEps)) * kTile - box.h
                        : static_cast<float>(toTile(moved.top()) + 1) * kTile;
    }
    const float actual = horizontal ? moved.x - box.x : moved.y - box.y;
    // Only accept the snap if it is a (partial) move in the travel direction.
    const bool sameDirection = d > 0 ? (actual >= -kEps && actual <= d) : (actual <= kEps && actual >= d);
    if (sameDirection && !level.overlapsSolid(moved)) {
        box = moved;
        return actual;
    }
    return 0.0f;
}

// After a blocked move along one axis, tries sliding the box sideways (up
// to `maxNudge` px) around the corner it clipped. Returns the sideways
// distance applied, or 0 if the obstacle is not just a corner.
float tryCornerNudge(const Level& level, RectF& box, float d, bool horizontal, float maxNudge) {
    for (int off = 1; off <= static_cast<int>(maxNudge); ++off) {
        for (int sign : {-1, 1}) {
            RectF shifted = box;
            (horizontal ? shifted.y : shifted.x) += static_cast<float>(sign * off);
            if (level.overlapsSolid(shifted)) continue;
            RectF ahead = shifted;
            (horizontal ? ahead.x : ahead.y) += d;
            if (level.overlapsSolid(ahead)) continue;
            // Slide at most as fast as we were moving, so it feels natural.
            const float amount = static_cast<float>(sign) * std::min(static_cast<float>(off), std::fabs(d));
            (horizontal ? box.y : box.x) += amount;
            return amount;
        }
    }
    return 0.0f;
}

} // namespace

char tileToChar(Tile t) {
    switch (t) {
    case Tile::Ground: return '.';
    case Tile::Wall: return '#';
    case Tile::Tree: return 'T';
    case Tile::Rock: return 'o';
    case Tile::Water: return '~';
    case Tile::Bridge: return '=';
    case Tile::Hazard: return '^';
    case Tile::SecretWall: return '%';
    case Tile::Exit: return 'E';
    }
    return '?';
}

Level::Level(int width, int height)
    : width_(width), height_(height), tiles_(static_cast<size_t>(width * height), Tile::Ground) {}

bool Level::fromAscii(const std::vector<std::string>& rows, Level& out, std::string* error) {
    auto fail = [&](const std::string& msg) {
        if (error) *error = msg;
        return false;
    };
    if (rows.empty() || rows[0].empty()) return fail("map is empty");

    const int width = static_cast<int>(rows[0].size());
    const int height = static_cast<int>(rows.size());
    Level level(width, height);
    int spawns = 0;
    int exits = 0;

    for (int y = 0; y < height; ++y) {
        if (static_cast<int>(rows[y].size()) != width)
            return fail("map row " + std::to_string(y + 1) + " is " + std::to_string(rows[y].size()) +
                        " wide, expected " + std::to_string(width));
        for (int x = 0; x < width; ++x) {
            const char c = rows[y][x];
            Tile t;
            if (!charToTile(c, t))
                return fail(std::string("unknown map character '") + c + "' at row " + std::to_string(y + 1) +
                            ", column " + std::to_string(x + 1));
            level.setTile(x, y, t);
            if (c == 'P') {
                // Player position is the feet: put them in the lower half of the tile.
                level.spawn = tileCenter(x, y) + Vec2{0.0f, 6.0f};
                ++spawns;
            } else if (c == 'E') {
                level.exit = tileCenter(x, y);
                ++exits;
            } else if (c == 'c') {
                level.coins.push_back(tileCenter(x, y));
            }
        }
    }
    if (spawns != 1) return fail("map needs exactly one player spawn 'P' (found " + std::to_string(spawns) + ")");
    if (exits < 1) return fail("map needs an exit 'E'");

    out = std::move(level);
    return true;
}

float Level::pixelWidth() const { return static_cast<float>(width_ * kTileSize); }

float Level::pixelHeight() const { return static_cast<float>(height_ * kTileSize); }

Tile Level::tileAt(int tx, int ty) const {
    // Outside the map counts as wall so nothing can leave the level.
    if (!inBounds(tx, ty)) return Tile::Wall;
    return tiles_[static_cast<size_t>(ty * width_ + tx)];
}

Tile Level::tileAtPixel(float px, float py) const { return tileAt(toTile(px), toTile(py)); }

void Level::setTile(int tx, int ty, Tile t) {
    if (inBounds(tx, ty)) tiles_[static_cast<size_t>(ty * width_ + tx)] = t;
}

bool Level::overlapsSolid(const RectF& box) const {
    const int x0 = toTile(box.left());
    const int x1 = toTile(box.right() - kEps);
    const int y0 = toTile(box.top());
    const int y1 = toTile(box.bottom() - kEps);
    for (int ty = y0; ty <= y1; ++ty)
        for (int tx = x0; tx <= x1; ++tx)
            if (isSolid(tileAt(tx, ty))) return true;
    return false;
}

bool Level::overlapsTile(const RectF& box, Tile t) const {
    const int x0 = toTile(box.left());
    const int x1 = toTile(box.right() - kEps);
    const int y0 = toTile(box.top());
    const int y1 = toTile(box.bottom() - kEps);
    for (int ty = y0; ty <= y1; ++ty)
        for (int tx = x0; tx <= x1; ++tx)
            if (tileAt(tx, ty) == t) return true;
    return false;
}

Vec2 Level::tileCenter(int tx, int ty) {
    return {(static_cast<float>(tx) + 0.5f) * kTile, (static_cast<float>(ty) + 0.5f) * kTile};
}

Vec2 moveAndCollide(const Level& level, const RectF& box, Vec2 delta, float cornerNudge, CollisionResult* result) {
    RectF b = box;
    Vec2 moved;
    CollisionResult res;

    const float longest = std::max(std::fabs(delta.x), std::fabs(delta.y));
    const int steps = std::max(1, static_cast<int>(std::ceil(longest / kMaxSubStep)));
    Vec2 step = delta * (1.0f / static_cast<float>(steps));
    // Only nudge around corners when moving (almost) straight along one axis;
    // diagonal movement already slides along walls by itself.
    const bool straightX = std::fabs(delta.y) < 0.25f * std::fabs(delta.x);
    const bool straightY = std::fabs(delta.x) < 0.25f * std::fabs(delta.y);

    for (int i = 0; i < steps; ++i) {
        if (step.x != 0.0f) {
            bool blocked = false;
            moved.x += moveAxis(level, b, step.x, true, blocked);
            if (blocked) {
                const float nudge = (cornerNudge > 0.0f && straightX)
                                        ? tryCornerNudge(level, b, step.x, true, cornerNudge)
                                        : 0.0f;
                if (nudge != 0.0f) {
                    moved.y += nudge;
                } else {
                    res.hitX = true;
                    step.x = 0.0f;
                }
            }
        }
        if (step.y != 0.0f) {
            bool blocked = false;
            moved.y += moveAxis(level, b, step.y, false, blocked);
            if (blocked) {
                const float nudge = (cornerNudge > 0.0f && straightY)
                                        ? tryCornerNudge(level, b, step.y, false, cornerNudge)
                                        : 0.0f;
                if (nudge != 0.0f) {
                    moved.x += nudge;
                } else {
                    res.hitY = true;
                    step.y = 0.0f;
                }
            }
        }
    }
    if (result) *result = res;
    return moved;
}

} // namespace pd
