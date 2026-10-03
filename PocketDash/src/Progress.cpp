#include "Progress.h"

#include "LevelLoader.h"
#include "SaveManager.h"
#include "World.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace pd {

namespace {

const Outfit kOutfits[kOutfitCount] = {
    {"CLASSIC", 0, 0xCE383C, 0x96242C, 0x3A70BA, 0xF2B034},
    {"FOREST", 1, 0x3E8E4A, 0x2A6234, 0x8A5A36, 0xF0E2B6},
    {"OCEAN", 2, 0x2A9AB0, 0x1C6C7E, 0x2C3E70, 0xF4F6F8},
    {"SUNSET", 4, 0xF08A2C, 0xB0601A, 0x6A4A9C, 0xF29AB8},
    {"BERRY", 6, 0xB8367E, 0x84245A, 0x4A3A5E, 0x8EE0B8},
    {"GOLDEN", 8, 0xF2C230, 0xB48A1C, 0xF4F0E6, 0xD2383C},
};

const LevelRecord kEmptyRecord{};

// "level.1-1.best_score" style keys.
std::string key(const std::string& id, const char* field) { return "level." + id + "." + field; }

// Index of a level in its world and the world number, or false.
bool locate(const std::string& id, int& world, size_t& index) {
    for (int w = 1; w <= kWorldCount; ++w) {
        const auto& ids = levels::worldLevelIds(w);
        const auto it = std::find(ids.begin(), ids.end(), id);
        if (it != ids.end()) {
            world = w;
            index = static_cast<size_t>(it - ids.begin());
            return true;
        }
    }
    return false;
}

int popcount(unsigned v) {
    int n = 0;
    for (; v; v &= v - 1) ++n;
    return n;
}

} // namespace

const Outfit& outfit(int index) { return kOutfits[std::clamp(index, 0, kOutfitCount - 1)]; }

std::string sanitizeInitials(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
        if (c >= 'A' && c <= 'Z') out += c;
        if (out.size() == 3) break;
    }
    while (out.size() < 3) out += 'A';
    return out;
}

const LevelRecord& Progress::record(const std::string& levelId) const {
    const auto it = records_.find(levelId);
    return it == records_.end() ? kEmptyRecord : it->second;
}

bool Progress::isUnlocked(const std::string& levelId) const {
    int world = 0;
    size_t index = 0;
    if (!locate(levelId, world, index)) return true;
    if (index == 0) return true;
    return record(levels::worldLevelIds(world)[index - 1]).cleared;
}

std::string Progress::continueLevel(int world) const {
    const auto& ids = levels::worldLevelIds(world);
    if (ids.empty()) return {};
    for (const auto& id : ids)
        if (isUnlocked(id) && !record(id).cleared) return id;
    return ids.back();
}

RecordUpdate Progress::recordClear(const std::string& levelId, const ClearResult& r) {
    LevelRecord& rec = records_[levelId];
    RecordUpdate u;
    u.firstClear = !rec.cleared;
    rec.cleared = true;
    if (r.score > rec.bestScore) {
        u.newBestScore = rec.bestScore > 0; // the first score is not a "new best"
        rec.bestScore = r.score;
    }
    if (r.time > 0.0f && (rec.bestTime <= 0.0f || r.time < rec.bestTime)) {
        u.newBestTime = rec.bestTime > 0.0f; // the first time is not a "new best"
        rec.bestTime = r.time;
    }
    if ((r.starsMask | rec.starsMask) != rec.starsMask) {
        u.newStars = true;
        rec.starsMask |= r.starsMask;
    }
    if (r.gems > rec.gems) {
        u.newGems = true;
        rec.gems = r.gems;
    }
    rec.secrets = std::max(rec.secrets, r.secrets);
    if (r.goldenStar && !rec.goldenStar) {
        u.newGolden = true;
        rec.goldenStar = true;
    }
    u.highScoreRank = highScoreRank(levelId, r.score);
    return u;
}

int Progress::highScoreRank(const std::string& levelId, int score) const {
    if (score <= 0) return -1;
    const auto& table = record(levelId).highScores;
    for (size_t i = 0; i < table.size(); ++i)
        if (score > table[i].score) return static_cast<int>(i);
    return table.size() < static_cast<size_t>(kHighScoreCount) ? static_cast<int>(table.size()) : -1;
}

void Progress::addHighScore(const std::string& levelId, const std::string& initials, int score) {
    const int rank = highScoreRank(levelId, score);
    if (rank < 0) return;
    auto& table = records_[levelId].highScores;
    table.insert(table.begin() + rank, HighScore{sanitizeInitials(initials), score});
    if (table.size() > static_cast<size_t>(kHighScoreCount)) table.resize(kHighScoreCount);
    lastInitials_ = sanitizeInitials(initials);
}

int Progress::totalGems() const {
    int n = 0;
    for (const auto& [id, rec] : records_) n += rec.gems;
    return n;
}

