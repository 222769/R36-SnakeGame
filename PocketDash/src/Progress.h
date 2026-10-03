#pragma once

#include <array>
#include <map>
#include <string>
#include <vector>

namespace pd {

class KeyValueStore;

// One high-score table row.
struct HighScore {
    std::string initials; // 3 letters
    int score = 0;
};

// Everything remembered about one level.
struct LevelRecord {
    bool cleared = false;
    int bestScore = 0;
    float bestTime = 0.0f;   // seconds, 0 = none yet
    unsigned starsMask = 0;  // bit i = star i found in some run
    int gems = 0;            // most gems found in one run
    int secrets = 0;         // most secrets found in one run
    bool goldenStar = false; // everything found in a single run
    std::vector<HighScore> highScores; // best first, at most kHighScoreCount
};

// The outcome of one cleared run.
struct ClearResult {
    int score = 0;
    float time = 0.0f;
    unsigned starsMask = 0;
    int gems = 0;
    int secrets = 0;
    bool goldenStar = false;
};

// What a clear improved (for "NEW BEST!" badges).
struct RecordUpdate {
    bool firstClear = false;
    bool newBestScore = false;
    bool newBestTime = false;
    bool newStars = false;
    bool newGems = false;
    bool newGolden = false;
    int highScoreRank = -1; // 0-based place the score qualifies for, -1 = none
};

// Hero outfits unlocked by gems found across all levels. Gems are never
// spent: reaching a total unlocks the outfit for good.
struct Outfit {
    const char* name;
    int gemsNeeded;
    unsigned cap, capDark, jacket, scarf; // 0xRRGGBB
};
constexpr int kOutfitCount = 6;
const Outfit& outfit(int index);

// Player progress: level unlocks, per-level records, high scores, gems and
// the chosen outfit. Persisted as save/progress.ini (see save/load). Has no
// SDL dependencies so it is unit-tested directly.
class Progress {
public:
    static constexpr int kHighScoreCount = 5;

    const LevelRecord& record(const std::string& levelId) const;
    // The first level of each world is always open; others open once the
    // previous level is cleared. Unknown ids (e.g. "test") are open.
    bool isUnlocked(const std::string& levelId) const;
    // First level of the world that is unlocked but not cleared, or the last
    // level when everything is done ("Play" on the main menu starts it).
    std::string continueLevel(int world) const;

    // Updates the records with a cleared run. High scores are entered
    // separately (they need initials): see addHighScore.
    RecordUpdate recordClear(const std::string& levelId, const ClearResult& result);
    // Place (0-based) `score` would take in the table, or -1.
    int highScoreRank(const std::string& levelId, int score) const;
    void addHighScore(const std::string& levelId, const std::string& initials, int score);

    int totalGems() const;
    int totalStars() const;
    int levelsCleared() const;
    int goldenStars() const;

    bool outfitUnlocked(int index) const;
    int selectedOutfit() const { return outfit_; }
    // Ignored if the outfit is still locked.
    void selectOutfit(int index);

    // Initials last typed, offered first next time.
    const std::string& lastInitials() const { return lastInitials_; }

    void load(const KeyValueStore& kv);
    void save(KeyValueStore& kv) const;

    // Removes everything (used by "erase save" and tests).
    void reset();

private:
    std::map<std::string, LevelRecord> records_;
    int outfit_ = 0;
    std::string lastInitials_ = "AAA";
};

// Sample progress for screenshots and demos (--demo-progress): the first
// levels cleared with records, high scores and some gems.
Progress demoProgress();

// Makes 3 upper-case letters out of anything ("ab" -> "ABA").
std::string sanitizeInitials(const std::string& s);

} // namespace pd
