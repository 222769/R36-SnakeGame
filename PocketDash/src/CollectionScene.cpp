#include "CollectionScene.h"

#include "Constants.h"
#include "Draw.h"
#include "Game.h"
#include "LevelLoader.h"
#include "Menu.h"
#include "TitleScene.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace pd {

namespace {

constexpr int kCardW = 92;
constexpr int kCardGap = 6;
constexpr int kCardY = 242;
constexpr int kCardH = 164;

int totalOf(int levels::Info::*field) {
    int n = 0;
    for (const auto& info : levels::catalog(1)) n += info.*field;
    return n;
}

} // namespace

CollectionScene::CollectionScene(Game& game) : Scene(game), index_(game.progress().selectedOutfit()) {}

void CollectionScene::update(float dt) {
    time_ += dt;
    if (deniedTime_ > 0.0f) deniedTime_ -= dt;
    menu::navigate(game_, index_, kOutfitCount, true, false);
    InputManager& in = game_.input();
    Progress& p = game_.progress();
    if (in.pressed(Action::A)) {
        if (!p.outfitUnlocked(index_)) {
            game_.audio().play(Sfx::Denied);
            deniedTime_ = 1.2f;
        } else if (p.selectedOutfit() != index_) {
            p.selectOutfit(index_);
            game_.applyOutfit();
            game_.saveProgress();
            game_.audio().play(Sfx::PowerUp);
        }
    } else if (in.pressed(Action::B)) {
        game_.audio().play(Sfx::MenuMove);
        game_.changeScene(std::make_unique<TitleScene>(game_, true));
    }
}

void CollectionScene::renderTotals(SDL_Renderer* r) const {
    const BitmapFont& font = game_.font();
    const Progress& p = game_.progress();
    const SDL_Rect box{30, 76, kScreenWidth - 60, 64};
    draw::roundedRect(r, box, SDL_Color{24, 30, 50, 215}, SDL_Color{255, 255, 255, 150}, SDL_Color{0, 0, 0, 90});
    SDL_Texture* items = game_.sprites().items();
    constexpr int s = Sprites::kItemSize;
    struct Stat {
        int frame;
        int have, total;
        const char* label;
        SDL_Color tint;
    };
    const Stat stats[] = {
        {Sprites::kItemGem, p.totalGems(), totalOf(&levels::Info::gems), "GEMS", ui::kMint},
        {Sprites::kItemStar, p.totalStars(), totalOf(&levels::Info::stars), "STARS", ui::kYellow},
        {Sprites::kItemStar, p.goldenStars(), static_cast<int>(levels::catalog(1).size()), "GOLDEN", SDL_Color{255, 220, 110, 255}},
    };
    for (int i = 0; i < 3; ++i) {
        const int x = box.x + 24 + i * 196;
        if (items) {
            const SDL_Rect src{stats[i].frame * s, 0, s, s};
            const SDL_Rect dst{x, box.y + 14, 34, 34};
            if (i == 2) SDL_SetTextureColorMod(items, 255, 236, 120);
            SDL_RenderCopy(r, items, &src, &dst);
            SDL_SetTextureColorMod(items, 255, 255, 255);
        }
        char text[24];
        std::snprintf(text, sizeof(text), "%d/%d", stats[i].have, stats[i].total);
        font.draw(r, x + 42, box.y + 8, text, 3, stats[i].tint);
        font.draw(r, x + 44, box.y + 40, stats[i].label, 1, ui::kWhite);
    }
}

void CollectionScene::renderGems(SDL_Renderer* r) const {
    // One slot per level: its gems found / total.
    const BitmapFont& font = game_.font();
    const Progress& p = game_.progress();
    SDL_Texture* items = game_.sprites().items();
    constexpr int s = Sprites::kItemSize;
    const auto& cat = levels::catalog(1);
    const int n = static_cast<int>(cat.size());
    if (n == 0) return;
    const int slotW = 66;
    const int x0 = kScreenWidth / 2 - (n * slotW - 6) / 2;
    const int y = 150;
    for (int i = 0; i < n; ++i) {
        const auto& info = cat[static_cast<size_t>(i)];
        const LevelRecord& rec = p.record(info.id);
        const bool all = info.gems > 0 && rec.gems >= info.gems;
        const SDL_Rect slot{x0 + i * slotW, y, slotW - 6, 60};
        draw::roundedRect(r, slot, all ? SDL_Color{40, 110, 100, 220} : SDL_Color{24, 30, 50, 200},
                          all ? SDL_Color{140, 240, 210, 255} : SDL_Color{0, 0, 0, 0}, SDL_Color{0, 0, 0, 60});
        font.drawCentered(r, slot.x + slot.w / 2, slot.y + 4, info.id, 2, ui::kWhite, false);
        if (items) {
            const SDL_Rect src{Sprites::kItemGem * s, 0, s, s};
            const SDL_Rect dst{slot.x + slot.w / 2 - 10, slot.y + 22, 20, 20};
            if (rec.gems == 0) {
                SDL_SetTextureColorMod(items, 60, 66, 84);
                SDL_SetTextureAlphaMod(items, 200);
            }
            SDL_RenderCopy(r, items, &src, &dst);
            SDL_SetTextureColorMod(items, 255, 255, 255);
            SDL_SetTextureAlphaMod(items, 255);
        }
        char text[12];
        std::snprintf(text, sizeof(text), "%d/%d", rec.gems, info.gems);
        font.drawCentered(r, slot.x + slot.w / 2, slot.y + 44, text, 1, all ? ui::kMint : ui::kGrey, false);
    }
}

