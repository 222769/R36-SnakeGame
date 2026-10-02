#include "Collectibles.h"

#include "Constants.h"
#include "Draw.h"
#include "Effects.h"
#include "Sprites.h"

#include <cmath>

namespace pd {

void CoinField::reset(const std::vector<Vec2>& positions) {
    coins_.clear();
    coins_.reserve(positions.size());
    for (Vec2 p : positions) coins_.push_back({p, false});
    collected_ = 0;
}

int CoinField::collect(Vec2 playerCenter, Effects* effects) {
    int count = 0;
    const float r2 = kPickupRadius * kPickupRadius;
    for (Coin& c : coins_) {
        if (c.taken || (c.pos - playerCenter).lengthSq() > r2) continue;
        c.taken = true;
        ++count;
        if (effects) effects->spawn(Effects::Type::Sparkle, c.pos, SDL_Color{255, 220, 80, 255});
    }
    collected_ += count;
    return count;
}

void CoinField::render(SDL_Renderer* r, SDL_Texture* sheet, Vec2 camera, float time) const {
    if (!sheet) return;
    constexpr int size = Sprites::kCoinSize * kPixelScale;
    // Spin sequence 0,1,2,1(mirrored).
    static const int kFrames[4] = {0, 1, 2, 1};
    const float viewLeft = camera.x - size;
    const float viewRight = camera.x + kScreenWidth + size;
    const float viewTop = camera.y - size;
    const float viewBottom = camera.y + kScreenHeight + size;

    for (size_t i = 0; i < coins_.size(); ++i) {
        const Coin& c = coins_[i];
        if (c.taken) continue;
        if (c.pos.x < viewLeft || c.pos.x > viewRight || c.pos.y < viewTop || c.pos.y > viewBottom) continue;

        // Each coin is out of phase with its neighbours so rows don't spin in lockstep.
        const float phase = time * 8.0f + static_cast<float>(i) * 0.7f;
        const int step = static_cast<int>(phase) % 4;
        const int bob = static_cast<int>(std::lround(std::sin(time * 3.0f + static_cast<float>(i)) * 2.0f));
        const int sx = static_cast<int>(c.pos.x - camera.x);
        const int sy = static_cast<int>(c.pos.y - camera.y);

        draw::fillEllipse(r, sx, sy + 12, 7, 2, SDL_Color{0, 0, 0, 60});
        const SDL_Rect src{kFrames[step] * Sprites::kCoinSize, 0, Sprites::kCoinSize, Sprites::kCoinSize};
        const SDL_Rect dst{sx - size / 2, sy - size / 2 + bob - 2, size, size};
        SDL_RenderCopyEx(r, sheet, &src, &dst, 0.0, nullptr, step == 3 ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
    }
}

} // namespace pd
