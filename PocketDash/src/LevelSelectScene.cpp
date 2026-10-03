#include "LevelSelectScene.h"

#include "Canvas.h"
#include "Constants.h"
#include "Draw.h"
#include "Game.h"
#include "HighScoresScene.h"
#include "LevelLoader.h"
#include "Menu.h"
#include "PlayScene.h"
#include "TitleScene.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace pd {

namespace {

// Stops along the path, in map pixels. The last one is the boss arena.
constexpr float kNodes[][2] = {{70, 222}, {156, 250}, {226, 176}, {316, 214},
                               {396, 140}, {474, 196}, {548, 248}, {536, 88}};
constexpr int kNodeCount = static_cast<int>(sizeof(kNodes) / sizeof(kNodes[0]));
constexpr int kNodeRadius = 19;

Vec2 node(int i) {
    i = std::clamp(i, 0, kNodeCount - 1);
    return {kNodes[i][0], kNodes[i][1]};
}

// Catmull-Rom point between node i and i+1.
Vec2 pathPoint(int i, float t) {
    const Vec2 p0 = node(i - 1), p1 = node(i), p2 = node(i + 1), p3 = node(i + 2);
    const float t2 = t * t, t3 = t2 * t;
    return (p1 * 2.0f + (p2 - p0) * t + (p0 * 2.0f - p1 * 5.0f + p2 * 4.0f - p3) * t2 +
            (p1 * 3.0f - p0 - p2 * 3.0f + p3) * t3) *
           0.5f;
}

} // namespace

LevelSelectScene::LevelSelectScene(Game& game, const std::string& selectId) : Scene(game) {
    ids_ = levels::worldLevelIds(1);
    const std::string want = selectId.empty() ? game.progress().continueLevel(1) : selectId;
    const auto it = std::find(ids_.begin(), ids_.end(), want);
    index_ = it == ids_.end() ? 0 : static_cast<int>(it - ids_.begin());
    index_ = std::min(index_, kNodeCount - 1);
    heroPos_ = nodePos(index_);
    buildMap(game.renderer());
    game.audio().playMusic(MusicTrack::Title);
}

Vec2 LevelSelectScene::nodePos(int i) const {
    return node(i) + Vec2{static_cast<float>(kMapX), static_cast<float>(kMapY)};
}

