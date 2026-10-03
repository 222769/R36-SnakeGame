#include "HighScoresScene.h"

#include "Constants.h"
#include "Draw.h"
#include "Game.h"
#include "LevelLoader.h"
#include "LevelSelectScene.h"
#include "Menu.h"
#include "TitleScene.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace pd {

HighScoresScene::HighScoresScene(Game& game, const std::string& levelId, bool fromLevelSelect)
    : Scene(game), fromLevelSelect_(fromLevelSelect) {
    ids_ = levels::worldLevelIds(1);
    const auto it = std::find(ids_.begin(), ids_.end(), levelId);
    index_ = it == ids_.end() ? 0 : static_cast<int>(it - ids_.begin());
    if (levelId.empty()) {
        // From the main menu: open on the first level that has scores.
        for (size_t i = 0; i < ids_.size(); ++i) {
            if (!game.progress().record(ids_[i]).highScores.empty()) {
                index_ = static_cast<int>(i);
                break;
            }
        }
    }
}

void HighScoresScene::update(float dt) {
    time_ += dt;
    menu::navigate(game_, index_, static_cast<int>(ids_.size()), true);
    InputManager& in = game_.input();
    if (in.pressed(Action::B) || in.pressed(Action::A) || in.pressed(Action::Start)) {
        game_.audio().play(Sfx::MenuMove);
        if (fromLevelSelect_)
            game_.changeScene(std::make_unique<LevelSelectScene>(game_, ids_.empty() ? std::string() : ids_[static_cast<size_t>(index_)]));
        else
            game_.changeScene(std::make_unique<TitleScene>(game_, true));
    }
}

void HighScoresScene::render(SDL_Renderer* r) {
    game_.backdrop().render(r, time_);
    ui::dimScreen(r, 60);
    const BitmapFont& font = game_.font();
    menu::drawHeader(r, font, "HIGH SCORES");
    if (ids_.empty()) return;

    const std::string& id = ids_[static_cast<size_t>(index_)];
    const auto& cat = levels::catalog(1);
    const Progress& p = game_.progress();
    const bool unlocked = p.isUnlocked(id);
    const std::string levelName = unlocked && index_ < static_cast<int>(cat.size()) ? cat[static_cast<size_t>(index_)].name : "???";

    const SDL_Rect box{90, 84, kScreenWidth - 180, 344};
    draw::roundedRect(r, box, SDL_Color{24, 30, 50, 225}, SDL_Color{255, 255, 255, 170}, SDL_Color{0, 0, 0, 100});

    // Level switcher: < 1-3 COIN FOREST >
    char title[80];
    std::snprintf(title, sizeof(title), "%s  %s", id.c_str(), levelName.c_str());
    font.drawCentered(r, kScreenWidth / 2, box.y + 14, title, 3, ui::kYellow);
    const int nudge = static_cast<int>(std::lround(std::sin(time_ * 5.0f) * 3.0f));
    if (index_ > 0) font.drawShadowed(r, box.x + 14 - nudge, box.y + 14, "<", 3, ui::kWhite);
    if (index_ + 1 < static_cast<int>(ids_.size()))
        font.drawShadowed(r, box.x + box.w - 30 + nudge, box.y + 14, ">", 3, ui::kWhite);

    const LevelRecord& rec = p.record(id);
    const int rowY = box.y + 64;
    constexpr int rowH = 44;
    const SDL_Color medal[3] = {{246, 196, 62, 255}, {200, 206, 216, 255}, {214, 140, 82, 255}};
    for (int i = 0; i < Progress::kHighScoreCount; ++i) {
        const int y = rowY + i * rowH;
        const bool has = i < static_cast<int>(rec.highScores.size());
        draw::roundedRect(r, SDL_Rect{box.x + 20, y, box.w - 40, rowH - 8},
                          i % 2 ? SDL_Color{255, 255, 255, 18} : SDL_Color{255, 255, 255, 34}, SDL_Color{0, 0, 0, 0});
        // Rank badge.
        const SDL_Color badge = i < 3 ? medal[i] : SDL_Color{110, 116, 140, 255};
        draw::fillCircle(r, box.x + 46, y + (rowH - 8) / 2, 13, badge);
        char rank[4];
        std::snprintf(rank, sizeof(rank), "%d", i + 1);
        font.drawCentered(r, box.x + 46, y + (rowH - 8) / 2 - 8, rank, 2, ui::kInk, false);
        if (has) {
            const HighScore& hs = rec.highScores[static_cast<size_t>(i)];
            font.draw(r, box.x + 80, y + 6, hs.initials, 3, ui::kWhite);
            char score[24];
            ui::formatScore(score, sizeof(score), hs.score);
            font.draw(r, box.x + box.w - 40 - BitmapFont::textWidth(score, 3), y + 6, score, 3, ui::kYellow);
        } else {
            font.draw(r, box.x + 80, y + 9, "- - -", 2, ui::kGrey);
        }
    }

    char line[64];
    if (rec.bestTime > 0.0f) {
        char clock[16];
        ui::formatTime(clock, sizeof(clock), rec.bestTime, true);
        std::snprintf(line, sizeof(line), "BEST TIME  %s", clock);
    } else {
        std::snprintf(line, sizeof(line), unlocked ? "NO CLEAR YET - GO FOR IT!" : "LEVEL LOCKED");
    }
    font.drawCentered(r, kScreenWidth / 2, box.y + box.h - 36, line, 2, ui::kMint);
    menu::drawHints(r, font, "< > LEVEL    B BACK");
}

} // namespace pd
