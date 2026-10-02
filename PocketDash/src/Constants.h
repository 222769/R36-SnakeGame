#pragma once

namespace pd {

// Logical resolution. Everything is drawn at 640x480 and scaled with
// nearest-neighbour filtering to the actual window/screen.
constexpr int kScreenWidth = 640;
constexpr int kScreenHeight = 480;

// Fixed simulation rate. Gameplay is updated in constant steps so physics
// behave identically on a fast PC and on the R36S.
constexpr int kTickRate = 60;
constexpr float kFixedDt = 1.0f / static_cast<float>(kTickRate);

// World tile size in pixels (16x16 pixel art drawn at 2x).
constexpr int kTileSize = 32;
constexpr int kPixelScale = 2;

} // namespace pd