void CollectionScene::renderOutfits(SDL_Renderer* r) const {
    const BitmapFont& font = game_.font();
    const Progress& p = game_.progress();
    SDL_Texture* previews = game_.sprites().outfitPreviews();
    const int x0 = kScreenWidth / 2 - (kOutfitCount * (kCardW + kCardGap) - kCardGap) / 2;
    font.drawCentered(r, kScreenWidth / 2, kCardY - 26, "OUTFITS", 2, ui::kWhite);
    for (int i = 0; i < kOutfitCount; ++i) {
        const Outfit& o = outfit(i);
        const bool unlocked = p.outfitUnlocked(i);
        const bool sel = i == index_;
        const bool worn = p.selectedOutfit() == i;
        const int lift = sel ? static_cast<int>(std::lround(4.0f + std::sin(time_ * 4.0f) * 2.0f)) : 0;
        const SDL_Rect card{x0 + i * (kCardW + kCardGap), kCardY - lift, kCardW, kCardH};
        draw::roundedRect(r, card, sel ? SDL_Color{255, 250, 236, 240} : SDL_Color{24, 30, 50, 210},
                          sel ? SDL_Color{246, 196, 62, 255} : SDL_Color{255, 255, 255, 60}, SDL_Color{0, 0, 0, 90});
        if (previews) {
            const SDL_Rect src{i * Sprites::kOutfitPreviewW, 0, Sprites::kOutfitPreviewW, Sprites::kOutfitPreviewH};
            const SDL_Rect dst{card.x + (kCardW - Sprites::kOutfitPreviewW) / 2, card.y + 10, Sprites::kOutfitPreviewW,
                               Sprites::kOutfitPreviewH};
            draw::softEllipse(r, dst.x + dst.w / 2, dst.y + dst.h - 4, 26, 7, SDL_Color{0, 0, 0, 90});
            if (!unlocked) SDL_SetTextureColorMod(previews, 40, 44, 60); // silhouette
            SDL_RenderCopy(r, previews, &src, &dst);
            SDL_SetTextureColorMod(previews, 255, 255, 255);
        }
        const SDL_Color ink = sel ? ui::kInk : ui::kWhite;
        font.drawCentered(r, card.x + kCardW / 2, card.y + 98, unlocked ? o.name : "???", 2, ink, false);
        char line[24];
        if (worn) {
            draw::roundedRect(r, SDL_Rect{card.x + 10, card.y + 128, kCardW - 20, 26}, SDL_Color{60, 160, 110, 255},
                              SDL_Color{0, 0, 0, 0});
            font.drawCentered(r, card.x + kCardW / 2, card.y + 133, "WORN", 2, ui::kWhite, false);
        } else if (!unlocked) {
            std::snprintf(line, sizeof(line), "%d GEMS", o.gemsNeeded);
            const bool flash = sel && deniedTime_ > 0.0f && static_cast<int>(deniedTime_ * 8.0f) % 2 == 0;
            font.drawCentered(r, card.x + kCardW / 2, card.y + 133, line, 2, flash ? ui::kPink : (sel ? ui::kInk : ui::kGrey), false);
        } else {
            font.drawCentered(r, card.x + kCardW / 2, card.y + 133, sel ? "A WEAR" : "", 2, ink, false);
        }
    }
}

void CollectionScene::render(SDL_Renderer* r) {
    game_.backdrop().render(r, time_);
    ui::dimScreen(r, 50);
    menu::drawHeader(r, game_.font(), "COLLECTION");
    renderTotals(r);
    renderGems(r);
    renderOutfits(r);
    menu::drawHints(r, game_.font(), "< > CHOOSE    A WEAR    B BACK");
}

} // namespace pd
