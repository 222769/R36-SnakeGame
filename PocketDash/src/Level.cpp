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
    case '.': case 'P': case 'c': case 'h': case 's': case 'b': case 'B': case 'm':
    case 'C': case 'k': case 'r': case '*': case 'g':
    case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8':
        out = Tile::Ground;
        return true;
    case 'K': case 'f': case 'S':
        out = Tile::Ground;
        return true;
    case 'R': case 'V': out = Tile::Water; return true; // rafts float on water
    case 'L': out = Tile::Lock; return true;
    case 'x': out = Tile::Crate; return true;
    case 'X': out = Tile::Boulder; return true;
    case ':': out = Tile::TinyGap; return true;
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
float moveAxis(const Level& level, RectF& box, float d, bool horizontal, bool& blocked, unsigned pass) {
    if (d == 0.0f) return 0.0f;
    RectF moved = box;
    (horizontal ? moved.x : moved.y) += d;
    if (!level.overlapsSolid(moved, pass)) {
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
    if (sameDirection && !level.overlapsSolid(moved, pass)) {
        box = moved;
        return actual;
    }
    return 0.0f;
}

// After a blocked move along one axis, tries sliding the box sideways (up
// to `maxNudge` px) around the corner it clipped. Returns the sideways
// distance applied, or 0 if the obstacle is not just a corner.
float tryCornerNudge(const Level& level, RectF& box, float d, bool horizontal, float maxNudge, unsigned pass) {
    for (int off = 1; off <= static_cast<int>(maxNudge); ++off) {
        for (int sign : {-1, 1}) {
            RectF shifted = box;
            (horizontal ? shifted.y : shifted.x) += static_cast<float>(sign * off);
            if (level.overlapsSolid(shifted, pass)) continue;
            RectF ahead = shifted;
            (horizontal ? ahead.x : ahead.y) += d;
            if (level.overlapsSolid(ahead, pass)) continue;
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
    case Tile::Crate: return 'x';
    case Tile::Boulder: return 'X';
    case Tile::TinyGap: return ':';
    case Tile::Lock: return 'L';
    }
    return '?';
}

Level::Level(int width, int height)
    : width_(width), height_(height), tiles_(static_cast<size_t>(width * height), Tile::Ground) {}

const char* objectiveName(Objective o) {
    switch (o) {
    case Objective::ReachExit: return "exit";
    case Objective::Coins: return "coins";
    case Objective::Stars: return "stars";
    case Objective::Rescue: return "rescue";
    case Objective::DefeatAll: return "defeat";
    }
    return "exit";
}

bool objectiveFromName(const std::string& s, Objective& out) {
    for (Objective o : {Objective::ReachExit, Objective::Coins, Objective::Stars, Objective::Rescue,
                        Objective::DefeatAll}) {
        if (s == objectiveName(o)) {
            out = o;
            return true;
        }
    }
    return false;
}

bool Level::fromAscii(const std::vector<std::string>& rows, Level& out, std::string* error, int firstLine) {
    auto where = [&](int row) {
        return firstLine > 0 ? "line " + std::to_string(firstLine + row) : "map row " + std::to_string(row + 1);
    };
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
            return fail(where(y) + ": map row is " + std::to_string(rows[y].size()) + " wide, expected " +
                        std::to_string(width));
        for (int x = 0; x < width; ++x) {
            const char c = rows[y][x];
            Tile t;
            if (!charToTile(c, t))
                return fail(where(y) + ": unknown map character '" + std::string(1, c) + "' at column " +
                            std::to_string(x + 1));
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
            } else if (c == 'h') {
                level.hearts.push_back(tileCenter(x, y));
            } else if (c == 's' || c == 'b' || c == 'B' || c == 'm') {
                EnemySpawn e;
                e.type = c == 's' ? EnemyType::Slime : c == 'm' ? EnemyType::Mushroom : EnemyType::Beetle;
                e.pos = tileCenter(x, y) + Vec2{0.0f, 6.0f}; // feet, like the player
                e.dir = c == 'B' ? Vec2{0.0f, 1.0f} : Vec2{1.0f, 0.0f};
                level.enemies.push_back(e);
            } else if (c == 'C' || c == 'k' || c == 'r') {
                CheckpointSpawn cp;
                cp.pos = tileCenter(x, y) + Vec2{0.0f, 6.0f};
                cp.hardest = c == 'C' ? Difficulty::Challenge : c == 'k' ? Difficulty::Normal : Difficulty::Relaxed;
                level.checkpoints.push_back(cp);
            } else if (c == '*') {
                level.stars.push_back(tileCenter(x, y));
            } else if (c == 'g') {
                level.gems.push_back(tileCenter(x, y));
            } else if (c >= '1' && c <= '8') {
                level.powerUps.push_back({static_cast<PowerUpType>(c - '0'), tileCenter(x, y)});
            } else if (c == 'K') {
                level.keys.push_back(tileCenter(x, y));
            } else if (c == 'f') {
                level.friends.push_back(tileCenter(x, y) + Vec2{0.0f, 6.0f});
            } else if (c == 'S') {
                level.signs.push_back({tileCenter(x, y) + Vec2{0.0f, 6.0f}, std::string()});
            } else if (c == 'R' || c == 'V') {
                level.rafts.push_back({tileCenter(x, y), c == 'R' ? Vec2{1.0f, 0.0f} : Vec2{0.0f, 1.0f}});
            }
        }
    }
    if (spawns != 1) return fail("map needs exactly one player spawn 'P' (found " + std::to_string(spawns) + ")");
    if (exits < 1) return fail("map needs an exit 'E'");

    level.groupSecrets();
    out = std::move(level);
    return true;
}