void LevelSelectScene::buildMap(SDL_Renderer* r) {
    // An island in a calm lake: grass with soft noise shading, a sandy rim,
    // a dirt path linking the stops, a pond and a few trees.
    Canvas c(kMapW, kMapH);
    const Col water = Col::rgb(70, 138, 178);
    c.fillRoundRect(0, 0, kMapW, kMapH, 26, water.scaled(0.9f));
    c.paint([&](int x, int y) {
        const float n = tileFbm(static_cast<float>(x), static_cast<float>(y), 128, 5u, 3);
        return Col::rgb(255, 255, 255, static_cast<int>(n * 28.0f));
    });
    // Island outline: a rounded blob from overlapping ellipses.
    const float blobs[][4] = {{150, 170, 150, 115}, {330, 190, 150, 100}, {480, 175, 120, 115}, {530, 95, 70, 70},
                              {80, 210, 70, 75}};
    for (const auto& b : blobs) c.fillEllipse(b[0], b[1] + 4, b[2] + 10, b[3] + 10, Col::rgb(214, 196, 140));
    for (const auto& b : blobs) c.fillEllipse(b[0], b[1], b[2], b[3], Col::rgb(110, 158, 72));
    // Grass texture over the island only (alpha follows the grass colour).
    Canvas shade(kMapW, kMapH);
    shade.paint([&](int x, int y) {
        const Col g = c.get(x, y);
        if (g.b > 0.4f || g.r > 0.6f) return Col{0, 0, 0, 0}; // not grass
        const float n = tileFbm(static_cast<float>(x), static_cast<float>(y), 64, 9u, 4);
        return (n > 0.5f ? Col::rgb(150, 190, 96) : Col::rgb(60, 104, 46)).withAlpha(std::fabs(n - 0.5f) * 0.9f);
    });
    for (int y = 0; y < kMapH; ++y)
        for (int x = 0; x < kMapW; ++x) c.blend(x, y, shade.get(x, y));
    // Pond.
    c.fillEllipse(96, 118, 34, 20, Col::rgb(214, 196, 140));
    c.fillEllipse(96, 118, 30, 17, water);
    c.softEllipse(88, 113, 14, 6, Col::rgb(230, 244, 250, 120));
    // Path: a wide dirt band with a lighter centre.
    for (int i = 0; i + 1 < kNodeCount; ++i) {
        for (int s = 0; s < 24; ++s) {
            const Vec2 a = pathPoint(i, static_cast<float>(s) / 24.0f);
            const Vec2 b = pathPoint(i, static_cast<float>(s + 1) / 24.0f);
            c.line(a + Vec2{0, 2}, b + Vec2{0, 2}, 12.0f, Col::rgb(70, 100, 44, 120));
            c.line(a, b, 11.0f, Col::rgb(196, 160, 106));
        }
    }
    for (int i = 0; i + 1 < kNodeCount; ++i)
        for (int s = 0; s < 24; s += 3) {
            const Vec2 a = pathPoint(i, static_cast<float>(s) / 24.0f);
            c.fillCircle(a.x, a.y, 1.6f, Col::rgb(228, 204, 156));
        }
    // Trees, away from the path and the stops.
    const float trees[][2] = {{40, 150}, {200, 110}, {262, 120}, {286, 268}, {360, 260}, {430, 248},
                              {446, 108}, {586, 170}, {120, 280}, {340, 110}, {598, 222}, {20, 230}};
    for (const auto& t : trees) {
        c.softEllipse(t[0] + 3, t[1] + 8, 13, 6, Col::rgb(20, 50, 20, 120));
        c.line({t[0], t[1] + 2}, {t[0], t[1] + 8}, 3.0f, Col::rgb(110, 76, 46));
        c.sphere(t[0], t[1] - 2, 11, 10, Col::rgb(62, 120, 56), 0.1f, 0.5f);
        c.sphere(t[0] - 4, t[1] - 6, 6, 6, Col::rgb(84, 146, 68), 0.1f, 0.6f);
    }
    // Boss arena: a stone ring around the last stop.
    const Vec2 boss = node(kNodeCount - 1);
    c.ring(boss.x, boss.y, kNodeRadius + 8.0f, kNodeRadius + 14.0f, Col::rgb(150, 146, 140));
    c.ring(boss.x, boss.y, kNodeRadius + 8.0f, kNodeRadius + 9.5f, Col::rgb(110, 106, 100));

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    if (SurfacePtr s = c.toSurface()) {
        map_.reset(SDL_CreateTextureFromSurface(r, s.get()));
        if (map_) SDL_SetTextureBlendMode(map_.get(), SDL_BLENDMODE_BLEND);
    }
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
}

void LevelSelectScene::update(float dt) {
    time_ += dt;
    if (lockedFlash_ > 0.0f) lockedFlash_ -= dt;
    const int count = std::min(static_cast<int>(ids_.size()), kNodeCount);
    InputManager& in = game_.input();

    int step = 0;
    if (in.repeated(Action::Right) || in.repeated(Action::Down)) step = 1;
    if (in.repeated(Action::Left) || in.repeated(Action::Up)) step = -1;
    if (step != 0) {
        const int next = std::clamp(index_ + step, 0, count - 1);
        if (next != index_) {
            heroFacingLeft_ = nodePos(next).x < nodePos(index_).x;
            index_ = next;
            lockedFlash_ = 0.0f;
            game_.audio().play(Sfx::MenuMove);
        }
    }
    // The hero walks to the selected stop.
    const Vec2 target = nodePos(index_);
    const Vec2 d = target - heroPos_;
    const float dist = d.length();
    const float move = 420.0f * dt;
    heroPos_ = dist <= move ? target : heroPos_ + d * (move / dist);

    if (count == 0) return;
    const std::string& id = ids_[static_cast<size_t>(index_)];
    if (in.pressed(Action::A) || in.pressed(Action::Start)) {
        if (game_.progress().isUnlocked(id)) {
            game_.audio().play(Sfx::MenuSelect);
            game_.changeScene(std::make_unique<PlayScene>(game_, id));
        } else {
            game_.audio().play(Sfx::Denied);
            lockedFlash_ = 1.6f;
        }
    } else if (in.pressed(Action::X)) {
        game_.audio().play(Sfx::MenuSelect);
        game_.changeScene(std::make_unique<HighScoresScene>(game_, id, true));
    } else if (in.pressed(Action::B)) {
        game_.audio().play(Sfx::MenuMove);
        game_.changeScene(std::make_unique<TitleScene>(game_, true));
    }
}