int Progress::totalStars() const {
    int n = 0;
    for (const auto& [id, rec] : records_) n += popcount(rec.starsMask);
    return n;
}

int Progress::levelsCleared() const {
    int n = 0;
    for (const auto& [id, rec] : records_) n += rec.cleared ? 1 : 0;
    return n;
}

int Progress::goldenStars() const {
    int n = 0;
    for (const auto& [id, rec] : records_) n += rec.goldenStar ? 1 : 0;
    return n;
}

bool Progress::outfitUnlocked(int index) const {
    return index >= 0 && index < kOutfitCount && totalGems() >= outfit(index).gemsNeeded;
}

void Progress::selectOutfit(int index) {
    if (outfitUnlocked(index)) outfit_ = index;
}

void Progress::reset() {
    records_.clear();
    outfit_ = 0;
    lastInitials_ = "AAA";
}

void Progress::load(const KeyValueStore& kv) {
    reset();
    // Collect the level ids that appear in "level.<id>.<field>" keys.
    for (const auto& [k, v] : kv.entries()) {
        if (k.rfind("level.", 0) != 0) continue;
        const size_t dot = k.rfind('.');
        if (dot <= 6) continue;
        const std::string id = k.substr(6, dot - 6);
        if (records_.count(id)) continue;
        LevelRecord rec;
        rec.cleared = kv.getBool(key(id, "cleared"), false);
        rec.bestScore = std::max(0, kv.getInt(key(id, "best_score"), 0));
        rec.bestTime = static_cast<float>(std::max(0, kv.getInt(key(id, "best_time_ms"), 0))) / 1000.0f;
        rec.starsMask = static_cast<unsigned>(std::clamp(kv.getInt(key(id, "stars"), 0), 0, 0xFFFF));
        rec.gems = std::clamp(kv.getInt(key(id, "gems"), 0), 0, 99);
        rec.secrets = std::clamp(kv.getInt(key(id, "secrets"), 0), 0, 99);
        rec.goldenStar = kv.getBool(key(id, "golden"), false);
        for (int i = 0; i < kHighScoreCount; ++i) {
            char field[16];
            std::snprintf(field, sizeof(field), "hs%d", i + 1);
            // "ABC 12345"
            const std::string row = kv.getString(key(id, field));
            if (row.size() < 5 || row[3] != ' ') continue;
            const int score = std::atoi(row.c_str() + 4);
            if (score > 0) rec.highScores.push_back(HighScore{sanitizeInitials(row.substr(0, 3)), score});
        }
        std::sort(rec.highScores.begin(), rec.highScores.end(),
                  [](const HighScore& a, const HighScore& b) { return a.score > b.score; });
        records_[id] = std::move(rec);
    }
    lastInitials_ = sanitizeInitials(kv.getString("initials", "AAA"));
    outfit_ = 0;
    selectOutfit(kv.getInt("outfit", 0));
}

void Progress::save(KeyValueStore& kv) const {
    kv.clear();
    kv.set("outfit", outfit_);
    kv.set("initials", lastInitials_);
    for (const auto& [id, rec] : records_) {
        kv.set(key(id, "cleared"), rec.cleared);
        kv.set(key(id, "best_score"), rec.bestScore);
        kv.set(key(id, "best_time_ms"), static_cast<int>(std::lround(rec.bestTime * 1000.0f)));
        kv.set(key(id, "stars"), static_cast<int>(rec.starsMask));
        kv.set(key(id, "gems"), rec.gems);
        kv.set(key(id, "secrets"), rec.secrets);
        kv.set(key(id, "golden"), rec.goldenStar);
        for (size_t i = 0; i < rec.highScores.size(); ++i) {
            char field[16];
            std::snprintf(field, sizeof(field), "hs%d", static_cast<int>(i) + 1);
            kv.set(key(id, field), rec.highScores[i].initials + " " + std::to_string(rec.highScores[i].score));
        }
    }
}

Progress demoProgress() {
    Progress p;
    const struct {
        const char* id;
        int score;
        float time;
        unsigned stars;
        int gems, secrets;
        bool golden;
    } runs[] = {
        {"1-1", 9850, 41.3f, 7, 1, 1, true},
        {"1-2", 7420, 58.6f, 5, 1, 0, false},
        {"1-3", 6610, 72.1f, 3, 0, 0, false},
        {"1-4", 8120, 66.4f, 7, 1, 1, false},
    };
    const char* names[] = {"ZOE", "MAX", "ANA", "LEO", "KAI"};
    for (const auto& run : runs) {
        p.recordClear(run.id, ClearResult{run.score, run.time, run.stars, run.gems, run.secrets, run.golden});
        for (int i = 0; i < 4; ++i) p.addHighScore(run.id, names[(i + run.score) % 5], run.score - i * 1370 - (run.score % 7) * 11);
    }
    p.selectOutfit(1);
    return p;
}

} // namespace pd