void Level::groupSecrets() {
    // Flood-fill connected (4-way) secret walls into numbered groups.
    secretGroups_.assign(tiles_.size(), -1);
    secretCount_ = 0;
    std::vector<int> stack;
    for (int y = 0; y < height_; ++y) {
        for (int x = 0; x < width_; ++x) {
            const size_t i = static_cast<size_t>(y * width_ + x);
            if (tiles_[i] != Tile::SecretWall || secretGroups_[i] >= 0) continue;
            const int16_t group = static_cast<int16_t>(secretCount_++);
            stack.assign(1, static_cast<int>(i));
            secretGroups_[i] = group;
            while (!stack.empty()) {
                const int cur = stack.back();
                stack.pop_back();
                const int cx = cur % width_;
                const int cy = cur / width_;
                const int dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
                for (const auto& d : dirs) {
                    const int nx = cx + d[0];
                    const int ny = cy + d[1];
                    if (!inBounds(nx, ny)) continue;
                    const size_t n = static_cast<size_t>(ny * width_ + nx);
                    if (tiles_[n] == Tile::SecretWall && secretGroups_[n] < 0) {
                        secretGroups_[n] = group;
                        stack.push_back(static_cast<int>(n));
                    }
                }
            }
        }
    }
}

int Level::secretAt(int tx, int ty) const {
    if (!inBounds(tx, ty) || secretGroups_.empty()) return -1;
    return secretGroups_[static_cast<size_t>(ty * width_ + tx)];
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

bool Level::overlapsSolid(const RectF& box, unsigned pass) const {
    const int x0 = toTile(box.left());
    const int x1 = toTile(box.right() - kEps);
    const int y0 = toTile(box.top());
    const int y1 = toTile(box.bottom() - kEps);
    for (int ty = y0; ty <= y1; ++ty)
        for (int tx = x0; tx <= x1; ++tx)
            if (isSolid(tileAt(tx, ty), pass)) return true;
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

Vec2 moveAndCollide(const Level& level, const RectF& box, Vec2 delta, float cornerNudge, CollisionResult* result,
                    unsigned pass) {
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
            moved.x += moveAxis(level, b, step.x, true, blocked, pass);
            if (blocked) {
                const float nudge = (cornerNudge > 0.0f && straightX)
                                        ? tryCornerNudge(level, b, step.x, true, cornerNudge, pass)
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
            moved.y += moveAxis(level, b, step.y, false, blocked, pass);
            if (blocked) {
                const float nudge = (cornerNudge > 0.0f && straightY)
                                        ? tryCornerNudge(level, b, step.y, false, cornerNudge, pass)
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