void LevelSelectScene::renderNode(SDL_Renderer* r, int i) const {
    const Progress& p = game_.progress();
    const std::string& id = ids_[static_cast<size_t>(i)];
    const LevelRecord& rec = p.record(id);
    const bool unlocked = p.isUnlocked(id);
    const bool boss = i == kNodeCount - 1;
    const Vec2 pos = nodePos(i);
    const int x = static_cast<int>(pos.x), y = static_cast<int>(pos.y);
    const int radius = boss ? kNodeRadius + 3 : kNodeRadius;
    const BitmapFont& font = game_.font();

    if (i == index_) {
        const int pulse = static_cast<int>(std::lround(3.0f + std::sin(time_ * 6.0f) * 2.0f));
        draw::fillCircle(r, x, y, radius + 4 + pulse, SDL_Color{255, 240, 180, 110});
    }
    draw::softEllipse(r, x + 2, y + 6, radius + 4, radius / 2 + 4, SDL_Color{0, 0, 0, 90});
    SDL_Color ring{255, 255, 255, 255};
    SDL_Color fill{240, 236, 226, 255};
    if (rec.cleared) {
        fill = SDL_Color{246, 196, 62, 255};
        ring = SDL_Color{176, 118, 22, 255};
    } else if (!unlocked) {
        fill = SDL_Color{128, 132, 140, 255};
        ring = SDL_Color{86, 88, 96, 255};
    } else {
        ring = SDL_Color{60, 120, 190, 255};
    }
    draw::fillCircle(r, x, y, radius, ring);
    draw::fillCircle(r, x, y, radius - 3, fill);
    draw::softEllipse(r, x - radius / 3, y - radius / 2, radius / 2, radius / 4, SDL_Color{255, 255, 255, 140});

    if (!unlocked) {
        // Padlock.
        draw::fillCircle(r, x, y - 4, 6, SDL_Color{60, 62, 70, 255});
        draw::fillCircle(r, x, y - 4, 3, fill);
        draw::roundedRect(r, SDL_Rect{x - 8, y - 3, 16, 12}, SDL_Color{60, 62, 70, 255}, SDL_Color{0, 0, 0, 0});
    } else {
        char label[16];
        std::snprintf(label, sizeof(label), "%d", i + 1);
        font.drawCentered(r, x, y - BitmapFont::lineHeight(2) / 2 + 1, label, 2,
                          rec.cleared ? SDL_Color{110, 64, 10, 255} : ui::kInk, false);
    }

    // Stars found (a row of three under the stop) and the Golden Star.
    SDL_Texture* items = game_.sprites().items();
    if (!items) return;
    constexpr int s = Sprites::kItemSize;
    const auto& info = levels::catalog(1);
    const int stars = i < static_cast<int>(info.size()) ? info[static_cast<size_t>(i)].stars : 3;
    if (unlocked && stars > 0) {
        const int iconW = 14;
        const int x0 = x - (stars * iconW) / 2;
        for (int k = 0; k < stars; ++k) {
            const int frame = (rec.starsMask >> k) & 1u ? Sprites::kItemStar : Sprites::kItemStarEmpty;
            const SDL_Rect src{frame * s, 0, s, s};
            const SDL_Rect dst{x0 + k * iconW, y + radius + 2, iconW, iconW};
            SDL_RenderCopy(r, items, &src, &dst);
        }
    }
    if (rec.goldenStar) {
        const SDL_Rect src{Sprites::kItemStar * s, 0, s, s};
        const int bob = static_cast<int>(std::lround(std::sin(time_ * 3.0f + i) * 2.0f));
        const SDL_Rect dst{x + radius - 10, y - radius - 10 + bob, 22, 22};
        SDL_SetTextureColorMod(items, 255, 236, 120);
        SDL_RenderCopy(r, items, &src, &dst);
        SDL_SetTextureColorMod(items, 255, 255, 255);
    }
}

