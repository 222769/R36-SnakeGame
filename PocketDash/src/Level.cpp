#include "Level.h"

#include "Constants.h"

namespace pd {

Level::Level(int width, int height)
    : width_(width), height_(height), tiles_(static_cast<size_t>(width * height), Tile::Empty) {}

float Level::pixelWidth() const { return static_cast<float>(width_ * kTileSize); }

float Level::pixelHeight() const { return static_cast<float>(height_ * kTileSize); }

Tile Level::tileAt(int tx, int ty) const {
    // Outside the map counts as wall so nothing can leave the level.
    if (!inBounds(tx, ty)) return Tile::Wall;
    return tiles_[static_cast<size_t>(ty * width_ + tx)];
}

void Level::setTile(int tx, int ty, Tile t) {
    if (inBounds(tx, ty)) tiles_[static_cast<size_t>(ty * width_ + tx)] = t;
}

} // namespace pd