void LevelSelectScene::renderInfo(SDL_Renderer* r) const {
    const BitmapFont& font = game_.font();
    const SDL_Rect box{20, 382, kScreenWidth - 40, 54};
    draw::roundedRect(r, box, SDL_Color{24, 30, 50, 215}, SDL_Color{255, 255, 255, 160}, SDL_Color{0, 0, 0, 80});
    if (ids_.empty()) return;
    const std::string& id = ids_[static_cast<size_t>(index_)];
    const auto& cat = levels::catalog(1);
    const levels::Info* info = index_ < static_cast<int>(cat.size()) ? &cat[static_cast<size_t>(index_)] : nullptr;
    const LevelRecord& rec = game_.progress().record(id);
    const bool unlocked = game_.progress().isUnlocked(id);

    char line[96];
    std::snprintf(line, sizeof(line), "%s  %s", id.c_str(), unlocked && info ? info->name.c_str() : "???");
    font.drawShadowed(r, box.x + 14, box.y + 8, line, 2, ui::kYellow);

    if (!unlocked) {
        const std::string prev = index_ > 0 ? ids_[static_cast<size_t>(index_ - 1)] : std::string();
        std::snprintf(line, sizeof(line), "LOCKED - CLEAR %s TO OPEN", prev.c_str());
        const bool flash = lockedFlash_ > 0.0f && static_cast<int>(lockedFlash_ * 8.0f) % 2 == 0;
        font.draw(r, box.x + 14, box.y + 30, line, 2, flash ? ui::kPink : ui::kGrey);
        return;
    }
    if (rec.cleared) {
        char score[24], clock[16];
        ui::formatScore(score, sizeof(score), rec.bestScore);
        ui::formatTime(clock, sizeof(clock), rec.bestTime, true);
        std::snprintf(line, sizeof(line), "BEST %s   %s", score, clock);
    } else {
        std::snprintf(line, sizeof(line), "NOT CLEARED YET");
    }
    font.draw(r, box.x + box.w - 14 - BitmapFont::textWidth(line, 2), box.y + 8, line, 2, ui::kWhite);

    if (info) {
        int stars = 0;
        for (unsigned m = rec.starsMask; m; m &= m - 1) ++stars;
        std::snprintf(line, sizeof(line), "STARS %d/%d   GEMS %d/%d   SECRETS %d/%d", stars, info->stars, rec.gems,
                      info->gems, rec.secrets, info->secrets);
        font.draw(r, box.x + 14, box.y + 30, line, 2, ui::kWhite);
    }
    if (rec.goldenStar) font.draw(r, box.x + box.w - 14 - BitmapFont::textWidth("GOLDEN STAR!", 2), box.y + 30,
                                  "GOLDEN STAR!", 2, ui::kYellow);
}

void LevelSelectScene::render(SDL_Renderer* r) {
    game_.backdrop().render(r, time_);
    ui::dimScreen(r, 40);
    if (map_) {
        draw::roundedRect(r, SDL_Rect{kMapX, kMapY, kMapW, kMapH}, SDL_Color{0, 0, 0, 0}, SDL_Color{0, 0, 0, 0},
                          SDL_Color{0, 0, 0, 120});
        const SDL_Rect dst{kMapX, kMapY, kMapW, kMapH};
        SDL_RenderCopy(r, map_.get(), nullptr, &dst);
    }
    const int count = std::min(static_cast<int>(ids_.size()), kNodeCount);
    for (int i = 0; i < count; ++i) renderNode(r, i);

    // The hero stands on the selected stop.
    if (SDL_Texture* sheet = game_.sprites().player()) {
        const bool walking = (heroPos_ - nodePos(index_)).length() > 1.0f;
        const int frame = walking ? static_cast<int>(time_ * 10.0f) % 2 : 0;
        const int row = walking ? Sprites::kRowSide : Sprites::kRowFront;
        const SDL_Rect src{frame * Sprites::kPlayerFrameW, row * Sprites::kPlayerFrameH, Sprites::kPlayerFrameW,
                           Sprites::kPlayerFrameH};
        const int bob = walking ? 0 : static_cast<int>(std::lround(std::fabs(std::sin(time_ * 3.0f)) * -3.0f));
        const SDL_Rect dst{static_cast<int>(heroPos_.x) - Sprites::kPlayerFrameW / 2,
                           static_cast<int>(heroPos_.y) - kNodeRadius - Sprites::kPlayerFrameH + 10 + bob,
                           Sprites::kPlayerFrameW, Sprites::kPlayerFrameH};
        SDL_RenderCopyEx(r, sheet, &src, &dst, 0.0, nullptr,
                         walking && heroFacingLeft_ ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
    }

    menu::drawHeader(r, game_.font(), "SUNNY MEADOWS");
    renderInfo(r);
    menu::drawHints(r, game_.font(), "A PLAY    X HIGH SCORES    B BACK");
}

} // namespace pd
